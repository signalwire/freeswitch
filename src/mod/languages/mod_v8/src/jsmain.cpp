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
 * jsmain.cpp -- JavaScript Main V8 script runner
 *
 */

#include "javascript.hpp"

#ifdef V8_ENABLE_DEBUGGING
#include <v8-debug.h>
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <string>
#include <iostream>
#include <sstream>
#include <fstream>

using namespace std;
using namespace v8;

#ifdef V8_ENABLE_DEBUGGING
void V8DispatchDebugMessages()
{
	Isolate* isolate = Isolate::GetCurrent();
	Persistent<Context> *persistent_contect = (Persistent<Context> *)isolate->GetData(1);
	HandleScope handle_scope(isolate);
	Local<Context> context = Local<Context>::New(isolate, *persistent_contect);
	Context::Scope scope(context);

	Debug::ProcessDebugMessages();
}
#endif

bool JSMain::FileExists(const char *file)
{
	ifstream fh(file);
	bool file_exists = false;

	if (fh) {
		fh.close();
		file_exists = true;
	}

	return file_exists;
}

const string JSMain::LoadFileToString(const string& filename)
{
	if (filename.length() == 0) {
		return "";
	}

	ifstream in(filename.c_str(), std::ios::in | std::ios::binary);

	if (in) {
		string contents;

		in.seekg(0, std::ios::end);
		contents.resize((size_t)in.tellg());

		in.seekg(0, std::ios::beg);
		in.read(&contents[0], contents.size());
		in.close();

		return contents;
	}

	return "";
}

JSMain::JSMain(void)
{
	isolate = Isolate::New();
	
	extenderClasses = new vector<const js_class_definition_t *>();
	extenderFunctions = new vector<js_function_t *>();
	extenderInstances = new vector<registered_instance_t*>();
	activeInstances = new set<JSBase *>();

	forcedTermination = false;
	forcedTerminationMessage = NULL;
	forcedTerminationLineNumber = 0;
	forcedTerminationScriptFile = NULL;
}

JSMain::~JSMain(void)
{
	bool enteredIsolate = false;

	for (size_t i = 0; i < extenderInstances->size(); i++) {
		registered_instance_t *inst = (*extenderInstances)[i];

		if (inst) {
			if (inst->name) free(inst->name);
			free(inst);
		}
	}

	for (size_t i = 0; i < extenderFunctions->size(); i++) {
		js_function_t *proc = (*extenderFunctions)[i];

		if (proc) {
			if (proc->name) free((char *)proc->name);
			free(proc);
		}
	}

	extenderInstances->clear();
	extenderClasses->clear();
	extenderFunctions->clear();

	if (!Isolate::GetCurrent()) {
		enteredIsolate = true;
		isolate->Enter();
	}

	/* It's probably not totally safe to call this here, but it whould always be empty by now anyway */
	DisposeActiveInstances();

	if (enteredIsolate) {
		isolate->Exit();
	}

	delete extenderClasses;
	delete extenderFunctions;
	delete extenderInstances;
	delete activeInstances;

	if (forcedTerminationMessage) free(forcedTerminationMessage);
	if (forcedTerminationScriptFile) free(forcedTerminationScriptFile);

	isolate->Dispose();
}

const string JSMain::GetExceptionInfo(Isolate* isolate, TryCatch* try_catch)
{
	HandleScope handle_scope(isolate);
	String::Utf8Value exception(try_catch->Exception());
	const char *exception_string = js_safe_str(*exception);
	Handle<Message> message = try_catch->Message();
	string res;

	if (message.IsEmpty()) {
		// V8 didn't provide any extra information about this error; just return the exception.
		res = exception_string;
	} else {
		String::Utf8Value filename(message->GetScriptResourceName());
		const char *filename_string = js_safe_str(*filename);
		int linenum = message->GetLineNumber();

		ostringstream ss;

		ss << filename_string << ":" << linenum << ": " << exception_string << "\r\n";
		
		// Print line of source code.
		String::Utf8Value sourceline(message->GetSourceLine());
		const char *sourceline_string = js_safe_str(*sourceline);

		ss << sourceline_string << "\r\n";

		// Print wavy underline.
		int start = message->GetStartColumn();

		for (int i = 0; i < start; i++) {
			ss << " ";
		}

		int end = message->GetEndColumn();

		for (int i = start; i < end; i++) {
			ss << "^";
		}

		res = ss.str();
	}

	return res;
}

void JSMain::Include(const v8::FunctionCallbackInfo<Value>& args)
{
	for (int i = 0; i < args.Length(); i++) {
		HandleScope handle_scope(args.GetIsolate());
		String::Utf8Value str(args[i]);

		// load_file loads the file with this name into a string
		string js_file = LoadFileToString(js_safe_str(*str));

		if (js_file.length() > 0) {
			Handle<String> source = String::NewFromUtf8(args.GetIsolate(), js_file.c_str());

			Handle<Script> script = Script::Compile(source, args[i]);

			args.GetReturnValue().Set(script->Run());

			return;
		}
	}
	
	args.GetReturnValue().Set(Undefined(args.GetIsolate()));
}

