// Copyright 2026 Picomisu contributors
// SPDX-License-Identifier: Apache-2.0

// PICO OS 5.13.7 libspatialaudio (spatial::Spatializer), reconstructed from the factory
// /system/lib64/libspatialaudio.so.

#define LOG_TAG "Spatializer"

#include <spatialaudio/Spatializer.h>

#include <set>

#include <android/media/ISpatializer.h>
#include <binder/IInterface.h>
#include <log/log.h>
#include <media/AudioSystem.h>
#include <media/IAudioPolicyService.h>

namespace spatial {

using android::AudioSystem;
using android::IAudioPolicyService;
using android::IBinder;
using android::IInterface;
using android::UNKNOWN_ERROR;
using android::sp;
using android::status_t;
using android::wp;
using android::binder::Status;
using android::media::ISpatializer;

std::mutex Spatializer::sLock;
sp<ISpatializer> Spatializer::sSpatializerService;
bool Spatializer::sServiceDied;
sp<Spatializer::ServiceDeathRecipient> Spatializer::sServiceDeathRecipient;
std::list<std::weak_ptr<Spatializer::SpatializerCallback>> Spatializer::sSpatializerCallbacks;

// Returns the cached spatializer of the audio policy service, or asks the audio policy
// service for it. There is no spatializer (nullptr) when the audio policy service has none;
// it is asked again at the next call then.
sp<ISpatializer> Spatializer::getSpatializer() {
    sp<ISpatializer> spatializer;
    sLock.lock();
    if (sSpatializerService != nullptr) {
        spatializer = sSpatializerService;
        sLock.unlock();
        return spatializer;
    }
    bool connected = false;
    {
        sp<IAudioPolicyService> aps = AudioSystem::get_audio_policy_service();
        spatializer = aps->getSpatializer();
        if (spatializer != nullptr) {
            sServiceDeathRecipient = new ServiceDeathRecipient();
            IInterface::asBinder(spatializer)->linkToDeath(sServiceDeathRecipient);
            sSpatializerService = spatializer;
            connected = true;
        }
    }
    spatializer = sSpatializerService;
    sLock.unlock();
    if (connected) {
        AudioSystem::setAfConnectedCallback(onAfConnectedCallback);
    }
    return spatializer;
}

void Spatializer::onAfConnectedCallback() {
    ALOGD("onAfConnectedCallback");
    std::set<std::shared_ptr<SpatializerCallback>> callbacks;
    sLock.lock();
    ALOGD("all callbacks %zu", sSpatializerCallbacks.size());
    if (!sServiceDied) {
        ALOGD("onAfConnectedCallback service is alive");
        sLock.unlock();
        return;
    }
    sServiceDied = false;
    for (auto it = sSpatializerCallbacks.begin(); it != sSpatializerCallbacks.end();) {
        std::shared_ptr<SpatializerCallback> callback = it->lock();
        if (callback == nullptr) {
            ALOGD("%s remove unavaiable callback", __func__);
            it = sSpatializerCallbacks.erase(it);
        } else {
            callbacks.insert(callback);
            ++it;
        }
    }
    sLock.unlock();
    for (const auto& callback : callbacks) {
        callback->onReconnectService();
    }
}

void Spatializer::releasePlayer(int64_t token) {
    sp<ISpatializer> spatializer = getSpatializer();
    if (spatializer == nullptr) {
        ALOGE("releasePlayer can't get spatializer service");
        return;
    }
    Status status = spatializer->releasePlayer(token);
    if (!status.isOk()) {
        ALOGE("releasePlayer falied");
    }
}

void Spatializer::setPlayerSessionId(int64_t token, int sessionId) {
    sp<ISpatializer> spatializer = getSpatializer();
    if (spatializer == nullptr) {
        ALOGE("setPlayerSessionId can't get spatializer service");
        return;
    }
    Status status = spatializer->setPlayerSessionId(token, sessionId);
    if (!status.isOk()) {
        ALOGE("setPlayerSessionId falied");
    }
}

status_t Spatializer::enableSpatialization(int64_t token, bool enable) {
    sp<ISpatializer> spatializer = getSpatializer();
    if (spatializer == nullptr) {
        return -1;
    }
    int32_t ret;
    Status status = spatializer->enableSpatialization(token, enable, &ret);
    return status.isOk() ? ret : UNKNOWN_ERROR;
}

bool Spatializer::isSpatializationEnabled(int64_t token) {
    sp<ISpatializer> spatializer = getSpatializer();
    if (spatializer == nullptr) {
        return false;
    }
    int32_t enabled;
    Status status = spatializer->isSpatializationEnabled(token, &enabled);
    return status.isOk() && enabled == 1;
}

status_t Spatializer::setAudioOrientation(int64_t token,
        float rotX, float rotY, float rotZ, float rotW) {
    sp<ISpatializer> spatializer = getSpatializer();
    if (spatializer == nullptr) {
        return -1;
    }
    int32_t ret;
    Status status = spatializer->setAudioOrientation(token, rotX, rotY, rotZ, rotW, &ret);
    return status.isOk() ? ret : UNKNOWN_ERROR;
}

status_t Spatializer::setAudioPose(int64_t token,
        float rotX, float rotY, float rotZ, float rotW, float posX, float posY, float posZ) {
    sp<ISpatializer> spatializer = getSpatializer();
    if (spatializer == nullptr) {
        return -1;
    }
    int32_t ret;
    Status status = spatializer->setAudioPose(token, rotX, rotY, rotZ, rotW,
            posX, posY, posZ, &ret);
    return status.isOk() ? ret : UNKNOWN_ERROR;
}

void Spatializer::ServiceDeathRecipient::binderDied(const wp<IBinder>& who __unused) {
    ALOGE("%s", __func__);
    sSpatializerService = nullptr;
    sServiceDied = true;
}

void Spatializer::addSpatializerCallback(const std::shared_ptr<SpatializerCallback>& callback) {
    std::lock_guard<std::mutex> lock(sLock);
    sSpatializerCallbacks.push_back(callback);
}

void Spatializer::removeSpatializerCallback(
        const std::shared_ptr<SpatializerCallback>& callback) {
    ALOGD("removeSpatializerCallback %p", callback.get());
    std::lock_guard<std::mutex> lock(sLock);
    for (auto it = sSpatializerCallbacks.begin(); it != sSpatializerCallbacks.end();) {
        std::shared_ptr<SpatializerCallback> registered = it->lock();
        if (registered == nullptr) {
            ALOGD("%s remove unavaiable callback", __func__);
            it = sSpatializerCallbacks.erase(it);
        } else if (registered == callback) {
            it = sSpatializerCallbacks.erase(it);
        } else {
            ++it;
        }
    }
}

} // namespace spatial
