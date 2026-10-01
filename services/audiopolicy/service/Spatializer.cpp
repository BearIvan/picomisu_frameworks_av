/*
**
** Copyright 2021, The Android Open Source Project
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/


#define LOG_TAG "Spatializer"
//#define LOG_NDEBUG 0
#include <utils/Log.h>

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>

#include <string>

#include <binder/IPCThreadState.h>
#include <cutils/properties.h>
#include <media/AudioEffect.h>
#include <media/PicoAudioDefs.h>
#include <media/audiohal/EffectsFactoryHalInterface.h>
#include <media/stagefright/foundation/AHandler.h>
#include <media/stagefright/foundation/AMessage.h>
#include <utils/String16.h>
#include <utils/ThreadDefs.h>

#include "Spatializer.h"

namespace android {

using binder::Status;

// ---------------------------------------------------------------------------

class Spatializer::EngineCallbackHandler : public AHandler {
public:
    explicit EngineCallbackHandler(wp<Spatializer> spatializer)
            : mSpatializer(spatializer) {
    }

    // PICO: messages of the per-player spatial audio controls and of the pose controller
    // (the Android 13 messages 0 to 2 are not used).
    enum {
        kWhatOnHeadPose = 3,            // head pose from the pose controller
        kWhatOnAudioOrientation = 4,    // content orientation of a spatialized player
        kWhatOnAudioPose = 5,           // content orientation and position of a player
        kWhatOnReleasePlayer = 6,       // a player with an orientation or position is released
    };

    void onMessageReceived(const sp<AMessage> &msg) override {
        switch (msg->what()) {
            case kWhatOnHeadPose: {
                sp<Spatializer> spatializer = mSpatializer.promote();
                if (spatializer == nullptr) {
                    ALOGW("%s: Cannot promote spatializer", __func__);
                    return;
                }
                std::vector<float> headPose(sHeadPoseKeys.size());
                for (size_t i = 0 ; i < sHeadPoseKeys.size(); i++) {
                    if (!msg->findFloat(sHeadPoseKeys[i], &headPose[i])) {
                        ALOGE("%s: Cannot find kTranslation0Key!", __func__);
                        return;
                    }
                }
                spatializer->onHeadPoseMsg(headPose);
                break;
            }
            case kWhatOnAudioOrientation: {
                sp<Spatializer> spatializer = mSpatializer.promote();
                if (spatializer == nullptr) {
                    ALOGW("%s: Cannot promote spatializer", __func__);
                    return;
                }
                std::vector<float> orientation(sAudioOrientationKeys.size());
                for (size_t i = 0 ; i < sAudioOrientationKeys.size(); i++) {
                    if (!msg->findFloat(sAudioOrientationKeys[i], &orientation[i])) {
                        ALOGE("%s: Cannot find kTranslation0Key!", __func__);
                        return;
                    }
                }
                spatializer->onAudioOrientationMsg(orientation);
                break;
            }
            case kWhatOnAudioPose: {
                sp<Spatializer> spatializer = mSpatializer.promote();
                if (spatializer == nullptr) {
                    ALOGW("%s: Cannot promote spatializer", __func__);
                    return;
                }
                std::vector<float> pose(sHeadPoseKeys.size());
                for (size_t i = 0 ; i < sHeadPoseKeys.size(); i++) {
                    if (!msg->findFloat(sHeadPoseKeys[i], &pose[i])) {
                        ALOGE("%s: Cannot find kTranslation0Key!", __func__);
                        return;
                    }
                }
                spatializer->onAudioOrientationMsg(pose);
                break;
            }
            case kWhatOnReleasePlayer: {
                sp<Spatializer> spatializer = mSpatializer.promote();
                if (spatializer == nullptr) {
                    ALOGW("%s: Cannot promote spatializer", __func__);
                    return;
                }
                spatializer->onReleasePlayerMsg();
                break;
            }
            default:
                LOG_ALWAYS_FATAL("Invalid callback message %d", msg->what());
        }
    }
private:
    wp<Spatializer> mSpatializer;
};

const std::vector<const char *> Spatializer::sHeadPoseKeys = {
    "rotationX", "rotationY", "rotationZ", "rotationW",
    "translationX", "translationY", "translationZ"
};

const std::vector<const char *> Spatializer::sAudioOrientationKeys = {
    "rotationX", "rotationY", "rotationZ", "rotationW"
};

// ---------------------------------------------------------------------------

namespace {

// PICO: the effects factory HAL of this release has no getDescriptors(type): the factory looks
// the spatializer up among all the effects and keeps the first one of type FX_IID_SPATIALIZER.
status_t getSpatializerDescriptors(const sp<EffectsFactoryHalInterface>& effectsFactoryHal,
                                   std::vector<effect_descriptor_t> *descriptors)
{
    uint32_t numEffects = 0;
    status_t status = effectsFactoryHal->queryNumberEffects(&numEffects);
    if (status < 0) {
        ALOGW("getEffectDescriptor() error %d from FactoryHal queryNumberEffects", status);
        return status;
    }
    for (uint32_t i = 0; i < numEffects; i++) {
        effect_descriptor_t descriptor;
        status = effectsFactoryHal->getDescriptor(i, &descriptor);
        if (status < 0) {
            ALOGW("getEffectDescriptor() error %d from FactoryHal getDescriptor", status);
            continue;
        }
        if (memcmp(&descriptor.type, FX_IID_SPATIALIZER, sizeof(effect_uuid_t)) == 0) {
            descriptors->push_back(descriptor);
            return NO_ERROR;
        }
    }
    ALOGW("getEffectDescriptor(): Effect not found by type.");
    return NAME_NOT_FOUND;
}

// Channel masks the spatializer engine accepts (positional, at least quad).
bool isChannelMaskSpatialized(audio_channel_mask_t channelMask)
{
    return audio_channel_mask_get_representation(channelMask)
                == AUDIO_CHANNEL_REPRESENTATION_POSITION
            && (channelMask & AUDIO_CHANNEL_OUT_QUAD) == AUDIO_CHANNEL_OUT_QUAD;
}

// PICO: ambisonic channel counts, (order + 1)^2 channels optionally followed by a non-diegetic
// stereo pair: 4, 6, 9, 11, 16, 18, 25 and 27.
bool isValidAmbisonicChannelCount(uint32_t channelCount)
{
    return channelCount <= 27 && ((1u << channelCount) & 0x0a050a50u) != 0;
}

}  // namespace

// ---------------------------------------------------------------------------
sp<Spatializer> Spatializer::create(SpatializerPolicyCallback *callback) {
    sp<Spatializer> spatializer;

    sp<EffectsFactoryHalInterface> effectsFactoryHal = EffectsFactoryHalInterface::create();
    if (effectsFactoryHal == nullptr) {
        ALOGW("%s failed to create effect factory interface", __func__);
        return spatializer;
    }

    std::vector<effect_descriptor_t> descriptors;
    status_t status = getSpatializerDescriptors(effectsFactoryHal, &descriptors);
    if (status != NO_ERROR) {
        ALOGW("%s failed to get spatializer descriptor, error %d", __func__, status);
        return spatializer;
    }
    ALOG_ASSERT(!descriptors.empty(),
            "%s getDescriptors() returned no error but empty list", __func__);

    //TODO: get supported spatialization modes from FX engine or descriptor

    sp<EffectHalInterface> effect;
    status = effectsFactoryHal->createEffect(&descriptors[0].uuid, AUDIO_SESSION_OUTPUT_STAGE,
            AUDIO_IO_HANDLE_NONE, &effect);
    ALOGI("%s FX create status %d effect %p", __func__, status, effect.get());

    if (status == NO_ERROR && effect != nullptr) {
        spatializer = new Spatializer(descriptors[0], callback);
        if (spatializer->loadEngineConfiguration(effect) != NO_ERROR) {
            spatializer.clear();
        }
    }

    return spatializer;
}

Spatializer::Spatializer(effect_descriptor_t engineDescriptor, SpatializerPolicyCallback *callback)
    : mEngineDescriptor(engineDescriptor),
      mPolicyCallback(callback) {
    ALOGV("%s", __func__);
}

void Spatializer::onFirstRef() {
    mLooper = new ALooper;
    mLooper->setName("Spatializer-looper");
    mLooper->start(
            /*runOnCallingThread*/false,
            /*canCallJava*/ false,
            PRIORITY_AUDIO);

    mHandler = new EngineCallbackHandler(this);
    mLooper->registerHandler(mHandler);
}

