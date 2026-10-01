/*
 * Copyright (C) 2026 Picomisu contributors
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

#ifndef ANDROID_HAPTIC_EFFECT_H
#define ANDROID_HAPTIC_EFFECT_H

// PICO: state of the Phoenix VCMotor (voice coil motor) haptic effects of the factory
// PICO OS 5.13.7 AudioFlinger (HapticEffect.cpp). AudioFlinger::setParameters() and
// getParameters() store and report the session of the haptic effect player through
// "key_setHapticEffectSessionId" / "key_getHapticEffectSessionId"; the media player
// service (MediaPlayerService::AudioOutput) reads it back to make the AudioTrack of that
// session a VCMotor track.
//
// Factory layout (function-local static of getInstance(), inlined in setParameters() and
// getParameters(), destroyed by the implicit ~HapticEffect()):
//   0x00 int      session id (static storage, not written by the constructor)
//   0x08 sp<>     null
//   0x10 int      not written by the constructor
//   0x14 status_t NO_INIT (-19)
//   0x18 16 bytes not written by the constructor
//   0x28 String8  empty
// Only the session id is used in the factory libaudioflinger: no code reads or writes the
// other fields (the library is linked with hidden visibility and section garbage
// collection, so functions of HapticEffect.cpp that were never called are gone). They are
// kept so that the object matches the factory one.

#include <stdint.h>

#include <utils/Errors.h>
#include <utils/RefBase.h>
#include <utils/String8.h>

namespace android {

class HapticEffect {
public:
    // function-local static instance, as in the factory setParameters()/getParameters()
    static HapticEffect& getInstance() {
        static HapticEffect instance;
        return instance;
    }

    void setSessionId(int sessionId) { mSessionId = sessionId; }
    int getSessionId() const { return mSessionId; }

private:
    HapticEffect() = default;

    int mSessionId = 0;                       // 0x00
    // Unused factory fields (see above). The type of the sp<> is unknown: a RefBase
    // subclass whose RefBase is its primary base.
    sp<RefBase> mUnknown08;                   // 0x08
    int mUnknown10 __unused = 0;              // 0x10
    status_t mUnknown14 __unused = NO_INIT;   // 0x14
    uint8_t mUnknown18[16] __unused = {};     // 0x18
    String8 mUnknown28;                       // 0x28
};

} // namespace android

#endif // ANDROID_HAPTIC_EFFECT_H
