/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2007, Damjan Jovanovic <d a m j a n d o t j o v a t g m a i l d o t c o m>
 *
 * Version: MPL 1.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 *
 * The Initial Developer of the Original Code is
 * Damjan Jovanovic <d a m j a n d o t j o v a t g m a i l d o t c o m>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * Damjan Jovanovic <d a m j a n d o t j o v a t g m a i l d o t c o m>
 *
 *
 * mod_java.c -- Java Module
 *
 */

#include <switch.h>
#include <jni.h>


static switch_memory_pool_t *memoryPool = NULL;
static switch_dso_handle_t *javaVMHandle = NULL;

JavaVM *javaVM = NULL;


SWITCH_MODULE_LOAD_FUNCTION(mod_java_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_java_shutdown);
SWITCH_MODULE_DEFINITION(mod_java, mod_java_load, mod_java_shutdown, NULL);


static void launch_java(switch_core_session_t *session, const char *data, JNIEnv * env)
{
	jclass Launcher = NULL;
	jmethodID launch = NULL;
	jstring uuid = NULL;
	jstring args = NULL;

	Launcher = (*env)->FindClass(env, "org/freeswitch/Launcher");
	if (Launcher == NULL) {
		(*env)->ExceptionDescribe(env);
		goto done;
	}

	launch = (*env)->GetStaticMethodID(env, Launcher, "launch", "(Ljava/lang/String;Ljava/lang/String;)V");
	if (launch == NULL) {
		(*env)->ExceptionDescribe(env);
		goto done;
	}

	uuid = (*env)->NewStringUTF(env, switch_core_session_get_uuid(session));
	if (uuid == NULL) {
		(*env)->ExceptionDescribe(env);
		goto done;
	}

	args = (*env)->NewStringUTF(env, data);
	if (args == NULL) {
		(*env)->ExceptionDescribe(env);
		goto done;
	}

	(*env)->CallStaticVoidMethod(env, Launcher, launch, uuid, args);
	if ((*env)->ExceptionOccurred(env))
		(*env)->ExceptionDescribe(env);

  done:
	if (args != NULL)
		(*env)->DeleteLocalRef(env, args);
	if (uuid != NULL)
		(*env)->DeleteLocalRef(env, uuid);
	if (Launcher != NULL)
		(*env)->DeleteLocalRef(env, Launcher);
}

SWITCH_STANDARD_APP(java_function)
{
	JNIEnv *env;
	jint res;

	if (javaVM == NULL)
		return;

	res = (*javaVM)->AttachCurrentThread(javaVM, (void *) &env, NULL);
	if (res == JNI_OK) {
		launch_java(session, data, env);
		(*javaVM)->DetachCurrentThread(javaVM);
	} else
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error attaching thread to Java VM!\n");
}

static switch_status_t load_config(JavaVMOption ** javaOptions, int *optionCount)
{
	switch_xml_t cfg, xml;
	switch_status_t status;

	xml = switch_xml_open_cfg("java.conf", &cfg, NULL);
	if (xml) {
		switch_xml_t javavm;
		switch_xml_t options;

		javavm = switch_xml_child(cfg, "javavm");
		if (javavm != NULL) {
			const char *path = switch_xml_attr_soft(javavm, "path");
			if (path != NULL) {
				status = switch_dso_load(&javaVMHandle, path, memoryPool);
				if (status != SWITCH_STATUS_SUCCESS)
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error loading %s\n", path);
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No Java VM path specified in java.conf.xml\n");
				status = SWITCH_STATUS_FALSE;
			}
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No Java VM specified in java.conf.xml\n");
			status = SWITCH_STATUS_FALSE;
			goto close;
		}

		options = switch_xml_child(cfg, "options");
		if (options != NULL) {
			switch_xml_t option;
			int i = 0;
			*optionCount = 0;
			for (option = switch_xml_child(options, "option"); option; option = option->next) {
				const char *value = switch_xml_attr_soft(option, "value");
				if (value != NULL)
					++ * optionCount;
			}
			*optionCount += 1;
			*javaOptions = switch_core_alloc(memoryPool, (switch_size_t) (*optionCount * sizeof(JavaVMOption)));
			if (*javaOptions == NULL) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Out of memory!\n");
				status = SWITCH_STATUS_FALSE;
				goto close;
			}
			for (option = switch_xml_child(options, "option"); option; option = option->next) {
				const char *value = switch_xml_attr_soft(option, "value");
				if (value == NULL)
					continue;
				(*javaOptions)[i].optionString = switch_core_strdup(memoryPool, value);
				if ((*javaOptions)[i].optionString == NULL) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Out of memory!\n");
					status = SWITCH_STATUS_FALSE;
					goto close;
				}
				++i;
			}
			(*javaOptions)[i].optionString = "-Djava.library.path=" SWITCH_PREFIX_DIR SWITCH_PATH_SEPARATOR "mod";
		}

	  close:
		switch_xml_free(xml);
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error opening java.conf.xml\n");
		status = SWITCH_STATUS_FALSE;
	}
	return status;
}