Spatializer::~Spatializer() {
    ALOGV("%s", __func__);
    if (mLooper != nullptr) {
        mLooper->stop();
        mLooper->unregisterHandler(mHandler->id());
    }
    mLooper.clear();
    mHandler.clear();
}

status_t Spatializer::loadEngineConfiguration(sp<EffectHalInterface> effect) {
    ALOGV("%s", __func__);

    std::vector<bool> supportsHeadTracking;
    status_t status = getHalParameter<false>(effect, SPATIALIZER_PARAM_HEADTRACKING_SUPPORTED,
                                         &supportsHeadTracking);
    if (status != NO_ERROR) {
        ALOGW("%s: cannot get SPATIALIZER_PARAM_HEADTRACKING_SUPPORTED", __func__);
        return status;
    }
    mSupportsHeadTracking = supportsHeadTracking[0];

    std::vector<int8_t> spatializationLevels;
    status = getHalParameter<true>(effect, SPATIALIZER_PARAM_SUPPORTED_LEVELS,
            &spatializationLevels);
    if (status != NO_ERROR) {
        ALOGW("%s: cannot get SPATIALIZER_PARAM_SUPPORTED_LEVELS", __func__);
        return status;
    }
    bool noneLevelFound = false;
    bool activeLevelFound = false;
    for (const auto spatializationLevel : spatializationLevels) {
        if (spatializationLevel == media::ISpatializationLevel::NONE) {
            noneLevelFound = true;
        } else {
            activeLevelFound = true;
        }
        // we don't detect duplicates.
        mLevels.emplace_back(spatializationLevel);
    }
    if (!noneLevelFound || !activeLevelFound) {
        ALOGW("%s: SPATIALIZER_PARAM_SUPPORTED_LEVELS must include NONE"
                " and another valid level",  __func__);
        return BAD_VALUE;
    }

    std::vector<int8_t> spatializationModes;
    status = getHalParameter<true>(effect, SPATIALIZER_PARAM_SUPPORTED_SPATIALIZATION_MODES,
            &spatializationModes);
    if (status != NO_ERROR) {
        ALOGW("%s: cannot get SPATIALIZER_PARAM_SUPPORTED_SPATIALIZATION_MODES", __func__);
        return status;
    }
    for (const auto spatializationMode : spatializationModes) {
        // we don't detect duplicates.
        mSpatializationModes.emplace_back(spatializationMode);
    }
    if (mSpatializationModes.empty()) {
        ALOGW("%s: SPATIALIZER_PARAM_SUPPORTED_SPATIALIZATION_MODES reports empty", __func__);
        return BAD_VALUE;
    }

    std::vector<audio_channel_mask_t> channelMasks;
    status = getHalParameter<true>(effect, SPATIALIZER_PARAM_SUPPORTED_CHANNEL_MASKS,
                                 &channelMasks);
    if (status != NO_ERROR) {
        ALOGW("%s: cannot get SPATIALIZER_PARAM_SUPPORTED_CHANNEL_MASKS", __func__);
        return status;
    }
    for (const auto channelMask : channelMasks) {
        if (!isChannelMaskSpatialized(channelMask)) {
            ALOGW("%s: ignoring channelMask:%#x", __func__, channelMask);
            continue;
        }
        // we don't detect duplicates.
        mChannelMasks.emplace_back(channelMask);
    }
    if (mChannelMasks.empty()) {
        ALOGW("%s: SPATIALIZER_PARAM_SUPPORTED_CHANNEL_MASKS reports empty", __func__);
        return BAD_VALUE;
    }

    // Currently we expose only RELATIVE_WORLD.
    // This is a limitation of the head tracking library based on a UX choice.
    mHeadTrackingModes.push_back(media::ISpatializerHeadTrackingMode::DISABLED);
    if (mSupportsHeadTracking) {
        mHeadTrackingModes.push_back(media::ISpatializerHeadTrackingMode::RELATIVE_WORLD);
    }
    return NO_ERROR;
}

