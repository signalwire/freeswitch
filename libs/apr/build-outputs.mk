# DO NOT EDIT. AUTOMATICALLY GENERATED.

passwd/fspr_getpass.lo: passwd/fspr_getpass.c .make.dirs include/fspr_allocator.h include/fspr_general.h include/fspr_errno.h include/fspr_want.h include/fspr_strings.h include/fspr_thread_mutex.h include/fspr_lib.h include/fspr_pools.h
strings/fspr_cpystrn.lo: strings/fspr_cpystrn.c .make.dirs include/fspr_allocator.h include/fspr_general.h include/fspr_errno.h include/fspr_want.h include/fspr_strings.h include/fspr_thread_mutex.h include/fspr_lib.h include/fspr_pools.h
strings/fspr_strnatcmp.lo: strings/fspr_strnatcmp.c .make.dirs include/fspr_allocator.h include/fspr_general.h include/fspr_errno.h include/fspr_want.h include/fspr_strings.h include/fspr_thread_mutex.h include/fspr_lib.h include/fspr_pools.h
strings/fspr_strings.lo: strings/fspr_strings.c .make.dirs include/fspr_allocator.h include/fspr_general.h include/fspr_errno.h include/fspr_want.h include/fspr_strings.h include/fspr_thread_mutex.h include/fspr_lib.h include/fspr_pools.h
strings/fspr_strtok.lo: strings/fspr_strtok.c .make.dirs include/fspr_allocator.h include/fspr_general.h include/fspr_errno.h include/fspr_want.h include/fspr_strings.h include/fspr_thread_mutex.h include/fspr_pools.h
strings/fspr_fnmatch.lo: strings/fspr_fnmatch.c .make.dirs include/fspr_allocator.h include/fspr_general.h include/fspr_errno.h include/fspr_want.h include/fspr_file_info.h include/fspr_user.h include/fspr_fnmatch.h include/fspr_strings.h include/fspr_thread_mutex.h include/fspr_lib.h include/fspr_time.h include/fspr_pools.h include/fspr_tables.h
strings/fspr_snprintf.lo: strings/fspr_snprintf.c .make.dirs include/fspr_allocator.h include/fspr_general.h include/fspr_network_io.h include/fspr_portable.h include/fspr_inherit.h include/fspr_file_info.h include/fspr_thread_mutex.h include/fspr_thread_proc.h include/fspr_dso.h include/fspr_tables.h include/fspr_errno.h include/fspr_proc_mutex.h include/fspr_user.h include/fspr_file_io.h include/fspr_strings.h include/fspr_want.h include/fspr_lib.h include/fspr_shm.h include/fspr_global_mutex.h include/fspr_time.h include/fspr_pools.h
tables/fspr_tables.lo: tables/fspr_tables.c .make.dirs include/fspr_allocator.h include/fspr_general.h include/fspr_errno.h include/fspr_want.h include/fspr_strings.h include/fspr_thread_mutex.h include/fspr_lib.h include/fspr_pools.h include/fspr_tables.h
tables/fspr_hash.lo: tables/fspr_hash.c .make.dirs include/fspr_allocator.h include/fspr_general.h include/fspr_errno.h include/fspr_want.h include/fspr_thread_mutex.h include/fspr_hash.h include/fspr_pools.h

OBJECTS_all = passwd/fspr_getpass.lo strings/fspr_cpystrn.lo strings/fspr_strnatcmp.lo strings/fspr_strings.lo strings/fspr_strtok.lo strings/fspr_fnmatch.lo strings/fspr_snprintf.lo tables/fspr_tables.lo tables/fspr_hash.lo

atomic/unix/fspr_atomic.lo: atomic/unix/fspr_atomic.c .make.dirs include/fspr_allocator.h include/fspr_general.h include/fspr_errno.h include/fspr_want.h include/fspr_thread_mutex.h include/fspr_atomic.h include/fspr_pools.h

OBJECTS_atomic_unix = atomic/unix/fspr_atomic.lo

dso/unix/dso.lo: dso/unix/dso.c .make.dirs include/fspr_allocator.h include/fspr_general.h include/fspr_network_io.h include/fspr_portable.h include/fspr_inherit.h include/fspr_file_info.h include/fspr_thread_mutex.h include/fspr_thread_proc.h include/fspr_dso.h include/fspr_tables.h include/fspr_errno.h include/fspr_proc_mutex.h include/fspr_user.h include/fspr_file_io.h include/fspr_strings.h include/fspr_want.h include/fspr_shm.h include/fspr_global_mutex.h include/fspr_time.h include/fspr_pools.h

OBJECTS_dso_unix = dso/unix/dso.lo

