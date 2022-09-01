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

#include "fspr_arch_shm.h"

#include "fspr_general.h"
#include "fspr_errno.h"
#include "fspr_user.h"
#include "fspr_strings.h"

static fspr_status_t shm_cleanup_owner(void *m_)
{
    fspr_shm_t *m = (fspr_shm_t *)m_;

    /* anonymous shared memory */
    if (m->filename == NULL) {
#if APR_USE_SHMEM_MMAP_ZERO || APR_USE_SHMEM_MMAP_ANON
        if (munmap(m->base, m->realsize) == -1) {
            return errno;
        }
        return APR_SUCCESS;
#endif
#if APR_USE_SHMEM_SHMGET_ANON
        if (shmdt(m->base) == -1) {
            return errno;
        }
        /* This segment will automatically remove itself after all
         * references have detached. */
        return APR_SUCCESS;
#endif
    }

    /* name-based shared memory */
    else {
#if APR_USE_SHMEM_MMAP_TMP
        if (munmap(m->base, m->realsize) == -1) {
            return errno;
        }
        return fspr_file_remove(m->filename, m->pool);
#endif
#if APR_USE_SHMEM_MMAP_SHM
        if (munmap(m->base, m->realsize) == -1) {
            return errno;
        }
        if (shm_unlink(m->filename) == -1) {
            return errno;
        }
        return APR_SUCCESS;
#endif
#if APR_USE_SHMEM_SHMGET
        /* Indicate that the segment is to be destroyed as soon
         * as all processes have detached. This also disallows any
         * new attachments to the segment. */
        if (shmctl(m->shmid, IPC_RMID, NULL) == -1) {
            return errno;
        }
        if (shmdt(m->base) == -1) {
            return errno;
        }
        return fspr_file_remove(m->filename, m->pool);
#endif
    }

    return APR_ENOTIMPL;
}

