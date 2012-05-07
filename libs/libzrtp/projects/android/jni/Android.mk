LOCAL_PATH := $(call my-dir)/../../..

include $(CLEAR_VARS)
	
LOCAL_MODULE    :=	libzrtp
MY_SRC_PATH 	:=	src

MY_SRC_FILES 	:= $(MY_SRC_PATH)/zrtp.c \
					$(MY_SRC_PATH)/zrtp_crc.c \
					$(MY_SRC_PATH)/zrtp_crypto_aes.c \
					$(MY_SRC_PATH)/zrtp_crypto_atl.c \
					$(MY_SRC_PATH)/zrtp_crypto_hash.c \
					$(MY_SRC_PATH)/zrtp_crypto_pk.c \
					$(MY_SRC_PATH)/zrtp_crypto_sas.c \
					$(MY_SRC_PATH)/zrtp_datatypes.c \
					$(MY_SRC_PATH)/zrtp_engine.c \
					$(MY_SRC_PATH)/zrtp_engine_driven.c \
					$(MY_SRC_PATH)/zrtp_iface_cache.c \
					$(MY_SRC_PATH)/zrtp_iface_scheduler.c \
					$(MY_SRC_PATH)/zrtp_iface_sys.c \
					$(MY_SRC_PATH)/zrtp_initiator.c \
					$(MY_SRC_PATH)/zrtp_legal.c \
					$(MY_SRC_PATH)/zrtp_list.c \
					$(MY_SRC_PATH)/zrtp_log.c \
					$(MY_SRC_PATH)/zrtp_pbx.c \
					$(MY_SRC_PATH)/zrtp_protocol.c \
					$(MY_SRC_PATH)/zrtp_responder.c \
					$(MY_SRC_PATH)/zrtp_rng.c \
					$(MY_SRC_PATH)/zrtp_srtp_builtin.c \
					$(MY_SRC_PATH)/zrtp_srtp_dm.c \
					$(MY_SRC_PATH)/zrtp_string.c \
					$(MY_SRC_PATH)/zrtp_utils.c \
					$(MY_SRC_PATH)/zrtp_utils_proto.c

MY_SRC_FILES 	+= 	third_party/bgaes/aes_modes.c \
					third_party/bgaes/sha2.c \
					third_party/bgaes/sha1.c \
					third_party/bgaes/aestab.c \
					third_party/bgaes/aeskey.c \
					third_party/bgaes/aescrypt.c

MY_SRC_FILES 	+= 	third_party/bnlib/bn.c \
					third_party/bnlib/bn32.c \
					third_party/bnlib/bninit32.c \
					third_party/bnlib/lbn32.c \
					third_party/bnlib/lbnmem.c \
					third_party/bnlib/legal.c

LOCAL_SRC_FILES := $(MY_SRC_FILES)
					
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/include

LOCAL_C_INCLUDES := $(LOCAL_PATH)/include
LOCAL_C_INCLUDES += $(LOCAL_PATH)/third_party/bnlib
LOCAL_C_INCLUDES += $(LOCAL_PATH)/third_party/bgaes

LOCAL_ARM_MODE := arm
LOCAL_CFLAGS := -DANDROID_NDK=5

#include $(BUILD_STATIC_LIBRARY)
include $(BUILD_SHARED_LIBRARY)

#
# Dummy shared library to build libzrtp.a
#

# include $(CLEAR_VARS)
# 
# LOCAL_MODULE    := libzrtp-dummy
# LOCAL_STATIC_LIBRARIES := libzrtp
# 
# include $(BUILD_SHARED_LIBRARY)
