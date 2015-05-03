LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
#第三方库
LOCAL_MODULE := CreateSecondLib
LOCAL_SRC_FILES := prebuilt/libCreateSecondLib.so

include $(PREBUILT_SHARED_LIBRARY)


include $(CLEAR_VARS)

LOCAL_MODULE    := UsbDetector
LOCAL_SRC_FILES := UsbDetector.c

#Android的NDK 日志链接库
#头文件 #include <android/log.h>
#开启 宏 LOCAL_CFLAGS +=  -UNDEBUG
LOCAL_LDLIBS    := -llog

LOCAL_CFLAGS += -UNDEBUG  -D_DEBUG
#宏定义 TOMCAT=2
LOCAL_CFLAGS += -DTOMCAT=2

LOCAL_CFLAGS    += -g
LOCAL_CFLAGS    += -ggdb
LOCAL_CFLAGS    += -O1


LOCAL_SHARED_LIBRARIES :=  CreateSecondLib

include $(BUILD_SHARED_LIBRARY)



include $(CLEAR_VARS)

LOCAL_MODULE    := Main
LOCAL_SRC_FILES := Main.c MainExt.c
LOCAL_LDLIBS    := -llog

include $(BUILD_EXECUTABLE)
