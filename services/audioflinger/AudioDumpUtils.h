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

#ifndef ANDROID_AUDIO_DUMP_UTILS_H
#define ANDROID_AUDIO_DUMP_UTILS_H

// PICO: PCM dumps of the factory PICO OS 5.13.7 AudioFlinger ("AudioDumpUtils").
//
// The dump points write raw PCM to /data/misc/audioserver/af_dump_*.raw:
// - when pico.audio.dump.enable is true and the property of the dump point
//   (pico.audio.dump.af_mixerd_pcm, pico.audio.dump.af_record_pcm,
//   pico.audio.dump.af_track_pcm) is true; the old dumps are removed each time
//   pico.audio.dump.enable becomes true;
// - or while the "ut_enable_audio_dump=1" parameter is set on AudioFlinger, which also
//   copies the smaps and maps of the audio server next to the dumps
//   ("audio_dump_file_upload_done=1" removes the dumps).

#include <sys/types.h>

#include <utils/String8.h>

namespace android {
namespace audioDataDump {

// directory of the dumps
extern const char kAudioDumpDir[];

// removes the dumps (regular files starting with "af_dump_", see clearOldFiles()) from dir
void clearOldFiles(const char* dir);

// appends size bytes of buf to the file at path
void dumpAudioPcm(const char* path, char* buf, size_t size);

// true when the dump point controlled by the given property should write its PCM
bool isDumpEnabled(const char* dumpPointProperty);

// handles "ut_enable_audio_dump" and "audio_dump_file_upload_done" of AudioFlinger::setParameters()
void setParameters(const String8& keyValuePairs);

} // namespace audioDataDump
} // namespace android

#endif // ANDROID_AUDIO_DUMP_UTILS_H