file_io/unix/flock.lo: file_io/unix/flock.c .make.dirs 
file_io/unix/readwrite.lo: file_io/unix/readwrite.c .make.dirs include/fspr_support.h include/fspr_allocator.h include/fspr_network_io.h include/fspr_general.h include/fspr_inherit.h include/fspr_file_info.h include/fspr_thread_mutex.h include/fspr_tables.h include/fspr_errno.h include/fspr_want.h include/fspr_user.h include/fspr_file_io.h include/fspr_strings.h include/fspr_time.h include/fspr_pools.h
file_io/unix/filepath_util.lo: file_io/unix/filepath_util.c .make.dirs include/fspr_allocator.h include/fspr_general.h include/fspr_errno.h include/fspr_want.h include/fspr_strings.h include/fspr_thread_mutex.h include/fspr_pools.h include/fspr_tables.h
file_io/unix/seek.lo: file_io/unix/seek.c .make.dirs 
file_io/unix/dir.lo: file_io/unix/dir.c .make.dirs include/fspr_allocator.h include/fspr_general.h include/fspr_network_io.h include/fspr_portable.h include/fspr_inherit.h include/fspr_file_info.h include/fspr_thread_mutex.h include/fspr_thread_proc.h include/fspr_dso.h include/fspr_tables.h include/fspr_errno.h include/fspr_proc_mutex.h include/fspr_user.h include/fspr_file_io.h include/fspr_strings.h include/fspr_want.h include/fspr_shm.h include/fspr_global_mutex.h include/fspr_time.h include/fspr_pools.h
file_io/unix/mktemp.lo: file_io/unix/mktemp.c .make.dirs include/fspr_allocator.h include/fspr_general.h include/fspr_network_io.h include/fspr_portable.h include/fspr_inherit.h include/fspr_file_info.h include/fspr_thread_mutex.h include/fspr_thread_proc.h include/fspr_dso.h include/fspr_tables.h include/fspr_errno.h include/fspr_proc_mutex.h include/fspr_user.h include/fspr_file_io.h include/fspr_strings.h include/fspr_want.h include/fspr_shm.h include/fspr_global_mutex.h include/fspr_time.h include/fspr_pools.h
file_io/unix/filedup.lo: file_io/unix/filedup.c .make.dirs include/fspr_allocator.h include/fspr_general.h include/fspr_network_io.h include/fspr_portable.h include/fspr_inherit.h include/fspr_file_info.h include/fspr_thread_mutex.h include/fspr_thread_proc.h include/fspr_dso.h include/fspr_tables.h include/fspr_errno.h include/fspr_want.h include/fspr_user.h include/fspr_file_io.h include/fspr_strings.h include/fspr_proc_mutex.h include/fspr_shm.h include/fspr_global_mutex.h include/fspr_time.h include/fspr_pools.h
file_io/unix/tempdir.lo: file_io/unix/tempdir.c .make.dirs include/fspr_allocator.h include/fspr_general.h include/fspr_errno.h include/fspr_env.h include/fspr_file_info.h include/fspr_user.h include/fspr_file_io.h include/fspr_strings.h include/fspr_want.h include/fspr_thread_mutex.h include/fspr_inherit.h include/fspr_time.h include/fspr_pools.h include/fspr_tables.h
file_io/unix/filepath.lo: file_io/unix/filepath.c .make.dirs include/fspr_allocator.h include/fspr_general.h include/fspr_errno.h include/fspr_want.h include/fspr_file_info.h include/fspr_user.h include/fspr_file_io.h include/fspr_strings.h include/fspr_inherit.h include/fspr_thread_mutex.h include/fspr_time.h include/fspr_pools.h include/fspr_tables.h
file_io/unix/pipe.lo: file_io/unix/pipe.c .make.dirs include/fspr_allocator.h include/fspr_general.h include/fspr_network_io.h include/fspr_portable.h include/fspr_inherit.h include/fspr_file_info.h include/fspr_thread_mutex.h include/fspr_thread_proc.h include/fspr_dso.h include/fspr_tables.h include/fspr_errno.h include/fspr_proc_mutex.h include/fspr_user.h include/fspr_file_io.h include/fspr_strings.h include/fspr_want.h include/fspr_shm.h include/fspr_global_mutex.h include/fspr_time.h include/fspr_pools.h
file_io/unix/open.lo: file_io/unix/open.c .make.dirs include/fspr_allocator.h include/fspr_general.h include/fspr_network_io.h include/fspr_portable.h include/fspr_inherit.h include/fspr_file_info.h include/fspr_thread_mutex.h include/fspr_thread_proc.h include/fspr_dso.h include/fspr_tables.h include/fspr_errno.h include/fspr_want.h include/fspr_user.h include/fspr_file_io.h include/fspr_strings.h include/fspr_proc_mutex.h include/fspr_hash.h include/fspr_shm.h include/fspr_global_mutex.h include/fspr_time.h include/fspr_pools.h
file_io/unix/filestat.lo: file_io/unix/filestat.c .make.dirs include/fspr_allocator.h include/fspr_general.h include/fspr_errno.h include/fspr_want.h include/fspr_file_info.h include/fspr_user.h include/fspr_file_io.h include/fspr_strings.h include/fspr_inherit.h include/fspr_thread_mutex.h include/fspr_time.h include/fspr_pools.h include/fspr_tables.h
file_io/unix/copy.lo: file_io/unix/copy.c .make.dirs include/fspr_allocator.h include/fspr_general.h include/fspr_errno.h include/fspr_inherit.h include/fspr_file_info.h include/fspr_user.h include/fspr_file_io.h include/fspr_want.h include/fspr_thread_mutex.h include/fspr_time.h include/fspr_pools.h include/fspr_tables.h
file_io/unix/fileacc.lo: file_io/unix/fileacc.c .make.dirs include/fspr_allocator.h include/fspr_general.h include/fspr_thread_mutex.h include/fspr_errno.h include/fspr_want.h include/fspr_pools.h include/fspr_strings.h
file_io/unix/fullrw.lo: file_io/unix/fullrw.c .make.dirs include/fspr_allocator.h include/fspr_general.h include/fspr_errno.h include/fspr_inherit.h include/fspr_file_info.h include/fspr_user.h include/fspr_file_io.h include/fspr_want.h include/fspr_thread_mutex.h include/fspr_time.h include/fspr_pools.h include/fspr_tables.h

OBJECTS_file_io_unix = file_io/unix/flock.lo file_io/unix/readwrite.lo file_io/unix/filepath_util.lo file_io/unix/seek.lo file_io/unix/dir.lo file_io/unix/mktemp.lo file_io/unix/filedup.lo file_io/unix/tempdir.lo file_io/unix/filepath.lo file_io/unix/pipe.lo file_io/unix/open.lo file_io/unix/filestat.lo file_io/unix/copy.lo file_io/unix/fileacc.lo file_io/unix/fullrw.lo

locks/unix/thread_rwlock.lo: locks/unix/thread_rwlock.c .make.dirs 
locks/unix/thread_mutex.lo: locks/unix/thread_mutex.c .make.dirs include/fspr_want.h
locks/unix/thread_cond.lo: locks/unix/thread_cond.c .make.dirs 
locks/unix/proc_mutex.lo: locks/unix/proc_mutex.c .make.dirs include/fspr_allocator.h include/fspr_general.h include/fspr_thread_mutex.h include/fspr_errno.h include/fspr_want.h include/fspr_pools.h include/fspr_strings.h
locks/unix/global_mutex.lo: locks/unix/global_mutex.c .make.dirs include/fspr_allocator.h include/fspr_general.h include/fspr_network_io.h include/fspr_portable.h include/fspr_inherit.h include/fspr_file_info.h include/fspr_thread_mutex.h include/fspr_thread_proc.h include/fspr_dso.h include/fspr_tables.h include/fspr_errno.h include/fspr_proc_mutex.h include/fspr_user.h include/fspr_file_io.h include/fspr_strings.h include/fspr_want.h include/fspr_shm.h include/fspr_global_mutex.h include/fspr_time.h include/fspr_pools.h

OBJECTS_locks_unix = locks/unix/thread_rwlock.lo locks/unix/thread_mutex.lo locks/unix/thread_cond.lo locks/unix/proc_mutex.lo locks/unix/global_mutex.lo

memory/unix/fspr_pools.lo: memory/unix/fspr_pools.c .make.dirs include/fspr_allocator.h include/fspr_general.h include/fspr_network_io.h include/fspr_portable.h include/fspr_env.h include/fspr_file_info.h include/fspr_thread_mutex.h include/fspr_thread_proc.h include/fspr_dso.h include/fspr_atomic.h include/fspr_tables.h include/fspr_errno.h include/fspr_want.h include/fspr_user.h include/fspr_file_io.h include/fspr_strings.h include/fspr_inherit.h include/fspr_hash.h include/fspr_lib.h include/fspr_proc_mutex.h include/fspr_shm.h include/fspr_global_mutex.h include/fspr_time.h include/fspr_pools.h

OBJECTS_memory_unix = memory/unix/fspr_pools.lo

