#all 所有平台 armeabi armeabi-v7a mips x86
#可以选择一个平台 编译
APP_ABI := armeabi-v7a
# With this you don't have to add -g to your compiler flags, ndk-build will do so automatically.
AAPP_OPTIM := debug

# 与 AndroidMainfest.xml miniSDK必须一致  否则Unknown Application ABI
APP_PLATFORM := android-14

 # cpu stuff
 # 没有作用 ,需要控制 APP_ABI
TARGET_CPU_ABI := armeabi-v7a
TARGET_CPU_ABI2 := armeabi
TARGET_CPU_SMP := true
TARGET_ARCH := arm
TARGET_ARCH_VARIANT := armv7-a-neon
TARGET_CPU_VARIANT := cortex-a7