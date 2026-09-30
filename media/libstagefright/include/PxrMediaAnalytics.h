// Copyright 2026 Picomisu contributors
// SPDX-License-Identifier: Apache-2.0

#ifndef PXR_MEDIA_ANALYTICS_H_
#define PXR_MEDIA_ANALYTICS_H_

#include <stdint.h>
#include <sys/types.h>

#include <string>

namespace android {

// PICO: the factory PICO OS libstagefright links libpxrmediametrics.so (a PICO
// clone of MediaAnalyticsItem that reports to /system/bin/pxrmediametrics).
// That library is not part of this tree; the image carries the factory binary.
// Instead of linking it, these helpers dlopen() it once and call the exported
// non-virtual members of android::pico::PxrMediaAnalyticsItem through dlsym().
// Every helper is a no-op (or returns nullptr/false/0) when the library or the
// symbol is missing, or when |item| is nullptr.
namespace pico {
class PxrMediaAnalyticsItem;  // opaque, allocated and owned by libpxrmediametrics
}  // namespace pico

namespace pxr {

// Layout of android::pico::PxrMediaAnalyticsItem::PerfData
// (PerfData(bool, short*, int, short*, int)). setPerformanceData() copies the
// pointers, so the arrays must outlive the item.
struct PerfData {
    bool mLowLatency;
    int16_t *mFps;
    int32_t mFpsCount;
    int16_t *mLatencyMs;
    int32_t mLatencyMsCount;
};

// PxrMediaAnalyticsItem::create(std::string); returns nullptr if unavailable.
pico::PxrMediaAnalyticsItem *create(const std::string &key);
// ~PxrMediaAnalyticsItem() followed by operator delete (what the factory code does).
void destroy(pico::PxrMediaAnalyticsItem *item);

bool deathNotifyToServer(pico::PxrMediaAnalyticsItem *item);
int64_t generateSessionID(pico::PxrMediaAnalyticsItem *item);
void setUid(pico::PxrMediaAnalyticsItem *item, uid_t uid);
void setInt32(pico::PxrMediaAnalyticsItem *item, const char *attr, int32_t value);
void setCString(pico::PxrMediaAnalyticsItem *item, const char *attr, const char *value);
// On success |*value| is a strdup()ed copy owned by the caller (free()).
bool getCString(pico::PxrMediaAnalyticsItem *item, const char *attr, char **value);
int32_t count(const pico::PxrMediaAnalyticsItem *item);
bool selfrecord(pico::PxrMediaAnalyticsItem *item);
// Builds a PerfData with the factory PerfData(bool, short*, int, short*, int)
// constructor and passes it to setPerformanceData().
void setPerformanceData(pico::PxrMediaAnalyticsItem *item, bool lowLatency,
        int16_t *fps, int32_t fpsCount, int16_t *latencyMs, int32_t latencyMsCount);

}  // namespace pxr

}  // namespace android

#endif  // PXR_MEDIA_ANALYTICS_H_
