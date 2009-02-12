cmake_minimum_required(VERSION 2.6)

INCLUDE_DIRECTORIES(include/) 

SET ( teletone_SRCS src/libteletone.h src/libteletone_detect.c src/libteletone_detect.h src/libteletone_generate.c src/libteletone_generate.h)
ADD_LIBRARY(teletone STATIC ${teletone_SRCS})







