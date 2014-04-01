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
 * fsxml.cpp -- JavaScript XML class
 *
 */

#include "fsxml.hpp"

using namespace std;
using namespace v8;

static const char js_class_name[] = "XML";

FSXML::~FSXML(void)
{
	if (!_rootObject) {
		/* This is the root object */
			
		/* Free the xml struct if it still exists */
		if (_xml) {
			switch_xml_free(_xml);
		}
		
		/* Free all sub-objects in the hash and destroy the hash */
		DestroyHash();
	} else if (_rootObject && _xml && _rootObject->_obj_hash) {
		/* This is not the root object. Remove this object from the root oject's hash */
		_rootObject->DeleteObjectInHash(_xml);
	}

	/* Clear JS reference to the root node */
	_rootJSObject.Reset();

	if (_pool) {
		switch_core_destroy_memory_pool(&_pool);
	}
}

string FSXML::GetJSClassName()
{
	return js_class_name;
}

void FSXML::Init()
{
	_xml = NULL;
	_obj_hash = NULL;
	_pool = NULL;
	_rootObject = NULL;
}

void FSXML::InitRootObject()
{
	/* Create hash and pool - for the root object only */
	if (switch_core_new_memory_pool(&_pool) == SWITCH_STATUS_SUCCESS && _pool) {
		if (switch_core_hash_init(&_obj_hash) != SWITCH_STATUS_SUCCESS) {
			switch_core_destroy_memory_pool(&_pool);
			_obj_hash = NULL;
			_pool = NULL;
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to init hash\n");
		}
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to create memory pool\n");
	}
}

void *FSXML::Construct(const v8::FunctionCallbackInfo<Value>& info)
{
	String::Utf8Value data(info[0]);
	switch_xml_t xml;

	if (*data && (xml = switch_xml_parse_str_dynamic(*data, SWITCH_TRUE))) {
		FSXML *obj = new FSXML(info);
		obj->_xml = xml;
		obj->InitRootObject();
		return obj;
	}

	info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Bad arguments!"));
	return NULL;
	/*
	var xml = new XML(XML_FS_CONF, fs_config_name_string);
	var xml = new XML(XML_FILE, file_name_string);
	var xml = new XML(XML_NEW, new);
	*/
}

Handle<Value> FSXML::GetJSObjFromXMLObj(const switch_xml_t xml, const v8::FunctionCallbackInfo<Value>& info)
{
	FSXML *newObj, *rootObj = NULL;

	if (!_rootObject) {
		rootObj = this;
	} else {
		rootObj = _rootObject;
	}
		
	if (!rootObj) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Could not find XML root node\n");
		return Handle<Value>();
	}

	/* Try to find an existing object in the hash */
	if (!(newObj = rootObj->FindObjectInHash(xml))) {
		/* Create a new object if it wasn't found in the hash */
		if ((newObj = new FSXML(info))) {
			if (_rootJSObject.IsEmpty()) {
				newObj->_rootJSObject.Reset(info.GetIsolate(), info.Holder()); /* The caller is the owner */
			} else {
				newObj->_rootJSObject.Reset(info.GetIsolate(), _rootJSObject); /* The owner is stored in the persistent object */
			}

			newObj->_xml = xml;
			newObj->_rootObject = rootObj;
			rootObj->StoreObjectInHash(xml, newObj);
			newObj->RegisterInstance(info.GetIsolate(), "", true);
		}
	}
		
	if (newObj) {
		return newObj->GetJavaScriptObject(); 
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to create new object - memory error?\n");
	return Handle<Value>();
}

void FSXML::StoreObjectInHash(switch_xml_t xml, FSXML *obj)
{
	if (!_obj_hash) {
		return;
	}

	char *key = switch_mprintf("%p", xml);
	switch_core_hash_insert(_obj_hash, key, (void *) obj);
	switch_safe_free(key);
}

FSXML *FSXML::FindObjectInHash(switch_xml_t xml)
{
	if (!_obj_hash) {
		return NULL;
	}

	char *key = switch_mprintf("%p", xml);
	FSXML *obj = (FSXML *)switch_core_hash_find(_obj_hash, key);
	switch_safe_free(key);

	return obj;
}

