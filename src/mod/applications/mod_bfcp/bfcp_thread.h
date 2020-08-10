/*!
 * MODULE   : mod_bfcp
 *
 * \Owners	: GSLab Pvt Ltd
 * 			: www.gslab.com
 * 			: © Copyright 2020 Great Software Laboratory. All rights reserved.
 *
 * The Original Code is mod_bfcp for FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 *
 * Contributor(s):
 *
 * Aman Thakral <aman.thakral@gslab.com>
 * Vishal Abhishek <vishal.abhishek@gslab.com>
 *
 * Reviewer(s):
 *
 * Sagar Joshi <sagar.joshi@gslab.com>
 * Prashanth Regalla <prashanth.regalla@gslab.com>
 *
 * bfcp_thread.h -- LIBBFCP ENDPOINT CODE
 *
 */
#include <pthread.h>

typedef pthread_mutex_t mod_bfcp_mutex_t;
#define mod_bfcp_mutex_init(a,b) pthread_mutex_init(a,b)
#define mod_bfcp_mutex_destroy(a) pthread_mutex_destroy(a)
#define mod_bfcp_mutex_lock(a)	pthread_mutex_lock(a);
#define mod_bfcp_mutex_unlock(a) pthread_mutex_unlock(a);