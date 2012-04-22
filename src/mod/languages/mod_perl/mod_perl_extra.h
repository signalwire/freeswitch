#ifndef MOD_PERL_EXTRA
#define MOD_PERL_EXTRA
SWITCH_BEGIN_EXTERN_C void mod_perl_conjure_event(PerlInterpreter * my_perl, switch_event_t *event, const char *name);
void mod_perl_conjure_stream(PerlInterpreter * my_perl, switch_stream_handle_t *stream, const char *name);

SWITCH_END_EXTERN_C
#endif