void JSMain::Log(const v8::FunctionCallbackInfo<Value>& args)
{
	HandleScope handle_scope(args.GetIsolate());
	String::Utf8Value str(args[0]);

	printf("%s\r\n", js_safe_str(*str));
	
	args.GetReturnValue().Set(Undefined(args.GetIsolate()));
}

void JSMain::Version(const v8::FunctionCallbackInfo<Value>& args)
{
	args.GetReturnValue().Set(String::NewFromUtf8(args.GetIsolate(), V8::GetVersion()));
}

const string JSMain::ExecuteScript(const string& filename, bool *resultIsError)
{
	// Get the file and load into a string.
	string data = LoadFileToString(filename);

	return ExecuteString(data, filename, resultIsError);
}

const string JSMain::ExecuteString(const string& scriptData, const string& fileName, bool *resultIsError)
{
	string res;
	bool isError = false;

	//solate->Enter();
	{
		Locker lock(isolate);
		Isolate::Scope iscope(isolate);
		{
			// Create a stack-allocated handle scope.
			HandleScope scope(isolate);

			isolate->SetData(0, this);

			Handle<ObjectTemplate> global = ObjectTemplate::New();
			global->Set(String::NewFromUtf8(isolate, "include"), FunctionTemplate::New(isolate, Include));
			global->Set(String::NewFromUtf8(isolate, "require"), FunctionTemplate::New(isolate, Include));
			global->Set(String::NewFromUtf8(isolate, "log"), FunctionTemplate::New(isolate, Log));

			for (size_t i = 0; i < extenderFunctions->size(); i++) {
				js_function_t *proc = (*extenderFunctions)[i];
				global->Set(String::NewFromUtf8(isolate, proc->name), FunctionTemplate::New(isolate, proc->func));
			}

			// Create a new context.
			Local<Context> context = Context::New(isolate, NULL, global);

			if (context.IsEmpty()) {
				return "Failed to create new JS context";
			}

			// Enter the created context for compiling and running the script.
			Context::Scope context_scope(context);

			// Register all plugins.
			for (size_t i = 0; i < extenderClasses->size(); i++) {
				JSBase::Register(isolate, (*extenderClasses)[i]);
			}

			// Register all instances.
			for (size_t i = 0; i < extenderInstances->size(); i++) {
				registered_instance_t *inst = (*extenderInstances)[i];
				inst->obj->RegisterInstance(isolate, inst->name, inst->auto_destroy);
			}

			// Create a string containing the JavaScript source code.
			Handle<String> source = String::NewFromUtf8(isolate, scriptData.c_str());

			TryCatch try_catch;

			// Compile the source code.
			Handle<Script> script = Script::Compile(source, Local<Value>::New(isolate, String::NewFromUtf8(isolate, fileName.c_str())));

			if (try_catch.HasCaught()) {
				res = JSMain::GetExceptionInfo(isolate, &try_catch);
				isError = true;
			} else {
				// Run the script
				Handle<Value> result = script->Run();
				if (try_catch.HasCaught()) {
					res = JSMain::GetExceptionInfo(isolate, &try_catch);
					isError = true;
				} else {
					if (forcedTermination) {
						forcedTermination = false;
						res = GetForcedTerminationMessage();
					}

					// return result as string.
					String::Utf8Value ascii(result);
					if (*ascii) {
						res = *ascii;
					}
#ifndef V8_FORCE_GC_AFTER_EXECUTION
					DisposeActiveInstances();
#endif
				}
			}

			isolate->SetData(0, NULL);
		}

#ifdef V8_FORCE_GC_AFTER_EXECUTION
		V8::ContextDisposedNotification();
		while (!V8::IdleNotification()) {}
		DisposeActiveInstances();
#endif
	}
	//isolate->Exit();
	
	if (resultIsError) {
		*resultIsError = isError;
	}

	return res;
}

/** Add a class to be used in JS, the definition passed here must never be destroyed */
void JSMain::AddJSExtenderClass(const js_class_definition_t *method)
{
	extenderClasses->push_back(method);
}

void JSMain::AddJSExtenderFunction(FunctionCallback func, const string& name)
{
	if (!func || name.length() == 0) {
		return;
	}

	js_function_t *proc = (js_function_t *)malloc(sizeof(*proc));

	if (proc) {
		memset(proc, 0, sizeof(*proc));

		proc->func = func;
		js_strdup(proc->name, name.c_str());

		extenderFunctions->push_back(proc);
	}
}

void JSMain::AddJSExtenderInstance(JSBase *instance, const string& objectName, bool autoDestroy)
{
	registered_instance_t *inst = (registered_instance_t *)malloc(sizeof(*inst));

	if (inst) {
		memset(inst, 0, sizeof(*inst));

		inst->obj = instance;
		if (objectName.size() > 0) js_strdup(inst->name, objectName.c_str());
		inst->auto_destroy = autoDestroy;

		extenderInstances->push_back(inst);
	}
}

