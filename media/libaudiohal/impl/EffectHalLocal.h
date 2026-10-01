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

#ifndef ANDROID_HARDWARE_EFFECT_HAL_LOCAL_H
#define ANDROID_HARDWARE_EFFECT_HAL_LOCAL_H

#include <hardware/audio_effect.h>
#include <media/audiohal/EffectHalInterface.h>

namespace android {
namespace effect {
namespace CPP_VERSION {

// PICO OS 5.13.7: an effect created in the audioserver process by the system effect factory
// (libeffects EffectCreate) instead of the audio effect HAL service. The factory libaudiohal@5.0
// creates the spatializer effect this way (EffectsFactoryHalHidl::createEffect).
class EffectHalLocal : public EffectHalInterface
{
  public:
    explicit EffectHalLocal(effect_handle_t handle);

    // Set the input buffer.
    virtual status_t setInBuffer(const sp<EffectBufferHalInterface>& buffer);

    // Set the output buffer.
    virtual status_t setOutBuffer(const sp<EffectBufferHalInterface>& buffer);

    // Effect process function.
    virtual status_t process();

    // Process reverse stream function. This function is used to pass
    // a reference stream to the effect engine.
    virtual status_t processReverse();

    // Send a command and receive a response to/from effect engine.
    virtual status_t command(uint32_t cmdCode, uint32_t cmdSize, void *pCmdData,
            uint32_t *replySize, void *pReplyData);

    // Returns the effect descriptor.
    virtual status_t getDescriptor(effect_descriptor_t *pDescriptor);

    // Free resources on the remote side.
    virtual status_t close();

    // Whether it's a local implementation.
    virtual bool isLocal() const;

    virtual status_t dump(int fd);

  private:
    effect_handle_t mHandle;
    sp<EffectBufferHalInterface> mInBuffer;
    sp<EffectBufferHalInterface> mOutBuffer;

    // The destructor automatically releases the effect.
    virtual ~EffectHalLocal();
};

} // namespace CPP_VERSION
} // namespace effect
} // namespace android

#endif // ANDROID_HARDWARE_EFFECT_HAL_LOCAL_H
