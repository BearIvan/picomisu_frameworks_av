// Copyright 2026 Picomisu contributors
// SPDX-License-Identifier: Apache-2.0

// PICO OS 5.13.7 libspatialaudio (spatial::SpatialAudio), reconstructed from the factory
// /system/lib64/libspatialaudio.so.

#define LOG_TAG "SpatialAudio"

#include <spatialaudio/SpatialAudio.h>

#include <unistd.h>

#include <log/log.h>
#include <spatialaudio/Spatializer.h>

namespace spatial {

using android::status_t;

class SpatializerCallback : public Spatializer::SpatializerCallback {
public:
    explicit SpatializerCallback(SpatialAudio* audio) : mAudio(audio) {
        ALOGD("SpatializerCallback %p", this);
    }
    ~SpatializerCallback() override = default;

    void onReconnectService() override;

private:
    friend class SpatialAudio;

    SpatialAudio* mAudio;
    std::mutex mLock;
};

int errFromStatus(status_t status) {
    switch (status) {
        case android::OK:
            return 0;
        case android::BAD_VALUE:
            return -4;
        case android::UNKNOWN_ERROR:
            return -1;
        default:
            ALOGW("Uncaught error type 0x%x", status);
            return -1;
    }
}

void SpatializerCallback::onReconnectService() {
    std::lock_guard<std::mutex> lock(mLock);
    if (mAudio != nullptr) {
        mAudio->onReconnectService();
    }
}

void SpatialAudio::onReconnectService() {
    ALOGD("onReconnectService %p %d", this, mSessionId);
    std::lock_guard<std::mutex> lock(mLock);
    if (mSessionId < 1) {
        return;
    }
    Spatializer::setPlayerSessionId(mToken, mSessionId);
    if (mOrientation.has_value()) {
        if (mPosition.has_value()) {
            ALOGD("onReconnectService %p has audio pos (%f %f %f %f), (%f %f %f)", this,
                    mOrientation->x, mOrientation->y, mOrientation->z, mOrientation->w,
                    mPosition->x, mPosition->y, mPosition->z);
            Spatializer::setAudioPose(mToken,
                    mOrientation->x, mOrientation->y, mOrientation->z, mOrientation->w,
                    mPosition->x, mPosition->y, mPosition->z);
        } else {
            ALOGD("onReconnectService %p has audio ori %f %f %f %f", this,
                    mOrientation->x, mOrientation->y, mOrientation->z, mOrientation->w);
            Spatializer::setAudioOrientation(mToken,
                    mOrientation->x, mOrientation->y, mOrientation->z, mOrientation->w);
        }
    }
    if (mSpatializationEnabled.has_value()) {
        ALOGD("onReconnectService %p has enableSpatial %d", this, *mSpatializationEnabled);
        Spatializer::enableSpatialization(mToken, *mSpatializationEnabled);
    }
}

SpatialAudio::SpatialAudio()
    : mToken((static_cast<int64_t>(getpid()) << 32)
            | (reinterpret_cast<uintptr_t>(this) & 0xffffffffU)),
      mSessionId(-1) {
    ALOGD("SpatialAudio create %p", this);
    mCallback = std::make_shared<SpatializerCallback>(this);
    Spatializer::addSpatializerCallback(mCallback);
}

SpatialAudio::~SpatialAudio() {
    ALOGD("~SpatialAudio %p", this);
    mSessionId = -1;
    {
        std::lock_guard<std::mutex> lock(mCallback->mLock);
        mCallback->mAudio = nullptr;
    }
    Spatializer::removeSpatializerCallback(mCallback);
    Spatializer::releasePlayer(mToken);
}

void SpatialAudio::setSessionId(int sessionId) {
    ALOGD("%s token 0x%08x sessionId %d, %p", __func__, static_cast<uint32_t>(mToken),
            sessionId, mCallback.get());
    mSessionId = sessionId;
    Spatializer::setPlayerSessionId(mToken, sessionId);
}

int SpatialAudio::setSpatializationEnabled(bool enabled) {
    if (mSessionId < 1) {
        return -3;
    }
    std::lock_guard<std::mutex> lock(mLock);
    mSpatializationEnabled = enabled;
    return errFromStatus(Spatializer::enableSpatialization(mToken, enabled));
}

bool SpatialAudio::isSpatializationEnabled() {
    if (mSessionId < 1) {
        return false;
    }
    return Spatializer::isSpatializationEnabled(mToken);
}

int SpatialAudio::setAudioOrientation(float rotX, float rotY, float rotZ, float rotW) {
    if (mSessionId < 1) {
        return -3;
    }
    std::lock_guard<std::mutex> lock(mLock);
    mOrientation = Orientation{rotX, rotY, rotZ, rotW};
    return errFromStatus(Spatializer::setAudioOrientation(mToken, rotX, rotY, rotZ, rotW));
}

int SpatialAudio::setAudioPose(float rotX, float rotY, float rotZ, float rotW,
        float posX, float posY, float posZ) {
    if (mSessionId < 1) {
        return -3;
    }
    std::lock_guard<std::mutex> lock(mLock);
    mPosition = Position{posX, posY, posZ};
    mOrientation = Orientation{rotX, rotY, rotZ, rotW};
    return errFromStatus(Spatializer::setAudioPose(mToken,
            rotX, rotY, rotZ, rotW, posX, posY, posZ));
}

} // namespace spatial