JSMain *JSMain::GetScriptInstanceFromIsolate(Isolate* isolate)
{
	if (isolate) {
		void *p = isolate->GetData(0);
		if (p) {
			return static_cast<JSMain *>(p);
		}
	}

	return NULL;
}

Isolate *JSMain::GetIsolate()
{
	return isolate;
}

void JSMain::Initialize()
{
	V8::InitializeICU(); // Initialize();
}

void JSMain::Dispose()
{
	// Make sure to cleanup properly!
	V8::LowMemoryNotification();
	while (!V8::IdleNotification()) {}

	V8::Dispose();
}

const vector<const js_class_definition_t *>& JSMain::GetExtenderClasses() const
{
	return *extenderClasses;
}

const vector<js_function_t *>& JSMain::GetExtenderFunctions() const
{
	return *extenderFunctions;
}

const vector<registered_instance_t*>& JSMain::GetExtenderInstances() const
{
	return *extenderInstances;
}

void JSMain::AddActiveInstance(JSBase *obj)
{
	activeInstances->insert(obj);
}

void JSMain::RemoveActiveInstance(JSBase *obj)
{
	if (obj) {
		set<JSBase *>::iterator it = activeInstances->find(obj);

		if (it != activeInstances->end()) {
			activeInstances->erase(it);
		}
	}
}

void JSMain::DisposeActiveInstances()
{
	set<JSBase *>::iterator it = activeInstances->begin();
	size_t c = activeInstances->size();

	while (it != activeInstances->end()) {
		JSBase *obj = *it;
		delete obj; /* After this, the iteratior might be invalid, since the slot in the set might be removed already */

		if (c == activeInstances->size()) {
			/* Nothing changed in the set, make sure to manually remove this instance from the set */
			activeInstances->erase(it);
		}

		it = activeInstances->begin();
		c = activeInstances->size();
	}
}

bool JSMain::GetForcedTermination(void)
{
	return forcedTermination;
}

void JSMain::ResetForcedTermination(void)
{
	forcedTermination = false;
}

const char *JSMain::GetForcedTerminationMessage(void)
{
	return js_safe_str(forcedTerminationMessage);
}

const char *JSMain::GetForcedTerminationScriptFile(void)
{
	return js_safe_str(forcedTerminationScriptFile);
}

int JSMain::GetForcedTerminationLineNumber(void)
{
	return forcedTerminationLineNumber;
}

void JSMain::ExitScript(Isolate *isolate, const char *msg)
{
	if (!isolate) {
		return;
	}

	JSMain *js = JSMain::GetScriptInstanceFromIsolate(isolate);

	if (js) {
		js->forcedTermination = true;

		/* Free old data if it exists already */
		if (js->forcedTerminationMessage) {
			free(js->forcedTerminationMessage);
			js->forcedTerminationMessage = NULL;
		}

		if (js->forcedTerminationScriptFile) {
			free(js->forcedTerminationScriptFile);
			js->forcedTerminationScriptFile = NULL;
		}

		/* Save message for later use */
		if (msg) {
			js_strdup(js->forcedTerminationMessage, msg);
		}

		js->forcedTerminationScriptFile = GetStackInfo(isolate, &js->forcedTerminationLineNumber);
	}

	V8::TerminateExecution(isolate);
}

char *JSMain::GetStackInfo(Isolate *isolate, int *lineNumber)
{
	HandleScope handle_scope(isolate);
	const char *file = __FILE__; /* Use current filename if we can't find the correct from JS stack */
	int line = __LINE__; /* Use current line number if we can't find the correct from JS stack */
	char *ret = NULL;

	/* Try to get the current stack trace (script file) */
	Local<StackTrace> stFile = StackTrace::CurrentStackTrace(isolate, 1, StackTrace::kScriptName);

	if (!stFile.IsEmpty()) {
		Local<StackFrame> sf = stFile->GetFrame(0);

		if (!sf.IsEmpty()) {
			Local<String> fn = sf->GetScriptName();

			if (!fn.IsEmpty()) {
				String::Utf8Value str(fn);

				if (*str) {
					js_strdup(ret, *str); // We must dup here
				}
			}
		}
	}

	/* dup current filename if we got nothing from stack */
	if (ret == NULL) {
		js_strdup(ret, file);
	}

	/* Try to get the current stack trace (line number) */
	if (lineNumber) {
		*lineNumber = 0;

		Local<StackTrace> stLine = StackTrace::CurrentStackTrace(isolate, 1, StackTrace::kLineNumber);

		if (!stLine.IsEmpty()) {
			Local<StackFrame> sf = stLine->GetFrame(0);

			if (!sf.IsEmpty()) {
				*lineNumber = sf->GetLineNumber();
			}
		}

		/* Use current file number if we got nothing from stack */
		if (*lineNumber == 0) {
			*lineNumber = line;
		}
	}

	/* Return dup'ed value - this must be freed by the calling function */
	return ret;
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
