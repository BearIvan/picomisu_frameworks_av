// Copyright 2026 Picomisu contributors
// SPDX-License-Identifier: Apache-2.0

// PICO OS backport of the Android 13 AudioDeviceTypeAddr, reconstructed from the factory
// libaudioclient (android::AudioDeviceTypeAddrForSpatial).

#include <media/AudioDeviceTypeAddrForSpatial.h>

#include <arpa/inet.h>
#include <iostream>
#include <regex>
#include <set>
#include <sstream>

#include <binder/Parcel.h>

namespace android {

namespace {

static const std::string SUPPRESSED = "SUPPRESSED";
static const std::regex MAC_ADDRESS_REGEX("([0-9a-fA-F]{2}:){5}[0-9a-fA-F]{2}");

bool isSenstiveAddress(const std::string &address) {
    if (std::regex_match(address, MAC_ADDRESS_REGEX)) {
        return true;
    }

    sockaddr_storage ss4;
    if (inet_pton(AF_INET, address.c_str(), &ss4) > 0) {
        return true;
    }

    sockaddr_storage ss6;
    if (inet_pton(AF_INET6, address.c_str(), &ss6) > 0) {
        return true;
    }

    return false;
}

} // namespace

AudioDeviceTypeAddrForSpatial::AudioDeviceTypeAddrForSpatial(
        audio_devices_t type, const std::string &address) :
        mType(type), mAddress(address) {
    mIsAddressSensitive = isSenstiveAddress(mAddress);
}

const char* AudioDeviceTypeAddrForSpatial::getAddress() const {
    return mAddress.c_str();
}

const std::string& AudioDeviceTypeAddrForSpatial::address() const {
    return mAddress;
}

void AudioDeviceTypeAddrForSpatial::setAddress(const std::string& address) {
    mAddress = address;
    mIsAddressSensitive = isSenstiveAddress(mAddress);
}

bool AudioDeviceTypeAddrForSpatial::equals(const AudioDeviceTypeAddrForSpatial& other) const {
    return mType == other.mType && mAddress == other.mAddress;
}

bool AudioDeviceTypeAddrForSpatial::operator<(const AudioDeviceTypeAddrForSpatial& other) const {
    if (mType < other.mType)  {
        return true;
    }
    if (mType > other.mType)  {
        return false;
    }

    if (mAddress < other.mAddress)  {
        return true;
    }
    // if (mAddress > other.mAddress)  return false;

    return false;
}

bool AudioDeviceTypeAddrForSpatial::operator==(const AudioDeviceTypeAddrForSpatial &rhs) const {
    return equals(rhs);
}

bool AudioDeviceTypeAddrForSpatial::operator!=(const AudioDeviceTypeAddrForSpatial &rhs) const {
    return !operator==(rhs);
}

void AudioDeviceTypeAddrForSpatial::reset() {
    mType = AUDIO_DEVICE_NONE;
    setAddress("");
}

std::string AudioDeviceTypeAddrForSpatial::toString(bool includeSensitiveInfo) const {
    std::stringstream sstream;
    sstream << "type:0x" << std::hex << mType;
    // IP and MAC address are sensitive information. The sensitive information will be suppressed
    // is `includeSensitiveInfo` is false.
    sstream << ", @:"
            << (!includeSensitiveInfo && mIsAddressSensitive ? SUPPRESSED : mAddress);
    return sstream.str();
}

status_t AudioDeviceTypeAddrForSpatial::readFromParcel(const Parcel *parcel) {
    status_t status;
    uint32_t rawDeviceType;
    if ((status = parcel->readUint32(&rawDeviceType)) != NO_ERROR) return status;
    mType = static_cast<audio_devices_t>(rawDeviceType);
    status = parcel->readUtf8FromUtf16(&mAddress);
    return status;
}

status_t AudioDeviceTypeAddrForSpatial::writeToParcel(Parcel *parcel) const {
    status_t status;
    if ((status = parcel->writeUint32(mType)) != NO_ERROR) return status;
    status = parcel->writeUtf8AsUtf16(mAddress);
    return status;
}

AudioDeviceTypeAddrForSpatialVector excludeDeviceTypeAddrsFrom(
        const AudioDeviceTypeAddrForSpatialVector& devices,
        const AudioDeviceTypeAddrForSpatialVector& devicesToExclude) {
    std::set<AudioDeviceTypeAddrForSpatial> devicesToExcludeSet(
            devicesToExclude.begin(), devicesToExclude.end());
    AudioDeviceTypeAddrForSpatialVector remainedDevices;
    for (const auto& device : devices) {
        if (devicesToExcludeSet.count(device) == 0) {
            remainedDevices.push_back(device);
        }
    }
    return remainedDevices;
}

std::string dumpAudioDeviceTypeAddrVector(const AudioDeviceTypeAddrForSpatialVector& vector,
                                          bool includeSensitiveInfo) {
    std::stringstream stream;
    for (auto it = vector.begin(); it != vector.end(); ++it) {
        if (it != vector.begin()) {
            stream << " ";
        }
        stream << it->toString(includeSensitiveInfo);
    }
    return stream.str();
}

} // namespace android
