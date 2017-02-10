ifeq ($(HAVE_FSL_IMX_IPU),true)

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
#LOCAL_SRC_FILES := mxc_v4l2_tvin.c
# mxc_v4l2_tvin_pdi.c is a attached file in PDI community on Nov 3, 2016 2:47 AM (https://community.nxp.com/thread/428784?messageTarget=all&start=50&mode=comments)
#LOCAL_SRC_FILES := mxc_v4l2_tvin_pdi.c
# This one is form rosen
LOCAL_SRC_FILES := PDI_mxc_v4l2_tvin.c
LOCAL_CFLAGS += -DBUILD_FOR_ANDROID
LOCAL_C_INCLUDES += $(LOCAL_PATH)
LOCAL_SHARED_LIBRARIES := libutils libc
LOCAL_MODULE := PDI_mxc-v4l2-tvin
LOCAL_MODULE_TAGS := tests
include $(BUILD_EXECUTABLE)

endif
