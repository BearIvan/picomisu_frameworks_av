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

// PICO: reconstructed from the factory PICO OS 5.13.7 libaudioflinger
// (android::audioDataDump::clearOldFiles/dumpAudioPcm and the dump code of
// AudioFlinger::setParameters, PlaybackThread::threadLoop_write, RecordThread::threadLoop and
// Track::getNextBuffer). The factory keeps its verbose logs.
#define LOG_TAG "AudioDumpUtils"
#define LOG_NDEBUG 0

#include "AudioDumpUtils.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <string>

#include <cutils/properties.h>
#include <cutils/str_parms.h>
#include <utils/Log.h>

namespace android {
namespace audioDataDump {

const char kAudioDumpDir[] = "/data/misc/audioserver/";

// last value of pico.audio.dump.enable seen by a dump point
static bool sAudioDumpEnabled = false;
// "ut_enable_audio_dump" parameter
static bool sUtAudioDumpEnabled = false;

void clearOldFiles(const char* dir)
{
    DIR* d = opendir(dir);
    if (d == nullptr) {
        ALOGE("Open dir error...");
        return;
    }
    // Note: as in the factory, every regular file that follows the first "af_dump_" file
    // in the directory order is removed as well.
    bool found = false;
    struct dirent* entry;
    while ((entry = readdir(d)) != nullptr) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        if (entry->d_type != DT_REG) {
            continue;
        }
        const std::string prefix = std::string(entry->d_name).substr(0, 8);
        if (strcmp("af_dump_", prefix.c_str()) == 0) {
            found = true;
        }
        if (found) {
            char path[256];
            snprintf(path, sizeof(path), "%s%s", dir, entry->d_name);
            remove(path);
            ALOGV("%s, file (%s)!", __func__, path);
        }
    }
    closedir(d);
}

void dumpAudioPcm(const char* path, char* buf, size_t size)
{
    FILE* fp = fopen(path, "ab");
    if (fp == nullptr) {
        ALOGE("dumpAudioPcm Unable to open file: %s", path);
        return;
    }
    const int written = fwrite(buf, 1, size, fp);
    if (size != (size_t)(ssize_t)written) {
        ALOGW("dumpAudioPcm fwrite want write %zd,  really written %d", size, written);
    }
    fclose(fp);
}

bool isDumpEnabled(const char* dumpPointProperty)
{
    bool dumpPoint = false;
    if (property_get_bool("pico.audio.dump.enable", false)) {
        if (!sAudioDumpEnabled) {
            clearOldFiles(kAudioDumpDir);
            sAudioDumpEnabled = true;
        }
        dumpPoint = property_get_bool(dumpPointProperty, false);
    } else if (sAudioDumpEnabled) {
        sAudioDumpEnabled = false;
    }
    return dumpPoint || sUtAudioDumpEnabled;
}

static void copyFile(const char* from, const char* to, FILE** in, FILE** out)
{
    *in = fopen(from, "r");
    *out = fopen(to, "w");
    if (*in == nullptr || *out == nullptr) {
        return;
    }
    char buf[1024];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), *in)) > 0) {
        fwrite(buf, 1, n, *out);
    }
}

void setParameters(const String8& keyValuePairs)
{
    ALOGI("setParameters %s", keyValuePairs.string());
    char value[32] = {};
    struct str_parms* parms = str_parms_create_str(keyValuePairs.string());
    if (parms == nullptr) {
        ALOGE("parms is null in %s", __func__);
        return;
    }
    if (str_parms_get_str(parms, "ut_enable_audio_dump", value, sizeof(value)) >= 0) {
        const int enable = atoi(value);
        if (enable >= 1 && !sUtAudioDumpEnabled) {
            // keep the memory maps of the audio server next to the dumps
            clearOldFiles(kAudioDumpDir);
            char smapsOut[128], smapsIn[128], mapsOut[128], mapsIn[128];
            snprintf(smapsOut, sizeof(smapsOut), "%saudioserver_smaps.txt", kAudioDumpDir);
            snprintf(smapsIn, sizeof(smapsIn), "/proc/%d/smaps", getpid());
            snprintf(mapsOut, sizeof(mapsOut), "%saudioserver_maps.txt", kAudioDumpDir);
            snprintf(mapsIn, sizeof(mapsIn), "/proc/%d/maps", getpid());
            FILE *smapsInFile, *smapsOutFile, *mapsInFile, *mapsOutFile;
            copyFile(smapsIn, smapsOut, &smapsInFile, &smapsOutFile);
            copyFile(mapsIn, mapsOut, &mapsInFile, &mapsOutFile);
            if (smapsInFile != nullptr) fclose(smapsInFile);
            if (smapsOutFile != nullptr) fclose(smapsOutFile);
            if (mapsInFile != nullptr) fclose(mapsInFile);
            if (mapsOutFile != nullptr) fclose(mapsOutFile);
        }
        sUtAudioDumpEnabled = enable > 0;
    }
    if (str_parms_get_str(parms, "audio_dump_file_upload_done", value, sizeof(value)) >= 0 &&
            atoi(value) >= 1) {
        clearOldFiles(kAudioDumpDir);
    }
    str_parms_destroy(parms);
}

} // namespace audioDataDump
} // namespace android