APR_DECLARE(fspr_status_t) fspr_shm_create(fspr_shm_t **m,
                                         fspr_size_t reqsize, 
                                         const char *filename,
                                         fspr_pool_t *pool)
{
    fspr_shm_t *new_m;
    fspr_status_t status;
#if APR_USE_SHMEM_SHMGET || APR_USE_SHMEM_SHMGET_ANON
    struct shmid_ds shmbuf;
    fspr_uid_t uid;
    fspr_gid_t gid;
#endif
#if APR_USE_SHMEM_MMAP_TMP || APR_USE_SHMEM_MMAP_SHM || \
    APR_USE_SHMEM_MMAP_ZERO
    int tmpfd;
#endif
#if APR_USE_SHMEM_SHMGET
    fspr_size_t nbytes;
    key_t shmkey;
#endif
#if APR_USE_SHMEM_MMAP_ZERO || APR_USE_SHMEM_SHMGET || \
    APR_USE_SHMEM_MMAP_TMP || APR_USE_SHMEM_MMAP_SHM
    fspr_file_t *file;   /* file where metadata is stored */
#endif

    /* Check if they want anonymous or name-based shared memory */
    if (filename == NULL) {
#if APR_USE_SHMEM_MMAP_ZERO || APR_USE_SHMEM_MMAP_ANON
        new_m = fspr_palloc(pool, sizeof(fspr_shm_t));
        new_m->pool = pool;
        new_m->reqsize = reqsize;
        new_m->realsize = reqsize + 
            APR_ALIGN_DEFAULT(sizeof(fspr_size_t)); /* room for metadata */
        new_m->filename = NULL;
    
#if APR_USE_SHMEM_MMAP_ZERO
        status = fspr_file_open(&file, "/dev/zero", APR_READ | APR_WRITE, 
                               APR_OS_DEFAULT, pool);
        if (status != APR_SUCCESS) {
            return status;
        }
        status = fspr_os_file_get(&tmpfd, file);
        if (status != APR_SUCCESS) {
            return status;
        }

        new_m->base = mmap(NULL, new_m->realsize, PROT_READ|PROT_WRITE,
                           MAP_SHARED, tmpfd, 0);
        if (new_m->base == (void *)MAP_FAILED) {
            return errno;
        }

        status = fspr_file_close(file);
        if (status != APR_SUCCESS) {
            return status;
        }

        /* store the real size in the metadata */
        *(fspr_size_t*)(new_m->base) = new_m->realsize;
        /* metadata isn't usable */
        new_m->usable = (char *)new_m->base + APR_ALIGN_DEFAULT(sizeof(fspr_size_t));

        fspr_pool_cleanup_register(new_m->pool, new_m, shm_cleanup_owner,
                                  fspr_pool_cleanup_null);
        *m = new_m;
        return APR_SUCCESS;

#elif APR_USE_SHMEM_MMAP_ANON
        new_m->base = mmap(NULL, new_m->realsize, PROT_READ|PROT_WRITE,
                           MAP_ANON|MAP_SHARED, -1, 0);
        if (new_m->base == (void *)MAP_FAILED) {
            return errno;
        }

        /* store the real size in the metadata */
        *(fspr_size_t*)(new_m->base) = new_m->realsize;
        /* metadata isn't usable */
        new_m->usable = (char *)new_m->base + APR_ALIGN_DEFAULT(sizeof(fspr_size_t));

        fspr_pool_cleanup_register(new_m->pool, new_m, shm_cleanup_owner,
                                  fspr_pool_cleanup_null);
        *m = new_m;
        return APR_SUCCESS;

#endif /* APR_USE_SHMEM_MMAP_ZERO */
#endif /* APR_USE_SHMEM_MMAP_ZERO || APR_USE_SHMEM_MMAP_ANON */
#if APR_USE_SHMEM_SHMGET_ANON

        new_m = fspr_palloc(pool, sizeof(fspr_shm_t));
        new_m->pool = pool;
        new_m->reqsize = reqsize;
        new_m->realsize = reqsize;
        new_m->filename = NULL;

        if ((new_m->shmid = shmget(IPC_PRIVATE, new_m->realsize,
                                   SHM_R | SHM_W | IPC_CREAT)) < 0) {
            return errno;
        }

        if ((new_m->base = shmat(new_m->shmid, NULL, 0)) == (void *)-1) {
            return errno;
        }
        new_m->usable = new_m->base;

        if (shmctl(new_m->shmid, IPC_STAT, &shmbuf) == -1) {
            return errno;
        }
        fspr_uid_current(&uid, &gid, pool);
        shmbuf.shm_perm.uid = uid;
        shmbuf.shm_perm.gid = gid;
        if (shmctl(new_m->shmid, IPC_SET, &shmbuf) == -1) {
            return errno;
        }

        /* Remove the segment once use count hits zero.
         * We will not attach to this segment again, since it is
         * anonymous memory, so it is ok to mark it for deletion.
         */
        if (shmctl(new_m->shmid, IPC_RMID, NULL) == -1) {
            return errno;
        }

        fspr_pool_cleanup_register(new_m->pool, new_m, shm_cleanup_owner,
                                  fspr_pool_cleanup_null);
        *m = new_m;
        return APR_SUCCESS;
#endif /* APR_USE_SHMEM_SHMGET_ANON */
        /* It is an error if they want anonymous memory but we don't have it. */
        return APR_ENOTIMPL; /* requested anonymous but we don't have it */
    }

    /* Name-based shared memory */
    else {
        new_m = fspr_palloc(pool, sizeof(fspr_shm_t));
        new_m->pool = pool;
        new_m->reqsize = reqsize;
        new_m->filename = fspr_pstrdup(pool, filename);

#if APR_USE_SHMEM_MMAP_TMP || APR_USE_SHMEM_MMAP_SHM
        new_m->realsize = reqsize + 
            APR_ALIGN_DEFAULT(sizeof(fspr_size_t)); /* room for metadata */
        /* FIXME: Ignore error for now. *
         * status = fspr_file_remove(file, pool);*/
        status = APR_SUCCESS;
    
#if APR_USE_SHMEM_MMAP_TMP
        /* FIXME: Is APR_OS_DEFAULT sufficient? */
        status = fspr_file_open(&file, filename, 
                               APR_READ | APR_WRITE | APR_CREATE | APR_EXCL,
                               APR_OS_DEFAULT, pool);
        if (status != APR_SUCCESS) {
            return status;
        }

        status = fspr_os_file_get(&tmpfd, file);
        if (status != APR_SUCCESS) {
            fspr_file_close(file); /* ignore errors, we're failing */
            fspr_file_remove(new_m->filename, new_m->pool);
            return status;
        }

        status = fspr_file_trunc(file, new_m->realsize);
        if (status != APR_SUCCESS) {
            fspr_file_close(file); /* ignore errors, we're failing */
            fspr_file_remove(new_m->filename, new_m->pool);
            return status;
        }

        new_m->base = mmap(NULL, new_m->realsize, PROT_READ | PROT_WRITE,
                           MAP_SHARED, tmpfd, 0);
        /* FIXME: check for errors */

        status = fspr_file_close(file);
        if (status != APR_SUCCESS) {
            return status;
        }
#endif /* APR_USE_SHMEM_MMAP_TMP */
#if APR_USE_SHMEM_MMAP_SHM
        tmpfd = shm_open(filename, O_RDWR | O_CREAT | O_EXCL, 0644);
        if (tmpfd == -1) {
            return errno;
        }

        status = fspr_os_file_put(&file, &tmpfd,
                                 APR_READ | APR_WRITE | APR_CREATE | APR_EXCL,
                                 pool); 
        if (status != APR_SUCCESS) {
            return status;
        }

        status = fspr_file_trunc(file, new_m->realsize);
        if (status != APR_SUCCESS) {
            shm_unlink(filename); /* we're failing, remove the object */
            return status;
        }
        new_m->base = mmap(NULL, reqsize, PROT_READ | PROT_WRITE,
                           MAP_SHARED, tmpfd, 0);

        /* FIXME: check for errors */

        status = fspr_file_close(file);
        if (status != APR_SUCCESS) {
            return status;
        }
#endif /* APR_USE_SHMEM_MMAP_SHM */

        /* store the real size in the metadata */
        *(fspr_size_t*)(new_m->base) = new_m->realsize;
        /* metadata isn't usable */
        new_m->usable = (char *)new_m->base + APR_ALIGN_DEFAULT(sizeof(fspr_size_t));

        fspr_pool_cleanup_register(new_m->pool, new_m, shm_cleanup_owner,
                                  fspr_pool_cleanup_null);
        *m = new_m;
        return APR_SUCCESS;

#endif /* APR_USE_SHMEM_MMAP_TMP || APR_USE_SHMEM_MMAP_SHM */

#if APR_USE_SHMEM_SHMGET
        new_m->realsize = reqsize;

        /* FIXME: APR_OS_DEFAULT is too permissive, switch to 600 I think. */
        status = fspr_file_open(&file, filename, 
                               APR_WRITE | APR_CREATE | APR_EXCL,
                               APR_OS_DEFAULT, pool);
        if (status != APR_SUCCESS) {
            return status;
        }

        /* ftok() (on solaris at least) requires that the file actually
         * exist before calling ftok(). */
        shmkey = ftok(filename, 1);
        if (shmkey == (key_t)-1) {
            return errno;
        }

        if ((new_m->shmid = shmget(shmkey, new_m->realsize,
                                   SHM_R | SHM_W | IPC_CREAT | IPC_EXCL)) < 0) {
            return errno;
        }

        if ((new_m->base = shmat(new_m->shmid, NULL, 0)) == (void *)-1) {
            return errno;
        }
        new_m->usable = new_m->base;

        if (shmctl(new_m->shmid, IPC_STAT, &shmbuf) == -1) {
            return errno;
        }
        fspr_uid_current(&uid, &gid, pool);
        shmbuf.shm_perm.uid = uid;
        shmbuf.shm_perm.gid = gid;
        if (shmctl(new_m->shmid, IPC_SET, &shmbuf) == -1) {
            return errno;
        }

        nbytes = sizeof(reqsize);
        status = fspr_file_write(file, (const void *)&reqsize,
                                &nbytes);
        if (status != APR_SUCCESS) {
            return status;
        }
        status = fspr_file_close(file);
        if (status != APR_SUCCESS) {
            return status;
        }

        fspr_pool_cleanup_register(new_m->pool, new_m, shm_cleanup_owner,
                                  fspr_pool_cleanup_null);
        *m = new_m; 
        return APR_SUCCESS;

#endif /* APR_USE_SHMEM_SHMGET */
    }

    return APR_ENOTIMPL;
}

