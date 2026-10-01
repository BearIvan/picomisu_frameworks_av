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

#ifndef ANDROID_PICO_AUDIO_EVENT_TRACKER_H
#define ANDROID_PICO_AUDIO_EVENT_TRACKER_H

// PICO: audio event tracking of the factory PICO OS 5.13.7 audio server.
//
// The factory libaudioflinger, libaudiopolicyservice and libaudiopolicymanager (the CAF
// AudioPolicyManagerCustom) link the PICO library libaudioeventtracking.so and report
// playback/capture starts and ends, record silencing, device changes and closed threads to
// android::pico::audioeventtracking::AudioEventTracker::getInstance(). That library is a
// factory prebuilt carried on the image (/system/lib64/libaudioeventtracking.so) and is not
// part of the source tree, so the audio server binds it at run time instead of linking it: the
// calls below resolve the exported (non virtual) AudioEventTracker methods once with dlsym()
// and are no-ops when the library or a symbol is missing.

#include <dlfcn.h>
#include <stdint.h>

#include <system/audio.h>

namespace android {
namespace pico {
namespace audioeventtracking {

// Argument of AudioEventTracker::onPlaybackStarted()/onCaptureStarted(), factory layout
// (read by TrackManager::onStart(), 0x140 bytes).
struct start_event_t {
    int32_t portId;                     // 0x000
    int32_t io;                         // 0x004 audio_io_handle_t of the stream
    uint32_t uid;                       // 0x008
    uint32_t device;                    // 0x00c audio_devices_t of the stream
    audio_attributes_t attributes;      // 0x010 content_type, usage, source, flags, tags
    int32_t stream;                     // 0x120 audio_stream_type_t (playback)
    audio_config_base_t config;         // 0x124 sample_rate, channel_mask, format
    uint32_t flags;                     // 0x130 audio_output_flags_t / audio_input_flags_t
    // playback (AudioPolicyService::doStartOutput), 0 for a capture:
    uint32_t mixerChannelMask;          // 0x134 channel mask of the spatializer mixer
    uint32_t spatializeFlags;           // 0x138 AUDIO_FLAG_ALWAYS_SPATIALIZE or
                                        //       AUDIO_FLAG_NEVER_SPATIALIZE of the client
    int32_t spatialized;                // 0x13c the client is spatialized
};
static_assert(sizeof(start_event_t) == 0x140, "factory start_event_t layout");

class AudioEventTrackerBridge {
public:
    static void onPlaybackStarted(const start_event_t& event) {
        const Api& api = get();
        if (api.onPlaybackStarted != nullptr) api.onPlaybackStarted(api.instance(), event);
    }
    static void onPlaybackEnded(int portId) {
        const Api& api = get();
        if (api.onPlaybackEnded != nullptr) api.onPlaybackEnded(api.instance(), portId);
    }
    static void onCaptureStarted(const start_event_t& event) {
        const Api& api = get();
        if (api.onCaptureStarted != nullptr) api.onCaptureStarted(api.instance(), event);
    }
    static void onCaptureEnded(int portId) {
        const Api& api = get();
        if (api.onCaptureEnded != nullptr) api.onCaptureEnded(api.instance(), portId);
    }
    static void onCaptureSilenced(int portId, bool silenced) {
        const Api& api = get();
        if (api.onCaptureSilenced != nullptr) api.onCaptureSilenced(api.instance(), portId, silenced);
    }
    static void onDeviceChanged(int io, uint32_t oldDevice, uint32_t newDevice) {
        const Api& api = get();
        if (api.onDeviceChanged != nullptr) {
            api.onDeviceChanged(api.instance(), io, oldDevice, newDevice);
        }
    }
    static void onAudioFlingerThreadClosed(int io) {
        const Api& api = get();
        if (api.onAudioFlingerThreadClosed != nullptr) {
            api.onAudioFlingerThreadClosed(api.instance(), io);
        }
    }

private:
    // AudioEventTracker* AudioEventTracker::getInstance() and the member functions, called
    // with the tracker as first (this) argument.
    struct Api {
        void* (*getInstance)() = nullptr;
        void (*onPlaybackStarted)(void*, const start_event_t&) = nullptr;
        void (*onPlaybackEnded)(void*, int) = nullptr;
        void (*onCaptureStarted)(void*, const start_event_t&) = nullptr;
        void (*onCaptureEnded)(void*, int) = nullptr;
        void (*onCaptureSilenced)(void*, int, bool) = nullptr;
        void (*onDeviceChanged)(void*, int, unsigned int, unsigned int) = nullptr;
        void (*onAudioFlingerThreadClosed)(void*, int) = nullptr;
        void* instance() const { return getInstance(); }
    };

    template <typename T>
    static void resolve(void* handle, const char* name, T* fn) {
        *fn = reinterpret_cast<T>(dlsym(handle, name));
    }

    static const Api& get() {
        static const Api api = [] {
            Api a;
            void* handle = dlopen("libaudioeventtracking.so", RTLD_NOW);
            if (handle == nullptr) {
                return a;
            }
            resolve(handle, "_ZN7android4pico18audioeventtracking17AudioEventTracker11getInstanceEv",
                    &a.getInstance);
            if (a.getInstance == nullptr) {
                return a;
            }
            resolve(handle, "_ZN7android4pico18audioeventtracking17AudioEventTracker"
                    "17onPlaybackStartedERKNS1_13start_event_tE", &a.onPlaybackStarted);
            resolve(handle, "_ZN7android4pico18audioeventtracking17AudioEventTracker"
                    "15onPlaybackEndedEi", &a.onPlaybackEnded);
            resolve(handle, "_ZN7android4pico18audioeventtracking17AudioEventTracker"
                    "16onCaptureStartedERKNS1_13start_event_tE", &a.onCaptureStarted);
            resolve(handle, "_ZN7android4pico18audioeventtracking17AudioEventTracker"
                    "14onCaptureEndedEi", &a.onCaptureEnded);
            resolve(handle, "_ZN7android4pico18audioeventtracking17AudioEventTracker"
                    "17onCaptureSilencedEib", &a.onCaptureSilenced);
            resolve(handle, "_ZN7android4pico18audioeventtracking17AudioEventTracker"
                    "15onDeviceChangedEijj", &a.onDeviceChanged);
            resolve(handle, "_ZN7android4pico18audioeventtracking17AudioEventTracker"
                    "26onAudioFlingerThreadClosedEi", &a.onAudioFlingerThreadClosed);
            return a;
        }();
        return api;
    }
};

} // namespace audioeventtracking
} // namespace pico
} // namespace android

#endif // ANDROID_PICO_AUDIO_EVENT_TRACKER_H
