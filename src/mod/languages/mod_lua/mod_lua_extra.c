using namespace LUA;

SWITCH_BEGIN_EXTERN_C

void mod_lua_conjure_event(lua_State * L, switch_event_t *event, const char *name, int destroy_me)
{
	Event *result = new Event(event);
	SWIG_NewPointerObj(L, result, SWIGTYPE_p_Event, destroy_me);
	lua_setglobal(L, name);
}


void mod_lua_conjure_stream(lua_State * L, switch_stream_handle_t *stream, const char *name, int destroy_me)
{
	Stream *result = new Stream(stream);
	SWIG_NewPointerObj(L, result, SWIGTYPE_p_Stream, destroy_me);
	lua_setglobal(L, name);
}


void mod_lua_conjure_session(lua_State * L, switch_core_session_t *session, const char *name, int destroy_me)
{
	Session *result = new Session(session);

	SWIG_NewPointerObj(L, result, SWIGTYPE_p_LUA__Session, destroy_me);
	result->setLUA(L);
	lua_setglobal(L, name);
}



SWITCH_END_EXTERN_C
