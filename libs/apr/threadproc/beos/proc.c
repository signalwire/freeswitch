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

#include "fspr_arch_threadproc.h"
#include "fspr_strings.h"

struct send_pipe {
	int in;
	int out;
	int err;
};

APR_DECLARE(fspr_status_t) fspr_procattr_create(fspr_procattr_t **new, fspr_pool_t *pool)
{
    (*new) = (fspr_procattr_t *)fspr_palloc(pool, 
              sizeof(fspr_procattr_t));

    if ((*new) == NULL) {
        return APR_ENOMEM;
    }
    (*new)->pool = pool;
    (*new)->parent_in = NULL;
    (*new)->child_in = NULL;
    (*new)->parent_out = NULL;
    (*new)->child_out = NULL;
    (*new)->parent_err = NULL;
    (*new)->child_err = NULL;
    (*new)->currdir = NULL; 
    (*new)->cmdtype = APR_PROGRAM;
    (*new)->detached = 0;
    return APR_SUCCESS;
}

APR_DECLARE(fspr_status_t) fspr_procattr_io_set(fspr_procattr_t *attr, fspr_int32_t in, 
                                              fspr_int32_t out, fspr_int32_t err)
{
    fspr_status_t status;
    if (in != 0) {
        if ((status = fspr_file_pipe_create(&attr->child_in, &attr->parent_in, 
                                   attr->pool)) != APR_SUCCESS) {
            return status;
        }
        switch (in) {
        case APR_FULL_BLOCK:
            fspr_file_pipe_timeout_set(attr->child_in, -1);
            fspr_file_pipe_timeout_set(attr->parent_in, -1);
            break;
        case APR_PARENT_BLOCK:
            fspr_file_pipe_timeout_set(attr->child_in, -1);
            break;
        case APR_CHILD_BLOCK:
            fspr_file_pipe_timeout_set(attr->parent_in, -1);
            break;
        default:
            break;
        }
    } 
    if (out) {
        if ((status = fspr_file_pipe_create(&attr->parent_out, &attr->child_out, 
                                   attr->pool)) != APR_SUCCESS) {
            return status;
        }
        switch (out) {
        case APR_FULL_BLOCK:
            fspr_file_pipe_timeout_set(attr->child_out, -1);
            fspr_file_pipe_timeout_set(attr->parent_out, -1);       
            break;
        case APR_PARENT_BLOCK:
            fspr_file_pipe_timeout_set(attr->child_out, -1);
            break;
        case APR_CHILD_BLOCK:
            fspr_file_pipe_timeout_set(attr->parent_out, -1);
            break;
        default:
            break;
        }
    } 
    if (err) {
        if ((status = fspr_file_pipe_create(&attr->parent_err, &attr->child_err, 
                                   attr->pool)) != APR_SUCCESS) {
            return status;
        }
        switch (err) {
        case APR_FULL_BLOCK:
            fspr_file_pipe_timeout_set(attr->child_err, -1);
            fspr_file_pipe_timeout_set(attr->parent_err, -1);
            break;
        case APR_PARENT_BLOCK:
            fspr_file_pipe_timeout_set(attr->child_err, -1);
            break;
        case APR_CHILD_BLOCK:
            fspr_file_pipe_timeout_set(attr->parent_err, -1);
            break;
        default:
            break;
        }
    } 
    return APR_SUCCESS;
}

APR_DECLARE(fspr_status_t) fspr_procattr_dir_set(fspr_procattr_t *attr, 
                                               const char *dir) 
{
    char * cwd;
    if (dir[0] != '/') {
        cwd = (char*)malloc(sizeof(char) * PATH_MAX);
        getcwd(cwd, PATH_MAX);
        attr->currdir = (char *)fspr_pstrcat(attr->pool, cwd, "/", dir, NULL);
        free(cwd);
    } else {
        attr->currdir = (char *)fspr_pstrdup(attr->pool, dir);
    }
    if (attr->currdir) {
        return APR_SUCCESS;
    }
    return APR_ENOMEM;
}

APR_DECLARE(fspr_status_t) fspr_procattr_cmdtype_set(fspr_procattr_t *attr,
                                                   fspr_cmdtype_e cmd) 
{
    attr->cmdtype = cmd;
    return APR_SUCCESS;
}

APR_DECLARE(fspr_status_t) fspr_procattr_detach_set(fspr_procattr_t *attr, fspr_int32_t detach) 
{
    attr->detached = detach;
    return APR_SUCCESS;
}

