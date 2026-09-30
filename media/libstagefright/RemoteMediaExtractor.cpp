/*
 * Copyright 2017, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

//#define LOG_NDEBUG 0
#define LOG_TAG "RemoteMediaExtractor"
#include <utils/Log.h>

#include <string.h>

#include <binder/IPCThreadState.h>
#include <media/stagefright/InterfaceUtils.h>
#include <media/MediaAnalyticsItem.h>
#include <media/MediaSource.h>
#include <media/stagefright/RemoteMediaExtractor.h>

#include "include/PxrMediaAnalytics.h"

// still doing some on/off toggling here.
#define MEDIA_LOG       1

namespace android {

// key for media statistics
static const char *kKeyExtractor = "extractor";

// attrs for media statistics
// NB: these are matched with public Java API constants defined
// in frameworks/base/media/java/android/media/MediaExtractor.java
// These must be kept synchronized with the constants there.
static const char *kExtractorFormat = "android.media.mediaextractor.fmt";
static const char *kExtractorMime = "android.media.mediaextractor.mime";
static const char *kExtractorTracks = "android.media.mediaextractor.ntrk";

RemoteMediaExtractor::RemoteMediaExtractor(
        MediaExtractor *extractor,
        const sp<DataSource> &source,
        const sp<RefBase> &plugin)
    :mExtractor(extractor),
     mSource(source),
     mExtractorPlugin(plugin) {

    mAnalyticsItem = nullptr;
    // PICO: only set for video containers, and released again below
    mPxrAnalyticsItem = nullptr;
    if (MEDIA_LOG) {
        mAnalyticsItem = MediaAnalyticsItem::create(kKeyExtractor);

        // we're in the extractor service, we want to attribute to the app
        // that invoked us.
        int uid = IPCThreadState::self()->getCallingUid();
        mAnalyticsItem->setUid(uid);

        // track the container format (mpeg, aac, wvm, etc)
        size_t ntracks = extractor->countTracks();
        mAnalyticsItem->setCString(kExtractorFormat, extractor->name());
        // tracks (size_t)
        mAnalyticsItem->setInt32(kExtractorTracks, ntracks);
        // metadata
        MetaDataBase pMetaData;
        if (extractor->getMetaData(pMetaData) != OK) {
            // PICO
            ALOGW("Sniff pMetaData is nullprt!");
        } else {
            String8 xx = pMetaData.toString();
            // 'titl' -- but this verges into PII
            // 'mime'
            const char *mime = nullptr;
            if (pMetaData.findCString(kKeyMIMEType, &mime)) {
                mAnalyticsItem->setCString(kExtractorMime,  mime);
            }
            // what else is interesting and not already available?

            // PICO: pxrmediametrics record of video containers
            if (mime != nullptr && !strncmp(mime, "video", 5)) {
                mPxrAnalyticsItem = pxr::create(kKeyExtractor);
                pxr::generateSessionID(mPxrAnalyticsItem);
                pxr::setUid(mPxrAnalyticsItem, uid);
                pxr::setCString(mPxrAnalyticsItem, "key_event", "media_extractor_start");
                pxr::setCString(mPxrAnalyticsItem, "file_type", mime);

                int32_t videoWidth = 0;
                if (pMetaData.findInt32(kKeyWidth, &videoWidth)) {
                    pxr::setInt32(mPxrAnalyticsItem, "video_width", videoWidth);
                }
                // NB: the factory code looks the height up with kKeyWidth too
                int32_t videoHeight = 0;
                if (pMetaData.findInt32(kKeyWidth, &videoHeight)) {
                    pxr::setInt32(mPxrAnalyticsItem, "video_height", videoHeight);
                }
                // NB: kKeyDuration is an int64 key, so this is only set when a
                // container stores it as int32 (factory behaviour)
                int32_t videoDuration = 0;
                if (pMetaData.findInt32(kKeyDuration, &videoDuration)) {
                    pxr::setInt32(mPxrAnalyticsItem, "video_duration", videoDuration);
                }
                int32_t videoFps = 0;
                if (pMetaData.findInt32(kKeyFrameRate, &videoFps)) {
                    pxr::setInt32(mPxrAnalyticsItem, "video_fps", videoFps);
                }
                int32_t videoBitrate = 0;
                if (pMetaData.findInt32(kKeyBitRate, &videoBitrate)) {
                    pxr::setInt32(mPxrAnalyticsItem, "video_bitrate", videoBitrate);
                }

                MetaDataBase trackMeta;
                size_t numTracks = extractor->countTracks();
                int32_t videoTrackCount = 0;
                int32_t audioTrackCount = 0;
                int32_t subtitleTrackCount = 0;
                for (int32_t i = 0; (size_t)i < numTracks; i++) {
                    extractor->getTrackMetaData(
                            trackMeta, i, MediaExtractor::kIncludeExtensiveMetaData);
                    const char *trackMime = nullptr;
                    if (!trackMeta.findCString(kKeyMIMEType, &trackMime)) {
                        ALOGD("trackIndex %d mime is null", i);
                        continue;
                    }
                    if (!strncmp(trackMime, "video", 5)) {
                        if (videoTrackCount == 0) {
                            pxr::setCString(mPxrAnalyticsItem, "video_encode_type", trackMime);
                        }
                        videoTrackCount++;
                    }
                    if (!strncmp(trackMime, "audio", 5)) {
                        if (audioTrackCount == 0) {
                            pxr::setCString(mPxrAnalyticsItem, "audio_encode_type", trackMime);
                        }
                        audioTrackCount++;
                    }
                    if (!strncmp(trackMime, "subtitle", 8)) {
                        subtitleTrackCount++;
                    }
                    pxr::setInt32(mPxrAnalyticsItem, "audio_track_count", audioTrackCount);
                    pxr::setInt32(mPxrAnalyticsItem, "subtitle_track_count", subtitleTrackCount);
                }

                if (pxr::count(mPxrAnalyticsItem) > 0) {
                    pxr::selfrecord(mPxrAnalyticsItem);
                    pxr::destroy(mPxrAnalyticsItem);
                    mPxrAnalyticsItem = nullptr;
                }
            }
        }
    }
}

RemoteMediaExtractor::~RemoteMediaExtractor() {
    delete mExtractor;
    mSource->close();
    mSource.clear();
    mExtractorPlugin = nullptr;
    // log the current record, provided it has some information worth recording
    if (MEDIA_LOG) {
        if (mAnalyticsItem != nullptr) {
            if (mAnalyticsItem->count() > 0) {
                mAnalyticsItem->selfrecord();
            }
        }
    }
    if (mAnalyticsItem != nullptr) {
        delete mAnalyticsItem;
        mAnalyticsItem = nullptr;
    }
}

size_t RemoteMediaExtractor::countTracks() {
    return mExtractor->countTracks();
}

sp<IMediaSource> RemoteMediaExtractor::getTrack(size_t index) {
    MediaTrack *source = mExtractor->getTrack(index);
    return (source == nullptr)
            ? nullptr : CreateIMediaSourceFromMediaSourceBase(this, source, mExtractorPlugin);
}

sp<MetaData> RemoteMediaExtractor::getTrackMetaData(size_t index, uint32_t flags) {
    sp<MetaData> meta = new MetaData();
    if (mExtractor->getTrackMetaData(*meta.get(), index, flags) == OK) {
        return meta;
    }
    return nullptr;
}

sp<MetaData> RemoteMediaExtractor::getMetaData() {
    sp<MetaData> meta = new MetaData();
    if (mExtractor->getMetaData(*meta.get()) == OK) {
        return meta;
    }
    return nullptr;
}

status_t RemoteMediaExtractor::getMetrics(Parcel *reply) {
    if (mAnalyticsItem == nullptr || reply == nullptr) {
        return UNKNOWN_ERROR;
    }

    mAnalyticsItem->writeToParcel(reply);
    return OK;
}

uint32_t RemoteMediaExtractor::flags() const {
    return mExtractor->flags();
}

status_t RemoteMediaExtractor::setMediaCas(const HInterfaceToken &casToken) {
    return mExtractor->setMediaCas((uint8_t*)casToken.data(), casToken.size());
}

const char * RemoteMediaExtractor::name() {
    return mExtractor->name();
}

////////////////////////////////////////////////////////////////////////////////

// static
sp<IMediaExtractor> RemoteMediaExtractor::wrap(
        MediaExtractor *extractor,
        const sp<DataSource> &source,
        const sp<RefBase> &plugin) {
    if (extractor == nullptr) {
        return nullptr;
    }
    return new RemoteMediaExtractor(extractor, source, plugin);
}

}  // namespace android
