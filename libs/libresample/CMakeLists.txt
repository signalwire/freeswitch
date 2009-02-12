cmake_minimum_required(VERSION 2.6)

INCLUDE_DIRECTORIES(include/) 

SET ( resample_SRCS src/resample.c src/filterkit.c src/resamplesubs.c include/libresample.h)
ADD_LIBRARY(resample STATIC ${resample_SRCS})