/** Gets the channel mask, sampling rate and format set for the spatializer input. */
audio_config_base_t Spatializer::getAudioInConfig() const {
    std::lock_guard<std::mutex> lock(mLock);
    audio_config_base_t config = AUDIO_CONFIG_BASE_INITIALIZER;
    // For now use highest supported channel count
    uint32_t maxCount = 0;
    for ( auto mask : mChannelMasks) {
        const uint32_t count = audio_channel_count_from_out_mask(mask);
        if (count > maxCount) {
            config.channel_mask = mask;
            maxCount = count;
        }
    }
    return config;
}

status_t Spatializer::registerCallback(
        const sp<media::INativeSpatializerCallback>& callback) {
    std::lock_guard<std::mutex> lock(mLock);
    if (callback == nullptr) {
        return BAD_VALUE;
    }

    sp<IBinder> binder = IInterface::asBinder(callback);
    status_t status = binder->linkToDeath(this);
    if (status == NO_ERROR) {
        mSpatializerCallback = callback;
    }
    ALOGV("%s status %d", __func__, status);
    return status;
}

// IBinder::DeathRecipient
void Spatializer::binderDied(__unused const wp<IBinder> &who) {
    {
        std::lock_guard<std::mutex> lock(mLock);
        mLevel = media::ISpatializationLevel::NONE;
        mSpatializerCallback.clear();
    }
    ALOGV("%s", __func__);
    mPolicyCallback->onCheckSpatializer();
}

// ISpatializer
Status Spatializer::getSupportedLevels(std::vector<uint8_t> *levels) {
    ALOGV("%s", __func__);
    if (levels == nullptr) {
        return Status::fromStatusT(BAD_VALUE);
    }
    levels->insert(levels->end(), mLevels.begin(), mLevels.end());
    return Status::ok();
}

Status Spatializer::setLevel(int8_t level) {
    ALOGV("%s level %d", __func__, (int)level);
    if (level != media::ISpatializationLevel::NONE
            && std::find(mLevels.begin(), mLevels.end(), (uint8_t)level) == mLevels.end()) {
        return Status::fromStatusT(BAD_VALUE);
    }
    sp<media::INativeSpatializerCallback> callback;
    bool levelChanged = false;
    {
        std::lock_guard<std::mutex> lock(mLock);
        levelChanged = mLevel != level;
        mLevel = level;
        callback = mSpatializerCallback;

        if (levelChanged && mEngine != nullptr) {
            checkEngineState_l();
        }
    }

    if (levelChanged) {
        mPolicyCallback->onCheckSpatializer();
        if (callback != nullptr) {
            callback->onLevelChanged(level);
        }
    }
    return Status::ok();
}

Status Spatializer::getLevel(int8_t *level) {
    if (level == nullptr) {
        return Status::fromStatusT(BAD_VALUE);
    }
    std::lock_guard<std::mutex> lock(mLock);
    *level = mLevel;
    ALOGV("%s level %d", __func__, (int)*level);
    return Status::ok();
}

Status Spatializer::isHeadTrackingSupported(bool *supports) {
    ALOGV("%s mSupportsHeadTracking %d", __func__, mSupportsHeadTracking);
    if (supports == nullptr) {
        return Status::fromStatusT(BAD_VALUE);
    }
    std::lock_guard<std::mutex> lock(mLock);
    *supports = mSupportsHeadTracking;
    return Status::ok();
}

Status Spatializer::getSupportedHeadTrackingModes(std::vector<uint8_t>* modes) {
    std::lock_guard<std::mutex> lock(mLock);
    ALOGV("%s", __func__);
    if (modes == nullptr) {
        return Status::fromStatusT(BAD_VALUE);
    }
    modes->insert(modes->end(), mHeadTrackingModes.begin(), mHeadTrackingModes.end());
    return Status::ok();
}

Status Spatializer::setDesiredHeadTrackingMode(int8_t mode) {
    ALOGV("%s mode %d", __func__, (int)mode);

    if (!mSupportsHeadTracking) {
        return Status::fromStatusT(INVALID_OPERATION);
    }
    std::lock_guard<std::mutex> lock(mLock);
    switch (mode) {
        case media::ISpatializerHeadTrackingMode::OTHER:
            return Status::fromStatusT(BAD_VALUE);
        case media::ISpatializerHeadTrackingMode::DISABLED:
        case media::ISpatializerHeadTrackingMode::RELATIVE_WORLD:
        case media::ISpatializerHeadTrackingMode::RELATIVE_SCREEN:
            // PICO: no head tracking processor, the desired mode is the actual mode
            mDesiredHeadTrackingMode = mode;
            break;
        default:
            break;
    }

    checkPoseController_l();

    return Status::ok();
}

