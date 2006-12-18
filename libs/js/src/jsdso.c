/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
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
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/* ATTENTION: This is an OSSP js extension to the Mozilla JavaScript engine.
   It was implemented by Ralf S. Engelschall <rse@engelschall.com> for OSSP. */

#if defined(OSSP) && defined(JS_HAS_DSO_OBJECT) && JS_HAS_DSO_OBJECT

/* own headers (part 1/2) */
#include "jsstddef.h"

/* system headers */
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>

/* own headers (part 2/2) */
#include "jstypes.h"
#include "jsapi.h"
#include "jsatom.h"
#include "jscntxt.h"
#include "jsconfig.h"
#include "jsobj.h"
#include "jsdso.h"

/* process local storage of DSO handles */
static void *dso_handle[10000];

/* type of the DSO load/unload functions */
typedef JSBool (*dso_func_t)(JSContext *cx);

/* ISO-C type coersion trick */
typedef union { void *vp; dso_func_t fp; } dso_func_ptr_t;

/* public C API function: DSO loading */
JS_PUBLIC_API(JSBool)
JS_DSOLoad(JSContext *cx, int *idp, const char *filename)
{
    int id;
    void *handle;
    dso_func_ptr_t func;
    JSBool rc;

    /* determine next free DSO handle slot */
    for (id = 0; dso_handle[id] != NULL && id < sizeof(dso_handle)/sizeof(dso_handle[0]); id++)
        ;
    if (id == sizeof(dso_handle)/sizeof(dso_handle[0])) {
        JS_ReportError(cx, "no more free DSO handle slots available");
        return JS_FALSE;
    }

    /* load DSO into process */
    if ((handle = dlopen(filename, RTLD_NOW)) == NULL) {
        JS_ReportError(cx, "unable to load DSO \"%s\": %s", filename, dlerror());
        return JS_FALSE;
    }

    /* resolve "js_DSO_load" function, call it and insist on a true return */
    if ((func.vp = dlsym(handle, "js_DSO_load")) == NULL) {
        JS_ReportError(cx, "unable to resolve symbol \"js_DSO_load\" in DSO \"%s\"", filename);
        dlclose(handle);
        return JS_FALSE;
    }
    rc = func.fp(cx);
    if (!rc) {
        JS_ReportError(cx, "function \"js_DSO_load\" in DSO \"%s\" returned error", filename);
        dlclose(handle);
        return JS_FALSE;
    }

    /* store DSO handle into process local storage */
    dso_handle[id] = handle;

    /* return DSO id to caller */
    if (idp != NULL)
        *idp = id;

    return JS_TRUE;
}

/* public C API function: DSO unloading */
JS_PUBLIC_API(JSBool)
JS_DSOUnload(JSContext *cx, int id)
{
    int idx;
    void *handle;
    dso_func_ptr_t func;
    JSBool rc;

    /* sanity check DSO id */
    if (id < 0 || id >= sizeof(dso_handle)/sizeof(dso_handle[0])) {
        JS_ReportError(cx, "invalid argument: DSO id #%d out of range", id);
        return JS_FALSE;
    }

    /* determine DSO handle */
    if ((handle = dso_handle[id]) == NULL) {
        JS_ReportError(cx, "invalid argument: DSO id #%d currently unused", id);
        return JS_FALSE;
    }

    /* resolve "js_DSO_unload" function and (if available only)
       call it and insist on a true return */
    if ((func.vp = dlsym(handle, "js_DSO_unload")) != NULL) {
        rc = func.fp(cx);
        if (!rc) {
            JS_ReportError(cx, "function \"js_DSO_unload\" in DSO with id #%d returned error", idx);
            return JS_FALSE;
        }
    }

    /* unload DSO from process */
    dlclose(handle);

    /* free DSO handle slot */
    dso_handle[id] = NULL;

    return JS_TRUE;
}

/* global JavaScript language DSO object method: id = DSO.load("filename.so") */
static JSBool dso_load(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSString *filename;
    char *c_filename;
    int id;

    /* usage sanity checks */
    if (argc == 0) {
        JS_ReportError(cx, "usage: id = DSO.load(filename)");
        return JS_FALSE;
    }
    if (argc != 1) {
        JS_ReportError(cx, "invalid number of arguments: %d received, %d expected", argc, 1);
        return JS_FALSE;
    }

    /* determine filename */
    if ((filename = js_ValueToString(cx, argv[0])) == NULL) {
        JS_ReportError(cx, "invalid argument");
        return JS_FALSE;
    }
    if ((c_filename = JS_GetStringBytes(filename)) == NULL) {
        JS_ReportError(cx, "invalid argument");
        return JS_FALSE;
    }

    /* load DSO */
    if (!JS_DSOLoad(cx, &id, c_filename))
        return JS_FALSE;

    /* return DSO handle id */
    *rval = INT_TO_JSVAL(id);

    return JS_TRUE;
}

/* global JavaScript language DSO object method: DSO.unload(id) */
static JSBool dso_unload(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    int id;
    JSBool rc;

    /* usage sanity checks */
    if (argc == 0) {
        JS_ReportError(cx, "usage: DSO.unload(id)");
        return JS_FALSE;
    }
    if (argc != 1) {
        JS_ReportError(cx, "invalid number of arguments: %d received, %d expected", argc, 1);
        return JS_FALSE;
    }

    /* determine DSO id */
    id = JSVAL_TO_INT(argv[0]);

    /* unload DSO */
    if (!JS_DSOUnload(cx, id))
        return JS_FALSE;

    return JS_TRUE;
}

/* JavaScript DSO class method definitions */
static JSFunctionSpec dso_methods[] = {
    { "load",   dso_load,   1, 0, 0 },
    { "unload", dso_unload, 1, 0, 0 },
    { NULL,     NULL,       0, 0, 0 }
};

/* JavaScript DSO class definition */
static JSClass dso_class = {
    "DSO",
    0,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub,
    JS_FinalizeStub,
    JSCLASS_NO_OPTIONAL_MEMBERS
};

/* JavaScript DSO class global initializer */
JSObject *js_InitDSOClass(JSContext *cx, JSObject *obj)
{
    JSObject *DSO;

    if ((DSO = JS_DefineObject(cx, obj, "DSO", &dso_class, NULL, 0)) == NULL)
        return NULL;
    if (!JS_DefineFunctions(cx, DSO, dso_methods))
        return NULL;
    return DSO;
}

#endif /* JS_HAS_DSO_OBJECT */

