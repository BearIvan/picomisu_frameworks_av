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

#pragma once

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

namespace android {

class HeadPoseProvider;

/**
 * PICO OS 5.13.7 spatializer pose controller.
 *
 * Unlike the Android 13 one (sensors + head tracking processor), the factory controller polls
 * the head pose of the PICO tracking service (HeadPoseProvider) on its own thread every update
 * period, predicted a fixed time ahead, and hands it to the listener (the Spatializer), which
 * forwards it to the spatializer effect.
 */
class SpatializerPoseController {
  public:
    /**
     * Listener interface for getting pose updates.
     */
    class Listener {
      public:
        virtual ~Listener() = default;

        /**
         * Head pose: rotation x, y, z, w then translation x, y, z.
         */
        virtual void onHeadPoseUpdate(const std::vector<float>& headPose) = 0;
    };

    /**
     * Ctor.
     * updatePeriod is the pose polling period (at least 4 ms), predictTime how far ahead the
     * pose is predicted (at least 0).
     */
    SpatializerPoseController(Listener* listener, std::chrono::nanoseconds updatePeriod,
                              std::chrono::nanoseconds predictTime);

    /** Dtor. */
    ~SpatializerPoseController();

  private:
    mutable std::mutex mMutex;
    Listener* const mListener;
    HeadPoseProvider* mProvider;
    const std::chrono::nanoseconds mUpdatePeriod;
    const std::chrono::nanoseconds mPredictTime;
    std::condition_variable mCondVar;
    bool mShouldExit = false;

    // The factory declares the thread before the condition variable and the exit flag; it is
    // last here so that it is started after them (it is started in the initializer list).
    std::thread mThread;
};

}  // namespace android