Status Spatializer::getActualHeadTrackingMode(int8_t *mode) {
    if (mode == nullptr) {
        return Status::fromStatusT(BAD_VALUE);
    }
    std::lock_guard<std::mutex> lock(mLock);
    *mode = mDesiredHeadTrackingMode;
    ALOGV("%s mode %d", __func__, (int)*mode);
    return Status::ok();
}

// PICO: the head tracking of the factory is driven by the PICO tracking service (see
// SpatializerPoseController), the Android 13 sensor and transform controls are no-ops.
Status Spatializer::recenterHeadTracker() {
    return Status::ok();
}

Status Spatializer::setGlobalTransform(const std::vector<float>& screenToStage __unused) {
    return Status::ok();
}

Status Spatializer::release() {
    ALOGV("%s", __func__);
    bool levelChanged = false;
    {
        std::lock_guard<std::mutex> lock(mLock);
        if (mSpatializerCallback == nullptr) {
            return Status::fromStatusT(INVALID_OPERATION);
        }

        sp<IBinder> binder = IInterface::asBinder(mSpatializerCallback);
        binder->unlinkToDeath(this);
        mSpatializerCallback.clear();

        levelChanged = mLevel != media::ISpatializationLevel::NONE;
        mLevel = media::ISpatializationLevel::NONE;
    }

    if (levelChanged) {
        mPolicyCallback->onCheckSpatializer();
    }
    return Status::ok();
}

Status Spatializer::setHeadSensor(int sensorHandle __unused) {
    return Status::ok();
}

Status Spatializer::setScreenSensor(int sensorHandle __unused) {
    return Status::ok();
}

Status Spatializer::setDisplayOrientation(float physicalToLogicalAngle __unused) {
    return Status::ok();
}

Status Spatializer::setHingeAngle(float hingeAngle) {
    std::lock_guard<std::mutex> lock(mLock);
    ALOGV("%s hingeAngle %f", __func__, hingeAngle);
    if (mEngine != nullptr) {
        setEffectParameter_l(SPATIALIZER_PARAM_HINGE_ANGLE, std::vector<float>{hingeAngle});
    }
    return Status::ok();
}

Status Spatializer::getSupportedModes(std::vector<uint8_t> *modes) {
    ALOGV("%s", __func__);
    if (modes == nullptr) {
        return Status::fromStatusT(BAD_VALUE);
    }
    *modes = mSpatializationModes;
    return Status::ok();
}

Status Spatializer::registerHeadTrackingCallback(
        const sp<media::ISpatializerHeadTrackingCallback>& callback) {
    ALOGV("%s callback %p", __func__, callback.get());
    std::lock_guard<std::mutex> lock(mLock);
    if (!mSupportsHeadTracking) {
        return Status::fromStatusT(INVALID_OPERATION);
    }
    mHeadTrackingCallback = callback;
    return Status::ok();
}

Status Spatializer::setParameter(int key, const std::vector<unsigned char>& value) {
    ALOGV("%s key %d", __func__, key);
    std::lock_guard<std::mutex> lock(mLock);
    status_t status = INVALID_OPERATION;
    if (mEngine != nullptr) {
        status = setEffectParameter_l(key, value);
    }
    return Status::fromStatusT(status);
}

Status Spatializer::getParameter(int key, std::vector<unsigned char> *value) {
    ALOGV("%s key %d value size %d", __func__, key,
          (value != nullptr ? (int)value->size() : -1));
    if (value == nullptr) {
        return Status::fromStatusT(BAD_VALUE);
    }
    std::lock_guard<std::mutex> lock(mLock);
    status_t status = INVALID_OPERATION;
    if (mEngine != nullptr) {
        ALOGV("%s key %d mEngine %p", __func__, key, mEngine.get());
        status = getEffectParameter_l(key, value);
    }
    return Status::fromStatusT(status);
}

Status Spatializer::getOutput(int *output) {
    ALOGV("%s", __func__);
    if (output == nullptr) {
        // As in Android 13 and the factory, the error status is not returned (sic).
        Status::fromStatusT(BAD_VALUE);
    }
    std::lock_guard<std::mutex> lock(mLock);
    *output = mOutput;
    ALOGV("%s got output %d", __func__, *output);
    return Status::ok();
}

// PICO: per-player spatial audio controls (libspatialaudio). Players are identified by the
// libspatialaudio player token and matched to the playback clients by pid and audio session.

Status Spatializer::releasePlayer(int64_t playerToken) {
    bool recenter = false;
    {
        std::lock_guard<std::mutex> lock(mLock);
        if (mSpatialInfos.find(playerToken) == mSpatialInfos.end()) {
            ALOGD("no spatial info for  0x%08x", (uint32_t)playerToken);
            return Status::ok();
        }
        const SpatialInfo& info = mSpatialInfos[playerToken];
        recenter = info.position.has_value() || info.orientation.has_value();
        mSpatialInfos.erase(playerToken);
    }
    if (recenter) {
        sp<AMessage> msg = new AMessage(EngineCallbackHandler::kWhatOnReleasePlayer, mHandler);
        msg->post();
    }
    return Status::ok();
}

