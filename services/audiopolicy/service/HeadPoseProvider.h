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

#ifndef ANDROID_MEDIA_HEADPOSEPROVIDER_H
#define ANDROID_MEDIA_HEADPOSEPROVIDER_H

#include <stdint.h>

namespace android {

// PICO OS 5.13.7: head pose source of the spatializer pose controller. The factory does not
// use the Android 13 sensor/head tracking libraries: the head pose comes from the PICO
// tracking service through libtrackingclient.pxr.so, loaded at run time.

// Head tracking data returned by GetHeadTrackingData() of libtrackingclient.pxr.so. Only the
// fields read by the factory pose controller are named (128 bytes on the factory stack).
struct HeadTrackingData {
    int64_t timestampNs;
    double position[3];         // x, y, z
    double orientationW;
    double orientation[3];      // x, y, z
    double reserved[8];
};

class HeadPoseProvider {
public:
    HeadPoseProvider() = default;
    ~HeadPoseProvider();

    // Loads the tracking client library (once per process) and creates the tracking client.
    // Returns false if the client is not available yet.
    bool init();

    // Gets the head tracking data predicted at predictTimeNs (CLOCK_MONOTONIC).
    // Returns 0 on success.
    int getHeadTrackingData(int64_t predictTimeNs, HeadTrackingData* data);

private:
    HeadPoseProvider(const HeadPoseProvider&) = delete;
    HeadPoseProvider& operator=(const HeadPoseProvider&) = delete;

    void* mClient = nullptr;
};

}  // namespace android

#endif  // ANDROID_MEDIA_HEADPOSEPROVIDER_H