misc/unix/charset.lo: misc/unix/charset.c .make.dirs include/fspr_allocator.h include/fspr_general.h include/fspr_network_io.h include/fspr_portable.h include/fspr_inherit.h include/fspr_file_info.h include/fspr_thread_mutex.h include/fspr_thread_proc.h include/fspr_dso.h include/fspr_tables.h include/fspr_errno.h include/fspr_proc_mutex.h include/fspr_user.h include/fspr_file_io.h include/fspr_strings.h include/fspr_want.h include/fspr_shm.h include/fspr_global_mutex.h include/fspr_time.h include/fspr_pools.h
misc/unix/env.lo: misc/unix/env.c .make.dirs include/fspr_allocator.h include/fspr_general.h include/fspr_errno.h include/fspr_want.h include/fspr_env.h include/fspr_thread_mutex.h include/fspr_pools.h
misc/unix/version.lo: misc/unix/version.c .make.dirs include/fspr_allocator.h include/fspr_general.h include/fspr_errno.h include/fspr_want.h include/fspr_version.h include/fspr_thread_mutex.h include/fspr_pools.h
misc/unix/rand.lo: misc/unix/rand.c .make.dirs include/fspr_allocator.h include/fspr_general.h include/fspr_errno.h include/fspr_want.h include/fspr_thread_mutex.h include/fspr_pools.h
misc/unix/start.lo: misc/unix/start.c .make.dirs include/fspr_allocator.h include/fspr_general.h include/fspr_errno.h include/fspr_signal.h include/fspr_want.h include/fspr_thread_mutex.h include/fspr_atomic.h include/fspr_pools.h
misc/unix/errorcodes.lo: misc/unix/errorcodes.c .make.dirs include/fspr_allocator.h include/fspr_general.h include/fspr_errno.h include/fspr_want.h include/fspr_strings.h include/fspr_thread_mutex.h include/fspr_lib.h include/fspr_dso.h include/fspr_pools.h
misc/unix/getopt.lo: misc/unix/getopt.c .make.dirs include/fspr_allocator.h include/fspr_general.h include/fspr_errno.h include/fspr_want.h include/fspr_strings.h include/fspr_thread_mutex.h include/fspr_lib.h include/fspr_pools.h
misc/unix/otherchild.lo: misc/unix/otherchild.c .make.dirs 

OBJECTS_misc_unix = misc/unix/charset.lo misc/unix/env.lo misc/unix/version.lo misc/unix/rand.lo misc/unix/start.lo misc/unix/errorcodes.lo misc/unix/getopt.lo misc/unix/otherchild.lo

mmap/unix/mmap.lo: mmap/unix/mmap.c .make.dirs include/fspr_allocator.h include/fspr_general.h include/fspr_network_io.h include/fspr_portable.h include/fspr_inherit.h include/fspr_file_info.h include/fspr_thread_mutex.h include/fspr_thread_proc.h include/fspr_dso.h include/fspr_tables.h include/fspr_errno.h include/fspr_want.h include/fspr_user.h include/fspr_file_io.h include/fspr_strings.h include/fspr_proc_mutex.h include/fspr_shm.h include/fspr_ring.h include/fspr_global_mutex.h include/fspr_time.h include/fspr_pools.h include/fspr_mmap.h
mmap/unix/common.lo: mmap/unix/common.c .make.dirs include/fspr_allocator.h include/fspr_general.h include/fspr_errno.h include/fspr_inherit.h include/fspr_file_info.h include/fspr_user.h include/fspr_file_io.h include/fspr_want.h include/fspr_thread_mutex.h include/fspr_ring.h include/fspr_tables.h include/fspr_time.h include/fspr_pools.h include/fspr_mmap.h

OBJECTS_mmap_unix = mmap/unix/mmap.lo mmap/unix/common.lo

network_io/unix/sockaddr.lo: network_io/unix/sockaddr.c .make.dirs include/fspr_allocator.h include/fspr_general.h include/fspr_errno.h include/fspr_want.h include/fspr_strings.h include/fspr_thread_mutex.h include/fspr_lib.h include/fspr_pools.h
network_io/unix/sockopt.lo: network_io/unix/sockopt.c .make.dirs include/fspr_allocator.h include/fspr_general.h include/fspr_thread_mutex.h include/fspr_errno.h include/fspr_want.h include/fspr_pools.h include/fspr_strings.h
network_io/unix/sendrecv.lo: network_io/unix/sendrecv.c .make.dirs include/fspr_support.h include/fspr_allocator.h include/fspr_user.h include/fspr_network_io.h include/fspr_general.h include/fspr_inherit.h include/fspr_file_info.h include/fspr_errno.h include/fspr_file_io.h include/fspr_want.h include/fspr_thread_mutex.h include/fspr_time.h include/fspr_pools.h include/fspr_tables.h
network_io/unix/multicast.lo: network_io/unix/multicast.c .make.dirs include/fspr_support.h include/fspr_allocator.h include/fspr_network_io.h include/fspr_portable.h include/fspr_inherit.h include/fspr_file_info.h include/fspr_general.h include/fspr_thread_mutex.h include/fspr_thread_proc.h include/fspr_dso.h include/fspr_tables.h include/fspr_errno.h include/fspr_want.h include/fspr_user.h include/fspr_file_io.h include/fspr_proc_mutex.h include/fspr_shm.h include/fspr_global_mutex.h include/fspr_time.h include/fspr_pools.h
network_io/unix/sockets.lo: network_io/unix/sockets.c .make.dirs include/fspr_support.h include/fspr_allocator.h include/fspr_network_io.h include/fspr_portable.h include/fspr_inherit.h include/fspr_file_info.h include/fspr_general.h include/fspr_thread_mutex.h include/fspr_thread_proc.h include/fspr_dso.h include/fspr_tables.h include/fspr_errno.h include/fspr_want.h include/fspr_user.h include/fspr_file_io.h include/fspr_strings.h include/fspr_proc_mutex.h include/fspr_shm.h include/fspr_global_mutex.h include/fspr_time.h include/fspr_pools.h
network_io/unix/inet_ntop.lo: network_io/unix/inet_ntop.c .make.dirs include/fspr_allocator.h include/fspr_general.h include/fspr_thread_mutex.h include/fspr_errno.h include/fspr_want.h include/fspr_pools.h include/fspr_strings.h
network_io/unix/inet_pton.lo: network_io/unix/inet_pton.c .make.dirs 

OBJECTS_network_io_unix = network_io/unix/sockaddr.lo network_io/unix/sockopt.lo network_io/unix/sendrecv.lo network_io/unix/multicast.lo network_io/unix/sockets.lo network_io/unix/inet_ntop.lo network_io/unix/inet_pton.lo

poll/unix/epoll.lo: poll/unix/epoll.c .make.dirs 
poll/unix/select.lo: poll/unix/select.c .make.dirs include/fspr_allocator.h include/fspr_general.h include/fspr_network_io.h include/fspr_portable.h include/fspr_inherit.h include/fspr_file_info.h include/fspr_thread_mutex.h include/fspr_thread_proc.h include/fspr_dso.h include/fspr_tables.h include/fspr_errno.h include/fspr_proc_mutex.h include/fspr_user.h include/fspr_file_io.h include/fspr_want.h include/fspr_poll.h include/fspr_shm.h include/fspr_global_mutex.h include/fspr_time.h include/fspr_pools.h
poll/unix/poll.lo: poll/unix/poll.c .make.dirs 
poll/unix/port.lo: poll/unix/port.c .make.dirs 
poll/unix/kqueue.lo: poll/unix/kqueue.c .make.dirs 

OBJECTS_poll_unix = poll/unix/epoll.lo poll/unix/select.lo poll/unix/poll.lo poll/unix/port.lo poll/unix/kqueue.lo

random/unix/sha2.lo: random/unix/sha2.c .make.dirs 
random/unix/fspr_random.lo: random/unix/fspr_random.c .make.dirs include/fspr_allocator.h include/fspr_general.h include/fspr_errno.h include/fspr_inherit.h include/fspr_file_info.h include/fspr_user.h include/fspr_file_io.h include/fspr_want.h include/fspr_thread_mutex.h include/fspr_random.h include/fspr_thread_proc.h include/fspr_time.h include/fspr_pools.h include/fspr_tables.h
random/unix/sha2_glue.lo: random/unix/sha2_glue.c .make.dirs include/fspr_allocator.h include/fspr_general.h include/fspr_errno.h include/fspr_want.h include/fspr_thread_mutex.h include/fspr_random.h include/fspr_pools.h

