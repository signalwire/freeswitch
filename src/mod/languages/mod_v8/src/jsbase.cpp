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
 * jsbase.cpp -- JavaScript Base class (all other JS class implementations must inherit JSBase)
 *
 */

#include "javascript.hpp"
#include <string.h>
#include <assert.h>

using namespace std;
using namespace v8;

JSBase::JSBase(JSMain *owner)
{
	persistentHandle = new Persistent<Object>();

	autoDestroy = false;

	if ((js = owner)) {
		js->AddActiveInstance(this);
	}
}

JSBase::JSBase(const v8::FunctionCallbackInfo<Value>& info)
{
	persistentHandle = new Persistent<Object>();

	autoDestroy = false;

	if ((js = JSMain::GetScriptInstanceFromIsolate(info.GetIsolate()))) {
		js->AddActiveInstance(this);
	}
}

JSBase::~JSBase(void)
{
	if (js) {
		js->RemoveActiveInstance(this);
	}

	if (persistentHandle->IsEmpty()) {
		delete persistentHandle;
		return;
	}

	/* If the object is still alive inside V8, set the internal field to NULL. But only if we're actually inside a JS context */
	if (
#if !V8FS_NEW_API
		/* IsNearDeath() was removed from V8; on the new API the weak callback
		 * already guarantees we are not racing the collector here. */
		!persistentHandle->IsNearDeath() &&
#endif
		!GetIsolate()->GetCurrentContext().IsEmpty() && (!js || !js->GetForcedTermination())) {
		Local<Object> jsObj = GetJavaScriptObject();
		jsObj->SetInternalField(0, Null(GetIsolate()));
	}

	persistentHandle->ClearWeak();
	persistentHandle->Reset();

	delete persistentHandle;
}

Local<Object> JSBase::GetJavaScriptObject()
{
	/* Returns the javascript object related to this C++ instance */
	return Local<Object>::New(GetIsolate(), *persistentHandle);
}

void JSBase::AddInstance(Isolate *isolate, const Local<Object>& handle, const Local<External>& object, bool autoDestroy)
{
	// Get the actual C++ class pointer
    JSBase *obj = static_cast<JSBase*>(object->Value());

	// Sanity check
	assert(obj->persistentHandle->IsEmpty());

	// Store the C++ instance in JavaScript engine.
	assert(handle->InternalFieldCount() > 0);
	handle->SetInternalField(0, object);

	obj->autoDestroy = autoDestroy;

	// Make the handle weak
	obj->persistentHandle->Reset(isolate, handle);
#if defined(V8_MAJOR_VERSION) && V8_MAJOR_VERSION >=5
	obj->persistentHandle->SetWeak<JSBase>(obj, WeakCallback, WeakCallbackType::kParameter);
#else
	obj->persistentHandle->SetWeak<JSBase>(obj, WeakCallback);
#endif
#if !V8FS_NEW_API
	/* MarkIndependent() was removed from V8; independence is now the default
	 * for weak handles, so this call is no longer needed on the new API. */
	obj->persistentHandle->MarkIndependent();
#endif
}

#if defined(V8_MAJOR_VERSION) && V8_MAJOR_VERSION >=5
void JSBase::WeakCallback(const WeakCallbackInfo<JSBase>& data)
#else
void JSBase::WeakCallback(const WeakCallbackData<Object, JSBase>& data)
#endif
{
#if defined(V8_MAJOR_VERSION) && V8_MAJOR_VERSION >=5
	JSBase *wrap = (JSBase*)data.GetParameter();
#else
	JSBase *wrap = data.GetParameter();
	Local<Object> pobj = data.GetValue();
#endif

	if (wrap->autoDestroy) {
		HandleScope scope(data.GetIsolate());
#if defined(V8_MAJOR_VERSION) && V8_MAJOR_VERSION >=5
#else
		assert(pobj == *wrap->persistentHandle);
#endif
		delete wrap;
	} else if (!wrap->persistentHandle->IsEmpty()) {
		wrap->persistentHandle->ClearWeak();
		wrap->persistentHandle->Reset();
	}
}

