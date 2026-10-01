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

#ifndef ANDROID_PICO_AUDIO_DEFS_H
#define ANDROID_PICO_AUDIO_DEFS_H

// PICO: definitions of the factory PICO OS 5.13.7 system/media headers (audio-base.h,
// audio_effects/effect_spatializer.h) used by the spatial audio backport, which the
// system/media of this tree does not have. The values are the factory ones.

#include <system/audio.h>
#include <system/audio_effect.h>

// Output flag of the "spatializer" mixPort of the vendor audio_policy_configuration.xml:
// the factory TypeConverter lists it after AUDIO_OUTPUT_FLAG_RAW, AudioFlinger::openOutput_l
// opens a SpatializerThread for it.
static constexpr audio_output_flags_t AUDIO_OUTPUT_FLAG_SPATIALIZER =
        static_cast<audio_output_flags_t>(0x40000);

// Effect type of the spatializer effect (vendor/system soundfx/libspatializer.so,
// implementation uuid 61e3f827-0417-4360-b0a7-3de052901cac), as in Android 13.
static const effect_uuid_t FX_IID_SPATIALIZER_ =
        { 0xccd4cf09, 0xa79d, 0x46c2, 0x9aae, { 0x06, 0xa1, 0x69, 0x8d, 0x6c, 0x8f } };
const effect_uuid_t * const FX_IID_SPATIALIZER = &FX_IID_SPATIALIZER_;

// Audio attributes flags of the spatial audio backport (android.media.AudioAttributes
// FLAG_CONTENT_SPATIALIZED, FLAG_NEVER_SPATIALIZE and the PICO FLAG_ALWAYS_SPATIALIZE,
// FLAG_SPATIALIZE_AMBISONIC). The audio policy service sets or clears
// AUDIO_FLAG_ALWAYS_SPATIALIZE from the per-player spatialization state and requests the
// spatializer output with it.
static constexpr audio_flags_mask_t AUDIO_FLAG_CONTENT_SPATIALIZED = 0x4000;
static constexpr audio_flags_mask_t AUDIO_FLAG_NEVER_SPATIALIZE = 0x8000;
static constexpr audio_flags_mask_t AUDIO_FLAG_ALWAYS_SPATIALIZE = 0x1000000;
static constexpr audio_flags_mask_t AUDIO_FLAG_SPATIALIZE_AMBISONIC = 0x2000000;

// Parameters of the spatializer effect (audio_effects/effect_spatializer.h of Android 13, plus
// the PICO parameters handled by soundfx/libspatializer.so; the names of the PICO ones are not
// in the factory binaries).
typedef enum {
    SPATIALIZER_PARAM_SUPPORTED_LEVELS,                 // uint8_t count + uint8_t[] levels
    SPATIALIZER_PARAM_LEVEL,                            // uint8_t level
    SPATIALIZER_PARAM_HEADTRACKING_SUPPORTED,           // bool
    SPATIALIZER_PARAM_HEADTRACKING_MODE,                // uint8_t mode
    SPATIALIZER_PARAM_SUPPORTED_CHANNEL_MASKS,          // uint32_t count + channel masks
    SPATIALIZER_PARAM_SUPPORTED_SPATIALIZATION_MODES,   // uint8_t count + uint8_t[] modes
    SPATIALIZER_PARAM_HEAD_TO_STAGE,                    // float[6]
    SPATIALIZER_PARAM_HINGE_ANGLE,                      // float
    // PICO: listener (head) pose, rotation x, y, z, w then translation x, y, z (float[7])
    SPATIALIZER_PARAM_LISTENER_POSE = 100,
    // PICO: orientation of the spatialized (ambisonic) content, rotation x, y, z, w (float[4]),
    // optionally followed by a translation x, y, z (float[7])
    SPATIALIZER_PARAM_AUDIO_ORIENTATION = 101,
    // PICO: recenters the content orientation when a player is released (int32_t 1)
    SPATIALIZER_PARAM_AUDIO_RECENTER = 103,
} t_spatializer_params;

#endif // ANDROID_PICO_AUDIO_DEFS_H
