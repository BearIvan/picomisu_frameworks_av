/*
 * Copyright (C) 2021 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ANDROID_MEDIA_SPATIALIZER_H
#define ANDROID_MEDIA_SPATIALIZER_H

// PICO OS 5.13.7: backport of the Android 13 audio policy service Spatializer, reconstructed
// from the factory libaudiopolicyservice.so. Besides the Android 13 ISpatializer API, the
// factory spatializer tracks the playback clients of the audio policy service to choose which
// of them are spatialized on the spatializer output (per player request, AudioFlinger track
// spatialization, mixer channel mask) and implements the PICO per-player API (player session,
// dynamic spatialization, content orientation and pose) used by libspatialaudio.

#include <algorithm>
#include <iterator>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string.h>
#include <vector>

#include <android/media/BnSpatializer.h>
#include <android/media/INativeSpatializerCallback.h>
#include <android/media/ISpatializerHeadTrackingCallback.h>
#include <android/media/SpatializationLevel.h>
#include <android/media/SpatializationMode.h>
#include <android/media/SpatializerHeadTrackingMode.h>
#include <media/AudioEffect.h>
#include <media/audiohal/EffectHalInterface.h>
#include <media/stagefright/foundation/ALooper.h>
#include <system/audio_effect.h>

#include "SpatializerPolicyCallback.h"
#include "SpatializerPoseController.h"

namespace android {


// ----------------------------------------------------------------------------

/**
 * The Spatializer class implements all functional controlling the multichannel spatializer
 * with head tracking implementation in the native audio service: audio policy and audio flinger.
 * It presents an AIDL interface available to the java audio service to discover the availability
 * of the feature and options, control its state and register an active head tracking sensor.
 * It maintains the current state of the platform spatializer and applies the stored parameters
 * when the spatializer engine is created and enabled.
 * Based on the requested spatializer level, it will request the creation of a specialized output
 * mixer to the audio policy service which will in turn notify the Spatializer of the output
 * stream on which a spatializer engine should be created, configured and enabled.
 * The spatializer also hosts the head tracking management logic. This logic receives the
 * desired head tracking mode and selected head tracking sensor, registers a sensor event listener
 * and derives the compounded head pose information to the spatializer engine.
 *
 * Workflow:
 * - Initialization: when the audio policy service starts, it checks if a spatializer effect
 * engine exists and if the audio policy manager reports a dedicated spatializer output profile.
 * If both conditions are met, a Spatializer object is created
 * - Capabilities discovery: AudioService will call AudioSystem::canBeSpatialized() and if true,
 * acquire an ISpatializer interface with AudioSystem::getSpatializer(). This interface
 * will be used to query the implementation capabilities and configure the spatializer.
 * - Enabling: when ISpatializer::setLevel() sets a level different from NONE the spatializer
 * is considered enabled. The audio policy callback onCheckSpatializer() is called. This
 * triggers a request to audio policy manager to open a spatialization output stream and a
 * spatializer mixer is created in the audio flinger. When an output is returned by audio policy
 * manager, Spatializer::attachOutput() is called which creates and enables the spatializer
 * stage engine on the specified output.
 * - Disabling: when the spatialization level is set to NONE, the spatializer is considered
 * disabled. The audio policy callback onCheckSpatializer() is called. This triggers a call
 * to Spatializer::detachOutput() and the spatializer engine is released. Then a request is
 * made to audio policy manager to release and close the spatializer output stream and the
 * spatializer mixer thread is destroyed.
 */

