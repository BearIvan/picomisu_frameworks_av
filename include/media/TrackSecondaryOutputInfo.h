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

#ifndef ANDROID_MEDIA_TRACK_SECONDARY_OUTPUT_INFO_H
#define ANDROID_MEDIA_TRACK_SECONDARY_OUTPUT_INFO_H

#include <sstream>
#include <binder/Parcel.h>
#include <binder/Parcelable.h>

namespace android {
namespace media {
#define RETURN_IF_FAILED(calledOnce)                                     \
    {                                                                    \
        status_t returnStatus = calledOnce;                              \
        if (returnStatus) {                                              \
            ALOGE("Failed at %s:%d (%s)", __FILE__, __LINE__, __func__); \
            return returnStatus;                                         \
         }                                                               \
    }

class TrackSecondaryOutputInfo : public Parcelable {
public:
    status_t readFromParcel(const Parcel* parcel) override {
        RETURN_IF_FAILED(parcel->readInt32(&portId));
        RETURN_IF_FAILED(parcel->readInt32Vector(&secondaryOutputIds));
        return OK;
    }

    // Same layout as the Android 12 android.media.TrackSecondaryOutputInfo AIDL parcelable.
    status_t writeToParcel(Parcel* parcel) const override {
        RETURN_IF_FAILED(parcel->writeInt32(portId));
        RETURN_IF_FAILED(parcel->writeInt32Vector(secondaryOutputIds));
        return OK;
    }

    // PICO OS 5.13.7 backport of the Android 12 TrackSecondaryOutputInfo: the port handle of
    // a playback track and the io handles of its secondary outputs, sent by the audio policy
    // manager to AudioFlinger (IAudioFlinger::updateSecondaryOutputs) when the dynamic policy
    // mixes change. Header only: the line numbers of the RETURN_IF_FAILED logs (38/39, 45/46)
    // are the factory ones.
    TrackSecondaryOutputInfo() = default;
    TrackSecondaryOutputInfo(int32_t portId, std::vector<int32_t> secondaryOutputIds) {
        this->portId = portId;
        this->secondaryOutputIds = secondaryOutputIds;
    }

    std::string toString() const {
        std::ostringstream os;
        os << "TrackSecondaryOutputInfo{";
        os << "portId: " << portId;
        os << ", secondaryOutputIds:(";
        for (const auto& secondaryOutputId : secondaryOutputIds) {
            os << secondaryOutputId << ";";
        }
        os << ")}";
        return os.str();
    }

    int32_t portId = 0;                       // audio_port_handle_t
    std::vector<int32_t> secondaryOutputIds;  // audio_io_handle_t[]
};

} // namespace media
} // namespace android

#endif // ANDROID_MEDIA_TRACK_SECONDARY_OUTPUT_INFO_H
