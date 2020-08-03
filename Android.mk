LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := libmutils
LOCAL_MODULE_TAGS := optional
LOCAL_SRC_FILES := \
	src/uri.cpp \
  src/global-error.cpp \
  src/log/rlog.cpp
LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/include
LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_C_INCLUDES)
LOCAL_CPPFLAGS := -std=c++11
LOCAL_SHARED_LIBRARIES := liblog
include $(BUILD_SHARED_LIBRARY)
