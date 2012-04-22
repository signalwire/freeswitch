cmake_minimum_required(VERSION 2.6)

INCLUDE_DIRECTORIES(include/) 

add_definitions(-DHAVE_CONFIG_H -DDARWIN -DSIGPROCMASK_SETS_THREAD_MASK)
INCLUDE_DIRECTORIES(BEFORE ${CMAKE_SOURCE_DIR}/libs/apr-util/include ${CMAKE_SOURCE_DIR}/libs/apr-util/include/private  ${CMAKE_SOURCE_DIR}/libs/apr/include)    


SET (apr-util_SRCS
crypto/apr_md5.c 
crypto/uuid.c 
crypto/apr_sha1.c 
crypto/getuuid.c
crypto/apr_md4.c
encoding/apr_base64.c
hooks/apr_hooks.c
misc/apr_reslist.c 
misc/apr_rmm.c 
misc/apr_date.c
misc/apu_version.c
misc/apr_queue.c
uri/apr_uri.c
strmatch/apr_strmatch.c
xlate/xlate.c 
)

LINK_LIBRARIES ("-liconv -ldl -lpthread")

ADD_LIBRARY(apr-util STATIC ${apr-util_SRCS})







