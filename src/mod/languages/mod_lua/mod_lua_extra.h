#ifndef MOD_LUA_EXTRA
#define MOD_LUA_EXTRA
SWITCH_BEGIN_EXTERN_C

int mod_lua_conjure_event(lua_State *L, switch_event_t *event, const char *name, int destroy_me);
int mod_lua_conjure_stream(lua_State *L, switch_stream_handle_t *stream, const char *name, int destroy_me);

SWITCH_END_EXTERN_C
#endif
