#!/bin/sh
# Create patch for Codec 2 support inside Asterisk

ORIG=~/asterisk-1.8.9.0-orig
CODEC2=~/asterisk-1.8.9.0-codec2
diff -ruN $ORIG/include/asterisk/frame.h $CODEC2/include/asterisk/frame.h > asterisk-codec2.patch
diff -ruN $ORIG/main/frame.c $CODEC2/main/frame.c >> asterisk-codec2.patch
diff -ruN $ORIG/main/channel.c $CODEC2/main/channel.c >> asterisk-codec2.patch
diff -ruN $ORIG/main/rtp_engine.c $CODEC2/main/rtp_engine.c >> asterisk-codec2.patch


