// Copyright 2026 Picomisu contributors
// SPDX-License-Identifier: Apache-2.0

// PICO: runtime bridge to the factory libpxrmediametrics.so
// (android::pico::PxrMediaAnalyticsItem), used by MediaCodec and
// RemoteMediaExtractor. The factory libstagefright calls these members
// directly (all of them are non-virtual; the class has no vtable), so they are
// called here as free functions taking |this| as the first argument.

//#define LOG_NDEBUG 0
#define LOG_TAG "PxrMediaAnalytics"
#include <utils/Log.h>

#include <dlfcn.h>

#include <mutex>
#include <new>

#include "include/PxrMediaAnalytics.h"

// The targets live in a library that is not built with CFI; keep the cross-DSO
// CFI indirect-call check out of the way of these dlsym() calls.
#define PXR_NO_CFI __attribute__((no_sanitize("cfi")))

namespace android {
namespace pxr {

namespace {

using pico::PxrMediaAnalyticsItem;

constexpr const char *kLibName = "libpxrmediametrics.so";

typedef PxrMediaAnalyticsItem *(*CreateFn)(std::string);
typedef void (*DtorFn)(PxrMediaAnalyticsItem *);
typedef bool (*DeathNotifyToServerFn)(PxrMediaAnalyticsItem *);
typedef int64_t (*GenerateSessionIDFn)(PxrMediaAnalyticsItem *);
typedef void (*SetUidFn)(PxrMediaAnalyticsItem *, uid_t);
typedef void (*SetInt32Fn)(PxrMediaAnalyticsItem *, const char *, int32_t);
typedef void (*SetCStringFn)(PxrMediaAnalyticsItem *, const char *, const char *);
typedef bool (*GetCStringFn)(PxrMediaAnalyticsItem *, const char *, char **);
typedef int32_t (*CountFn)(const PxrMediaAnalyticsItem *);
typedef bool (*SelfrecordFn)(PxrMediaAnalyticsItem *);
typedef void (*PerfDataCtorFn)(PerfData *, bool, int16_t *, int32_t, int16_t *, int32_t);
typedef void (*SetPerformanceDataFn)(PxrMediaAnalyticsItem *, const PerfData &);

struct Api {
    CreateFn create = nullptr;
    DtorFn dtor = nullptr;
    DeathNotifyToServerFn deathNotifyToServer = nullptr;
    GenerateSessionIDFn generateSessionID = nullptr;
    SetUidFn setUid = nullptr;
    SetInt32Fn setInt32 = nullptr;
    SetCStringFn setCString = nullptr;
    GetCStringFn getCString = nullptr;
    CountFn count = nullptr;
    SelfrecordFn selfrecord = nullptr;
    PerfDataCtorFn perfDataCtor = nullptr;
    SetPerformanceDataFn setPerformanceData = nullptr;
};

Api gApi;
std::once_flag gApiOnce;

template <typename T>
void lookup(void *handle, const char *symbol, T *fn) {
    *fn = reinterpret_cast<T>(dlsym(handle, symbol));
    if (*fn == nullptr) {
        ALOGW("%s: missing %s", kLibName, symbol);
    }
}

void loadApi() {
    // Loaded once and never unloaded: items created by the library may be alive
    // until the process exits.
    void *handle = dlopen(kLibName, RTLD_NOW);
    if (handle == nullptr) {
        ALOGW("dlopen(%s) failed: %s", kLibName, dlerror());
        return;
    }
    lookup(handle, "_ZN7android4pico21PxrMediaAnalyticsItem6createENSt3__112basic_stringIc"
            "NS2_11char_traitsIcEENS2_9allocatorIcEEEE", &gApi.create);
    lookup(handle, "_ZN7android4pico21PxrMediaAnalyticsItemD1Ev", &gApi.dtor);
    lookup(handle, "_ZN7android4pico21PxrMediaAnalyticsItem19deathNotifyToServerEv",
            &gApi.deathNotifyToServer);
    lookup(handle, "_ZN7android4pico21PxrMediaAnalyticsItem17generateSessionIDEv",
            &gApi.generateSessionID);
    lookup(handle, "_ZN7android4pico21PxrMediaAnalyticsItem6setUidEj", &gApi.setUid);
    lookup(handle, "_ZN7android4pico21PxrMediaAnalyticsItem8setInt32EPKci", &gApi.setInt32);
    lookup(handle, "_ZN7android4pico21PxrMediaAnalyticsItem10setCStringEPKcS3_",
            &gApi.setCString);
    lookup(handle, "_ZN7android4pico21PxrMediaAnalyticsItem10getCStringEPKcPPc",
            &gApi.getCString);
    lookup(handle, "_ZNK7android4pico21PxrMediaAnalyticsItem5countEv", &gApi.count);
    lookup(handle, "_ZN7android4pico21PxrMediaAnalyticsItem10selfrecordEv", &gApi.selfrecord);
    lookup(handle, "_ZN7android4pico21PxrMediaAnalyticsItem8PerfDataC1EbPsiS3_i",
            &gApi.perfDataCtor);
    lookup(handle, "_ZN7android4pico21PxrMediaAnalyticsItem18setPerformanceDataERKNS1_8PerfDataE",
            &gApi.setPerformanceData);
}

const Api &api() {
    std::call_once(gApiOnce, loadApi);
    return gApi;
}

}  // namespace

PXR_NO_CFI
PxrMediaAnalyticsItem *create(const std::string &key) {
    const Api &a = api();
    // Without the destructor an item could never be released again.
    if (a.create == nullptr || a.dtor == nullptr) {
        return nullptr;
    }
    return a.create(key);
}

PXR_NO_CFI
void destroy(PxrMediaAnalyticsItem *item) {
    const Api &a = api();
    if (item == nullptr || a.dtor == nullptr) {
        return;
    }
    // Non-virtual destructor; the item was allocated with operator new by create().
    a.dtor(item);
    ::operator delete(static_cast<void *>(item));
}

PXR_NO_CFI
bool deathNotifyToServer(PxrMediaAnalyticsItem *item) {
    const Api &a = api();
    if (item == nullptr || a.deathNotifyToServer == nullptr) {
        return false;
    }
    return a.deathNotifyToServer(item);
}

PXR_NO_CFI
int64_t generateSessionID(PxrMediaAnalyticsItem *item) {
    const Api &a = api();
    if (item == nullptr || a.generateSessionID == nullptr) {
        return 0;
    }
    return a.generateSessionID(item);
}

PXR_NO_CFI
void setUid(PxrMediaAnalyticsItem *item, uid_t uid) {
    const Api &a = api();
    if (item == nullptr || a.setUid == nullptr) {
        return;
    }
    a.setUid(item, uid);
}

PXR_NO_CFI
void setInt32(PxrMediaAnalyticsItem *item, const char *attr, int32_t value) {
    const Api &a = api();
    if (item == nullptr || a.setInt32 == nullptr) {
        return;
    }
    a.setInt32(item, attr, value);
}

PXR_NO_CFI
void setCString(PxrMediaAnalyticsItem *item, const char *attr, const char *value) {
    const Api &a = api();
    if (item == nullptr || a.setCString == nullptr) {
        return;
    }
    a.setCString(item, attr, value);
}

PXR_NO_CFI
bool getCString(PxrMediaAnalyticsItem *item, const char *attr, char **value) {
    const Api &a = api();
    if (item == nullptr || a.getCString == nullptr) {
        return false;
    }
    return a.getCString(item, attr, value);
}

PXR_NO_CFI
int32_t count(const PxrMediaAnalyticsItem *item) {
    const Api &a = api();
    if (item == nullptr || a.count == nullptr) {
        return 0;
    }
    return a.count(item);
}

PXR_NO_CFI
bool selfrecord(PxrMediaAnalyticsItem *item) {
    const Api &a = api();
    if (item == nullptr || a.selfrecord == nullptr) {
        return false;
    }
    return a.selfrecord(item);
}

PXR_NO_CFI
void setPerformanceData(PxrMediaAnalyticsItem *item, bool lowLatency,
        int16_t *fps, int32_t fpsCount, int16_t *latencyMs, int32_t latencyMsCount) {
    const Api &a = api();
    if (item == nullptr || a.setPerformanceData == nullptr) {
        return;
    }
    PerfData perfData;
    if (a.perfDataCtor != nullptr) {
        a.perfDataCtor(&perfData, lowLatency, fps, fpsCount, latencyMs, latencyMsCount);
    } else {
        perfData.mLowLatency = lowLatency;
        perfData.mFps = fps;
        perfData.mFpsCount = fpsCount;
        perfData.mLatencyMs = latencyMs;
        perfData.mLatencyMsCount = latencyMsCount;
    }
    a.setPerformanceData(item, perfData);
}

}  // namespace pxr
}  // namespace android
