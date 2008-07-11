#ifndef MOD_PYTHON_EXTRA
#define MOD_PYTHON_EXTRA
SWITCH_BEGIN_EXTERN_C

PyObject *mod_python_conjure_event(PyObject *module, switch_event_t *event, const char *name);
PyObject *mod_python_conjure_stream(PyObject *module, switch_stream_handle_t *stream, const char *name);
PyObject *mod_python_conjure_session(PyObject *module, switch_core_session_t *session, const char *name);
PyObject *mod_python_conjure_DTMF(char digit, int32_t duration);

SWITCH_END_EXTERN_C
#endif