void FSXML::DeleteObjectInHash(switch_xml_t xml)
{
	if (!_obj_hash) {
		return;
	}

	if (FindObjectInHash(xml)) {
		char *key = switch_mprintf("%p", xml);
		switch_core_hash_delete(_obj_hash, key);
		switch_safe_free(key);
	}
}

void FSXML::DestroyHash()
{
	switch_hash_index_t *hi;
	switch_hash_t *tmp = _obj_hash;

	_obj_hash = NULL; /* NULL this, so nothing else tries to modify the hash during cleanup */

	if (!tmp) {
		return;
	}

	/* First destroy all objects in the hash */
	for (hi = switch_core_hash_first( tmp); hi; hi = switch_core_hash_next(&hi)) {
		const void *var = NULL;
		void *val = NULL;
		FSXML *obj;

		switch_core_hash_this(hi, &var, NULL, &val);

		if (val) {
			obj = static_cast<FSXML *>(val);
			delete obj;
		}
	}

	/* And finally destroy the hash itself */
	switch_core_hash_destroy(&tmp);
}

JS_XML_FUNCTION_IMPL(GetChild)
{
	if (info.Length() > 0) {
		String::Utf8Value name(info[0]);
		string attr_name, attr_value;
		switch_xml_t xml = NULL;

		/* Check if attribute name/value was provided as well */
		if (info.Length() > 1) {
			String::Utf8Value str(info[1]);
			attr_name = js_safe_str(*str);

			if (info.Length() > 2) {
				String::Utf8Value str2(info[2]);
				attr_value = js_safe_str(*str2);
			}
		}

		/* Find the XML child */
		if (*name && !attr_name.length()) {
			xml = switch_xml_child(_xml, *name);
		} else if (*name) {
			xml = switch_xml_find_child(_xml, *name, attr_name.c_str(), attr_value.c_str());
		}

		/* Return the JS object */
		if (xml) {
			Handle<Value> jsObj = GetJSObjFromXMLObj(xml, info);

			if (jsObj.IsEmpty()) {
				info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Failed!"));
			} else {
				info.GetReturnValue().Set(jsObj);
			}
		} else {
			info.GetReturnValue().Set(Null(info.GetIsolate()));
		}
	} else {
		info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Invalid arguments"));
	}
}

JS_XML_FUNCTION_IMPL(AddChild)
{
	if (info.Length() > 0) {
		String::Utf8Value name(info[0]);
		switch_xml_t xml;
		int offset = 0;

		if (info.Length() > 1) {
			offset = info[1]->Int32Value();
		}

		/* Add new child */
		xml = switch_xml_add_child_d(_xml, js_safe_str(*name), offset);

		/* Return the JS object */
		if (xml) {
			Handle<Value> jsObj = GetJSObjFromXMLObj(xml, info);

			if (jsObj.IsEmpty()) {
				info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Failed!"));
			} else {
				info.GetReturnValue().Set(jsObj);
			}
		} else {
			info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "XML error"));
		}
	} else {
		info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Invalid arguments"));
	}
}

JS_XML_FUNCTION_IMPL(Next)
{
	switch_xml_t xml;

	/* Get next */
	xml = switch_xml_next(_xml);

	/* Return the JS object */
	if (xml) {
		Handle<Value> jsObj = GetJSObjFromXMLObj(xml, info);

		if (jsObj.IsEmpty()) {
			info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Failed!"));
		} else {
			info.GetReturnValue().Set(jsObj);
		}
	} else {
		info.GetReturnValue().Set(Null(info.GetIsolate()));
	}
}

JS_XML_FUNCTION_IMPL(GetAttribute)
{
	if (info.Length() > 0) {
		String::Utf8Value name(info[0]);
		info.GetReturnValue().Set(String::NewFromUtf8(info.GetIsolate(), switch_xml_attr_soft(_xml, js_safe_str(*name))));
	} else {
		info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Invalid arguments"));
	}
}

