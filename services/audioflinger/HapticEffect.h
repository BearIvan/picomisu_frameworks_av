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
// "key_setHapticEffectSessionId" / "key_getHapticEffectSessionId".

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

    int mSessionId = 0;         // 0x00
};

} // namespace android

#endif // ANDROID_HAPTIC_EFFECT_H
