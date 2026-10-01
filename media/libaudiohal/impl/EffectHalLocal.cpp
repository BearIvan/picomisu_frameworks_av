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

#define LOG_TAG "EffectHalLocal"
//#define LOG_NDEBUG 0

#include <string.h>

#include <media/EffectsFactoryApi.h>
#include <utils/Log.h>

#include "EffectHalLocal.h"

namespace android {
namespace effect {
namespace CPP_VERSION {

// PICO: reconstructed from the factory libaudiohal@5.0 (the Android 8 local effect wrapper with
// a check of the effect handle in every entry point).

EffectHalLocal::EffectHalLocal(effect_handle_t handle)
        : mHandle(handle) {
}

EffectHalLocal::~EffectHalLocal() {
    if (mHandle != nullptr) {
        int status = EffectRelease(mHandle);
        ALOGW_IF(status, "Error releasing effect %p: %s", mHandle, strerror(-status));
        mHandle = nullptr;
    }
}

status_t EffectHalLocal::setInBuffer(const sp<EffectBufferHalInterface>& buffer) {
    mInBuffer = buffer;
    return OK;
}

status_t EffectHalLocal::setOutBuffer(const sp<EffectBufferHalInterface>& buffer) {
    mOutBuffer = buffer;
    return OK;
}

status_t EffectHalLocal::process() {
    if (mHandle == nullptr || mInBuffer == nullptr || mOutBuffer == nullptr) {
        return NO_INIT;
    }
    return (*mHandle)->process(mHandle, mInBuffer->audioBuffer(), mOutBuffer->audioBuffer());
}

status_t EffectHalLocal::processReverse() {
    if (mHandle == nullptr || mInBuffer == nullptr || mOutBuffer == nullptr) {
        return NO_INIT;
    }
    return (*mHandle)->process_reverse(
            mHandle, mInBuffer->audioBuffer(), mOutBuffer->audioBuffer());
}

status_t EffectHalLocal::command(uint32_t cmdCode, uint32_t cmdSize, void *pCmdData,
        uint32_t *replySize, void *pReplyData) {
    if (mHandle == nullptr) {
        return NO_INIT;
    }
    return (*mHandle)->command(mHandle, cmdCode, cmdSize, pCmdData, replySize, pReplyData);
}

status_t EffectHalLocal::getDescriptor(effect_descriptor_t *pDescriptor) {
    if (mHandle == nullptr) {
        return NO_INIT;
    }
    if (pDescriptor == nullptr) {
        return BAD_VALUE;
    }
    memset(pDescriptor, 0, sizeof(effect_descriptor_t));
    return (*mHandle)->get_descriptor(mHandle, pDescriptor);
}

status_t EffectHalLocal::close() {
    return mHandle == nullptr ? NO_INIT : OK;
}

bool EffectHalLocal::isLocal() const {
    return true;
}

status_t EffectHalLocal::dump(int fd) {
    if (mHandle == nullptr) {
        return NO_INIT;
    }
    return (*mHandle)->command(mHandle, EFFECT_CMD_DUMP, sizeof(fd), &fd, nullptr, nullptr);
}

} // namespace CPP_VERSION
} // namespace effect
} // namespace android