OBJECTS_random_unix = random/unix/sha2.lo random/unix/fspr_random.lo random/unix/sha2_glue.lo

shmem/unix/shm.lo: shmem/unix/shm.c .make.dirs include/fspr_allocator.h include/fspr_general.h include/fspr_errno.h include/fspr_want.h include/fspr_user.h include/fspr_strings.h include/fspr_thread_mutex.h include/fspr_pools.h

OBJECTS_shmem_unix = shmem/unix/shm.lo

support/unix/waitio.lo: support/unix/waitio.c .make.dirs include/fspr_support.h include/fspr_allocator.h include/fspr_network_io.h include/fspr_general.h include/fspr_inherit.h include/fspr_file_info.h include/fspr_thread_mutex.h include/fspr_tables.h include/fspr_errno.h include/fspr_want.h include/fspr_user.h include/fspr_file_io.h include/fspr_poll.h include/fspr_time.h include/fspr_pools.h

OBJECTS_support_unix = support/unix/waitio.lo

threadproc/unix/procsup.lo: threadproc/unix/procsup.c .make.dirs 
threadproc/unix/thread.lo: threadproc/unix/thread.c .make.dirs include/fspr_allocator.h include/fspr_general.h include/fspr_network_io.h include/fspr_portable.h include/fspr_inherit.h include/fspr_file_info.h include/fspr_thread_mutex.h include/fspr_thread_proc.h include/fspr_dso.h include/fspr_tables.h include/fspr_errno.h include/fspr_proc_mutex.h include/fspr_user.h include/fspr_file_io.h include/fspr_want.h include/fspr_shm.h include/fspr_global_mutex.h include/fspr_time.h include/fspr_pools.h
threadproc/unix/signals.lo: threadproc/unix/signals.c .make.dirs include/fspr_allocator.h include/fspr_general.h include/fspr_errno.h include/fspr_signal.h include/fspr_strings.h include/fspr_want.h include/fspr_thread_mutex.h include/fspr_pools.h
threadproc/unix/proc.lo: threadproc/unix/proc.c .make.dirs include/fspr_allocator.h include/fspr_general.h include/fspr_network_io.h include/fspr_portable.h include/fspr_signal.h include/fspr_file_info.h include/fspr_thread_mutex.h include/fspr_thread_proc.h include/fspr_dso.h include/fspr_proc_mutex.h include/fspr_tables.h include/fspr_errno.h include/fspr_want.h include/fspr_user.h include/fspr_file_io.h include/fspr_strings.h include/fspr_inherit.h include/fspr_random.h include/fspr_shm.h include/fspr_global_mutex.h include/fspr_time.h include/fspr_pools.h
threadproc/unix/threadpriv.lo: threadproc/unix/threadpriv.c .make.dirs include/fspr_allocator.h include/fspr_general.h include/fspr_network_io.h include/fspr_portable.h include/fspr_inherit.h include/fspr_file_info.h include/fspr_thread_mutex.h include/fspr_thread_proc.h include/fspr_dso.h include/fspr_tables.h include/fspr_errno.h include/fspr_proc_mutex.h include/fspr_user.h include/fspr_file_io.h include/fspr_want.h include/fspr_shm.h include/fspr_global_mutex.h include/fspr_time.h include/fspr_pools.h

OBJECTS_threadproc_unix = threadproc/unix/procsup.lo threadproc/unix/thread.lo threadproc/unix/signals.lo threadproc/unix/proc.lo threadproc/unix/threadpriv.lo

time/unix/time.lo: time/unix/time.c .make.dirs include/fspr_allocator.h include/fspr_general.h include/fspr_network_io.h include/fspr_portable.h include/fspr_inherit.h include/fspr_file_info.h include/fspr_thread_mutex.h include/fspr_thread_proc.h include/fspr_dso.h include/fspr_tables.h include/fspr_errno.h include/fspr_want.h include/fspr_user.h include/fspr_file_io.h include/fspr_strings.h include/fspr_proc_mutex.h include/fspr_lib.h include/fspr_shm.h include/fspr_global_mutex.h include/fspr_time.h include/fspr_pools.h
time/unix/timestr.lo: time/unix/timestr.c .make.dirs include/fspr_allocator.h include/fspr_general.h include/fspr_network_io.h include/fspr_portable.h include/fspr_inherit.h include/fspr_file_info.h include/fspr_thread_mutex.h include/fspr_thread_proc.h include/fspr_dso.h include/fspr_tables.h include/fspr_errno.h include/fspr_want.h include/fspr_user.h include/fspr_file_io.h include/fspr_proc_mutex.h include/fspr_lib.h include/fspr_shm.h include/fspr_global_mutex.h include/fspr_time.h include/fspr_pools.h

OBJECTS_time_unix = time/unix/time.lo time/unix/timestr.lo

user/unix/userinfo.lo: user/unix/userinfo.c .make.dirs include/fspr_allocator.h include/fspr_general.h include/fspr_network_io.h include/fspr_portable.h include/fspr_inherit.h include/fspr_file_info.h include/fspr_thread_mutex.h include/fspr_thread_proc.h include/fspr_dso.h include/fspr_tables.h include/fspr_errno.h include/fspr_want.h include/fspr_user.h include/fspr_file_io.h include/fspr_strings.h include/fspr_proc_mutex.h include/fspr_shm.h include/fspr_global_mutex.h include/fspr_time.h include/fspr_pools.h
user/unix/groupinfo.lo: user/unix/groupinfo.c .make.dirs include/fspr_allocator.h include/fspr_general.h include/fspr_network_io.h include/fspr_portable.h include/fspr_inherit.h include/fspr_file_info.h include/fspr_thread_mutex.h include/fspr_thread_proc.h include/fspr_dso.h include/fspr_tables.h include/fspr_errno.h include/fspr_proc_mutex.h include/fspr_user.h include/fspr_file_io.h include/fspr_strings.h include/fspr_want.h include/fspr_shm.h include/fspr_global_mutex.h include/fspr_time.h include/fspr_pools.h

OBJECTS_user_unix = user/unix/userinfo.lo user/unix/groupinfo.lo

OBJECTS_unix = $(OBJECTS_all) $(OBJECTS_atomic_unix) $(OBJECTS_dso_unix) $(OBJECTS_file_io_unix) $(OBJECTS_locks_unix) $(OBJECTS_memory_unix) $(OBJECTS_misc_unix) $(OBJECTS_mmap_unix) $(OBJECTS_network_io_unix) $(OBJECTS_poll_unix) $(OBJECTS_random_unix) $(OBJECTS_shmem_unix) $(OBJECTS_support_unix) $(OBJECTS_threadproc_unix) $(OBJECTS_time_unix) $(OBJECTS_user_unix)

dso/aix/dso.lo: dso/aix/dso.c .make.dirs include/fspr_allocator.h include/fspr_general.h include/fspr_network_io.h include/fspr_portable.h include/fspr_inherit.h include/fspr_file_info.h include/fspr_thread_mutex.h include/fspr_thread_proc.h include/fspr_dso.h include/fspr_tables.h include/fspr_errno.h include/fspr_proc_mutex.h include/fspr_user.h include/fspr_file_io.h include/fspr_want.h include/fspr_shm.h include/fspr_global_mutex.h include/fspr_time.h include/fspr_pools.h