class Spatializer : public media::BnSpatializer,
                    public IBinder::DeathRecipient,
                    private SpatializerPoseController::Listener {
  public:
    static sp<Spatializer> create(SpatializerPolicyCallback *callback);

           ~Spatializer() override;

    /** RefBase */
    void onFirstRef() override;

    /** ISpatializer, see ISpatializer.aidl */
    binder::Status release() override;
    binder::Status getSupportedLevels(std::vector<uint8_t>* levels) override;
    binder::Status setLevel(int8_t level) override;
    binder::Status getLevel(int8_t *level) override;
    binder::Status isHeadTrackingSupported(bool *supports) override;
    binder::Status getSupportedHeadTrackingModes(std::vector<uint8_t>* modes) override;
    binder::Status setDesiredHeadTrackingMode(int8_t mode) override;
    binder::Status getActualHeadTrackingMode(int8_t* mode) override;
    binder::Status recenterHeadTracker() override;
    binder::Status setGlobalTransform(const std::vector<float>& screenToStage) override;
    binder::Status setHeadSensor(int sensorHandle) override;
    binder::Status setScreenSensor(int sensorHandle) override;
    binder::Status setDisplayOrientation(float physicalToLogicalAngle) override;
    binder::Status setHingeAngle(float hingeAngle) override;
    binder::Status getSupportedModes(std::vector<uint8_t>* modes) override;
    binder::Status registerHeadTrackingCallback(
        const sp<media::ISpatializerHeadTrackingCallback>& callback) override;
    binder::Status setParameter(int key, const std::vector<unsigned char>& value) override;
    binder::Status getParameter(int key, std::vector<unsigned char> *value) override;
    binder::Status getOutput(int *output) override;
    // PICO: per-player spatial audio controls (libspatialaudio)
    binder::Status releasePlayer(int64_t playerToken) override;
    binder::Status setPlayerSessionId(int64_t playerToken, int32_t sessionId) override;
    binder::Status enableSpatialization(int64_t playerToken, bool enable,
                                        int32_t* result) override;
    binder::Status isSpatializationEnabled(int64_t playerToken, int32_t* result) override;
    binder::Status setAudioOrientation(int64_t playerToken, float rotX, float rotY, float rotZ,
                                       float rotW, int32_t* result) override;
    binder::Status setAudioPose(int64_t playerToken, float rotX, float rotY, float rotZ,
                                float rotW, float posX, float posY, float posZ,
                                int32_t* result) override;

    /** IBinder::DeathRecipient. Listen to the death of the INativeSpatializerCallback. */
    void binderDied(const wp<IBinder>& who) override;

    /** Registers a INativeSpatializerCallback when a client is attached to this Spatializer
     * by audio policy service.
     */
    status_t registerCallback(const sp<media::INativeSpatializerCallback>& callback);

    status_t loadEngineConfiguration(sp<EffectHalInterface> effect);

    /** Level getter for use by local classes. */
    int8_t getLevel() const { std::lock_guard<std::mutex> lock(mLock); return mLevel; }

    /** Called by audio policy service when the special output mixer dedicated to spatialization
     * is opened and the spatializer engine must be created.
     */
    status_t attachOutput(audio_io_handle_t output);
    /** Called by audio policy service when the special output mixer dedicated to spatialization
     * is closed and the spatializer engine must be release.
     */
    audio_io_handle_t detachOutput();
    /** Returns the output stream the spatializer is attached to. */
    audio_io_handle_t getOutput() const { std::lock_guard<std::mutex> lock(mLock); return mOutput; }

    /** Gets the channel mask, sampling rate and format set for the spatializer input. */
    audio_config_base_t getAudioInConfig() const;

    // PICO: playback clients of the audio policy service.
    /** Spatialization state set with enableSpatialization() for the player of this pid and
     * session, if any. */
    std::optional<bool> getDynamicSpatializationState(pid_t pid, audio_session_t session);
    /** Called by audio policy service when a playback client is created (getOutputForAttr()) */
    void onSetOutputForAttr(const audio_attributes_t& attributes, const audio_config_t& config,
                            audio_io_handle_t io, uid_t uid, pid_t pid, audio_session_t session,
                            audio_port_handle_t portId, audio_stream_type_t stream,
                            bool isSpatialized);
    /** Called by audio policy service when a playback client starts, stops, is released */
    void onStartOutput(audio_port_handle_t portId);
    void onStopOutput(audio_port_handle_t portId);
    void onReleaseOutput(audio_port_handle_t portId);
    /** Called by audio policy service when the client process pid dies */
    void releasePlayers(pid_t pid);

    status_t dump(int fd, const Vector<String16>& args) override;

private:
    Spatializer(effect_descriptor_t engineDescriptor,
                     SpatializerPolicyCallback *callback);

    /** SpatializerPoseController::Listener */
    void onHeadPoseUpdate(const std::vector<float>& headPose) override;

    // PICO: playback client of the audio policy service (between getOutputForAttr() and
    // releaseOutput()).
    struct AudioPlaybackClient {
        AudioPlaybackClient(const audio_attributes_t& attributes, const audio_config_t& config,
                            audio_io_handle_t io, uid_t uid, pid_t pid, audio_session_t session,
                            audio_port_handle_t portId, audio_stream_type_t stream,
                            bool requestedSpatialization) :
            attributes(attributes), config(config), io(io), uid(uid), pid(pid),
            session(session), portId(portId), stream(stream),
            requestedSpatialization(requestedSpatialization) {}

        const audio_attributes_t attributes;
        const audio_config_t config;
        const audio_io_handle_t io;
        const uid_t uid;
        const pid_t pid;
        const audio_session_t session;
        const audio_port_handle_t portId;
        const audio_stream_type_t stream;
        bool active = false;                    // started
        bool requestedSpatialization;           // to be spatialized on the spatializer output
        bool spatialized = false;               // spatialized by the spatializer mixer
    };

    // PICO: content position and orientation of a player.
    struct Position {
        float x;
        float y;
        float z;
    };
    struct Orientation {
        float x;
        float y;
        float z;
        float w;
    };

    // PICO: spatial audio state of a player (libspatialaudio player token).
    struct SpatialInfo {
        pid_t pid = 0;
        audio_session_t session = static_cast<audio_session_t>(-1);
        std::optional<Position> position;
        std::optional<Orientation> orientation;
        std::optional<bool> spatializationEnabled;
    };

    /**
     * Get parameters from spatializer engine HAL
     */
    template<bool MULTI_VALUES, typename T>
    status_t getHalParameter(sp<EffectHalInterface> effect, uint32_t type,
                                          std::vector<T> *values) {
        static_assert(sizeof(T) <= sizeof(uint32_t), "The size of T must less than 32 bits");

        uint32_t cmd[sizeof(effect_param_t) / sizeof(uint32_t) + 1];
        uint32_t reply[sizeof(effect_param_t) / sizeof(uint32_t) + 2 + kMaxEffectParamValues];

        effect_param_t *p = (effect_param_t *)cmd;
        p->psize = sizeof(uint32_t);
        if (MULTI_VALUES) {
            p->vsize = (kMaxEffectParamValues + 1) * sizeof(T);
        } else {
            p->vsize = sizeof(T);
        }
        *(uint32_t *)p->data = type;
        uint32_t replySize = sizeof(effect_param_t) + p->psize + p->vsize;

        status_t status = effect->command(EFFECT_CMD_GET_PARAM,
                                          sizeof(effect_param_t) + sizeof(uint32_t), cmd,
                                          &replySize, reply);
        if (status != NO_ERROR) {
            return status;
        }
        p = (effect_param_t *)reply;
        if (p->status != NO_ERROR) {
            return p->status;
        }
        if (replySize <
                sizeof(effect_param_t) + sizeof(uint32_t) + (MULTI_VALUES ? 2 : 1) * sizeof(T)) {
            return BAD_VALUE;
        }

        T *params = (T *)((uint8_t *)reply + sizeof(effect_param_t) + sizeof(uint32_t));
        int numParams = 1;
        if (MULTI_VALUES) {
            numParams = (int)*params++;
        }
        if (numParams > kMaxEffectParamValues) {
            return BAD_VALUE;
        }
        (*values).clear();
        std::copy(&params[0], &params[numParams], back_inserter(*values));
        return NO_ERROR;
    }

    /**
     * Set parameters on spatializer engine AudioEffect
     */
    template<typename T>
    status_t setEffectParameter_l(uint32_t type, const std::vector<T>& values) {
        static_assert(sizeof(T) <= sizeof(uint32_t), "The size of T must less than 32 bits");

        uint32_t cmd[sizeof(effect_param_t) / sizeof(uint32_t) + 1 + values.size()];
        effect_param_t *p = (effect_param_t *)cmd;
        p->psize = sizeof(uint32_t);
        p->vsize = sizeof(T) * values.size();
        *(uint32_t *)p->data = type;
        memcpy((uint32_t *)p->data + 1, values.data(), sizeof(T) * values.size());

        status_t status = mEngine->setParameter(p);
        if (status != NO_ERROR) {
            return status;
        }
        if (p->status != NO_ERROR) {
            return p->status;
        }
        return NO_ERROR;
    }

    /**
     * Get parameters from spatializer engine AudioEffect
     */
    template<typename T>
    status_t getEffectParameter_l(uint32_t type, std::vector<T> *values) {
        static_assert(sizeof(T) <= sizeof(uint32_t), "The size of T must less than 32 bits");

        uint32_t cmd[sizeof(effect_param_t) / sizeof(uint32_t) + 1 + values->size()];
        effect_param_t *p = (effect_param_t *)cmd;
        p->psize = sizeof(uint32_t);
        p->vsize = sizeof(T) * values->size();
        *(uint32_t *)p->data = type;

        status_t status = mEngine->getParameter(p);

        if (status != NO_ERROR) {
            return status;
        }
        if (p->status != NO_ERROR) {
            return p->status;
        }

        int numValues = std::min(p->vsize / sizeof(T), values->size());
        (*values).clear();
        T *retValues = (T *)((uint8_t *)p->data + sizeof(uint32_t));
        for (int i = 0; i < numValues; i++) {
            (*values).push_back(retValues[i]);
        }

        return NO_ERROR;
    }

    void checkPoseController_l();
    void checkEngineState_l();

    // PICO
    void updateActiveTracks_l();
    void updateActiveClientsSpatialization_l();
    void enableClientSpatialization_l(std::shared_ptr<AudioPlaybackClient>& client);
    void postAudioOrientation(const Orientation& orientation);
    // Engine callback handler messages
    void onHeadPoseMsg(const std::vector<float>& headPose);
    void onAudioOrientationMsg(const std::vector<float>& orientation);
    void onReleasePlayerMsg();

    /** Effect engine descriptor */
    const effect_descriptor_t mEngineDescriptor;
    /** Callback interface to parent audio policy service */
    SpatializerPolicyCallback* const mPolicyCallback;

    /** Mutex protecting internal state */
    mutable std::mutex mLock;

    /** Client AudioEffect for the engine */
    sp<AudioEffect> mEngine;
    /** Output stream the spatializer mixer thread is attached to */
    audio_io_handle_t mOutput = AUDIO_IO_HANDLE_NONE;

    /** Callback interface to the client (AudioService) controlling this`Spatializer */
    sp<media::INativeSpatializerCallback> mSpatializerCallback;

    /** Callback interface for head tracking */
    sp<media::ISpatializerHeadTrackingCallback> mHeadTrackingCallback;

    /** Requested spatialization level */
    int8_t mLevel = media::ISpatializationLevel::NONE;

    /** Control logic for head-tracking, etc. */
    std::shared_ptr<SpatializerPoseController> mPoseController;

    /** Last requested head tracking mode (PICO: also reported as the actual mode) */
    int8_t mDesiredHeadTrackingMode = media::ISpatializerHeadTrackingMode::DISABLED;

    std::vector<uint8_t> mLevels;
    std::vector<uint8_t> mHeadTrackingModes;
    std::vector<uint8_t> mSpatializationModes;
    std::vector<audio_channel_mask_t> mChannelMasks;
    bool mSupportsHeadTracking = false;

    // Looper thread for mEngine callbacks
    class EngineCallbackHandler;

    sp<ALooper> mLooper;
    sp<EngineCallbackHandler> mHandler;

    size_t mNumActiveTracks = 0;

    // PICO: playback clients by port id, spatial audio state of the players by player token
    std::map<audio_port_handle_t, std::shared_ptr<AudioPlaybackClient>> mClients;
    std::map<int64_t, SpatialInfo> mSpatialInfos;

    // PICO: mixer configuration of the spatializer output (channel mask of the spatialized
    // clients), sent to AudioFlinger with onSetMixerConfig()
    audio_config_base_t mMixerConfig = { 48000, AUDIO_CHANNEL_OUT_5POINT1, AUDIO_FORMAT_DEFAULT };

    static constexpr int kMaxEffectParamValues = 10;

    // Head pose (rotation x, y, z, w, translation x, y, z) and orientation (rotation x, y, z, w)
    // keys of the engine callback handler messages
    static const std::vector<const char *> sHeadPoseKeys;
    static const std::vector<const char *> sAudioOrientationKeys;
};


}; // namespace android

#endif // ANDROID_MEDIA_SPATIALIZER_H
