SWITCH_BEGIN_EXTERN_C void mod_perl_conjure_event(PerlInterpreter * my_perl, switch_event_t *event, const char *name)
{
	Event *result = 0;
	SV *sv;
	PERL_SET_CONTEXT(my_perl);
	sv = sv_2mortal(get_sv(name, TRUE));
	result = (Event *) new Event(event);
	SWIG_Perl_MakePtr(sv, result, SWIGTYPE_p_Event, SWIG_OWNER | SWIG_SHADOW);
}


void mod_perl_conjure_stream(PerlInterpreter * my_perl, switch_stream_handle_t *stream, const char *name)
{
	Stream *result = 0;
	SV *sv;
	PERL_SET_CONTEXT(my_perl);
	sv = sv_2mortal(get_sv(name, TRUE));
	result = (Stream *) new Stream(stream);
	SWIG_Perl_MakePtr(sv, result, SWIGTYPE_p_Stream, SWIG_OWNER | SWIG_SHADOW);
}



SWITCH_END_EXTERN_C
