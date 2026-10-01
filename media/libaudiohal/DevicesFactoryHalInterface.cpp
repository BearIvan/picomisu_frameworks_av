/*
 * Copyright (C) 2017 The Android Open Source Project
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

#include <android/hardware/audio/5.0/IDevicesFactory.h>

#include <libaudiohal/FactoryHalHidl.h>

namespace android {

// static
// PICO: the factory PICO OS 5.13.7 libaudiohal (0x1034) only knows the audio HAL 5.0 (the HAL of
// the device); it links and ships no libaudiohal@2.0/@4.0.
sp<DevicesFactoryHalInterface> DevicesFactoryHalInterface::create() {
    if (hardware::audio::V5_0::IDevicesFactory::getService() != nullptr) {
        return V5_0::createDevicesFactoryHal();
    }
    return nullptr;
}

} // namespace android
