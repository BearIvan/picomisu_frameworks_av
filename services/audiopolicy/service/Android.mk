LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
    AudioPolicyService.cpp \
    AudioPolicyEffects.cpp \
    AudioPolicyInterfaceImpl.cpp \
    AudioPolicyClientImpl.cpp \
    Spatializer.cpp \
    SpatializerPoseController.cpp \
    HeadPoseProvider.cpp

LOCAL_C_INCLUDES := \
    frameworks/av/services/audioflinger \
    $(call include-path-for, audio-utils)

LOCAL_HEADER_LIBRARIES := \
    libaudiopolicycommon \
    libaudiopolicyengine_interface_headers \
    libaudiopolicymanager_interface_headers \
    libaudiohal_headers \
    libaudioeventtracking_headers

LOCAL_SHARED_LIBRARIES := \
    libcutils \
    libutils \
    liblog \
    libbinder \
    libaudioclient \
    libaudioutils \
    libhardware_legacy \
    libaudiopolicymanager \
    libmedia_helper \
    libmediametrics \
    libmediautils \
    libeffectsconfig \
    libsensorprivacy \
    libaudiohal \
    libstagefright_foundation

# PICO: audio event tracking (factory libaudioeventtracking.so, 64-bit only).
LOCAL_SHARED_LIBRARIES_arm64 := libaudioeventtracking

LOCAL_EXPORT_SHARED_LIBRARY_HEADERS := \
    libsensorprivacy

LOCAL_STATIC_LIBRARIES := \
    libaudiopolicycomponents

LOCAL_MULTILIB := $(AUDIOSERVER_MULTILIB)

LOCAL_MODULE:= libaudiopolicyservice

LOCAL_CFLAGS += -fvisibility=hidden
LOCAL_CFLAGS += -Wall -Werror

include $(BUILD_SHARED_LIBRARY)