OBJECTS_dso_aix = dso/aix/dso.lo

OBJECTS_aix = $(OBJECTS_all) $(OBJECTS_atomic_unix) $(OBJECTS_dso_aix) $(OBJECTS_file_io_unix) $(OBJECTS_locks_unix) $(OBJECTS_memory_unix) $(OBJECTS_misc_unix) $(OBJECTS_mmap_unix) $(OBJECTS_network_io_unix) $(OBJECTS_poll_unix) $(OBJECTS_random_unix) $(OBJECTS_shmem_unix) $(OBJECTS_support_unix) $(OBJECTS_threadproc_unix) $(OBJECTS_time_unix) $(OBJECTS_user_unix)

dso/beos/dso.lo: dso/beos/dso.c .make.dirs include/fspr_allocator.h include/fspr_general.h include/fspr_network_io.h include/fspr_portable.h include/fspr_inherit.h include/fspr_file_info.h include/fspr_thread_mutex.h include/fspr_thread_proc.h include/fspr_dso.h include/fspr_tables.h include/fspr_errno.h include/fspr_proc_mutex.h include/fspr_user.h include/fspr_file_io.h include/fspr_want.h include/fspr_shm.h include/fspr_global_mutex.h include/fspr_time.h include/fspr_pools.h

OBJECTS_dso_beos = dso/beos/dso.lo

locks/beos/thread_rwlock.lo: locks/beos/thread_rwlock.c .make.dirs include/fspr_allocator.h include/fspr_general.h include/fspr_network_io.h include/fspr_portable.h include/fspr_inherit.h include/fspr_file_info.h include/fspr_thread_mutex.h include/fspr_thread_proc.h include/fspr_dso.h include/fspr_tables.h include/fspr_errno.h include/fspr_proc_mutex.h include/fspr_user.h include/fspr_file_io.h include/fspr_strings.h include/fspr_want.h include/fspr_shm.h include/fspr_global_mutex.h include/fspr_time.h include/fspr_pools.h
locks/beos/thread_mutex.lo: locks/beos/thread_mutex.c .make.dirs include/fspr_allocator.h include/fspr_general.h include/fspr_network_io.h include/fspr_portable.h include/fspr_inherit.h include/fspr_file_info.h include/fspr_thread_mutex.h include/fspr_thread_proc.h include/fspr_dso.h include/fspr_tables.h include/fspr_errno.h include/fspr_proc_mutex.h include/fspr_user.h include/fspr_file_io.h include/fspr_strings.h include/fspr_want.h include/fspr_shm.h include/fspr_global_mutex.h include/fspr_time.h include/fspr_pools.h
locks/beos/thread_cond.lo: locks/beos/thread_cond.c .make.dirs include/fspr_allocator.h include/fspr_general.h include/fspr_network_io.h include/fspr_portable.h include/fspr_inherit.h include/fspr_file_info.h include/fspr_thread_mutex.h include/fspr_thread_proc.h include/fspr_dso.h include/fspr_tables.h include/fspr_errno.h include/fspr_proc_mutex.h include/fspr_user.h include/fspr_file_io.h include/fspr_strings.h include/fspr_want.h include/fspr_shm.h include/fspr_global_mutex.h include/fspr_time.h include/fspr_pools.h
locks/beos/proc_mutex.lo: locks/beos/proc_mutex.c .make.dirs include/fspr_allocator.h include/fspr_general.h include/fspr_network_io.h include/fspr_portable.h include/fspr_inherit.h include/fspr_file_info.h include/fspr_thread_mutex.h include/fspr_thread_proc.h include/fspr_dso.h include/fspr_tables.h include/fspr_errno.h include/fspr_proc_mutex.h include/fspr_user.h include/fspr_file_io.h include/fspr_strings.h include/fspr_want.h include/fspr_shm.h include/fspr_global_mutex.h include/fspr_time.h include/fspr_pools.h

OBJECTS_locks_beos = locks/beos/thread_rwlock.lo locks/beos/thread_mutex.lo locks/beos/thread_cond.lo locks/beos/proc_mutex.lo

network_io/beos/socketcommon.lo: network_io/beos/socketcommon.c .make.dirs 
network_io/beos/sendrecv.lo: network_io/beos/sendrecv.c .make.dirs include/fspr_allocator.h include/fspr_general.h include/fspr_thread_mutex.h include/fspr_errno.h include/fspr_want.h include/fspr_time.h include/fspr_pools.h

OBJECTS_network_io_beos = network_io/beos/socketcommon.lo network_io/beos/sendrecv.lo

shmem/beos/shm.lo: shmem/beos/shm.c .make.dirs include/fspr_allocator.h include/fspr_general.h include/fspr_network_io.h include/fspr_portable.h include/fspr_inherit.h include/fspr_file_info.h include/fspr_thread_mutex.h include/fspr_thread_proc.h include/fspr_dso.h include/fspr_tables.h include/fspr_errno.h include/fspr_want.h include/fspr_user.h include/fspr_file_io.h include/fspr_strings.h include/fspr_proc_mutex.h include/fspr_lib.h include/fspr_shm.h include/fspr_global_mutex.h include/fspr_time.h include/fspr_pools.h

OBJECTS_shmem_beos = shmem/beos/shm.lo

threadproc/beos/fspr_proc_stub.lo: threadproc/beos/fspr_proc_stub.c .make.dirs 
threadproc/beos/thread.lo: threadproc/beos/thread.c .make.dirs include/fspr_allocator.h include/fspr_general.h include/fspr_network_io.h include/fspr_portable.h include/fspr_inherit.h include/fspr_file_info.h include/fspr_thread_mutex.h include/fspr_thread_proc.h include/fspr_dso.h include/fspr_tables.h include/fspr_errno.h include/fspr_proc_mutex.h include/fspr_user.h include/fspr_file_io.h include/fspr_want.h include/fspr_shm.h include/fspr_global_mutex.h include/fspr_time.h include/fspr_pools.h
threadproc/beos/proc.lo: threadproc/beos/proc.c .make.dirs include/fspr_allocator.h include/fspr_general.h include/fspr_thread_mutex.h include/fspr_errno.h include/fspr_want.h include/fspr_pools.h include/fspr_strings.h
threadproc/beos/threadpriv.lo: threadproc/beos/threadpriv.c .make.dirs 
threadproc/beos/threadproc_common.lo: threadproc/beos/threadproc_common.c .make.dirs 

OBJECTS_threadproc_beos = threadproc/beos/fspr_proc_stub.lo threadproc/beos/thread.lo threadproc/beos/proc.lo threadproc/beos/threadpriv.lo threadproc/beos/threadproc_common.lo

OBJECTS_beos = $(OBJECTS_all) $(OBJECTS_atomic_unix) $(OBJECTS_dso_beos) $(OBJECTS_file_io_unix) $(OBJECTS_locks_beos) $(OBJECTS_memory_unix) $(OBJECTS_misc_unix) $(OBJECTS_mmap_unix) $(OBJECTS_network_io_beos) $(OBJECTS_poll_unix) $(OBJECTS_random_unix) $(OBJECTS_shmem_beos) $(OBJECTS_support_unix) $(OBJECTS_threadproc_beos) $(OBJECTS_time_unix) $(OBJECTS_user_unix)

