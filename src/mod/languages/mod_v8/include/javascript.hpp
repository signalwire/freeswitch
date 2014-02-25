/*
 * mod_v8 for FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2013-2014, Peter Olsson <peter@olssononline.se>
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
 * The Original Code is mod_v8 for FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 *
 * The Initial Developer of the Original Code is
 * Peter Olsson <peter@olssononline.se>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Peter Olsson <peter@olssononline.se>
 *
 * javascript.hpp -- Header file for main JavaScript classes
 *
 */

#ifndef V8_JAVASCRIPT_H
#define V8_JAVASCRIPT_H

#include <stdint.h>
#include <v8.h>

#include <string>
#include <vector>
#include <set>
#include <assert.h>

/* Enable this define enable V8 debugging protocol, this is not yet working */
//#define V8_ENABLE_DEBUGGING

/*
 * Enable this define to force a GC after the script has finished execution.
 * This is only to help debug memory leaks, and should not be needed for anything else
 */
//#define V8_FORCE_GC_AFTER_EXECUTION


/* Macro for easy V8 "get property" callback definition */
#define JS_GET_PROPERTY_DEF(method_name, class_name) \
	static void method_name(v8::Local<v8::String> property, const v8::PropertyCallbackInfo<v8::Value>& info)\
	{\
		JS_CHECK_SCRIPT_STATE();\
		class_name *obj = JSBase::GetInstance<class_name>(info.Holder());\
		if (obj) {\
			obj->method_name##Impl(property, info);\
		} else {\
			int line;\
			char *file = JSMain::GetStackInfo(info.GetIsolate(), &line);\
			v8::String::Utf8Value str(info.Holder());\
			switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, "mod_v8", line, NULL, SWITCH_LOG_DEBUG, "No valid internal data available for %s when calling %s\n", *str ? *str : "[unknown]", #class_name "::" #method_name "()");\
			free(file);\
			info.GetReturnValue().Set(false);\
		}\
	}\
	void method_name##Impl(v8::Local<v8::String> property, const v8::PropertyCallbackInfo<v8::Value>& info)

/* Macro for easy V8 "set property" callback definition */
#define JS_SET_PROPERTY_DEF(method_name, class_name) \
	static void method_name(v8::Local<v8::String> property, v8::Local<v8::Value> value, const v8::PropertyCallbackInfo<void>& info)\
	{\
		JS_CHECK_SCRIPT_STATE();\
		class_name *obj = JSBase::GetInstance<class_name>(info.Holder());\
		if (obj) {\
			obj->method_name##Impl(property, value, info);\
		} else {\
			int line;\
			char *file = JSMain::GetStackInfo(info.GetIsolate(), &line);\
			v8::String::Utf8Value str(info.Holder());\
			switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, "mod_v8", line, NULL, SWITCH_LOG_DEBUG, "No valid internal data available for %s when calling %s\n", *str ? *str : "[unknown]", #class_name "::" #method_name "()");\
			free(file);\
			info.GetReturnValue().Set(false);\
		}\
	}\
	void method_name##Impl(v8::Local<v8::String> property, v8::Local<v8::Value> value, const v8::PropertyCallbackInfo<void>& info)

/* Macro for easy V8 "function" callback definition */
#define JS_FUNCTION_DEF(method_name, class_name) \
	static void method_name(const v8::FunctionCallbackInfo<v8::Value>& info)\
	{\
		JS_CHECK_SCRIPT_STATE();\
		class_name *obj = JSBase::GetInstance<class_name>(info.Holder());\
		if (obj) {\
			obj->method_name##Impl(info);\
		} else {\
			int line;\
			char *file = JSMain::GetStackInfo(info.GetIsolate(), &line);\
			v8::String::Utf8Value str(info.Holder());\
			switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, "mod_v8", line, NULL, SWITCH_LOG_DEBUG, "No valid internal data available for %s when calling %s\n", *str ? *str : "[unknown]", #class_name "::" #method_name "()");\
			free(file);\
			info.GetReturnValue().Set(false);\
		}\
	}\
	void method_name##Impl(const v8::FunctionCallbackInfo<v8::Value>& info)

