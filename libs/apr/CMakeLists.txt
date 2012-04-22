cmake_minimum_required(VERSION 2.6)

INCLUDE_DIRECTORIES(include/) 

add_definitions(-DHAVE_CONFIG_H -DDARWIN -DSIGPROCMASK_SETS_THREAD_MASK)
INCLUDE_DIRECTORIES(BEFORE ${CMAKE_SOURCE_DIR}/libs/apr/include/arch/unix/ ./include)    

SET (apr_SRCS
  passwd/apr_getpass.c
  strings/apr_cpystrn.c
  strings/apr_strnatcmp.c
  strings/apr_strings.c
  strings/apr_strtok.c
  strings/apr_fnmatch.c
  strings/apr_snprintf.c
  tables/apr_tables.c
  tables/apr_hash.c
  atomic/unix/apr_atomic.c
  dso/unix/dso.c
  file_io/unix/flock.c
  file_io/unix/readwrite.c
  file_io/unix/filepath_util.c
  file_io/unix/seek.c
  file_io/unix/dir.c
  file_io/unix/mktemp.c
  file_io/unix/filedup.c
  file_io/unix/tempdir.c
  file_io/unix/filepath.c
  file_io/unix/pipe.c
  file_io/unix/open.c
  file_io/unix/filestat.c
  file_io/unix/copy.c
  file_io/unix/fileacc.c
  file_io/unix/fullrw.c
  locks/unix/thread_rwlock.c
  locks/unix/thread_mutex.c
  locks/unix/thread_cond.c
  locks/unix/proc_mutex.c
  locks/unix/global_mutex.c
  memory/unix/apr_pools.c
  misc/unix/charset.c
  misc/unix/env.c
  misc/unix/version.c
  misc/unix/rand.c
  misc/unix/start.c
  misc/unix/errorcodes.c
  misc/unix/getopt.c
  misc/unix/otherchild.c
  mmap/unix/mmap.c
  mmap/unix/common.c
  network_io/unix/sockaddr.c
  network_io/unix/sockopt.c
  network_io/unix/sendrecv.c
  network_io/unix/multicast.c
  network_io/unix/sockets.c
  network_io/unix/inet_ntop.c
  network_io/unix/inet_pton.c
  poll/unix/epoll.c
  poll/unix/select.c
  poll/unix/poll.c
  poll/unix/port.c
  poll/unix/kqueue.c
  random/unix/sha2.c
  random/unix/apr_random.c
  random/unix/sha2_glue.c
  shmem/unix/shm.c
  support/unix/waitio.c
  threadproc/unix/procsup.c
  threadproc/unix/thread.c
  threadproc/unix/signals.c
  threadproc/unix/proc.c
  threadproc/unix/threadpriv.c
  time/unix/time.c
  time/unix/timestr.c
  user/unix/userinfo.c
  user/unix/groupinfo.c)


ADD_LIBRARY(apr STATIC ${apr_SRCS})