Status Spatializer::setPlayerSessionId(int64_t playerToken, int32_t sessionId) {
    ALOGD("setPlayerSessionId playerToken 0x%08x sessionId %d", (uint32_t)playerToken, sessionId);
    const pid_t pid = IPCThreadState::self()->getCallingPid();
    std::lock_guard<std::mutex> lock(mLock);
    SpatialInfo& info = mSpatialInfos[playerToken];
    info.pid = pid;
    info.session = static_cast<audio_session_t>(sessionId);
    return Status::ok();
}

Status Spatializer::enableSpatialization(int64_t playerToken, bool enable, int32_t* result) {
    ALOGD("enableSpatialization token 0x%08x enable %d", (uint32_t)playerToken, enable);
    std::lock_guard<std::mutex> lock(mLock);
    SpatialInfo& info = mSpatialInfos[playerToken];
    info.spatializationEnabled = enable;
    if (info.session > 0) {
        for (const auto& entry : mClients) {
            std::shared_ptr<AudioPlaybackClient> client = entry.second;
            if (client->session != info.session && client->pid != info.pid) {
                continue;
            }
            ALOGD("port %d session %d io %d, spatialOutput %d",
                  client->portId, info.session, client->io, mOutput);
            if (client->io == mOutput) {
                if (client->requestedSpatialization != enable) {
                    client->requestedSpatialization = enable;
                    mPolicyCallback->onSetSpatializationEnabled(client->io, entry.first, enable);
                    if (enable) {
                        if (!client->spatialized && client->active) {
                            enableClientSpatialization_l(client);
                        }
                    } else {
                        client->spatialized = false;
                    }
                }
            } else {
                client->requestedSpatialization = enable;
                if (enable && mOutput != AUDIO_IO_HANDLE_NONE) {
                    // move the track to the spatializer output
                    ALOGI("invalid track %d in output %d", info.session, client->io);
                    mPolicyCallback->onInvalidateTrack(client->io, client->portId);
                }
            }
        }
    }
    *result = 0;
    return Status::ok();
}

Status Spatializer::isSpatializationEnabled(int64_t playerToken, int32_t* result) {
    ALOGD("isSpatializationEnabled playerToken 0x%08x", (uint32_t)playerToken);
    *result = 0;
    std::lock_guard<std::mutex> lock(mLock);
    if (mSpatialInfos.find(playerToken) == mSpatialInfos.end()) {
        return Status::ok();
    }
    const SpatialInfo& info = mSpatialInfos[playerToken];
    if (info.session <= 0) {
        return Status::ok();
    }
    for (const auto& entry : mClients) {
        std::shared_ptr<AudioPlaybackClient> client = entry.second;
        if (client->session == info.session && client->spatialized) {
            *result = 1;
            break;
        }
    }
    ALOGD("isSpatializationEnabled enable %d", *result);
    return Status::ok();
}

Status Spatializer::setAudioOrientation(int64_t playerToken, float rotX, float rotY, float rotZ,
                                        float rotW, int32_t* result) {
    const Orientation orientation = { rotX, rotY, rotZ, rotW };
    std::unique_lock<std::mutex> lock(mLock);
    SpatialInfo& info = mSpatialInfos[playerToken];
    info.orientation = orientation;
    if (info.session > 0) {
        for (const auto& entry : mClients) {
            std::shared_ptr<AudioPlaybackClient> client = entry.second;
            if (client->spatialized && client->session == info.session
                    && client->pid == info.pid) {
                lock.unlock();
                postAudioOrientation(orientation);
                *result = 0;
                return Status::ok();
            }
        }
    }
    *result = 0;
    return Status::ok();
}

Status Spatializer::setAudioPose(int64_t playerToken, float rotX, float rotY, float rotZ,
                                 float rotW, float posX, float posY, float posZ,
                                 int32_t* result) {
    std::unique_lock<std::mutex> lock(mLock);
    SpatialInfo& info = mSpatialInfos[playerToken];
    info.position = Position{ posX, posY, posZ };
    info.orientation = Orientation{ rotX, rotY, rotZ, rotW };
    if (info.session > 0) {
        for (const auto& entry : mClients) {
            std::shared_ptr<AudioPlaybackClient> client = entry.second;
            if (client->spatialized && client->session == info.session
                    && client->pid == info.pid) {
                lock.unlock();
                sp<AMessage> msg = new AMessage(EngineCallbackHandler::kWhatOnAudioPose, mHandler);
                msg->setFloat("rotationX", rotX);
                msg->setFloat("rotationY", rotY);
                msg->setFloat("rotationZ", rotZ);
                msg->setFloat("rotationW", rotW);
                msg->setFloat("translationX", posX);
                msg->setFloat("translationY", posY);
                msg->setFloat("translationZ", posZ);
                // PICO: the factory never posts this message (sic): the pose is only stored.
                *result = 0;
                return Status::ok();
            }
        }
    }
    *result = 0;
    return Status::ok();
}

void Spatializer::postAudioOrientation(const Orientation& orientation) {
    sp<AMessage> msg = new AMessage(EngineCallbackHandler::kWhatOnAudioOrientation, mHandler);
    msg->setFloat("rotationX", orientation.x);
    msg->setFloat("rotationY", orientation.y);
    msg->setFloat("rotationZ", orientation.z);
    msg->setFloat("rotationW", orientation.w);
    msg->post();
}