/* Macros for V8 callback implementations */
#define JS_GET_PROPERTY_IMPL(method_name, class_name) void class_name::method_name##Impl(v8::Local<v8::String> property, const v8::PropertyCallbackInfo<v8::Value>& info)
#define JS_SET_PROPERTY_IMPL(method_name, class_name) void class_name::method_name##Impl(v8::Local<v8::String> property, v8::Local<v8::Value> value, const v8::PropertyCallbackInfo<void>& info)
#define JS_FUNCTION_IMPL(method_name, class_name) void class_name::method_name##Impl(const v8::FunctionCallbackInfo<v8::Value>& info)

/* Macros for V8 callback definitions (class static version) */
#define JS_GET_PROPERTY_DEF_STATIC(method_name) static void method_name(v8::Local<v8::String> property, const v8::PropertyCallbackInfo<v8::Value>& info)
#define JS_SET_PROPERTY_DEF_STATIC(method_name) static void method_name(v8::Local<v8::String> property, v8::Local<v8::Value> value, const v8::PropertyCallbackInfo<void>& info)
#define JS_FUNCTION_DEF_STATIC(method_name) static void method_name(const v8::FunctionCallbackInfo<v8::Value>& info)

/* Macros for V8 callback implementations (class static version) */
#define JS_GET_PROPERTY_IMPL_STATIC(method_name, class_name) void class_name::method_name(v8::Local<v8::String> property, const v8::PropertyCallbackInfo<v8::Value>& info)
#define JS_SET_PROPERTY_IMPL_STATIC(method_name, class_name) void class_name::method_name(v8::Local<v8::String> property, v8::Local<v8::Value> value, const v8::PropertyCallbackInfo<void>& info)
#define JS_FUNCTION_IMPL_STATIC(method_name, class_name) void class_name::method_name(const v8::FunctionCallbackInfo<v8::Value>& info)

/* Macro for basic script state check (to know if the script is being terminated), should be called before calling any callback actual code */
#define JS_CHECK_SCRIPT_STATE() \
	if (v8::V8::IsExecutionTerminating(info.GetIsolate())) return;\
	if (JSMain::GetScriptInstanceFromIsolate(info.GetIsolate()) && JSMain::GetScriptInstanceFromIsolate(info.GetIsolate())->GetForcedTermination()) return

/* strdup function for all platforms */
#ifdef NDEBUG
#if (_MSC_VER >= 1500)			// VC9+
#define js_strdup(ptr, s) (void)( (!!(ptr = _strdup(s))) || (fprintf(stderr,"ABORT! Malloc failure at: %s:%d", __FILE__, __LINE__),abort(), 0), ptr)
#else
#define js_strdup(ptr, s) (void)( (!!(ptr = strdup(s))) || (fprintf(stderr,"ABORT! Malloc failure at: %s:%d", __FILE__, __LINE__),abort(), 0), ptr)
#endif
#else
#if (_MSC_VER >= 1500)			// VC9+
#define js_strdup(ptr, s) (void)(assert(((ptr) = _strdup(s))),ptr);__analysis_assume( ptr )
#else
#define js_strdup(ptr, s) (void)(assert(((ptr) = strdup((s)))),ptr)
#endif
#endif

/* Makes sure to return a valid char pointer */
#define js_safe_str(s) (s ? s : "")

/* JS Constructor callback definition */
typedef void * void_pointer_t;
typedef void_pointer_t (*ConstructorCallback)(const v8::FunctionCallbackInfo<v8::Value>& info);

/* JS Function definition */
typedef struct {
	const char *name;						/* Name of the function */
	v8::FunctionCallback func;				/* Function callback */
} js_function_t;

/* JS Property definition */
typedef struct {
	const char *name;						/* Name of the property */
	v8::AccessorGetterCallback get;			/* The property getter */
	v8::AccessorSetterCallback set;			/* The property setter */
} js_property_t;

/* JS Class definition */
typedef struct {
	const char *name;						/* The name of the class */
	ConstructorCallback constructor;		/* The constructor definition */
	const js_function_t *functions;			/* An array of function definitions */
	const js_property_t *properties;		/* An array of property definitions */
} js_class_definition_t;