JS_XML_FUNCTION_IMPL(SetAttribute)
{
	if (info.Length() > 0) {
		String::Utf8Value name(info[0]);
		string val;

		if (info.Length() > 1) {
			String::Utf8Value str(info[1]);
			val = js_safe_str(*str);
		}

		if (switch_xml_set_attr_d(_xml, js_safe_str(*name), val.c_str())) {
			info.GetReturnValue().Set(true);
		} else {
			info.GetReturnValue().Set(false);
		}
	} else {
		info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "Invalid arguments"));
	}
}

JS_XML_IMPL_STATIC(Remove)
{
	JS_CHECK_SCRIPT_STATE();

	FSXML *obj = JSBase::GetInstance<FSXML>(info);
	
	if (obj) {
		switch_xml_remove(obj->_xml);
		if (!obj->_rootObject) {
			/* If this is the root node, make sure we don't try to free _xml again */
			obj->_xml = NULL;
		}
		delete obj;
	} else {
		String::Utf8Value str(info.Holder());
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "No valid internal data available for %s when calling FSXML::Remove()\n", *str ? *str : "[unknown]");
	}
}

JS_XML_FUNCTION_IMPL(Copy)
{
	FSXML *obj;
	switch_xml_t xml;

	/* Create a new xml object from the existing. The new object will be a new root object */
	xml = switch_xml_dup(_xml);

	if (xml && (obj = new FSXML(info))) {
		obj->_xml = xml;
		obj->InitRootObject();
		obj->RegisterInstance(info.GetIsolate(), "", true);
		info.GetReturnValue().Set(obj->GetJavaScriptObject());
	} else {
		info.GetIsolate()->ThrowException(String::NewFromUtf8(info.GetIsolate(), "XML error"));
	}
}

JS_XML_FUNCTION_IMPL(Serialize)
{
	char *data = switch_xml_toxml(_xml, SWITCH_FALSE);
	info.GetReturnValue().Set(String::NewFromUtf8(info.GetIsolate(), js_safe_str(data)));
	switch_safe_free(data);
}

JS_XML_GET_PROPERTY_IMPL(GetNameProperty)
{
	const char *data = switch_xml_name(_xml);
	info.GetReturnValue().Set(String::NewFromUtf8(info.GetIsolate(), js_safe_str(data)));
}

JS_XML_GET_PROPERTY_IMPL(GetDataProperty)
{
	const char *data = switch_xml_txt(_xml);
	info.GetReturnValue().Set(String::NewFromUtf8(info.GetIsolate(), js_safe_str(data)));
}

JS_XML_SET_PROPERTY_IMPL(SetDataProperty)
{
	String::Utf8Value str(value);
	switch_xml_set_txt_d(_xml, js_safe_str(*str));
}

JS_XML_GET_PROPERTY_IMPL(GetErrorProperty)
{
	const char *data = switch_xml_error(_xml);
	info.GetReturnValue().Set(String::NewFromUtf8(info.GetIsolate(), js_safe_str(data)));
}

static const js_function_t xml_methods[] = {
	{"getChild", FSXML::GetChild},
	{"addChild", FSXML::AddChild},
	{"next", FSXML::Next},
	{"getAttribute", FSXML::GetAttribute},
	{"setAttribute", FSXML::SetAttribute},
	{"remove", FSXML::Remove},
	{"copy", FSXML::Copy},
	{"serialize", FSXML::Serialize},
	{0}
};

static const js_property_t xml_props[] = {
	{"name", FSXML::GetNameProperty, JSBase::DefaultSetProperty},
	{"data", FSXML::GetDataProperty, FSXML::SetDataProperty},
	{"error", FSXML::GetErrorProperty, JSBase::DefaultSetProperty},
	{0}
};

static const js_class_definition_t xml_desc = {
	js_class_name,
	FSXML::Construct,
	xml_methods,
	xml_props
};

static switch_status_t xml_load(const v8::FunctionCallbackInfo<Value>& info)
{
	JSBase::Register(info.GetIsolate(), &xml_desc);
	return SWITCH_STATUS_SUCCESS;
}

static const v8_mod_interface_t xml_module_interface = {
	/*.name = */ js_class_name,
	/*.js_mod_load */ xml_load
};

const v8_mod_interface_t *FSXML::GetModuleInterface()
{
	return &xml_module_interface;
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