dso/os2/dso.lo: dso/os2/dso.c .make.dirs include/fspr_allocator.h include/fspr_general.h include/fspr_network_io.h include/fspr_portable.h include/fspr_inherit.h include/fspr_file_info.h include/fspr_thread_mutex.h include/fspr_thread_proc.h include/fspr_dso.h include/fspr_tables.h include/fspr_errno.h include/fspr_proc_mutex.h include/fspr_user.h include/fspr_file_io.h include/fspr_strings.h include/fspr_want.h include/fspr_shm.h include/fspr_global_mutex.h include/fspr_time.h include/fspr_pools.h

OBJECTS_dso_os2 = dso/os2/dso.lo

file_io/os2/dir_make_recurse.lo: file_io/os2/dir_make_recurse.c .make.dirs include/fspr_allocator.h include/fspr_general.h include/fspr_errno.h include/fspr_inherit.h include/fspr_file_info.h include/fspr_user.h include/fspr_file_io.h include/fspr_strings.h include/fspr_want.h include/fspr_thread_mutex.h include/fspr_lib.h include/fspr_time.h include/fspr_pools.h include/fspr_tables.h
file_io/os2/filesys.lo: file_io/os2/filesys.c .make.dirs include/fspr_allocator.h include/fspr_general.h include/fspr_errno.h include/fspr_want.h include/fspr_strings.h include/fspr_thread_mutex.h include/fspr_lib.h include/fspr_pools.h
file_io/os2/flock.lo: file_io/os2/flock.c .make.dirs 
file_io/os2/readwrite.lo: file_io/os2/readwrite.c .make.dirs include/fspr_allocator.h include/fspr_general.h include/fspr_errno.h include/fspr_inherit.h include/fspr_file_info.h include/fspr_user.h include/fspr_file_io.h include/fspr_strings.h include/fspr_want.h include/fspr_thread_mutex.h include/fspr_lib.h include/fspr_time.h include/fspr_pools.h include/fspr_tables.h
file_io/os2/filepath_util.lo: file_io/os2/filepath_util.c .make.dirs 
file_io/os2/seek.lo: file_io/os2/seek.c .make.dirs include/fspr_allocator.h include/fspr_general.h include/fspr_errno.h include/fspr_inherit.h include/fspr_file_info.h include/fspr_user.h include/fspr_file_io.h include/fspr_want.h include/fspr_thread_mutex.h include/fspr_lib.h include/fspr_time.h include/fspr_pools.h include/fspr_tables.h
file_io/os2/dir.lo: file_io/os2/dir.c .make.dirs include/fspr_allocator.h include/fspr_general.h include/fspr_network_io.h include/fspr_portable.h include/fspr_inherit.h include/fspr_file_info.h include/fspr_thread_mutex.h include/fspr_thread_proc.h include/fspr_dso.h include/fspr_tables.h include/fspr_errno.h include/fspr_proc_mutex.h include/fspr_user.h include/fspr_file_io.h include/fspr_strings.h include/fspr_want.h include/fspr_lib.h include/fspr_shm.h include/fspr_global_mutex.h include/fspr_time.h include/fspr_pools.h
file_io/os2/mktemp.lo: file_io/os2/mktemp.c .make.dirs 
file_io/os2/filedup.lo: file_io/os2/filedup.c .make.dirs include/fspr_allocator.h include/fspr_general.h include/fspr_errno.h include/fspr_inherit.h include/fspr_file_info.h include/fspr_user.h include/fspr_file_io.h include/fspr_strings.h include/fspr_want.h include/fspr_thread_mutex.h include/fspr_lib.h include/fspr_time.h include/fspr_pools.h include/fspr_tables.h
file_io/os2/tempdir.lo: file_io/os2/tempdir.c .make.dirs 
file_io/os2/maperrorcode.lo: file_io/os2/maperrorcode.c .make.dirs include/fspr_allocator.h include/fspr_general.h include/fspr_errno.h include/fspr_inherit.h include/fspr_file_info.h include/fspr_user.h include/fspr_file_io.h include/fspr_want.h include/fspr_thread_mutex.h include/fspr_time.h include/fspr_pools.h include/fspr_tables.h
file_io/os2/filepath.lo: file_io/os2/filepath.c .make.dirs 
file_io/os2/pipe.lo: file_io/os2/pipe.c .make.dirs include/fspr_allocator.h include/fspr_general.h include/fspr_network_io.h include/fspr_portable.h include/fspr_inherit.h include/fspr_file_info.h include/fspr_thread_mutex.h include/fspr_thread_proc.h include/fspr_dso.h include/fspr_tables.h include/fspr_errno.h include/fspr_want.h include/fspr_user.h include/fspr_file_io.h include/fspr_strings.h include/fspr_proc_mutex.h include/fspr_lib.h include/fspr_shm.h include/fspr_global_mutex.h include/fspr_time.h include/fspr_pools.h
file_io/os2/open.lo: file_io/os2/open.c .make.dirs include/fspr_allocator.h include/fspr_inherit.h include/fspr_file_info.h include/fspr_thread_mutex.h include/fspr_thread_proc.h include/fspr_tables.h include/fspr_errno.h include/fspr_want.h include/fspr_user.h include/fspr_file_io.h include/fspr_strings.h include/fspr_shm.h include/fspr_general.h include/fspr_network_io.h include/fspr_portable.h include/fspr_dso.h include/fspr_proc_mutex.h include/fspr_lib.h include/fspr_global_mutex.h include/fspr_time.h include/fspr_pools.h
file_io/os2/filestat.lo: file_io/os2/filestat.c .make.dirs include/fspr_allocator.h include/fspr_general.h include/fspr_errno.h include/fspr_inherit.h include/fspr_file_info.h include/fspr_user.h include/fspr_file_io.h include/fspr_strings.h include/fspr_want.h include/fspr_thread_mutex.h include/fspr_lib.h include/fspr_time.h include/fspr_pools.h include/fspr_tables.h
file_io/os2/copy.lo: file_io/os2/copy.c .make.dirs 
file_io/os2/fileacc.lo: file_io/os2/fileacc.c .make.dirs 
file_io/os2/fullrw.lo: file_io/os2/fullrw.c .make.dirs 

OBJECTS_file_io_os2 = file_io/os2/dir_make_recurse.lo file_io/os2/filesys.lo file_io/os2/flock.lo file_io/os2/readwrite.lo file_io/os2/filepath_util.lo file_io/os2/seek.lo file_io/os2/dir.lo file_io/os2/mktemp.lo file_io/os2/filedup.lo file_io/os2/tempdir.lo file_io/os2/maperrorcode.lo file_io/os2/filepath.lo file_io/os2/pipe.lo file_io/os2/open.lo file_io/os2/filestat.lo file_io/os2/copy.lo file_io/os2/fileacc.lo file_io/os2/fullrw.lo

