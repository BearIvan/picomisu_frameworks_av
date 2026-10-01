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

#define LOG_TAG "HeadPoseProvider"
//#define LOG_NDEBUG 0

#include "HeadPoseProvider.h"

#include <dlfcn.h>
#include <mutex>
#include <string>

#include <utils/Log.h>
#include <utils/Timers.h>

namespace android {

namespace {

// Entry points of libtrackingclient.pxr.so (PICO tracking service client).
typedef void* (*CreateClientFn)();
typedef void (*DestroyClientFn)(void* client);
typedef int (*GetHeadTrackingDataFn)(void* client, int64_t predictTimeNs,
                                     HeadTrackingData* data, int32_t count);
typedef int (*GetHeadDataFn)(void* client, void* data);
typedef void (*ServiceStatusCallback)(void* context, int key, void* value);
typedef int (*SetServiceStatusCallbackFn)(void* client, void* context,
                                          ServiceStatusCallback callback);

struct TrackingClientInterface {
    CreateClientFn createClient;
    DestroyClientFn destroyClient;
    GetHeadTrackingDataFn getHeadTrackingData;
    GetHeadDataFn getHeadData;
    SetServiceStatusCallbackFn setServiceStatusCallback;
};

std::mutex sLock;
TrackingClientInterface* sInterface = nullptr;  // guarded by sLock while loading

void service_status_callback(void* context, int key, void* value)
{
    ALOGI("%s, context %p, key %d", __func__, context, key);
    switch (key) {
        case 0:
            ALOGI("%s, input_device %d", __func__, *static_cast<int*>(value));
            break;
        case 1:
            ALOGI("%s, home_state %d", __func__, *static_cast<int*>(value));
            break;
        case 2:
            ALOGI("%s, reset_state %d", __func__, *static_cast<int*>(value));
            break;
        case 3:
            ALOGI("%s, service_state %d", __func__, *static_cast<int*>(value));
            break;
        default:
            break;
    }
}

// Loads the tracking client library, called with sLock held.
bool findInterface()
{
    static const std::string kLibName = "libtrackingclient.pxr.so";

    void* handle = dlopen(kLibName.c_str(), RTLD_NOW | RTLD_NODELETE);
    if (handle == nullptr) {
        ALOGE("Error opening %s %s\n", kLibName.c_str(), dlerror());
    } else {
        sInterface = new TrackingClientInterface{};
        if ((sInterface->createClient =
                reinterpret_cast<CreateClientFn>(dlsym(handle, "CreateClient"))) == nullptr) {
            ALOGE("%s %s failed", __func__, "CreateClient");
        } else if ((sInterface->destroyClient =
                reinterpret_cast<DestroyClientFn>(dlsym(handle, "DestroyClient"))) == nullptr) {
            ALOGE("%s %s failed", __func__, "DestroyClient");
        } else if ((sInterface->getHeadTrackingData = reinterpret_cast<GetHeadTrackingDataFn>(
                dlsym(handle, "GetHeadTrackingData"))) == nullptr) {
            ALOGE("%s %s failed", __func__, "GetHeadTrackingData");
        } else if ((sInterface->getHeadData =
                reinterpret_cast<GetHeadDataFn>(dlsym(handle, "GetHeadData"))) == nullptr) {
            ALOGE("%s %s failed", __func__, "GetHeadData");
        } else if ((sInterface->setServiceStatusCallback =
                reinterpret_cast<SetServiceStatusCallbackFn>(
                        dlsym(handle, "SetServiceStatusCallback"))) == nullptr) {
            ALOGE("%s %s failed", __func__, "SetServiceStatusCallback");
        } else {
            return true;
        }
    }
    delete sInterface;
    sInterface = nullptr;
    return false;
}

}  // namespace

HeadPoseProvider::~HeadPoseProvider()
{
    if (mClient != nullptr) {
        if (sInterface != nullptr) {
            sInterface->destroyClient(mClient);
        } else {
            ALOGE("has tracking client but client inteface is null");
        }
    }
}

bool HeadPoseProvider::init()
{
    {
        std::lock_guard<std::mutex> lock(sLock);
        if (sInterface == nullptr && !findInterface()) {
            return false;
        }
    }
    if (mClient == nullptr) {
        mClient = sInterface->createClient();
        if (mClient == nullptr) {
            return false;
        }
        if (sInterface->setServiceStatusCallback != nullptr) {
            sInterface->setServiceStatusCallback(mClient, nullptr, service_status_callback);
        }
    }
    return true;
}

int HeadPoseProvider::getHeadTrackingData(int64_t predictTimeNs, HeadTrackingData* data)
{
    if (mClient == nullptr) {
        return -1;
    }
    if (predictTimeNs <= 0) {
        predictTimeNs = systemTime(SYSTEM_TIME_MONOTONIC);
    }
    return sInterface->getHeadTrackingData(mClient, predictTimeNs, data, 1);
}

}  // namespace android