/* Import/export definitions (used by extra loadable modules) */
#ifdef WIN32
/* WIN32 */
#ifdef JSMOD_IMPORT
#define JSMOD_EXPORT __declspec(dllimport)
#else
#define JSMOD_EXPORT __declspec(dllexport)
#endif
#else
/* Not WIN32 */
#ifdef JSMOD_IMPORT
#define JSMOD_EXPORT
#else
#if (HAVE_VISIBILITY != 1)
#define JSMOD_EXPORT
#else
#define JSMOD_EXPORT __attribute__ ((visibility("default")))
#endif
#endif
#endif

/* JSMain class prototype */
class JSMOD_EXPORT JSMain;

/* Base class used by all C++ classes implemented in JS */
class JSMOD_EXPORT JSBase
{
private:
	v8::Persistent<v8::Object> *persistentHandle;	/* The persistent handle of the JavaScript object for this instance */
	bool autoDestroy;								/* flag to tell if this instance should be auto destroyed during JavaScript GC */
	JSMain *js;										/* The "owner" of this instance */

	/* The callback that happens when the V8 GC cleans up object instances */
	static void WeakCallback(const v8::WeakCallbackData<v8::Object, JSBase>& data);

	/* Internal basic constructor when creating a new instance from JS. It will call the actual user code inside */
	static void CreateInstance(const v8::FunctionCallbackInfo<v8::Value>& args);

	/* Store a C++ instance to a JS object's private data */
	static void AddInstance(v8::Isolate *isolate, const v8::Handle<v8::Object>& handle, const v8::Handle<v8::External>& object, bool autoDestroy);
public:
	JSBase(JSMain *owner);
	JSBase(const v8::FunctionCallbackInfo<v8::Value>& info);
	virtual ~JSBase(void);

	/* Returns the JS object related to the C++ instance */
	v8::Handle<v8::Object> GetJavaScriptObject();

	/* Register a C++ class inside V8 (must be called within a entered isolate, and context) */
	static void Register(v8::Isolate *isolate, const js_class_definition_t *desc);

	/* Register an existing C++ class instance inside V8 (must be called within a entered isolate, and context) */
	void RegisterInstance(v8::Isolate *isolate, std::string name, bool autoDestroy);

	/* Get a JSBase instance from JavaScript callback arguments  */
	template <typename T> static T *GetInstance(const v8::FunctionCallbackInfo<v8::Value>& info)
	{
		v8::HandleScope scope(info.GetIsolate());
		return GetInstance<T>(info.Holder());
	}

	/* Get a JSBase instance from a JavaScript object */
	template <typename T> static T *GetInstance(const v8::Local<v8::Object>& self)
	{
		v8::Local<v8::Value> val = self->GetInternalField(0);

		if (!val.IsEmpty() && val->IsExternal()) {
			v8::Local<v8::External> wrap = v8::Local<v8::External>::Cast(val);
			JSBase *ptr = static_cast<JSBase*>(wrap->Value());
			return dynamic_cast<T*>(ptr); /* If we're trying to cast to the wrong type, dynamic_cast will return NULL */
		} else {
			return NULL;
		}
	}

	/* Get a JavaScript function from a JavaScript argument (can be either a string or the actual function) */
	static v8::Handle<v8::Function> GetFunctionFromArg(v8::Isolate *isolate, const v8::Local<v8::Value>& arg);

	/* Default JS setter callback, to be used for read only values */
	static void DefaultSetProperty(v8::Local<v8::String> property, v8::Local<v8::Value> value, const v8::PropertyCallbackInfo<void>& info);

	/* Get the name of the JavaScript class - must be overridden by the actual implementation */
	virtual std::string GetJSClassName() = 0;

	/* Get the JavaScript class instance that owns this instance */
	JSMain *GetOwner();

	/* Get the JavaScript isolate that's active for the current context */
	v8::Isolate *GetIsolate();

	/* Get autoDestroy variable */
	bool GetAutoDestroy();
};

/* Definition of the class registration method */
typedef void (*JSExtenderRegisterMethod)(js_class_definition_t *class_definition);