APR_DECLARE(fspr_status_t) fspr_proc_fork(fspr_proc_t *proc, fspr_pool_t *pool)
{
    int pid;
    
    if ((pid = fork()) < 0) {
        return errno;
    }
    else if (pid == 0) {
		/* This is really ugly...
		 * The semantics of BeOS's fork() are that areas (used for shared
		 * memory) get COW'd :-( The only way we can make shared memory
		 * work across fork() is therefore to find any areas that have
		 * been created and then clone them into our address space.
         * Thankfully only COW'd areas have the lock variable set at
         * anything but 0, so we can use that to find the areas we need to
         * copy. Of course what makes it even worse is that the loop through
         * the area's will go into an infinite loop, eating memory and then
         * eventually segfault unless we know when we reach then end of the
         * "original" areas and stop. Why? Well, we delete the area and then
         * add another to the end of the list...
		 */
		area_info ai;
		int32 cookie = 0;
        area_id highest = 0;
		
        while (get_next_area_info(0, &cookie, &ai) == B_OK)
            if (ai.area	> highest)
                highest = ai.area;
        cookie = 0;
        while (get_next_area_info(0, &cookie, &ai) == B_OK) {
            if (ai.area > highest)
                break;
            if (ai.lock > 0) {
                area_id original = find_area(ai.name);
                delete_area(ai.area);
                clone_area(ai.name, &ai.address, B_CLONE_ADDRESS,
                           ai.protection, original);
            }
        }
		
        proc->pid = pid;
        proc->in = NULL; 
        proc->out = NULL; 
        proc->err = NULL;
        return APR_INCHILD;
    }
    proc->pid = pid;
    proc->in = NULL; 
    proc->out = NULL; 
    proc->err = NULL; 
    return APR_INPARENT;
}

APR_DECLARE(fspr_status_t) fspr_procattr_child_errfn_set(fspr_procattr_t *attr,
                                                       fspr_child_errfn_t *errfn)
{
    /* won't ever be called on this platform, so don't save the function pointer */
    return APR_SUCCESS;
}

APR_DECLARE(fspr_status_t) fspr_procattr_error_check_set(fspr_procattr_t *attr,
                                                       fspr_int32_t chk)
{
    /* won't ever be used on this platform, so don't save the flag */
    return APR_SUCCESS;
}

APR_DECLARE(fspr_status_t) fspr_procattr_addrspace_set(fspr_procattr_t *attr,
                                                       fspr_int32_t addrspace)
{
    /* won't ever be used on this platform, so don't save the flag */
    return APR_SUCCESS;
}

APR_DECLARE(fspr_status_t) fspr_proc_create(fspr_proc_t *new, const char *progname, 
                                          const char * const *args,
                                          const char * const *env, 
                                          fspr_procattr_t *attr, 
                                          fspr_pool_t *pool)
{
    int i=0,nargs=0;
    char **newargs = NULL;
    thread_id newproc, sender;
    struct send_pipe *sp;        
    char * dir = NULL;
	    
    sp = (struct send_pipe *)fspr_palloc(pool, sizeof(struct send_pipe));

    new->in = attr->parent_in;
    new->err = attr->parent_err;
    new->out = attr->parent_out;
	sp->in  = attr->child_in  ? attr->child_in->filedes  : -1;
	sp->out = attr->child_out ? attr->child_out->filedes : -1;
	sp->err = attr->child_err ? attr->child_err->filedes : -1;

    i = 0;
    while (args && args[i]) {
        i++;
    }

	newargs = (char**)malloc(sizeof(char *) * (i + 4));
	newargs[0] = strdup("/boot/home/config/bin/fspr_proc_stub");
    if (attr->currdir == NULL) {
        /* we require the directory , so use a temp. variable */
        dir = malloc(sizeof(char) * PATH_MAX);
        getcwd(dir, PATH_MAX);
        newargs[1] = strdup(dir);
        free(dir);
    } else {
        newargs[1] = strdup(attr->currdir);
    }
    newargs[2] = strdup(progname);
    i=0;nargs = 3;

    while (args && args[i]) {
        newargs[nargs] = strdup(args[i]);
        i++;nargs++;
    }
    newargs[nargs] = NULL;

    /* ### we should be looking at attr->cmdtype in here... */

    newproc = load_image(nargs, (const char**)newargs, (const char**)env);

    /* load_image copies the data so now we can free it... */
    while (--nargs >= 0)
        free (newargs[nargs]);
    free(newargs);
        
    if (newproc < B_NO_ERROR) {
        return errno;
    }

    resume_thread(newproc);

    if (attr->child_in) {
        fspr_file_close(attr->child_in);
    }
    if (attr->child_out) {
        fspr_file_close(attr->child_out);
    }
    if (attr->child_err) {
        fspr_file_close(attr->child_err);
    }

    send_data(newproc, 0, (void*)sp, sizeof(struct send_pipe));
    new->pid = newproc;

    /* before we go charging on we need the new process to get to a 
     * certain point.  When it gets there it'll let us know and we
     * can carry on. */
    receive_data(&sender, (void*)NULL,0);
    
    return APR_SUCCESS;
}

