cmake_minimum_required(VERSION 2.6)

INCLUDE_DIRECTORIES(include/) 

SET ( stfu_SRCS stfu.c stfu.h)
ADD_LIBRARY(stfu STATIC ${stfu_SRCS})