static switch_status_t create_java_vm(JavaVMOption * options, int optionCount)
{
	jint(JNICALL * pJNI_CreateJavaVM) (JavaVM **, void **, void *);
	switch_status_t status;

	status = switch_dso_sym((void *) &pJNI_CreateJavaVM, javaVMHandle, "JNI_CreateJavaVM");
	if (status == SWITCH_STATUS_SUCCESS) {
		JNIEnv *env;
		JavaVMInitArgs initArgs;
		jint res;

		memset(&initArgs, 0, sizeof(initArgs));
		initArgs.version = JNI_VERSION_1_4;
		initArgs.nOptions = optionCount;
		initArgs.options = options;
		initArgs.ignoreUnrecognized = JNI_TRUE;

		res = pJNI_CreateJavaVM(&javaVM, (void *) &env, &initArgs);
		if (res == JNI_OK) {
			(*javaVM)->DetachCurrentThread(javaVM);
			status = SWITCH_STATUS_SUCCESS;
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error creating Java VM!\n");
			status = SWITCH_STATUS_FALSE;
		}
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Specified Java VM doesn't have JNI_CreateJavaVM\n");
		status = SWITCH_STATUS_FALSE;
	}
	return status;
}

SWITCH_MODULE_LOAD_FUNCTION(mod_java_load)
{
	switch_status_t status;
	JavaVMOption *options = NULL;
	int optionCount = 0;
	switch_application_interface_t *app_interface;

	*module_interface = switch_loadable_module_create_module_interface(pool, modname);
	SWITCH_ADD_APP(app_interface, "java", NULL, NULL, java_function, NULL, SAF_SUPPORT_NOMEDIA);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Java Framework Loading...\n");

	if (javaVM != NULL)
		return SWITCH_STATUS_SUCCESS;

	status = switch_core_new_memory_pool(&memoryPool);
	if (status == SWITCH_STATUS_SUCCESS) {
		status = load_config(&options, &optionCount);
		if (status == SWITCH_STATUS_SUCCESS) {
			status = create_java_vm(options, optionCount);
			if (status == SWITCH_STATUS_SUCCESS)
				return SWITCH_STATUS_SUCCESS;
			switch_dso_unload(javaVMHandle);
		}
		switch_core_destroy_memory_pool(&memoryPool);
	} else
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error creating memory pool\n");

	return status == SWITCH_STATUS_SUCCESS ? SWITCH_STATUS_NOUNLOAD : status;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_java_shutdown)
{
	if (javaVM == NULL)
		return SWITCH_STATUS_FALSE;

	(*javaVM)->DestroyJavaVM(javaVM);
	javaVM = NULL;
	switch_dso_unload(javaVMHandle);
	switch_core_destroy_memory_pool(&memoryPool);
	return SWITCH_STATUS_SUCCESS;
}