APR_DECLARE(fspr_status_t) fspr_shm_remove(const char *filename,
                                         fspr_pool_t *pool)
{
#if APR_USE_SHMEM_SHMGET
    fspr_status_t status;
    fspr_file_t *file;  
    key_t shmkey;
    int shmid;
#endif

#if APR_USE_SHMEM_MMAP_TMP
    return fspr_file_remove(filename, pool);
#endif
#if APR_USE_SHMEM_MMAP_SHM
    if (shm_unlink(filename) == -1) {
        return errno;
    }
    return APR_SUCCESS;
#endif
#if APR_USE_SHMEM_SHMGET
    /* Presume that the file already exists; just open for writing */    
    status = fspr_file_open(&file, filename, APR_WRITE,
                           APR_OS_DEFAULT, pool);
    if (status) {
        return status;
    }

    /* ftok() (on solaris at least) requires that the file actually
     * exist before calling ftok(). */
    shmkey = ftok(filename, 1);
    if (shmkey == (key_t)-1) {
        goto shm_remove_failed;
    }

    fspr_file_close(file);

    if ((shmid = shmget(shmkey, 0, SHM_R | SHM_W)) < 0) {
        goto shm_remove_failed;
    }

    /* Indicate that the segment is to be destroyed as soon
     * as all processes have detached. This also disallows any
     * new attachments to the segment. */
    if (shmctl(shmid, IPC_RMID, NULL) == -1) {
        goto shm_remove_failed;
    }
    return fspr_file_remove(filename, pool);

shm_remove_failed:
    status = errno;
    /* ensure the file has been removed anyway. */
    fspr_file_remove(filename, pool);
    return status;
#endif

    /* No support for anonymous shm */
    return APR_ENOTIMPL;
} 

