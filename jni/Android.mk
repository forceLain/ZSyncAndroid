LOCAL_PATH := $(call my-dir)
APP_OPTIM := debug

include $(CLEAR_VARS)

FILE_LIST1 := $(wildcard $(LOCAL_PATH)/*.c)
FILE_LIST2 := $(wildcard $(LOCAL_PATH)/librcksum/*.c)
FILE_LIST3 := $(wildcard $(LOCAL_PATH)/libzsync/*.c)
FILE_LIST4 := $(wildcard $(LOCAL_PATH)/zlib/*.c)

LOCAL_LDLIBS := -L$(SYSROOT)/usr/lib -llog 
LOCAL_MODULE    := zsyncandroid
LOCAL_SRC_FILES := $(FILE_LIST1:$(LOCAL_PATH)/%=%) \
  $(FILE_LIST2:$(LOCAL_PATH)/%=%) \
  $(FILE_LIST3:$(LOCAL_PATH)/%=%) \
  $(FILE_LIST4:$(LOCAL_PATH)/%=%) \

include $(BUILD_SHARED_LIBRARY)