// SpatializerPoseController::Listener
void Spatializer::onHeadPoseUpdate(const std::vector<float>& headPose) {
    if (headPose.size() != sHeadPoseKeys.size()) {
        ALOGE("onHeadPoseUpdate pose item error");
        return;
    }
    sp<AMessage> msg = new AMessage(EngineCallbackHandler::kWhatOnHeadPose, mHandler);
    for (size_t i = 0 ; i < sHeadPoseKeys.size(); i++) {
        msg->setFloat(sHeadPoseKeys[i], headPose[i]);
    }
    msg->post();
}

void Spatializer::onHeadPoseMsg(const std::vector<float>& headPose) {
    std::lock_guard<std::mutex> lock(mLock);
    if (mEngine != nullptr) {
        setEffectParameter_l(SPATIALIZER_PARAM_LISTENER_POSE, headPose);
    }
}

void Spatializer::onAudioOrientationMsg(const std::vector<float>& orientation) {
    std::lock_guard<std::mutex> lock(mLock);
    if (mEngine != nullptr) {
        setEffectParameter_l(SPATIALIZER_PARAM_AUDIO_ORIENTATION, orientation);
    }
}

void Spatializer::onReleasePlayerMsg() {
    std::lock_guard<std::mutex> lock(mLock);
    if (mEngine != nullptr) {
        setEffectParameter_l(SPATIALIZER_PARAM_AUDIO_RECENTER, std::vector<int32_t>{1});
    }
}

status_t Spatializer::attachOutput(audio_io_handle_t output) {
    bool outputChanged = false;
    sp<media::INativeSpatializerCallback> callback;

    {
        std::lock_guard<std::mutex> lock(mLock);
        ALOGV("%s output %d mOutput %d", __func__, (int)output, (int)mOutput);
        if (mOutput != AUDIO_IO_HANDLE_NONE) {
            LOG_ALWAYS_FATAL_IF(mEngine == nullptr, "%s output set without FX engine", __func__);
            // remove FX instance
            mEngine->setEnabled(false);
            mEngine.clear();
            mPoseController.reset();
        }
        // create FX instance on output
        mEngine = new AudioEffect(nullptr /* type */, String16("android"),
                                  &mEngineDescriptor.uuid,
                                  0 /* priority */,
                                  nullptr /* cbf */,
                                  nullptr /* user */,
                                  AUDIO_SESSION_OUTPUT_STAGE,
                                  output);
        status_t status = mEngine->initCheck();
        ALOGV("%s mEngine create status %d", __func__, (int)status);
        if (status != NO_ERROR) {
            return status;
        }

        outputChanged = mOutput != output;
        mOutput = output;
        // PICO: spatialize the active clients of the new output, move the others
        updateActiveClientsSpatialization_l();
        size_t numActiveTracks = 0;
        for (const auto& entry : mClients) {
            const std::shared_ptr<AudioPlaybackClient>& client = entry.second;
            if (client->active && client->io == mOutput) {
                numActiveTracks += client->spatialized;
            }
        }
        mNumActiveTracks = numActiveTracks;
        checkEngineState_l();
        if (mSupportsHeadTracking) {
            checkPoseController_l();
        }
        callback = mSpatializerCallback;
    }

    if (outputChanged && callback != nullptr) {
        callback->onOutputChanged(output);
    }

    return NO_ERROR;
}

audio_io_handle_t Spatializer::detachOutput() {
    audio_io_handle_t output = AUDIO_IO_HANDLE_NONE;
    sp<media::INativeSpatializerCallback> callback;

    {
        std::lock_guard<std::mutex> lock(mLock);
        ALOGV("%s mOutput %d", __func__, (int)mOutput);
        if (mOutput == AUDIO_IO_HANDLE_NONE) {
            return output;
        }
        // remove FX instance
        mEngine->setEnabled(false);
        mEngine.clear();
        output = mOutput;
        mOutput = AUDIO_IO_HANDLE_NONE;
        callback = mSpatializerCallback;
    }

    if (callback != nullptr) {
        callback->onOutputChanged(AUDIO_IO_HANDLE_NONE);
    }
    return output;
}

void Spatializer::checkPoseController_l() {
    bool isControllerNeeded =
            mDesiredHeadTrackingMode != media::ISpatializerHeadTrackingMode::DISABLED
            && mNumActiveTracks > 0;

    if (isControllerNeeded) {
        if (mPoseController == nullptr) {
            mPoseController = std::make_shared<SpatializerPoseController>(
                    static_cast<SpatializerPoseController::Listener*>(this),
                    std::chrono::microseconds(property_get_int32(
                            "persist.audio.spatial_pose.update_period", 16000)),
                    std::chrono::microseconds(property_get_int32(
                            "persist.audio.spatial_pose.predict_time", 34000)));
            LOG_ALWAYS_FATAL_IF(mPoseController == nullptr,
                                "%s could not allocate pose controller", __func__);
        }
    } else if (mPoseController != nullptr) {
        mPoseController.reset();
    }
}

void Spatializer::checkEngineState_l() {
    if (mEngine != nullptr) {
        if (mLevel != media::ISpatializationLevel::NONE && mNumActiveTracks > 0) {
            mEngine->setEnabled(true);
            setEffectParameter_l(SPATIALIZER_PARAM_LEVEL,
                    std::vector<int8_t>{ mLevel });
            setEffectParameter_l(SPATIALIZER_PARAM_HEADTRACKING_MODE,
                    std::vector<int8_t>{ mDesiredHeadTrackingMode });
        } else {
            setEffectParameter_l(SPATIALIZER_PARAM_LEVEL,
                    std::vector<int8_t>{ media::ISpatializationLevel::NONE });
            mEngine->setEnabled(false);
        }
    }
}