APR_DECLARE(fspr_status_t) fspr_shm_destroy(fspr_shm_t *m)
{
    return fspr_pool_cleanup_run(m->pool, m, shm_cleanup_owner);
}

static fspr_status_t shm_cleanup_attach(void *m_)
{
    fspr_shm_t *m = (fspr_shm_t *)m_;

    if (m->filename == NULL) {
        /* It doesn't make sense to detach from an anonymous memory segment. */
        return APR_EINVAL;
    }
    else {
#if APR_USE_SHMEM_MMAP_TMP || APR_USE_SHMEM_MMAP_SHM
        if (munmap(m->base, m->realsize) == -1) {
            return errno;
        }
        return APR_SUCCESS;
#endif /* APR_USE_SHMEM_MMAP_TMP || APR_USE_SHMEM_MMAP_SHM */
#if APR_USE_SHMEM_SHMGET
        if (shmdt(m->base) == -1) {
            return errno;
        }
        return APR_SUCCESS;
#endif
    }

    return APR_ENOTIMPL;
}

APR_DECLARE(fspr_status_t) fspr_shm_attach(fspr_shm_t **m,
                                         const char *filename,
                                         fspr_pool_t *pool)
{
    if (filename == NULL) {
        /* It doesn't make sense to attach to a segment if you don't know
         * the filename. */
        return APR_EINVAL;
    }
    else {
#if APR_USE_SHMEM_MMAP_TMP || APR_USE_SHMEM_MMAP_SHM
        fspr_shm_t *new_m;
        fspr_status_t status;
        int tmpfd;
        fspr_file_t *file;   /* file where metadata is stored */
        fspr_size_t nbytes;

        new_m = fspr_palloc(pool, sizeof(fspr_shm_t));
        new_m->pool = pool;
        new_m->filename = fspr_pstrdup(pool, filename);

        status = fspr_file_open(&file, filename, 
                               APR_READ | APR_WRITE,
                               APR_OS_DEFAULT, pool);
        if (status != APR_SUCCESS) {
            return status;
        }
        status = fspr_os_file_get(&tmpfd, file);
        if (status != APR_SUCCESS) {
            return status;
        }

        nbytes = sizeof(new_m->realsize);
        status = fspr_file_read(file, (void *)&(new_m->realsize),
                               &nbytes);
        if (status != APR_SUCCESS) {
            return status;
        }

        status = fspr_os_file_get(&tmpfd, file);
        if (status != APR_SUCCESS) {
            fspr_file_close(file); /* ignore errors, we're failing */
            fspr_file_remove(new_m->filename, new_m->pool);
            return status;
        }

        new_m->reqsize = new_m->realsize - sizeof(fspr_size_t);

        new_m->base = mmap(NULL, new_m->realsize, PROT_READ | PROT_WRITE,
                           MAP_SHARED, tmpfd, 0);
        /* FIXME: check for errors */
        
        status = fspr_file_close(file);
        if (status != APR_SUCCESS) {
            return status;
        }

        /* metadata isn't part of the usable segment */
        new_m->usable = (char *)new_m->base + APR_ALIGN_DEFAULT(sizeof(fspr_size_t));

        fspr_pool_cleanup_register(new_m->pool, new_m, shm_cleanup_attach,
                                  fspr_pool_cleanup_null);
        *m = new_m;
        return APR_SUCCESS;

#endif /* APR_USE_SHMEM_MMAP_TMP || APR_USE_SHMEM_MMAP_SHM */
#if APR_USE_SHMEM_SHMGET
        fspr_shm_t *new_m;
        fspr_status_t status;
        fspr_file_t *file;   /* file where metadata is stored */
        fspr_size_t nbytes;
        key_t shmkey;

        new_m = fspr_palloc(pool, sizeof(fspr_shm_t));

        status = fspr_file_open(&file, filename, 
                               APR_READ, APR_OS_DEFAULT, pool);
        if (status != APR_SUCCESS) {
            return status;
        }

        nbytes = sizeof(new_m->reqsize);
        status = fspr_file_read(file, (void *)&(new_m->reqsize),
                               &nbytes);
        if (status != APR_SUCCESS) {
            return status;
        }
        status = fspr_file_close(file);
        if (status != APR_SUCCESS) {
            return status;
        }

        new_m->filename = fspr_pstrdup(pool, filename);
        new_m->pool = pool;
        shmkey = ftok(filename, 1);
        if (shmkey == (key_t)-1) {
            return errno;
        }
        if ((new_m->shmid = shmget(shmkey, 0, SHM_R | SHM_W)) == -1) {
            return errno;
        }
        if ((new_m->base = shmat(new_m->shmid, NULL, 0)) == (void *)-1) {
            return errno;
        }
        new_m->usable = new_m->base;
        new_m->realsize = new_m->reqsize;

        fspr_pool_cleanup_register(new_m->pool, new_m, shm_cleanup_attach,
                                  fspr_pool_cleanup_null);
        *m = new_m;
        return APR_SUCCESS;

#endif /* APR_USE_SHMEM_SHMGET */
    }

    return APR_ENOTIMPL;
}

APR_DECLARE(fspr_status_t) fspr_shm_detach(fspr_shm_t *m)
{
    fspr_status_t rv = shm_cleanup_attach(m);
    fspr_pool_cleanup_kill(m->pool, m, shm_cleanup_attach);
    return rv;
}

APR_DECLARE(void *) fspr_shm_baseaddr_get(const fspr_shm_t *m)
{
    return m->usable;
}

APR_DECLARE(fspr_size_t) fspr_shm_size_get(const fspr_shm_t *m)
{
    return m->reqsize;
}

APR_POOL_IMPLEMENT_ACCESSOR(shm)

APR_DECLARE(fspr_status_t) fspr_os_shm_get(fspr_os_shm_t *osshm,
                                         fspr_shm_t *shm)
{
    return APR_ENOTIMPL;
}

APR_DECLARE(fspr_status_t) fspr_os_shm_put(fspr_shm_t **m,
                                         fspr_os_shm_t *osshm,
                                         fspr_pool_t *pool)
{
    return APR_ENOTIMPL;
}    