#/bin/sh /Users/shaneburrell/freeswitch-svn-real/libs/apr/libtool --silent --mode=link  gcc -g -O2   -DHAVE_CONFIG_H -DDARWIN -DSIGPROCMASK_SETS_THREAD_MASK -no-cpp-precomp   -I./include -I/Users/shaneburrell/freeswitch-svn-real/arch/unix -I./include/arch/unix -I/Users/shaneburrell/freeswitch-svn-real  -version-info 2:8:2    -o libapr-1.la -rpath /usr/local/freeswitch/lib passwd/apr_getpass.lo strings/apr_cpystrn.lo strings/apr_strnatcmp.lo strings/apr_strings.lo strings/apr_strtok.lo strings/apr_fnmatch.lo strings/apr_snprintf.lo tables/apr_tables.lo tables/apr_hash.lo atomic/unix/apr_atomic.lo dso/unix/dso.lo file_io/unix/flock.lo file_io/unix/readwrite.lo file_io/unix/filepath_util.lo file_io/unix/seek.lo file_io/unix/dir.lo file_io/unix/mktemp.lo file_io/unix/filedup.lo file_io/unix/tempdir.lo file_io/unix/filepath.lo file_io/unix/pipe.lo file_io/unix/open.lo file_io/unix/filestat.lo file_io/unix/copy.lo file_io/unix/fileacc.lo file_io/unix/fullrw.lo locks/unix/thread_rwlock.lo locks/unix/thread_mutex.lo locks/unix/thread_cond.lo locks/unix/proc_mutex.lo locks/unix/global_mutex.lo memory/unix/apr_pools.lo misc/unix/charset.lo misc/unix/env.lo misc/unix/version.lo misc/unix/rand.lo misc/unix/start.lo misc/unix/errorcodes.lo misc/unix/getopt.lo misc/unix/otherchild.lo mmap/unix/mmap.lo mmap/unix/common.lo network_io/unix/sockaddr.lo network_io/unix/sockopt.lo network_io/unix/sendrecv.lo network_io/unix/multicast.lo network_io/unix/sockets.lo network_io/unix/inet_ntop.lo network_io/unix/inet_pton.lo poll/unix/epoll.lo poll/unix/select.lo poll/unix/poll.lo poll/unix/port.lo poll/unix/kqueue.lo random/unix/sha2.lo random/unix/apr_random.lo random/unix/sha2_glue.lo shmem/unix/shm.lo support/unix/waitio.lo threadproc/unix/procsup.lo threadproc/unix/thread.lo threadproc/unix/signals.lo threadproc/unix/proc.lo threadproc/unix/threadpriv.lo time/unix/time.lo time/unix/timestr.lo user/unix/userinfo.lo user/unix/groupinfo.lo   -ldl  -lpthread

#awk -f /Users/shaneburrell/freeswitch-svn-real/libs/apr/build/make_var_export.awk /Users/shaneburrell/freeswitch-svn-real/apr_thread_mutex.h /Users/shaneburrell/freeswitch-svn-real/apr_env.h /Users/shaneburrell/freeswitch-svn-real/apr_poll.h /Users/shaneburrell/freeswitch-svn-real/apr_fnmatch.h /Users/shaneburrell/freeswitch-svn-real/apr_global_mutex.h /Users/shaneburrell/freeswitch-svn-real/apr_pools.h /Users/shaneburrell/freeswitch-svn-real/apr_want.h /Users/shaneburrell/freeswitch-svn-real/apr_file_io.h /Users/shaneburrell/freeswitch-svn-real/apr_version.h /Users/shaneburrell/freeswitch-svn-real/apr_mmap.h /Users/shaneburrell/freeswitch-svn-real/apr_dso.h /Users/shaneburrell/freeswitch-svn-real/apr_thread_proc.h /Users/shaneburrell/freeswitch-svn-real/apr_errno.h /Users/shaneburrell/freeswitch-svn-real/apr_shm.h /Users/shaneburrell/freeswitch-svn-real/apr_network_io.h /Users/shaneburrell/freeswitch-svn-real/apr_signal.h /Users/shaneburrell/freeswitch-svn-real/apr_user.h /Users/shaneburrell/freeswitch-svn-real/apr_support.h /Users/shaneburrell/freeswitch-svn-real/apr_atomic.h /Users/shaneburrell/freeswitch-svn-real/apr_random.h /Users/shaneburrell/freeswitch-svn-real/apr_thread_cond.h /Users/shaneburrell/freeswitch-svn-real/apr_thread_rwlock.h /Users/shaneburrell/freeswitch-svn-real/apr_getopt.h /Users/shaneburrell/freeswitch-svn-real/apr_inherit.h /Users/shaneburrell/freeswitch-svn-real/apr_strings.h /Users/shaneburrell/freeswitch-svn-real/apr_general.h /Users/shaneburrell/freeswitch-svn-real/apr_proc_mutex.h /Users/shaneburrell/freeswitch-svn-real/apr_tables.h /Users/shaneburrell/freeswitch-svn-real/apr_ring.h /Users/shaneburrell/freeswitch-svn-real/apr_file_info.h /Users/shaneburrell/freeswitch-svn-real/apr_allocator.h /Users/shaneburrell/freeswitch-svn-real/apr_portable.h /Users/shaneburrell/freeswitch-svn-real/apr_hash.h /Users/shaneburrell/freeswitch-svn-real/apr_time.h /Users/shaneburrell/freeswitch-svn-real/apr_lib.h > export_vars.c

