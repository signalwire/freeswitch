/* Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include "fspr.h"
#include "fspr_general.h"
#include "fspr_proc_mutex.h"
#include "fspr_global_mutex.h"
#include "fspr_thread_proc.h"

#if !APR_HAS_THREADS
int main(void)
{
    printf("This test requires APR thread support.\n");
    return 0;
}

#else /* APR_HAS_THREADS */

static fspr_thread_mutex_t *thread_mutex;
static fspr_proc_mutex_t *proc_mutex;
static fspr_global_mutex_t *global_mutex;
static fspr_pool_t *p;
static volatile int counter;
typedef enum {TEST_GLOBAL, TEST_PROC} test_mode_e;

static void lock_init(fspr_lockmech_e mech, test_mode_e test_mode)
{
    if (test_mode == TEST_PROC) {
        assert(fspr_proc_mutex_create(&proc_mutex,
                                     NULL,
                                     mech,
                                     p) == APR_SUCCESS);
    }
    else {
        assert(fspr_global_mutex_create(&global_mutex,
                                       NULL,
                                       mech,
                                       p) == APR_SUCCESS);
    }
}

static void lock_destroy(test_mode_e test_mode)
{
    if (test_mode == TEST_PROC) {
        assert(fspr_proc_mutex_destroy(proc_mutex) == APR_SUCCESS);
    }
    else {
        assert(fspr_global_mutex_destroy(global_mutex) == APR_SUCCESS);
    }
}

static void lock_grab(test_mode_e test_mode)
{
    if (test_mode == TEST_PROC) {
        assert(fspr_proc_mutex_lock(proc_mutex) == APR_SUCCESS);
    }
    else {
        assert(fspr_global_mutex_lock(global_mutex) == APR_SUCCESS);
    }
}

static void lock_release(test_mode_e test_mode)
{
    if (test_mode == TEST_PROC) {
        assert(fspr_proc_mutex_unlock(proc_mutex) == APR_SUCCESS);
    }
    else {
        assert(fspr_global_mutex_unlock(global_mutex) == APR_SUCCESS);
    }
}

static void * APR_THREAD_FUNC eachThread(fspr_thread_t *id, void *p)
{
    test_mode_e test_mode = (test_mode_e)p;

    lock_grab(test_mode);
    ++counter;
    assert(fspr_thread_mutex_lock(thread_mutex) == APR_SUCCESS);
    assert(fspr_thread_mutex_unlock(thread_mutex) == APR_SUCCESS);
    lock_release(test_mode);
    return NULL;
}

static void test_mech_mode(fspr_lockmech_e mech, const char *mech_name,
                           test_mode_e test_mode)
{
  fspr_thread_t *threads[20];
  int numThreads = 5;
  int i;
  fspr_status_t rv;

  printf("Trying %s mutexes with mechanism `%s'...\n",
         test_mode == TEST_GLOBAL ? "global" : "proc", mech_name);

  assert(numThreads <= sizeof(threads) / sizeof(threads[0]));

  assert(fspr_pool_create(&p, NULL) == APR_SUCCESS);

  assert(fspr_thread_mutex_create(&thread_mutex, 0, p) == APR_SUCCESS);
  assert(fspr_thread_mutex_lock(thread_mutex) == APR_SUCCESS);
  
  lock_init(mech, test_mode);

  counter = 0;

  i = 0;
  while (i < numThreads)
  {
    rv = fspr_thread_create(&threads[i],
                           NULL,
                           eachThread,
                           (void *)test_mode,
                           p);
    if (rv != APR_SUCCESS) {
      fprintf(stderr, "fspr_thread_create->%d\n", rv);
      exit(1);
    }
    ++i;
  }

  fspr_sleep(fspr_time_from_sec(5));

  if (test_mode == TEST_PROC) {
      printf("  Mutex mechanism `%s' is %sglobal in scope on this platform.\n",
             mech_name, counter == 1 ? "" : "not ");
  }
  else {
      if (counter != 1) {
          fprintf(stderr, "\n!!!fspr_global_mutex operations are broken on this "
                  "platform for mutex mechanism `%s'!\n"
                  "They don't block out threads within the same process.\n",
                  mech_name);
          fprintf(stderr, "counter value: %d\n", counter);
          exit(1);
      }
      else {
          printf("  no problems encountered...\n");
      }
  }
  
  assert(fspr_thread_mutex_unlock(thread_mutex) == APR_SUCCESS);

  i = 0;
  while (i < numThreads)
  {
    fspr_status_t ignored;

    rv = fspr_thread_join(&ignored,
                         threads[i]);
    assert(rv == APR_SUCCESS);
    ++i;
  }

  lock_destroy(test_mode);
  fspr_thread_mutex_destroy(thread_mutex);
  fspr_pool_destroy(p);
}

static void test_mech(fspr_lockmech_e mech, const char *mech_name)
{
    test_mech_mode(mech, mech_name, TEST_PROC);
    test_mech_mode(mech, mech_name, TEST_GLOBAL);
}

int main(void)
{
    struct {
        fspr_lockmech_e mech;
        const char *mech_name;
    } lockmechs[] = {
        {APR_LOCK_DEFAULT, "default"}
#if APR_HAS_FLOCK_SERIALIZE
        ,{APR_LOCK_FLOCK, "flock"}
#endif
#if APR_HAS_SYSVSEM_SERIALIZE
        ,{APR_LOCK_SYSVSEM, "sysvsem"}
#endif
#if APR_HAS_POSIXSEM_SERIALIZE
        ,{APR_LOCK_POSIXSEM, "posix"}
#endif
#if APR_HAS_FCNTL_SERIALIZE
        ,{APR_LOCK_FCNTL, "fcntl"}
#endif
#if APR_HAS_PROC_PTHREAD_SERIALIZE
        ,{APR_LOCK_PROC_PTHREAD, "proc_pthread"}
#endif
    };
    int i;
        
    assert(fspr_initialize() == APR_SUCCESS);

    for (i = 0; i < sizeof(lockmechs) / sizeof(lockmechs[0]); i++) {
        test_mech(lockmechs[i].mech, lockmechs[i].mech_name);
    }
    
    fspr_terminate();
    return 0;
}

#endif /* APR_HAS_THREADS */
