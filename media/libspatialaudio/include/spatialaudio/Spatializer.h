// Copyright 2026 Picomisu contributors
// SPDX-License-Identifier: Apache-2.0

#ifndef SPATIALAUDIO_SPATIALIZER_H
#define SPATIALAUDIO_SPATIALIZER_H

#include <stdint.h>

#include <list>
#include <memory>
#include <mutex>

#include <binder/IBinder.h>
#include <utils/Errors.h>
#include <utils/StrongPointer.h>

namespace android {
namespace media {
class ISpatializer;
}
}

namespace spatial {

// PICO OS 5.13.7 libspatialaudio: process-wide client of the spatializer of the audio policy
// service (IAudioPolicyService::getSpatializer()). The per-player calls identify the player by
// a token (see SpatialAudio). The service is looked up lazily and cached; after the audio
// server died, the registered callbacks are asked to restore their state when AudioFlinger is
// connected again (AudioSystem::setAfConnectedCallback()).
class Spatializer {
public:
    class SpatializerCallback {
    public:
        virtual ~SpatializerCallback() = default;
        virtual void onReconnectService() = 0;
    };

    static void releasePlayer(int64_t token);
    static void setPlayerSessionId(int64_t token, int sessionId);
    static android::status_t enableSpatialization(int64_t token, bool enable);
    static bool isSpatializationEnabled(int64_t token);
    static android::status_t setAudioOrientation(int64_t token,
            float rotX, float rotY, float rotZ, float rotW);
    static android::status_t setAudioPose(int64_t token,
            float rotX, float rotY, float rotZ, float rotW, float posX, float posY, float posZ);

    static void addSpatializerCallback(const std::shared_ptr<SpatializerCallback>& callback);
    static void removeSpatializerCallback(const std::shared_ptr<SpatializerCallback>& callback);

private:
    class ServiceDeathRecipient : public android::IBinder::DeathRecipient {
    public:
        void binderDied(const android::wp<android::IBinder>& who) override;
    };

    static android::sp<android::media::ISpatializer> getSpatializer();
    static void onAfConnectedCallback();

    static std::mutex sLock;
    static android::sp<android::media::ISpatializer> sSpatializerService;
    static bool sServiceDied;
    static android::sp<ServiceDeathRecipient> sServiceDeathRecipient;
    static std::list<std::weak_ptr<SpatializerCallback>> sSpatializerCallbacks;
};

} // namespace spatial

#endif // SPATIALAUDIO_SPATIALIZER_H
