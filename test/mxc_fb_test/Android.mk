ifeq ($(HAVE_FSL_IMX_IPU),true)

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_SRC_FILES := mxc_fb_vsync_test.c
LOCAL_CFLAGS += -DBUILD_FOR_ANDROID
LOCAL_C_INCLUDES += $(LOCAL_PATH)
LOCAL_SHARED_LIBRARIES := libutils libc
LOCAL_MODULE := mxc_fb_vsync_test
LOCAL_MODULE_TAGS := tests
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_SRC_FILES := mxc_set_fb_test.c
LOCAL_CFLAGS += -DBUILD_FOR_ANDROID
LOCAL_C_INCLUDES += $(LOCAL_PATH)
LOCAL_SHARED_LIBRARIES := libutils libc
LOCAL_MODULE := mxc_set_fb_test
LOCAL_MODULE_TAGS := tests
include $(BUILD_EXECUTABLE)

endif