#awk -f /Users/shaneburrell/freeswitch-svn-real/libs/apr/build/make_exports.awk /Users/shaneburrell/freeswitch-svn-real/apr_thread_mutex.h /Users/shaneburrell/freeswitch-svn-real/apr_env.h /Users/shaneburrell/freeswitch-svn-real/apr_poll.h /Users/shaneburrell/freeswitch-svn-real/apr_fnmatch.h /Users/shaneburrell/freeswitch-svn-real/apr_global_mutex.h /Users/shaneburrell/freeswitch-svn-real/apr_pools.h /Users/shaneburrell/freeswitch-svn-real/apr_want.h /Users/shaneburrell/freeswitch-svn-real/apr_file_io.h /Users/shaneburrell/freeswitch-svn-real/apr_version.h /Users/shaneburrell/freeswitch-svn-real/apr_mmap.h /Users/shaneburrell/freeswitch-svn-real/apr_dso.h /Users/shaneburrell/freeswitch-svn-real/apr_thread_proc.h /Users/shaneburrell/freeswitch-svn-real/apr_errno.h /Users/shaneburrell/freeswitch-svn-real/apr_shm.h /Users/shaneburrell/freeswitch-svn-real/apr_network_io.h /Users/shaneburrell/freeswitch-svn-real/apr_signal.h /Users/shaneburrell/freeswitch-svn-real/apr_user.h /Users/shaneburrell/freeswitch-svn-real/apr_support.h /Users/shaneburrell/freeswitch-svn-real/apr_atomic.h /Users/shaneburrell/freeswitch-svn-real/apr_random.h /Users/shaneburrell/freeswitch-svn-real/apr_thread_cond.h /Users/shaneburrell/freeswitch-svn-real/apr_thread_rwlock.h /Users/shaneburrell/freeswitch-svn-real/apr_getopt.h /Users/shaneburrell/freeswitch-svn-real/apr_inherit.h /Users/shaneburrell/freeswitch-svn-real/apr_strings.h /Users/shaneburrell/freeswitch-svn-real/apr_general.h /Users/shaneburrell/freeswitch-svn-real/apr_proc_mutex.h /Users/shaneburrell/freeswitch-svn-real/apr_tables.h /Users/shaneburrell/freeswitch-svn-real/apr_ring.h /Users/shaneburrell/freeswitch-svn-real/apr_file_info.h /Users/shaneburrell/freeswitch-svn-real/apr_allocator.h /Users/shaneburrell/freeswitch-svn-real/apr_portable.h /Users/shaneburrell/freeswitch-svn-real/apr_hash.h /Users/shaneburrell/freeswitch-svn-real/apr_time.h /Users/shaneburrell/freeswitch-svn-real/apr_lib.h > exports.c
#gcc -E -DHAVE_CONFIG_H -DDARWIN -DSIGPROCMASK_SETS_THREAD_MASK -no-cpp-precomp   -I./include -I/Users/shaneburrell/freeswitch-svn-real/arch/unix -I./include/arch/unix -I/Users/shaneburrell/freeswitch-svn-real  exports.c | grep "ap_hack_" | sed -e 's/^.*[)]\(.*\);$/\1/' >> apr.exp
#gcc -E -DHAVE_CONFIG_H -DDARWIN -DSIGPROCMASK_SETS_THREAD_MASK -no-cpp-precomp   -I./include -I/Users/shaneburrell/freeswitch-svn-real/arch/unix -I./include/arch/unix -I/Users/shaneburrell/freeswitch-svn-real  export_vars.c | sed -e 's/^\#[^!]*//' | sed -e '/^$/d' >> apr.exp






















