/*
 * Copyright (C) 2016 The Android Open Source Project
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

#define LOG_TAG "EffectsFactoryHalHidl"
//#define LOG_NDEBUG 0

#include <string.h>

#include <cutils/native_handle.h>
#include <media/EffectsFactoryApi.h>

#include "EffectsFactoryHalHidl.h"
#include "ConversionHelperHidl.h"
#include "EffectBufferHalHidl.h"
#include "EffectHalHidl.h"
#include "EffectHalLocal.h"
#include "HidlUtils.h"

using ::android::hardware::audio::common::CPP_VERSION::implementation::HidlUtils;
using ::android::hardware::Return;

namespace android {
namespace effect {
namespace CPP_VERSION {

using namespace ::android::hardware::audio::common::CPP_VERSION;
using namespace ::android::hardware::audio::effect::CPP_VERSION;

// PICO: implementation uuid of the PICO spatializer effect (vendor audio_effects.xml
// "spatializer", libspatializer.so), as in the factory libaudiohal@5.0.
static const effect_uuid_t kSpatializerImplUuid =
        { 0x61e3f827, 0x0417, 0x4360, 0xb0a7, { 0x3d, 0xe0, 0x52, 0x90, 0x1c, 0xac } };

EffectsFactoryHalHidl::EffectsFactoryHalHidl() : ConversionHelperHidl("EffectsFactory") {
    mEffectsFactory = IEffectsFactory::getService();
    if (mEffectsFactory == 0) {
        ALOGE("Failed to obtain IEffectsFactory service, terminating process.");
        exit(1);
    }
}

status_t EffectsFactoryHalHidl::queryAllDescriptors() {
    if (mEffectsFactory == 0) return NO_INIT;
    Result retval = Result::NOT_INITIALIZED;
    Return<void> ret = mEffectsFactory->getAllDescriptors(
            [&](Result r, const hidl_vec<EffectDescriptor>& result) {
                retval = r;
                if (retval == Result::OK) {
                    mLastDescriptors = result;
                }
            });
    if (ret.isOk()) {
        return retval == Result::OK ? OK : NO_INIT;
    }
    mLastDescriptors.resize(0);
    return processReturn(__FUNCTION__, ret);
}

status_t EffectsFactoryHalHidl::queryNumberEffects(uint32_t *pNumEffects) {
    status_t queryResult = queryAllDescriptors();
    if (queryResult == OK) {
        *pNumEffects = mLastDescriptors.size();
    }
    return queryResult;
}

status_t EffectsFactoryHalHidl::getDescriptor(
        uint32_t index, effect_descriptor_t *pDescriptor) {
    // TODO: We need somehow to track the changes on the server side
    // or figure out how to convert everybody to query all the descriptors at once.
    // TODO: check for nullptr
    if (mLastDescriptors.size() == 0) {
        status_t queryResult = queryAllDescriptors();
        if (queryResult != OK) return queryResult;
    }
    if (index >= mLastDescriptors.size()) return NAME_NOT_FOUND;
    EffectHalHidl::effectDescriptorToHal(mLastDescriptors[index], pDescriptor);
    return OK;
}

status_t EffectsFactoryHalHidl::getDescriptor(
        const effect_uuid_t *pEffectUuid, effect_descriptor_t *pDescriptor) {
    // TODO: check for nullptr
    if (mEffectsFactory == 0) return NO_INIT;
    Uuid hidlUuid;
    HidlUtils::uuidFromHal(*pEffectUuid, &hidlUuid);
    Result retval = Result::NOT_INITIALIZED;
    Return<void> ret = mEffectsFactory->getDescriptor(hidlUuid,
            [&](Result r, const EffectDescriptor& result) {
                retval = r;
                if (retval == Result::OK) {
                    EffectHalHidl::effectDescriptorToHal(result, pDescriptor);
                }
            });
    if (ret.isOk()) {
        if (retval == Result::OK) return OK;
        else if (retval == Result::INVALID_ARGUMENTS) return NAME_NOT_FOUND;
        else return NO_INIT;
    }
    return processReturn(__FUNCTION__, ret);
}

status_t EffectsFactoryHalHidl::createEffect(
        const effect_uuid_t *pEffectUuid, int32_t sessionId, int32_t ioId,
        sp<EffectHalInterface> *effect) {
    if (mEffectsFactory == 0) return NO_INIT;
    // PICO: as the factory, the spatializer effect is created in this process by the system
    // effect factory (libeffects, /system/lib64/soundfx/libspatializer.so).
    if (pEffectUuid != nullptr &&
            memcmp(pEffectUuid, &kSpatializerImplUuid, sizeof(effect_uuid_t)) == 0) {
        effect_handle_t handle;
        int status = EffectCreate(pEffectUuid, sessionId, ioId, &handle);
        if (status == 0) {
            *effect = new EffectHalLocal(handle);
        }
        return status;
    }
    Uuid hidlUuid;
    HidlUtils::uuidFromHal(*pEffectUuid, &hidlUuid);
    Result retval = Result::NOT_INITIALIZED;
    Return<void> ret = mEffectsFactory->createEffect(
            hidlUuid, sessionId, ioId,
            [&](Result r, const sp<IEffect>& result, uint64_t effectId) {
                retval = r;
                if (retval == Result::OK) {
                    *effect = new EffectHalHidl(result, effectId);
                }
            });
    if (ret.isOk()) {
        if (retval == Result::OK) return OK;
        else if (retval == Result::INVALID_ARGUMENTS) return NAME_NOT_FOUND;
        else return NO_INIT;
    }
    return processReturn(__FUNCTION__, ret);
}

status_t EffectsFactoryHalHidl::dumpEffects(int fd) {
    if (mEffectsFactory == 0) return NO_INIT;
    native_handle_t* hidlHandle = native_handle_create(1, 0);
    hidlHandle->data[0] = fd;
    Return<void> ret = mEffectsFactory->debug(hidlHandle, {} /* options */);
    native_handle_delete(hidlHandle);
    return processReturn(__FUNCTION__, ret);
}

status_t EffectsFactoryHalHidl::allocateBuffer(size_t size, sp<EffectBufferHalInterface>* buffer) {
    return EffectBufferHalHidl::allocate(size, buffer);
}

status_t EffectsFactoryHalHidl::mirrorBuffer(void* external, size_t size,
                          sp<EffectBufferHalInterface>* buffer) {
    return EffectBufferHalHidl::mirror(external, size, buffer);
}

} // namespace CPP_VERSION
} // namespace effect
} // namespace android