// PICO: number of spatialized active clients of the spatializer output.
void Spatializer::updateActiveTracks_l() {
    int numActiveTracks = 0;
    for (const auto& entry : mClients) {
        const std::shared_ptr<AudioPlaybackClient> client = entry.second;
        if (client->active && client->io == mOutput) {
            numActiveTracks += client->spatialized;
        }
    }
    if (mNumActiveTracks != (size_t)numActiveTracks) {
        mNumActiveTracks = numActiveTracks;
        checkEngineState_l();
        checkPoseController_l();
    }
}

// PICO: called when the spatializer output changes: the active clients requesting
// spatialization on the spatializer output are spatialized, the active clients requesting
// spatialization on another output are invalidated (moved to the spatializer output).
void Spatializer::updateActiveClientsSpatialization_l() {
    for (const auto& entry : mClients) {
        std::shared_ptr<AudioPlaybackClient> client = entry.second;
        ALOGE("%s port %d session %d io %d, spatialOutput %d, active %d", __func__,
              client->portId, client->session, client->io, mOutput, client->active);
        if (!client->active) {
            continue;
        }
        if (client->io == mOutput) {
            mPolicyCallback->onSetSpatializationEnabled(client->io, entry.first,
                                                        client->requestedSpatialization);
            if (client->requestedSpatialization) {
                enableClientSpatialization_l(client);
            } else if (client->spatialized) {
                client->spatialized = false;
            }
        } else if (client->requestedSpatialization
                || (client->attributes.flags & AUDIO_FLAG_ALWAYS_SPATIALIZE) != 0) {
            ALOGD("invalid track %d in output %d", client->session, client->io);
            mPolicyCallback->onInvalidateTrack(client->io, client->portId);
        }
    }
}

// PICO: spatializes a client of the spatializer output. The first spatialized client sets the
// mixer channel mask (its positional mask, 5.1 for an index mask, the ambisonic index mask
// for ambisonic content); the next ones are spatialized if they match the mixer: positional
// clients with more channels widen it.
void Spatializer::enableClientSpatialization_l(std::shared_ptr<AudioPlaybackClient>& client) {
    size_t numSpatializedClients = 0;
    for (const auto& entry : mClients) {
        const std::shared_ptr<AudioPlaybackClient> c = entry.second;
        if (c->active && c->io == mOutput) {
            numSpatializedClients += c->spatialized;
        }
    }

    const bool ambisonic = (client->attributes.flags & AUDIO_FLAG_SPATIALIZE_AMBISONIC) != 0;
    const audio_channel_mask_t channelMask = client->config.channel_mask;
    if (numSpatializedClients == 0) {
        audio_channel_mask_t mixerChannelMask;
        bool spatialized = true;
        if (!ambisonic) {
            mixerChannelMask = audio_channel_mask_get_representation(channelMask)
                    == AUDIO_CHANNEL_REPRESENTATION_INDEX ? AUDIO_CHANNEL_OUT_5POINT1
                                                          : channelMask;
        } else if (!isValidAmbisonicChannelCount(
                audio_channel_count_from_out_mask(channelMask))) {
            ALOGW("channelMask 0x%x is not a valid ambisonic channels,use default mixerChannelMask",
                  channelMask);
            mixerChannelMask = AUDIO_CHANNEL_OUT_5POINT1;
            spatialized = false;
        } else {
            mixerChannelMask = audio_channel_mask_for_index_assignment_from_count(
                    audio_channel_count_from_out_mask(channelMask));
        }
        client->spatialized = spatialized;
        if (mixerChannelMask != mMixerConfig.channel_mask) {
            ALOGD("first client, update mixer channel mask 0x%x -> 0x%x",
                  mMixerConfig.channel_mask, mixerChannelMask);
            mMixerConfig.channel_mask = mixerChannelMask;
            mPolicyCallback->onSetMixerConfig(mOutput, mMixerConfig);
        }
    } else if (audio_channel_mask_get_representation(mMixerConfig.channel_mask)
            == AUDIO_CHANNEL_REPRESENTATION_INDEX) {
        // ambisonic mixer
        if (ambisonic) {
            const uint32_t channelCount = audio_channel_count_from_out_mask(channelMask);
            // The factory compares with the channel count of the first word of the mixer
            // configuration, the sample rate (sic).
            if (isValidAmbisonicChannelCount(channelCount)
                    && channelCount == audio_channel_count_from_out_mask(
                            static_cast<audio_channel_mask_t>(mMixerConfig.sample_rate))) {
                client->spatialized = true;
            }
        }
    } else if (!ambisonic) {
        // positional mixer
        if (audio_channel_mask_get_representation(channelMask)
                    == AUDIO_CHANNEL_REPRESENTATION_POSITION
                && channelMask != mMixerConfig.channel_mask
                && audio_channel_count_from_out_mask(channelMask)
                        > audio_channel_count_from_out_mask(mMixerConfig.channel_mask)) {
            ALOGI("update mixer channel mask 0x%x -> 0x%x",
                  mMixerConfig.channel_mask, channelMask);
            mMixerConfig.channel_mask = channelMask;
            mPolicyCallback->onSetMixerConfig(mOutput, mMixerConfig);
        }
        client->spatialized = true;
    }

    if (client->spatialized) {
        // restore the content orientation of the player
        for (const auto& entry : mSpatialInfos) {
            const SpatialInfo& info = entry.second;
            if (info.session == client->session && info.pid == client->pid
                    && info.orientation.has_value()) {
                postAudioOrientation(*info.orientation);
                return;
            }
        }
    }
}

