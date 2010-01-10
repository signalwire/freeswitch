include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_expr

mod_LTLIBRARIES = mod_expr.la
mod_expr_la_SOURCES  = mod_expr.c expreval.c exprinit.c exprobj.c exprutil.c exprfunc.c exprmem.c exprpars.c exprval.c
mod_expr_la_CFLAGS   = $(AM_CFLAGS)
mod_expr_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_expr_la_LDFLAGS  = -avoid-version -module -no-undefined -shared -lm