locks/os2/thread_rwlock.lo: locks/os2/thread_rwlock.c .make.dirs include/fspr_allocator.h include/fspr_general.h include/fspr_network_io.h include/fspr_portable.h include/fspr_inherit.h include/fspr_file_info.h include/fspr_thread_mutex.h include/fspr_thread_proc.h include/fspr_dso.h include/fspr_tables.h include/fspr_errno.h include/fspr_want.h include/fspr_user.h include/fspr_file_io.h include/fspr_strings.h include/fspr_proc_mutex.h include/fspr_lib.h include/fspr_shm.h include/fspr_global_mutex.h include/fspr_time.h include/fspr_pools.h
locks/os2/thread_mutex.lo: locks/os2/thread_mutex.c .make.dirs include/fspr_allocator.h include/fspr_general.h include/fspr_network_io.h include/fspr_portable.h include/fspr_inherit.h include/fspr_file_info.h include/fspr_thread_mutex.h include/fspr_thread_proc.h include/fspr_dso.h include/fspr_tables.h include/fspr_errno.h include/fspr_want.h include/fspr_user.h include/fspr_file_io.h include/fspr_strings.h include/fspr_proc_mutex.h include/fspr_lib.h include/fspr_shm.h include/fspr_global_mutex.h include/fspr_time.h include/fspr_pools.h
locks/os2/thread_cond.lo: locks/os2/thread_cond.c .make.dirs include/fspr_allocator.h include/fspr_general.h include/fspr_network_io.h include/fspr_portable.h include/fspr_inherit.h include/fspr_file_info.h include/fspr_thread_mutex.h include/fspr_thread_proc.h include/fspr_dso.h include/fspr_tables.h include/fspr_errno.h include/fspr_want.h include/fspr_user.h include/fspr_file_io.h include/fspr_strings.h include/fspr_proc_mutex.h include/fspr_lib.h include/fspr_shm.h include/fspr_global_mutex.h include/fspr_time.h include/fspr_pools.h
locks/os2/proc_mutex.lo: locks/os2/proc_mutex.c .make.dirs include/fspr_allocator.h include/fspr_general.h include/fspr_network_io.h include/fspr_portable.h include/fspr_inherit.h include/fspr_file_info.h include/fspr_thread_mutex.h include/fspr_thread_proc.h include/fspr_dso.h include/fspr_tables.h include/fspr_errno.h include/fspr_want.h include/fspr_user.h include/fspr_file_io.h include/fspr_strings.h include/fspr_proc_mutex.h include/fspr_lib.h include/fspr_shm.h include/fspr_global_mutex.h include/fspr_time.h include/fspr_pools.h

OBJECTS_locks_os2 = locks/os2/thread_rwlock.lo locks/os2/thread_mutex.lo locks/os2/thread_cond.lo locks/os2/proc_mutex.lo

network_io/os2/sockaddr.lo: network_io/os2/sockaddr.c .make.dirs 
network_io/os2/sockopt.lo: network_io/os2/sockopt.c .make.dirs include/fspr_allocator.h include/fspr_general.h include/fspr_network_io.h include/fspr_errno.h include/fspr_want.h include/fspr_file_info.h include/fspr_user.h include/fspr_file_io.h include/fspr_strings.h include/fspr_inherit.h include/fspr_thread_mutex.h include/fspr_lib.h include/fspr_time.h include/fspr_pools.h include/fspr_tables.h
network_io/os2/sendrecv_udp.lo: network_io/os2/sendrecv_udp.c .make.dirs include/fspr_support.h include/fspr_general.h include/fspr_network_io.h include/fspr_inherit.h include/fspr_file_info.h include/fspr_allocator.h include/fspr_thread_mutex.h include/fspr_tables.h include/fspr_errno.h include/fspr_want.h include/fspr_user.h include/fspr_file_io.h include/fspr_lib.h include/fspr_time.h include/fspr_pools.h
network_io/os2/sendrecv.lo: network_io/os2/sendrecv.c .make.dirs include/fspr_allocator.h include/fspr_general.h include/fspr_network_io.h include/fspr_errno.h include/fspr_want.h include/fspr_file_info.h include/fspr_user.h include/fspr_file_io.h include/fspr_inherit.h include/fspr_thread_mutex.h include/fspr_lib.h include/fspr_time.h include/fspr_pools.h include/fspr_tables.h
network_io/os2/os2calls.lo: network_io/os2/os2calls.c .make.dirs include/fspr_allocator.h include/fspr_inherit.h include/fspr_file_info.h include/fspr_thread_mutex.h include/fspr_thread_proc.h include/fspr_tables.h include/fspr_errno.h include/fspr_want.h include/fspr_user.h include/fspr_file_io.h include/fspr_shm.h include/fspr_general.h include/fspr_network_io.h include/fspr_portable.h include/fspr_dso.h include/fspr_proc_mutex.h include/fspr_lib.h include/fspr_global_mutex.h include/fspr_time.h include/fspr_pools.h
network_io/os2/sockets.lo: network_io/os2/sockets.c .make.dirs include/fspr_allocator.h include/fspr_general.h include/fspr_network_io.h include/fspr_portable.h include/fspr_inherit.h include/fspr_file_info.h include/fspr_thread_mutex.h include/fspr_thread_proc.h include/fspr_dso.h include/fspr_tables.h include/fspr_errno.h include/fspr_want.h include/fspr_user.h include/fspr_file_io.h include/fspr_strings.h include/fspr_proc_mutex.h include/fspr_lib.h include/fspr_shm.h include/fspr_global_mutex.h include/fspr_time.h include/fspr_pools.h
network_io/os2/inet_ntop.lo: network_io/os2/inet_ntop.c .make.dirs 
network_io/os2/inet_pton.lo: network_io/os2/inet_pton.c .make.dirs 

OBJECTS_network_io_os2 = network_io/os2/sockaddr.lo network_io/os2/sockopt.lo network_io/os2/sendrecv_udp.lo network_io/os2/sendrecv.lo network_io/os2/os2calls.lo network_io/os2/sockets.lo network_io/os2/inet_ntop.lo network_io/os2/inet_pton.lo

poll/os2/pollset.lo: poll/os2/pollset.c .make.dirs include/fspr_allocator.h include/fspr_general.h include/fspr_network_io.h include/fspr_errno.h include/fspr_inherit.h include/fspr_file_info.h include/fspr_user.h include/fspr_file_io.h include/fspr_want.h include/fspr_thread_mutex.h include/fspr_poll.h include/fspr_time.h include/fspr_pools.h include/fspr_tables.h
poll/os2/poll.lo: poll/os2/poll.c .make.dirs include/fspr_allocator.h include/fspr_general.h include/fspr_network_io.h include/fspr_errno.h include/fspr_inherit.h include/fspr_file_info.h include/fspr_user.h include/fspr_file_io.h include/fspr_want.h include/fspr_thread_mutex.h include/fspr_poll.h include/fspr_time.h include/fspr_pools.h include/fspr_tables.h

OBJECTS_poll_os2 = poll/os2/pollset.lo poll/os2/poll.lo

shmem/os2/shm.lo: shmem/os2/shm.c .make.dirs include/fspr_allocator.h include/fspr_general.h include/fspr_network_io.h include/fspr_portable.h include/fspr_inherit.h include/fspr_file_info.h include/fspr_thread_mutex.h include/fspr_thread_proc.h include/fspr_dso.h include/fspr_tables.h include/fspr_errno.h include/fspr_want.h include/fspr_user.h include/fspr_file_io.h include/fspr_strings.h include/fspr_proc_mutex.h include/fspr_lib.h include/fspr_shm.h include/fspr_global_mutex.h include/fspr_time.h include/fspr_pools.h

OBJECTS_shmem_os2 = shmem/os2/shm.lo

