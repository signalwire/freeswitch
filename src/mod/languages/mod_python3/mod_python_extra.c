SWITCH_BEGIN_EXTERN_C PyObject * mod_python_conjure_event(switch_event_t *event)
{
	PyObject *obj;
	Event *result = new Event(event, 0);

	obj = SWIG_NewPointerObj(SWIG_as_voidptr(result), SWIGTYPE_p_Event, SWIG_POINTER_OWN);

	return obj;
}


PyObject *mod_python_conjure_stream(switch_stream_handle_t *stream)
{
	PyObject *obj;
	Stream *result = new Stream(stream);

	obj = SWIG_NewPointerObj(SWIG_as_voidptr(result), SWIGTYPE_p_Stream, SWIG_POINTER_OWN);

	return obj;
}

PyObject *mod_python_conjure_session(PyObject * module, switch_core_session_t *session)
{
	PyObject *obj;

	PYTHON::Session * result = new PYTHON::Session(session);
	obj = SWIG_NewPointerObj(SWIG_as_voidptr(result), SWIGTYPE_p_PYTHON__Session, SWIG_POINTER_OWN);

	result->setPython(module);
	result->setSelf(obj);

#if 0
	if (module && name) {
		PyDict_SetItem(PyModule_GetDict(module), Py_BuildValue("s", name), obj);
		Py_DECREF(obj);
	}
#endif

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
