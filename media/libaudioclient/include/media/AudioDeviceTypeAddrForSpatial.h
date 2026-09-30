// Copyright 2026 Picomisu contributors
// SPDX-License-Identifier: Apache-2.0

#ifndef ANDROID_AUDIO_DEVICE_TYPE_ADDR_FOR_SPATIAL_H
#define ANDROID_AUDIO_DEVICE_TYPE_ADDR_FOR_SPATIAL_H

#include <string>
#include <vector>

#include <binder/Parcelable.h>
#include <system/audio.h>
#include <utils/Errors.h>

namespace android {

// PICO OS backport of the Android 13 AudioDeviceTypeAddr (libaudiofoundation), used by the
// spatializer entry points of the audio policy service (canBeSpatialized). Layout and parcel
// format of the factory libaudioclient.
struct AudioDeviceTypeAddrForSpatial : public Parcelable {
    AudioDeviceTypeAddrForSpatial() = default;

    AudioDeviceTypeAddrForSpatial(audio_devices_t type, const std::string& address);

    const char* getAddress() const;

    const std::string& address() const;

    void setAddress(const std::string& address);

    bool isAddressSensitive() const { return mIsAddressSensitive; }

    bool equals(const AudioDeviceTypeAddrForSpatial& other) const;

    AudioDeviceTypeAddrForSpatial& operator= (const AudioDeviceTypeAddrForSpatial&) = default;

    bool operator==(const AudioDeviceTypeAddrForSpatial& rhs) const;

    bool operator!=(const AudioDeviceTypeAddrForSpatial& rhs) const;

    bool operator<(const AudioDeviceTypeAddrForSpatial& other) const;

    void reset();

    std::string toString(bool includeSensitiveInfo=false) const;

    status_t readFromParcel(const Parcel *parcel) override;

    status_t writeToParcel(Parcel *parcel) const override;

    audio_devices_t mType = AUDIO_DEVICE_NONE;

private:
    std::string mAddress;
    bool mIsAddressSensitive = false;
};

using AudioDeviceTypeAddrForSpatialVector = std::vector<AudioDeviceTypeAddrForSpatial>;

/**
 * Return a collection of audio device types from a collection of AudioDeviceTypeAddr
 */
AudioDeviceTypeAddrForSpatialVector excludeDeviceTypeAddrsFrom(
        const AudioDeviceTypeAddrForSpatialVector& devices,
        const AudioDeviceTypeAddrForSpatialVector& devicesToExclude);

std::string dumpAudioDeviceTypeAddrVector(const AudioDeviceTypeAddrForSpatialVector& vector,
                                          bool includeSensitiveInfo=false);

} // namespace android

#endif // ANDROID_AUDIO_DEVICE_TYPE_ADDR_FOR_SPATIAL_H