threadproc/os2/thread.lo: threadproc/os2/thread.c .make.dirs include/fspr_allocator.h include/fspr_inherit.h include/fspr_file_info.h include/fspr_thread_mutex.h include/fspr_thread_proc.h include/fspr_tables.h include/fspr_errno.h include/fspr_want.h include/fspr_user.h include/fspr_file_io.h include/fspr_shm.h include/fspr_general.h include/fspr_network_io.h include/fspr_portable.h include/fspr_dso.h include/fspr_proc_mutex.h include/fspr_lib.h include/fspr_global_mutex.h include/fspr_time.h include/fspr_pools.h
threadproc/os2/signals.lo: threadproc/os2/signals.c .make.dirs 
threadproc/os2/proc.lo: threadproc/os2/proc.c .make.dirs include/fspr_allocator.h include/fspr_general.h include/fspr_network_io.h include/fspr_portable.h include/fspr_signal.h include/fspr_file_info.h include/fspr_thread_mutex.h include/fspr_thread_proc.h include/fspr_dso.h include/fspr_proc_mutex.h include/fspr_tables.h include/fspr_errno.h include/fspr_want.h include/fspr_user.h include/fspr_file_io.h include/fspr_strings.h include/fspr_inherit.h include/fspr_lib.h include/fspr_shm.h include/fspr_global_mutex.h include/fspr_time.h include/fspr_pools.h
threadproc/os2/threadpriv.lo: threadproc/os2/threadpriv.c .make.dirs include/fspr_allocator.h include/fspr_inherit.h include/fspr_file_info.h include/fspr_thread_mutex.h include/fspr_thread_proc.h include/fspr_tables.h include/fspr_errno.h include/fspr_want.h include/fspr_user.h include/fspr_file_io.h include/fspr_shm.h include/fspr_general.h include/fspr_network_io.h include/fspr_portable.h include/fspr_dso.h include/fspr_proc_mutex.h include/fspr_lib.h include/fspr_global_mutex.h include/fspr_time.h include/fspr_pools.h

OBJECTS_threadproc_os2 = threadproc/os2/thread.lo threadproc/os2/signals.lo threadproc/os2/proc.lo threadproc/os2/threadpriv.lo

OBJECTS_os2 = $(OBJECTS_all) $(OBJECTS_atomic_unix) $(OBJECTS_dso_os2) $(OBJECTS_file_io_os2) $(OBJECTS_locks_os2) $(OBJECTS_memory_unix) $(OBJECTS_misc_unix) $(OBJECTS_mmap_unix) $(OBJECTS_network_io_os2) $(OBJECTS_poll_os2) $(OBJECTS_random_unix) $(OBJECTS_shmem_os2) $(OBJECTS_support_unix) $(OBJECTS_threadproc_os2) $(OBJECTS_time_unix) $(OBJECTS_user_unix)

atomic/os390/atomic.lo: atomic/os390/atomic.c .make.dirs include/fspr_allocator.h include/fspr_general.h include/fspr_thread_mutex.h include/fspr_errno.h include/fspr_want.h include/fspr_atomic.h include/fspr_pools.h

OBJECTS_atomic_os390 = atomic/os390/atomic.lo

dso/os390/dso.lo: dso/os390/dso.c .make.dirs include/fspr_allocator.h include/fspr_general.h include/fspr_network_io.h include/fspr_portable.h include/fspr_inherit.h include/fspr_file_info.h include/fspr_thread_mutex.h include/fspr_thread_proc.h include/fspr_dso.h include/fspr_tables.h include/fspr_errno.h include/fspr_want.h include/fspr_user.h include/fspr_file_io.h include/fspr_strings.h include/fspr_proc_mutex.h include/fspr_shm.h include/fspr_global_mutex.h include/fspr_time.h include/fspr_pools.h

OBJECTS_dso_os390 = dso/os390/dso.lo

OBJECTS_os390 = $(OBJECTS_all) $(OBJECTS_atomic_os390) $(OBJECTS_dso_os390) $(OBJECTS_file_io_unix) $(OBJECTS_locks_unix) $(OBJECTS_memory_unix) $(OBJECTS_misc_unix) $(OBJECTS_mmap_unix) $(OBJECTS_network_io_unix) $(OBJECTS_poll_unix) $(OBJECTS_random_unix) $(OBJECTS_shmem_unix) $(OBJECTS_support_unix) $(OBJECTS_threadproc_unix) $(OBJECTS_time_unix) $(OBJECTS_user_unix)

HEADERS = $(top_srcdir)/include/fspr_thread_mutex.h $(top_srcdir)/include/fspr_env.h $(top_srcdir)/include/fspr_poll.h $(top_srcdir)/include/fspr_fnmatch.h $(top_srcdir)/include/fspr_global_mutex.h $(top_srcdir)/include/fspr_pools.h $(top_srcdir)/include/fspr_want.h $(top_srcdir)/include/fspr_file_io.h $(top_srcdir)/include/fspr_version.h $(top_srcdir)/include/fspr_mmap.h $(top_srcdir)/include/fspr_dso.h $(top_srcdir)/include/fspr_thread_proc.h $(top_srcdir)/include/fspr_errno.h $(top_srcdir)/include/fspr_shm.h $(top_srcdir)/include/fspr_network_io.h $(top_srcdir)/include/fspr_signal.h $(top_srcdir)/include/fspr_user.h $(top_srcdir)/include/fspr_support.h $(top_srcdir)/include/fspr_atomic.h $(top_srcdir)/include/fspr_random.h $(top_srcdir)/include/fspr_thread_cond.h $(top_srcdir)/include/fspr_thread_rwlock.h $(top_srcdir)/include/fspr_getopt.h $(top_srcdir)/include/fspr_inherit.h $(top_srcdir)/include/fspr_strings.h $(top_srcdir)/include/fspr_general.h $(top_srcdir)/include/fspr_proc_mutex.h $(top_srcdir)/include/fspr_tables.h $(top_srcdir)/include/fspr_ring.h $(top_srcdir)/include/fspr_file_info.h $(top_srcdir)/include/fspr_allocator.h $(top_srcdir)/include/fspr_portable.h $(top_srcdir)/include/fspr_hash.h $(top_srcdir)/include/fspr_time.h $(top_srcdir)/include/fspr_lib.h

SOURCE_DIRS = random/unix dso/os2 time/unix locks/unix user/unix locks/beos tables support/unix file_io/unix mmap/unix atomic/unix poll/os2 dso/os390 atomic/os390 dso/beos poll/unix passwd network_io/beos threadproc/os2 network_io/os2 threadproc/beos shmem/unix network_io/unix file_io/os2 dso/aix threadproc/unix misc/unix shmem/beos dso/unix locks/os2 shmem/os2 memory/unix strings $(EXTRA_SOURCE_DIRS)

BUILD_DIRS = atomic atomic/os390 atomic/unix dso dso/aix dso/beos dso/os2 dso/os390 dso/unix file_io file_io/os2 file_io/unix locks locks/beos locks/os2 locks/unix memory memory/unix misc misc/unix mmap mmap/unix network_io network_io/beos network_io/os2 network_io/unix passwd poll poll/os2 poll/unix random random/unix shmem shmem/beos shmem/os2 shmem/unix strings support support/unix tables threadproc threadproc/beos threadproc/os2 threadproc/unix time time/unix user user/unix

.make.dirs: $(srcdir)/build-outputs.mk
	@for d in $(BUILD_DIRS); do test -d $$d || mkdir $$d; done
	@echo timestamp > $@
