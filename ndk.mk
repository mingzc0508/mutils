LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := libmutils
LOCAL_SRC_FILES := \
  src/uri.cpp \
  src/global-error.cpp \
  src/log/rlog.cpp
LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/include
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/include
LOCAL_SHARED_LIBRARIES := liblog
include $(BUILD_SHARED_LIBRARY)