std::optional<bool> Spatializer::getDynamicSpatializationState(pid_t pid,
                                                               audio_session_t session) {
    std::lock_guard<std::mutex> lock(mLock);
    for (const auto& entry : mSpatialInfos) {
        const SpatialInfo& info = entry.second;
        if (info.session == session && info.pid == pid) {
            return info.spatializationEnabled;
        }
    }
    return std::nullopt;
}

void Spatializer::onSetOutputForAttr(const audio_attributes_t& attributes,
                                     const audio_config_t& config, audio_io_handle_t io,
                                     uid_t uid, pid_t pid, audio_session_t session,
                                     audio_port_handle_t portId, audio_stream_type_t stream,
                                     bool isSpatialized) {
    std::shared_ptr<AudioPlaybackClient> client = std::make_shared<AudioPlaybackClient>(
            attributes, config, io, uid, pid, session, portId, stream, isSpatialized);
    ALOGD("onSetOutputForAttr portId %d, io %d", portId, io);
    std::lock_guard<std::mutex> lock(mLock);
    mClients[portId] = client;
    for (const auto& entry : mSpatialInfos) {
        const SpatialInfo& info = entry.second;
        if (info.pid == pid && info.session == session
                && info.spatializationEnabled.has_value()) {
            client->requestedSpatialization = *info.spatializationEnabled;
            break;
        }
    }
}

void Spatializer::onStartOutput(audio_port_handle_t portId) {
    ALOGD("onStartOutput in %d", portId);
    std::lock_guard<std::mutex> lock(mLock);
    auto it = mClients.find(portId);
    if (it == mClients.end()) {
        ALOGE("%s can't find client for portId %d", __func__, portId);
        return;
    }
    std::shared_ptr<AudioPlaybackClient> client = it->second;
    client->active = true;
    if (client->io == mOutput && client->requestedSpatialization) {
        enableClientSpatialization_l(client);
        updateActiveTracks_l();
    }
    ALOGD("onStartOutput out %d", portId);
}

void Spatializer::onStopOutput(audio_port_handle_t portId) {
    ALOGD("onStopOutput %d", portId);
    std::lock_guard<std::mutex> lock(mLock);
    auto it = mClients.find(portId);
    if (it == mClients.end()) {
        ALOGE("%s can't find client for portId %d", __func__, portId);
        return;
    }
    it->second->active = false;
    if (it->second->spatialized) {
        it->second->spatialized = false;
        updateActiveTracks_l();
    }
}

void Spatializer::onReleaseOutput(audio_port_handle_t portId) {
    ALOGD("onReleaseOutput %d", portId);
    std::lock_guard<std::mutex> lock(mLock);
    auto it = mClients.find(portId);
    if (it == mClients.end()) {
        ALOGE("%s can't find client for portId %d", __func__, portId);
        return;
    }
    if (it->second->active) {
        it->second->active = false;
        if (it->second->spatialized) {
            it->second->spatialized = false;
            updateActiveTracks_l();
        }
    }
    mClients.erase(it);
}

void Spatializer::releasePlayers(pid_t pid) {
    std::lock_guard<std::mutex> lock(mLock);
    bool recenter = false;
    for (auto it = mSpatialInfos.begin(); it != mSpatialInfos.end();) {
        if (it->second.pid == pid) {
            ALOGI("releasePlayers pid %d session %d", pid, it->second.session);
            if (it->second.position.has_value() || it->second.orientation.has_value()) {
                recenter = true;
            }
            it = mSpatialInfos.erase(it);
        } else {
            ++it;
        }
    }
    if (recenter) {
        sp<AMessage> msg = new AMessage(EngineCallbackHandler::kWhatOnReleasePlayer, mHandler);
        msg->post();
    }
}

status_t Spatializer::dump(int fd, const Vector<String16>& args __unused) {
    std::string result;
    std::unique_lock<std::mutex> lock(mLock);
    dprintf(fd, "Spatializer: mixerChannel 0x%x\n", mMixerConfig.channel_mask);
    for (const auto& entry : mClients) {
        const std::shared_ptr<AudioPlaybackClient> client = entry.second;
        result.append("io: " + std::to_string(client->io)
                + ", port: " + std::to_string(client->portId)
                + ", pid: " + std::to_string(client->pid)
                + ", session: " + std::to_string(client->session)
                + ", active: " + std::to_string(client->active)
                + ", reqSpatial: " + std::to_string(client->requestedSpatialization)
                + ", spatial: " + std::to_string(client->spatialized)
                + "\n");
    }
    result.append("\n");
    for (const auto& entry : mSpatialInfos) {
        const SpatialInfo& info = entry.second;
        char buffer[128];
        snprintf(buffer, sizeof(buffer), "Player: 0x%08x, pid: %d, ses: %d",
                 (uint32_t)entry.first, info.pid, info.session);
        result.append(buffer);
        if (info.spatializationEnabled.has_value()) {
            result.append(", ena " + std::to_string(*info.spatializationEnabled));
        } else {
            result.append(", ena N");
        }
        if (info.position.has_value()) {
            result.append(", pos " + std::to_string(info.position->x)
                    + " " + std::to_string(info.position->y)
                    + " " + std::to_string(info.position->z));
        } else {
            result.append(", pos N");
        }
        if (info.orientation.has_value()) {
            result.append(", ori " + std::to_string(info.orientation->x)
                    + " " + std::to_string(info.orientation->y)
                    + " " + std::to_string(info.orientation->z)
                    + " " + std::to_string(info.orientation->w));
        } else {
            result.append(", ori N");
        }
        result.append("\n");
    }
    lock.unlock();
    write(fd, result.c_str(), result.size());
    return NO_ERROR;
}

} // namespace android
