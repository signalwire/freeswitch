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
 * fsxml.hpp -- JavaScript XML class header
 *
 */

#ifndef FS_XML_H
#define FS_XML_H

#include "mod_v8.h"

/* Macros for easier V8 callback definitions */
#define JS_XML_GET_PROPERTY_DEF(method_name) JS_GET_PROPERTY_DEF(method_name, FSXML)
#define JS_XML_SET_PROPERTY_DEF(method_name) JS_SET_PROPERTY_DEF(method_name, FSXML)
#define JS_XML_FUNCTION_DEF(method_name) JS_FUNCTION_DEF(method_name, FSXML)
#define JS_XML_GET_PROPERTY_IMPL(method_name) JS_GET_PROPERTY_IMPL(method_name, FSXML)
#define JS_XML_SET_PROPERTY_IMPL(method_name) JS_SET_PROPERTY_IMPL(method_name, FSXML)
#define JS_XML_FUNCTION_IMPL(method_name) JS_FUNCTION_IMPL(method_name, FSXML)
#define JS_XML_IMPL_STATIC(method_name) JS_FUNCTION_IMPL_STATIC(method_name, FSXML)

class FSXML : public JSBase
{
private:
	switch_xml_t _xml;
	v8::Persistent<v8::Object> _rootJSObject; /* Always keep a reference to the root, so JS doesn't try to clean it up in GC */
	FSXML *_rootObject;
	switch_hash_t *_obj_hash;
	switch_memory_pool_t *_pool;

	void Init();
	void InitRootObject();
	v8::Handle<v8::Value> GetJSObjFromXMLObj(const switch_xml_t xml, const v8::FunctionCallbackInfo<v8::Value>& info);
	void StoreObjectInHash(switch_xml_t xml, FSXML *obj);
	FSXML *FindObjectInHash(switch_xml_t xml);
	void DeleteObjectInHash(switch_xml_t xml);
	void DestroyHash();
public:
	FSXML(JSMain *owner) : JSBase(owner) { Init(); }
	FSXML(const v8::FunctionCallbackInfo<v8::Value>& info) : JSBase(info) { Init(); }
	virtual ~FSXML(void);
	virtual std::string GetJSClassName();

	static const v8_mod_interface_t *GetModuleInterface();

	/* Methods available from JavaScript */
	static void *Construct(const v8::FunctionCallbackInfo<v8::Value>& info);
	JS_XML_FUNCTION_DEF(GetChild);
	JS_XML_FUNCTION_DEF(AddChild);
	JS_XML_FUNCTION_DEF(Next);
	JS_XML_FUNCTION_DEF(GetAttribute);
	JS_XML_FUNCTION_DEF(SetAttribute);
	JS_FUNCTION_DEF_STATIC(Remove);
	JS_XML_FUNCTION_DEF(Copy);
	JS_XML_FUNCTION_DEF(Serialize);
	JS_XML_GET_PROPERTY_DEF(GetNameProperty);
	JS_XML_GET_PROPERTY_DEF(GetDataProperty);
	JS_XML_SET_PROPERTY_DEF(SetDataProperty);
	JS_XML_GET_PROPERTY_DEF(GetErrorProperty);
};

#endif /* FS_XML_H */

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
