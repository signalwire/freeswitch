SWITCH_BEGIN_EXTERN_C 

PyObject *mod_python_conjure_event(PyObject *module, switch_event_t *event, const char *name)
{
	PyObject *obj;
	Event *result = new Event(event, 0);

	obj = SWIG_NewPointerObj(SWIG_as_voidptr(result), SWIGTYPE_p_Event, SWIG_POINTER_OWN );
	if (module && name) {
		PyDict_SetItem(PyModule_GetDict(module), Py_BuildValue("s", name), obj);
	}

	return obj;
}


PyObject *mod_python_conjure_stream(PyObject *module, switch_stream_handle_t *stream, const char *name)
{
	PyObject *obj;
	Stream *result = new Stream(stream);

	obj = SWIG_NewPointerObj(SWIG_as_voidptr(result), SWIGTYPE_p_Stream, SWIG_POINTER_OWN );

	if (module && name) {
		PyDict_SetItem(PyModule_GetDict(module), Py_BuildValue("s", name), obj);
	}

	return obj;
}

PyObject *mod_python_conjure_session(PyObject *module, switch_core_session_t *session, const char *name)
{
	PyObject *obj;
	
	PYTHON::Session *result = new PYTHON::Session(session);
	obj = SWIG_NewPointerObj(SWIG_as_voidptr(result), SWIGTYPE_p_PYTHON__Session, SWIG_POINTER_OWN);
	
	result->setPython(module);
	result->setSelf(obj);

	if (module && name) {
		PyDict_SetItem(PyModule_GetDict(module), Py_BuildValue("s", name), obj);
		Py_DECREF(obj);
	}
	
	return obj;
	
}

PyObject *mod_python_conjure_DTMF(char digit, int32_t duration)
{
	PyObject *obj;
	
	DTMF *result = new DTMF(digit, duration);
	obj = SWIG_NewPointerObj(SWIG_as_voidptr(result), SWIGTYPE_p_DTMF, SWIG_POINTER_OWN);
	
	return obj;
	
}



SWITCH_END_EXTERN_C