APR_DECLARE(fspr_status_t) fspr_proc_wait_all_procs(fspr_proc_t *proc,
                                                  int *exitcode,
                                                  fspr_exit_why_e *exitwhy,
                                                  fspr_wait_how_e waithow, 
                                                  fspr_pool_t *p)
{
    proc->pid = -1;
    return fspr_proc_wait(proc, exitcode, exitwhy, waithow);
} 

APR_DECLARE(fspr_status_t) fspr_proc_wait(fspr_proc_t *proc,
                                        int *exitcode, 
                                        fspr_exit_why_e *exitwhy,
                                        fspr_wait_how_e waithow)
{
    pid_t pstatus;
    int waitpid_options = WUNTRACED;
    int exit_int;
    int ignore;
    fspr_exit_why_e ignorewhy;

    if (exitcode == NULL) {
        exitcode = &ignore;
    }
    if (exitwhy == NULL) {
        exitwhy = &ignorewhy;
    }

    if (waithow != APR_WAIT) {
        waitpid_options |= WNOHANG;
    }

    if ((pstatus = waitpid(proc->pid, &exit_int, waitpid_options)) > 0) {
        proc->pid = pstatus;
        if (WIFEXITED(exit_int)) {
            *exitwhy = APR_PROC_EXIT;
            *exitcode = WEXITSTATUS(exit_int);
        }
        else if (WIFSIGNALED(exit_int)) {
            *exitwhy = APR_PROC_SIGNAL;
            *exitcode = WTERMSIG(exit_int);
        }
        else {

            /* unexpected condition */
            return APR_EGENERAL;
        }
        return APR_CHILD_DONE;
    }
    else if (pstatus == 0) {
        return APR_CHILD_NOTDONE;
    }

    return errno;
} 

APR_DECLARE(fspr_status_t) fspr_procattr_child_in_set(fspr_procattr_t *attr, fspr_file_t *child_in,
                                   fspr_file_t *parent_in)
{
    if (attr->child_in == NULL && attr->parent_in == NULL)
        fspr_file_pipe_create(&attr->child_in, &attr->parent_in, attr->pool);

    if (child_in != NULL)
        fspr_file_dup(&attr->child_in, child_in, attr->pool);

    if (parent_in != NULL)
        fspr_file_dup(&attr->parent_in, parent_in, attr->pool);

    return APR_SUCCESS;
}

APR_DECLARE(fspr_status_t) fspr_procattr_child_out_set(fspr_procattr_t *attr, fspr_file_t *child_out,
                                                     fspr_file_t *parent_out)
{
    if (attr->child_out == NULL && attr->parent_out == NULL)
        fspr_file_pipe_create(&attr->child_out, &attr->parent_out, attr->pool);

    if (child_out != NULL)
        fspr_file_dup(&attr->child_out, child_out, attr->pool);

    if (parent_out != NULL)
        fspr_file_dup(&attr->parent_out, parent_out, attr->pool);

    return APR_SUCCESS;
}

APR_DECLARE(fspr_status_t) fspr_procattr_child_err_set(fspr_procattr_t *attr, fspr_file_t *child_err,
                                                     fspr_file_t *parent_err)
{
    if (attr->child_err == NULL && attr->parent_err == NULL)
        fspr_file_pipe_create(&attr->child_err, &attr->parent_err, attr->pool);

    if (child_err != NULL)
        fspr_file_dup(&attr->child_err, child_err, attr->pool);

    if (parent_err != NULL)
        fspr_file_dup(&attr->parent_err, parent_err, attr->pool);

    return APR_SUCCESS;
}

APR_DECLARE(fspr_status_t) fspr_procattr_limit_set(fspr_procattr_t *attr, fspr_int32_t what, 
                                                  void *limit)
{
    return APR_ENOTIMPL;
}

APR_DECLARE(fspr_status_t) fspr_procattr_user_set(fspr_procattr_t *attr, 
                                                const char *username,
                                                const char *password)
{
    return APR_ENOTIMPL;
}

APR_DECLARE(fspr_status_t) fspr_procattr_group_set(fspr_procattr_t *attr,
                                                 const char *groupname)
{
    return APR_ENOTIMPL;
}
