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
	if (!persistentHandle->IsNearDeath() && !GetIsolate()->GetCurrentContext().IsEmpty() && (!js || !js->GetForcedTermination())) {
		Handle<Object> jsObj = GetJavaScriptObject();
		jsObj->SetInternalField(0, Null(GetIsolate()));
	}

	persistentHandle->ClearWeak();
	persistentHandle->Reset();

	delete persistentHandle;
}

Handle<Object> JSBase::GetJavaScriptObject()
{
	/* Returns the javascript object related to this C++ instance */
	return Local<Object>::New(GetIsolate(), *persistentHandle);
}

void JSBase::AddInstance(Isolate *isolate, const Handle<Object>& handle, const Handle<External>& object, bool autoDestroy)
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
	obj->persistentHandle->SetWeak<JSBase>(obj, WeakCallback);
	obj->persistentHandle->MarkIndependent();
}

void JSBase::WeakCallback(const WeakCallbackData<Object, JSBase>& data)
{
	JSBase *wrap = data.GetParameter();
	Local<Object> pobj = data.GetValue();

	if (wrap->autoDestroy) {
		HandleScope scope(data.GetIsolate());
		assert(pobj == *wrap->persistentHandle);
		delete wrap;
	} else if (!wrap->persistentHandle->IsEmpty()) {
		wrap->persistentHandle->ClearWeak();
		wrap->persistentHandle->Reset();
	}
}

void JSBase::CreateInstance(const v8::FunctionCallbackInfo<Value>& args)
{
	Handle<External> external;
	bool autoDestroy = true;
	bool constructorFailed = false;

	if (!args.IsConstructCall()) {
		args.GetIsolate()->ThrowException(String::NewFromUtf8(args.GetIsolate(), "Seems you forgot the 'new' operator."));
		return;
	}

	if (args[0]->IsExternal()) {
		// The argument is an existing object, just use that.
		external = Handle<External>::Cast(args[0]);
		autoDestroy = args[1]->BooleanValue();
	} else {
		// Create a new C++ instance
		Handle<External> ex = Handle<External>::Cast(args.Callee()->GetHiddenValue(String::NewFromUtf8(args.GetIsolate(), "constructor_method")));

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
		args.GetIsolate()->ThrowException(String::NewFromUtf8(args.GetIsolate(), "This class cannot be created from javascript."));
	} else {
		/* Use whatever was set from the constructor */
	}
}

void JSBase::Register(Isolate *isolate, const js_class_definition_t *desc)
{
	// Get the context's global scope (that's where we'll put the constructor)
	Handle<Object> global = isolate->GetCurrentContext()->Global();

	// Create function template for our constructor it will call the JSBase::createInstance method
	Handle<FunctionTemplate> function = FunctionTemplate::New(isolate, JSBase::CreateInstance);
	function->SetClassName(String::NewFromUtf8(isolate, desc->name));

	// Make room for saving the C++ object reference somewhere
	function->InstanceTemplate()->SetInternalFieldCount(1);

	// Add methods to the object
	for (int i = 0;; i++) {
		if (!desc->functions[i].func) break;
		function->InstanceTemplate()->Set(String::NewFromUtf8(isolate, desc->functions[i].name), FunctionTemplate::New(isolate, desc->functions[i].func));
	}

	// Add properties to the object
	for (int i = 0;; i++) {
		if (!desc->properties[i].get) break;
		function->InstanceTemplate()->SetAccessor(String::NewFromUtf8(isolate, desc->properties[i].name), desc->properties[i].get, desc->properties[i].set);
	}

	function->GetFunction()->SetHiddenValue(String::NewFromUtf8(isolate, "constructor_method"), External::New(isolate, (void *)desc->constructor));

	// Set the function in the global scope, to make it available
	global->Set(v8::String::NewFromUtf8(isolate, desc->name), function->GetFunction());
}

void JSBase::RegisterInstance(Isolate *isolate, string name, bool autoDestroy)
{
	// Get the context's global scope (that's where we'll put the constructor)
	Handle<Object> global = isolate->GetCurrentContext()->Global();

	Local<Function> func = Local<Function>::Cast(global->Get(v8::String::NewFromUtf8(isolate, this->GetJSClassName().c_str())));

	// Add the C++ instance as an argument, so it won't try to create another one.
	Handle<Value> args[] = { External::New(isolate, this), Boolean::New(isolate, autoDestroy) };
	Handle<Object> newObj = func->NewInstance(2, args);

	// Add the instance to JavaScript.
	if (name.size() > 0) {
		global->Set(String::NewFromUtf8(isolate, name.c_str()), newObj);
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

Handle<Function> JSBase::GetFunctionFromArg(Isolate *isolate, const Local<Value>& arg)
{
	Handle<Function> func;

	if (!arg.IsEmpty() && arg->IsFunction()) {
		// Cast the argument directly to a function
		func = Handle<Function>::Cast(arg);
	} else if (!arg.IsEmpty() && arg->IsString()) {
		Handle<String> tmp = Handle<String>::Cast(arg);
		if (!tmp.IsEmpty() && *tmp) {
			// Fetch the actual function pointer from the global context (by function name)
			Handle<Value> val = isolate->GetCurrentContext()->Global()->Get(tmp);
			if (!val.IsEmpty() && val->IsFunction()) {
				func = Handle<Function>::Cast(val);
			}
		}
	}

	if (!func.IsEmpty() && func->IsFunction()) {
		return func;
	} else {
		return Handle<Function>();
	}
}

void JSBase::DefaultSetProperty(v8::Local<v8::String> property, v8::Local<v8::Value> value, const v8::PropertyCallbackInfo<void>& info)
{
	info.GetIsolate()->ThrowException(v8::String::NewFromUtf8(info.GetIsolate(), "this property cannot be changed!"));
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
