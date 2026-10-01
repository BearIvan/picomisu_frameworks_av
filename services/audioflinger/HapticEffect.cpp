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

// PICO: HapticEffect.cpp of the factory PICO OS 5.13.7 libaudioflinger.
//
// All that is left of this file in the factory library is a file scope std::mutex (0xe66a8),
// constant initialised, whose destructor _GLOBAL__sub_I_HapticEffect.cpp registers with
// __cxa_atexit. No code references it: the functions that used it were never called and were
// removed by the linker. HapticEffect::getInstance() and its accessors are inline (see
// HapticEffect.h).

#include <mutex>

#include "HapticEffect.h"

namespace android {
namespace {

// factory 0xe66a8, unused
std::mutex sHapticEffectLock;

} // namespace
} // namespace android
