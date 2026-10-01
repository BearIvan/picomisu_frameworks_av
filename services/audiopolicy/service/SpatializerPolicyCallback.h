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

#ifndef ANDROID_MEDIA_SPATIALIZER_POLICY_CALLBACK_H
#define ANDROID_MEDIA_SPATIALIZER_POLICY_CALLBACK_H

#include <system/audio.h>
#include <utils/Errors.h>

namespace android {

// PICO OS 5.13.7 spatializer backport: interface of the audio policy service used by the
// Spatializer (in Android 13 it is declared in Spatializer.h; it is kept apart here so that
// AudioPolicyService.h does not need the spatializer dependencies).
class SpatializerPolicyCallback {
public:
    /** Called when a stage output is added or removed */
    virtual void onCheckSpatializer() = 0;
    // PICO: AudioFlinger requests of the spatializer, forwarded by the audio policy service.
    /** Enables or disables the spatialization of the track portId of the spatializer output */
    virtual status_t onSetSpatializationEnabled(audio_io_handle_t output,
                                                audio_port_handle_t portId, bool enabled) = 0;
    /** Recreates the track portId of the output (to move it to or from the spatializer) */
    virtual status_t onInvalidateTrack(audio_io_handle_t output, audio_port_handle_t portId) = 0;
    /** Sets the mixer configuration (channel mask) of the spatializer output */
    virtual status_t onSetMixerConfig(audio_io_handle_t output,
                                      const audio_config_base_t& config) = 0;
    virtual ~SpatializerPolicyCallback() = default;
};

}  // namespace android

#endif  // ANDROID_MEDIA_SPATIALIZER_POLICY_CALLBACK_H
