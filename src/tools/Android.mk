LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

#
# serv tool
#
LOCAL_HDR_FILES :=

LOCAL_SRC_FILES := \
	migrate.c \
	service.c \
	serv.c

SERVAL_INCLUDE_DIR=$(LOCAL_PATH)/../../include

LOCAL_C_INCLUDES += \
	$(SERVAL_INCLUDE_DIR)

LOCAL_SHARED_LIBRARIES := libdl libservalctrl

EXTRA_DEFINES:=
DEFINES=-DOS_ANDROID -DOS_LINUX $(EXTRA_DEFINES)
LOCAL_CFLAGS:=-O2 -g $(DEFINES)
LOCAL_CPPFLAGS +=$(DEFINES)
LOCAL_PRELINK_MODULE := false
LOCAL_MODULE := serv

include $(BUILD_EXECUTABLE)