/* The struct holding a C++ class instance, to be used in JS */
typedef struct {
	JSBase *obj;			/* The class instance to be used in JS */
	char *name;				/* The name of the instance within JS */
	bool auto_destroy;		/* Flag to know if the instance should be auto destroyed when not needed by JS anymore */
} registered_instance_t;

/* Main class for executing a V8 JavaScript */
class JSMOD_EXPORT JSMain
{
private:
	v8::Isolate* isolate;										/* The V8 isolate for this script instance */

	std::vector<const js_class_definition_t *> *extenderClasses;/* List holding C++ classes to be registered in JS on execution */
	std::vector<js_function_t *> *extenderFunctions;			/* List holding C++ functions to be registered in JS on execution */
	std::vector<registered_instance_t*> *extenderInstances;		/* List holding C++ class instances to be registered in JS on execution */
	std::set<JSBase *> *activeInstances;						/* List holding all active instances right now (in a running script) */

	bool forcedTermination;										/* Is set to true if script is triggering a forced termination of the script */
	char *forcedTerminationMessage;								/* The message given during forced termination */
	int forcedTerminationLineNumber;							/* The JS line number that called the exit function */
	char *forcedTerminationScriptFile;							/* The JS script file that called the exit function */

	/* Internal Log function accessable from JS - used just for testing */
	static void Log(const v8::FunctionCallbackInfo<v8::Value>& args);
public:
	JSMain(void);
	~JSMain(void);
	
	void AddJSExtenderFunction(v8::FunctionCallback func, const std::string& name);	/* Add a C++ function to be registered when running the script */
	void AddJSExtenderClass(const js_class_definition_t *method);					/* Add a C++ class to be registered when running the script */
	void AddJSExtenderInstance(JSBase *instance, const std::string& objectName, bool autoDestroy);	/* Add a C++ class instance to be registered when running the script */

	static JSMain *GetScriptInstanceFromIsolate(v8::Isolate* isolate);	/* Get the JavaScript C++ instance from a V8 isolate */
	v8::Isolate *GetIsolate();											/* Get the V8 isolate from the current instance */

	const std::string ExecuteScript(const std::string& filename, bool *resultIsError);
	const std::string ExecuteString(const std::string& scriptData, const std::string& fileName, bool *resultIsError);

	static void Initialize();											/* Initialize the V8 engine */
	static void Dispose();												/* Deinitialize the V8 engine */

	static void Include(const v8::FunctionCallbackInfo<v8::Value>& args);		/* Adds functionality to include another JavaScript from the running script */
	static void Version(const v8::FunctionCallbackInfo<v8::Value>& args);		/* Internal Version function accessable from JS - used to get the current V( version */
	static const std::string GetExceptionInfo(v8::Isolate* isolate, v8::TryCatch* try_catch);	/* Get the exception information from a V8 TryCatch instance */

	const std::vector<const js_class_definition_t *>& GetExtenderClasses() const;/* Returns the list of class definitions */
	const std::vector<js_function_t *>& GetExtenderFunctions() const;			/* Returns the list of function definitions */
	const std::vector<registered_instance_t*>& GetExtenderInstances() const;	/* Returns the list of class instance definitions */

	/* Methods to keep track of all created C++ instances within JS */
	void AddActiveInstance(JSBase *obj);
	void RemoveActiveInstance(JSBase *obj);
	void DisposeActiveInstances();

	static bool FileExists(const char *file);
	static const std::string LoadFileToString(const std::string& filename);

	/* Data related to forced script termination */
	bool GetForcedTermination(void);
	void ResetForcedTermination(void);
	const char *GetForcedTerminationMessage(void);
	const char *GetForcedTerminationScriptFile(void);
	int GetForcedTerminationLineNumber(void);

	/* Method to force termination of a script */
	static void ExitScript(v8::Isolate *isolate, const char *msg);

	/* Get the filename and line number of the current JS stack */
	static char *GetStackInfo(v8::Isolate *isolate, int *lineNumber);
};

#ifdef V8_ENABLE_DEBUGGING
void V8DispatchDebugMessages();
#endif

#endif /* V8_JAVASCRIPT_H */

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet:
 */
