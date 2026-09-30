// Copyright 2026 Picomisu contributors
// SPDX-License-Identifier: Apache-2.0

#ifndef SPATIALAUDIO_SPATIALAUDIO_H
#define SPATIALAUDIO_SPATIALAUDIO_H

#include <stdint.h>

#include <memory>
#include <mutex>
#include <optional>

#include <utils/Errors.h>

namespace spatial {

// Forwards the reconnection of the spatializer service to its SpatialAudio (SpatialAudio.cpp).
class SpatializerCallback;

// Converts a status of the spatializer service to the result of the PlayerSpatialHelper API:
// 0 (OK), -4 (BAD_VALUE) or -1 (any other error, logged).
int errFromStatus(android::status_t status);

// PICO OS 5.13.7 libspatialaudio: spatial audio state of one player (android.media.
// PlayerSpatialHelperImpl, libmedia_jni). The player is identified at the spatializer by a
// token (pid << 32 | low 32 bits of this) and its audio session id; the last orientation,
// pose and spatialization state are kept to be restored when the audio server restarted.
// Results: 0 on success, -3 without audio session (session id < 1), -4 bad value, -1 other.
class SpatialAudio {
public:
    SpatialAudio();
    ~SpatialAudio();

    void setSessionId(int sessionId);
    int setSpatializationEnabled(bool enabled);
    bool isSpatializationEnabled();
    int setAudioOrientation(float rotX, float rotY, float rotZ, float rotW);
    int setAudioPose(float rotX, float rotY, float rotZ, float rotW,
            float posX, float posY, float posZ);

    void onReconnectService();

private:
    struct Orientation {
        float x;
        float y;
        float z;
        float w;
    };
    struct Position {
        float x;
        float y;
        float z;
    };

    int64_t mToken;
    int mSessionId;
    std::optional<Orientation> mOrientation;
    std::optional<Position> mPosition;
    std::optional<bool> mSpatializationEnabled;
    std::shared_ptr<SpatializerCallback> mCallback;
    std::mutex mLock;
};

} // namespace spatial

#endif // SPATIALAUDIO_SPATIALAUDIO_H