void JSBase::CreateInstance(const v8::FunctionCallbackInfo<Value>& args)
{
	Local<External> external;
	bool autoDestroy = true;
	bool constructorFailed = false;

	if (!args.IsConstructCall()) {
		args.GetIsolate()->ThrowException(js_new_string(args.GetIsolate(), "Seems you forgot the 'new' operator."));
		return;
	}

	if (args[0]->IsExternal()) {
		// The argument is an existing object, just use that.
		external = Local<External>::Cast(args[0]);
		autoDestroy = js_to_bool(args[1]);
	} else {
		// Create a new C++ instance
#if defined(V8_MAJOR_VERSION) && V8_MAJOR_VERSION >=5
		Local<External> ex = Local<External>::Cast(args.Data());
#else
		Local<External> ex = Local<External>::Cast(args.Callee()->GetHiddenValue(js_new_string(args.GetIsolate(), "constructor_method")));
#endif

		if (ex->Value()) {
			ConstructorCallback cb = (ConstructorCallback)ex->Value();
			void *p = cb(args);

			if (p) {
				external = External::New(args.GetIsolate(), p);
			} else {
				constructorFailed = true;
			}
		}
	}

	// Add this instance to Javascript.
	if (!external.IsEmpty()) {
		AddInstance(args.GetIsolate(), args.This(), external, autoDestroy);

		// Return the newly created object
		args.GetReturnValue().Set(args.This());
	} else if (!constructorFailed) {
		args.GetIsolate()->ThrowException(js_new_string(args.GetIsolate(), "This class cannot be created from javascript."));
	} else {
		/* Use whatever was set from the constructor */
	}
}

void JSBase::Register(Isolate *isolate, const js_class_definition_t *desc)
{
	// Get the context's global scope (that's where we'll put the constructor)
	Local<Object> global = isolate->GetCurrentContext()->Global();

	Local<External> data = External::New(isolate, (void *)desc->constructor);

	// Create function template for our constructor it will call the JSBase::createInstance method
	Local<FunctionTemplate> function = FunctionTemplate::New(isolate, JSBase::CreateInstance, data);	
	function->SetClassName(js_new_string(isolate, desc->name));

	// Make room for saving the C++ object reference somewhere
	function->InstanceTemplate()->SetInternalFieldCount(1);

	// Add methods to the object
	for (int i = 0;; i++) {
		if (!desc->functions[i].func) break;
		function->InstanceTemplate()->Set(js_new_string(isolate, desc->functions[i].name), FunctionTemplate::New(isolate, desc->functions[i].func));
	}

	// Add properties to the object
	for (int i = 0;; i++) {
		if (!desc->properties[i].get) break;
		function->InstanceTemplate()->SetAccessor(js_new_string(isolate, desc->properties[i].name), desc->properties[i].get, desc->properties[i].set);
	}

#if defined(V8_MAJOR_VERSION) && V8_MAJOR_VERSION >=5
#else
	function->GetFunction(js_current_context()).ToLocalChecked()->SetHiddenValue(js_new_string(isolate, "constructor_method"), External::New(isolate, (void *)desc->constructor));
#endif

	// Set the function in the global scope, to make it available
	js_obj_set(global, js_new_string(isolate, desc->name), function->GetFunction(js_current_context()).ToLocalChecked());
}

void JSBase::RegisterInstance(Isolate *isolate, string name, bool autoDestroy)
{
	// Get the context's global scope (that's where we'll put the constructor)
	Local<Context> context = isolate->GetCurrentContext();
	Local<Object> global = context->Global();

	Local<Function> func = Local<Function>::Cast(js_obj_get(global, js_new_string(isolate, this->GetJSClassName().c_str())));

	// Add the C++ instance as an argument, so it won't try to create another one.
	Local<Value> args[] = { External::New(isolate, this), Boolean::New(isolate, autoDestroy) };
	Local<Object> newObj = func->NewInstance(context, 2, args).ToLocalChecked();

	// Add the instance to JavaScript.
	if (name.size() > 0) {
		js_obj_set(global, js_new_string(isolate, name.c_str()), newObj);
	}
}

JSMain *JSBase::GetOwner()
{
	return js;
}

Isolate *JSBase::GetIsolate()
{
	return (js && js->GetIsolate()) ? js->GetIsolate() : NULL;
}

bool JSBase::GetAutoDestroy()
{
	return autoDestroy;
}

Local<Function> JSBase::GetFunctionFromArg(Isolate *isolate, const Local<Value>& arg)
{
	Local<Function> func;

	if (!arg.IsEmpty() && arg->IsFunction()) {
		// Cast the argument directly to a function
		func = Local<Function>::Cast(arg);
	} else if (!arg.IsEmpty() && arg->IsString()) {
		Local<String> tmp = Local<String>::Cast(arg);
		if (!tmp.IsEmpty() && *tmp) {
			// Fetch the actual function pointer from the global context (by function name)
			Local<Value> val = js_obj_get(isolate->GetCurrentContext()->Global(), tmp);
			if (!val.IsEmpty() && val->IsFunction()) {
				func = Local<Function>::Cast(val);
			}
		}
	}

	if (!func.IsEmpty() && func->IsFunction()) {
		return func;
	} else {
		return Local<Function>();
	}
}

void JSBase::DefaultSetProperty(v8::Local<v8::String> property, v8::Local<v8::Value> value, const v8::PropertyCallbackInfo<void>& info)
{
	info.GetIsolate()->ThrowException(js_new_string(info.GetIsolate(), "this property cannot be changed!"));
}

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
