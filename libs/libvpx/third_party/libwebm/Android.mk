LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE:= libwebm
LOCAL_SRC_FILES:= mkvparser.cpp \
                  mkvreader.cpp \
                  mkvmuxer.cpp \
                  mkvmuxerutil.cpp \
                  mkvwriter.cpp
include $(BUILD_STATIC_LIBRARY)
