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

#include "SpatializerPoseController.h"

#define LOG_TAG "SpatializerPoseController"
//#define LOG_NDEBUG 0

#include <algorithm>
#include <pthread.h>

#include <utils/Log.h>
#include <utils/Timers.h>

#include "HeadPoseProvider.h"

namespace android {

namespace {

// Minimum pose update period.
constexpr std::chrono::nanoseconds kMinUpdatePeriod = std::chrono::milliseconds(4);

// Delay before retrying to connect to the tracking service. The factory logs it in
// milliseconds but waits the same count in microseconds.
constexpr int64_t kInitRetryDelay = 1000;

}  // namespace

SpatializerPoseController::SpatializerPoseController(Listener* listener,
                                                     std::chrono::nanoseconds updatePeriod,
                                                     std::chrono::nanoseconds predictTime)
    : mListener(listener),
      mProvider(new HeadPoseProvider()),
      mUpdatePeriod(std::max(updatePeriod, kMinUpdatePeriod)),
      mPredictTime(std::max(predictTime, std::chrono::nanoseconds(0))),
      mThread([this] {
          if (pthread_setname_np(pthread_self(), "SpatialPose") != 0) {
              ALOGW("Failed to set SpatializerPoseController thread name");
          }

          // Connect to the tracking service.
          while (!mProvider->init()) {
              ALOGE("init head tracker failed! try later %lldms", (long long)kInitRetryDelay);
              std::unique_lock<std::mutex> lock(mMutex);
              if (mCondVar.wait_for(lock, std::chrono::microseconds(kInitRetryDelay),
                                    [this] { return mShouldExit; })) {
                  return;
              }
          }

          ALOGI("Update pose period %lldns predict %lldns",
                (long long)mUpdatePeriod.count(), (long long)mPredictTime.count());
          while (true) {
              std::vector<float> headPose;
              const nsecs_t now = systemTime(SYSTEM_TIME_MONOTONIC);
              HeadTrackingData data;
              if (mProvider->getHeadTrackingData(mPredictTime.count() + now, &data) != 0) {
                  ALOGW("get head tracking data failed!");
              } else {
                  headPose.push_back(static_cast<float>(data.orientation[0]));
                  headPose.push_back(static_cast<float>(data.orientation[1]));
                  headPose.push_back(static_cast<float>(data.orientation[2]));
                  headPose.push_back(static_cast<float>(data.orientationW));
                  headPose.push_back(static_cast<float>(data.position[0]));
                  headPose.push_back(static_cast<float>(data.position[1]));
                  headPose.push_back(static_cast<float>(data.position[2]));
                  mListener->onHeadPoseUpdate(headPose);
              }

              std::unique_lock<std::mutex> lock(mMutex);
              if (mCondVar.wait_for(lock, mUpdatePeriod, [this] { return mShouldExit; })) {
                  break;
              }
          }
      }) {
    ALOGD("SpatializerPoseController");
}

SpatializerPoseController::~SpatializerPoseController() {
    ALOGD("~SpatializerPoseController");
    {
        std::unique_lock<std::mutex> lock(mMutex);
        mShouldExit = true;
        mCondVar.notify_all();
    }
    mThread.join();
    if (mProvider != nullptr) {
        delete mProvider;
        mProvider = nullptr;
    }
}

}  // namespace android
