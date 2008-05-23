/* ====================================================================
 * Copyright (c) 1995-2000 The Apache Group.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the Apache Group
 *    for use in the Apache HTTP server project (http://www.apache.org/)."
 *
 * 4. The names "Apache Server" and "Apache Group" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    apache@apache.org.
 *
 * 5. Products derived from this software may not be called "Apache"
 *    nor may "Apache" appear in their names without prior written
 *    permission of the Apache Group.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the Apache Group
 *    for use in the Apache HTTP server project (http://www.apache.org/)."
 *
 * THIS SOFTWARE IS PROVIDED BY THE APACHE GROUP ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE APACHE GROUP OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * ====================================================================
 *
 * This software consists of voluntary contributions made by many
 * individuals on behalf of the Apache Group and was originally based
 * on public domain software written at the National Center for
 * Supercomputing Applications, University of Illinois, Urbana-Champaign.
 * For more information on the Apache Group and the Apache HTTP server
 * project, please see <http://www.apache.org/>.
 *
 */

/* 
 * mod_gzip.c
 *
 * Apache gzip compression module.
 *
 * This module adds 'on the fly' compression of HTTP content to
 * any Apache Web Server. It uses the IETF Content-encoding standard(s).
 *
 * It will compress both static files and the output of any CGI
 * program inclding shell scripts, perl scripts, executables,
 * PHP used as CGI, etc.
 *
 * There is NO client-side software required for using this module
 * other than any fully HTTP 1.1 compliant user agent.
 *
 * Any fully HTTP 1.1 compliant user agent will be able to receive and
 * automatically decode the compressed content.
 *
 * All fully HTTP 1.1 compliant user agents that are capable of receiving
 * gzip encoded data will indicate their ability to do so by adding the
 * standard "Accept-Encoding: gzip" field to the inbound request header.
 * 
 * This module may be compiled as a stand-alone external 'plug-in'
 * or be compiled into the Apache core server as a 'built-in' module.
 *
 * Sponsor: Remote Communications, Inc. http://www.RemoteCommunications.com/
 * Authors: Konstantin Balashov, Alex Kosobrukhov and Kevin Kiley.
 * Contact: info@RemoteCommunications.com
 *
 * Initial public release date: 13-Oct-2000
 *
 * Miscellaneous release notes:
 *
 * THIS IS A COMPLETELY SELF-CONTAINED MODULE. MOD_GZIP.C IS THE
 * ONY SOURCE CODE FILE THERE IS AND THERE ARE NO MODULE SPECIFIC
 * HEADER FILES OR THE NEED FOR ANY 3RD PARTY COMPRESSION LIBRARIES.
 * ALL OF THE COMPRESSION CODE NEEDED BY THIS MODULE IS CONTAINED
 * WITHIN THIS SINGLE SOURCE FILE.
 *
 * Many standard compression libraries are not designed or optimized
 * for use as real-time compression codecs nor are they guaranteed
 * to be 'thread-safe'. The internal compression code used by mod_gzip
 * is all of those things. It is a highly-optimized and thread-safe
 * implementation of the standard LZ77 + Huffman compression
 * technique that has come to be known as GZIP.
 *
 * MOD_GZIP LOG FORMATS...
 *
 * mod_gzip makes a number of statistical items for each transaction
 * available through the use of Apache's 'LogFormat' directives which
 * can be specified in the httpd.conf Apache config file
 *
 * mod_gzip uses the standard Apache NOTES interface to allow compression
 * information to be added to the Apache Web Server log files.
 *
 * Standard NOTES may be added to Apache logs using the following syntax
 * in any LogFormat directive...
 * * %...{Foobar}n:  The contents of note "Foobar" from another module.
 *
 * Additional notes about logging compression information...
 *
 * The Apache LogFormat directive is unable to actually display
 * the 'percent' symbol since it is used exclusively as a 'pickup'
 * character in the formatting string and cannot be 'escaped' so
 * all logging of compression ratios cannot use the PERCENT symbol.
 * Use ASCII 'pct.' designation instead for all PERCENTAGE values.
 *
 * Example: This will display the compression ratio percentage along
 * with the standard CLF ( Common Log Format ) information...
 *
 * Available 'mod_gzip' compression information 'notes'...
 *
 * %{mod_gzip_result}n - A short 'result' message. Could be OK or DECLINED, etc.
 * %{mod_gzip_input_size}n - The size ( in bytes ) of the requested object.
 * %{mod_gzip_output_size}n - The size ( in bytes ) of the compressed version.
 * %{mod_gzip_compression_ration}n - The compression rate achieved.
 *
 *  LogFormat "%h %l %u %t \"%r\" %>s %b mod_gzip: %{mod_gzip_compression_ratio}npct." common_with_mod_gzip_info1
 *  LogFormat "%h %l %u %t \"%r\" %>s %b mod_gzip: %{mod_gzip_result}n In:%{mod_gzip_input_size}n Out:%{mod_gzip_output_size}n:%{mod_gzip_compression_ratio}npct." common_with_mod_gzip_info2
 *
 * If you create your own custom 'LogFormat' lines don't forget that
 * the entire LogFormat line must be encased in quote marks or you
 * won't get the right results. The visible effect of there not being
 * and end-quote on a LogFormat line is that the NAME you are choosing
 * for the LogFormat line is the only thing that will appear in the
 * log file that tries to use the unbalanced line.
 *
 * Also... when using the %{mod_gzip_xxxxx}n note references in your
 * LogFormat line don't forget to add the lowercase letter 'n' after
 * the closing bracket to indicate that this is a module 'note' value.
 *
 * Once a LogFormat directive has been added to your httpd.conf file
 * which displays whatever level of compression information desired
 * simply use the 'name' associated with that LogFormat line in
 * the 'CustomLog' directive for 'access.log'.
 *
 * Example: The line below simply changes the default access.log format
 * for Apache to the special mog_gzip information record defined above...
 * CustomLog logs/access.log common
 *
 *  CustomLog logs/access.log common_with_mod_gzip_info2
 *
 * Using the 'common_with_mod_gzip_info1' LogFormat line for Apache's
 * normal access.log file produces the following results in the access.log
 * file when a gigantic 679,188 byte online CD music collection HTML
 * document called 'music.htm' is requested and the Server delivers the
 * file via mod_gzip compressed 93 percent down to only 48,951 bytes...
 *
 * 216.20.10.1 [12/Oct...] "GET /music.htm HTTP/1.1" 200 48951 mod_gzip: 93pct.
 *
 * The line below shows what will appear in the Apache access.log file
 * if the more detailed 'common_with_mod_gzip_info2' LogFormat line is used.
 * The line has been intentionally 'wrapped' for better display below
 * but would normally appear as a single line entry in access.log.
 *
 * 216.20.10.1 [12/Oct...] "GET /music.htm HTTP/1.1" 200 48951
 *                          mod_gzip: OK In:679188 Out:48951:93pct.
 *
 * The 'OK' result string shows that the compression was successful.
 * The 'In:' value is the size (in bytes) of the requested file and
 * the 'Out:' value is the size (in bytes) after compression followed
 * by a colon and a number showing that the document was compressed
 * 93 percent before being returned to the user.
 *
 * Please NOTE that if you add any ASCII strings to your LogFormat
 * string then they will appear in your log file regardless of
 * whether this module was actually 'called' to process the
 * transaction or not. If the module was not called to handle the
 * transaction then the places where the statistical information
 * associated with the 'NOTES' references would normally appear
 * will be filled in with 'dashes' to denote 'no value available'.
 *
 * MOD_GZIP RUNTIME DEBUG...
 *
 * If you set your default Apache logging level to 'LogLevel debug'
 * in your httpd.conf file then this module will add certain
 * diagnostic debug messages to your error log for each and every
 * transaction that is actually passed to the module.
 *
 * If Apache does not 'call' this module to handle a particular
 * transaction then no special log information will appear in
 * your error log(s) for that transaction.
 *
 * MOD_GZIP CONFIGURATION DIRECTIVES...
 *
 * The section that follows is a sample mod_gzip configuration
 * section that will provide basic compression of all static
 * TEXT and HTML files as well as dynamic compression of most
 * standard CGI including Shell scripts, Perl, PHP, etc.
 *
 * The configuration directives themselves are documented in more
 * detail in the README and INSTALL files that accompany this module.
 *
 * You should be able to simply 'cut and paste' the follwing section
 * directly into the BOTTOM of your current httpd.conf Apache
 * configuration file and be able to start using mod_gzip immediately.
 *

#
# MOD_GZIP Configuration Directives
#
# All you should have to do to get up and running using
# mod_gzip with some basic STATIC and DYNAMIC compression
# capabilites is copy the mod_gzip dynamic library to your
# ../modules directory and then add this entire example
# configuration section to the BOTTOM of your httpd.conf file.
#
# Add this entire section including all lines down to where
# it says '# End of MOD_GZIP Configuration Directives'.
#
# The LoadModule command is included here for clarity
# but you may want to move it the the BOTTOM of your
# current LoadModule list in httpd.conf.
#
# Change the 'mod_gzip_temp_dir' to the name of a directory
# on your machine where temporary workfiles can be created
# and destroyed. This directory MUST be readable/writable
# by the Server itself while it is running. If the directory
# does not exist you must create it yourself with the right
# permissions before running the Server.
#
# If no 'mod_gzip_temp_dir' is specified then the default location
# for temporary workfiles will be 'ServerRoot' directory.
#
# The special mod_gzip log formats are, of course, optional.
#
# You must, of course, load the right module name for your OS
# so make sure the correct 'LoadModule' command is uncommented
# directly below...

# Load Win32 module...
LoadModule gzip_module modules/ApacheModuleGzip.dll

# Load UNIX module...
# LoadModule gzip_module modules/mod_gzip.so

LogFormat "%h %l %u %t \"%r\" %>s %b mod_gzip: %{mod_gzip_compression_ratio}npct." common_with_mod_gzip_info1
LogFormat "%h %l %u %t \"%r\" %>s %b mod_gzip: %{mod_gzip_result}n In:%{mod_gzip_input_size}n Out:%{mod_gzip_output_size}n:%{mod_gzip_compression_ratio}npct." common_with_mod_gzip_info2

# NOTE: This 'CustomLog' directive shows how to set your access.log file
# to use the mod_gzip format but please remember that for every 'CustomLog'
# directive that Apache finds in httpd.conf there will be corresponding
# line of output in the access.log file. If you only want ONE line of
# results in access.log for each transaction then be sure to comment out
# any other 'CustomLog' directives so that this is the only one.

CustomLog logs/access.log common_with_mod_gzip_info2

# Runtime control directives...

mod_gzip_on                 Yes
mod_gzip_do_cgi             Yes
mod_gzip_do_static_files    Yes
mod_gzip_minimum_file_size  300
mod_gzip_maximum_inmem_size 60000
mod_gzip_keep_workfiles     No
mod_gzip_temp_dir           "C:/Program Files/Apache Group/Apache/temp"

# Item lists...
#
# Item names can be any one of the following...
#
# cgi-script - A valid 'handler' name
# text/*     - A valid MIME type name ( '*' wildcard allowed )
# .phtml     - A valid file type extension

# Dynamic items...
#
# NOTE: FOR NOW ALL DYNAMIC ITEMS SHOULD BE
# DECLARED BEFORE ANY STATIC ITEMS TO PREVENT
# PICKUP CONFLICTS. IF YOU USE !cgi-script
# BE SURE IT IS DECLARED BEFORE ANY text/*
# MIME TYPE ENTRIES.
#
# The items listed here are the types of dynamic
# output that will be compressed...
#
# Dynamic items MUST have the "!" BANG character
# on the front of the item name.
#
mod_gzip_item_include !cgi-script
mod_gzip_item_include !.php
mod_gzip_item_include !.php3
mod_gzip_item_include !.phtml

# Static items...
#
# The items listed here are the types of static
# files that will be compressed...
#
# NOTE: FOR NOW ALL STATIC INCLUDES MUST
# COME AFTER DYNAMIC INCLUDES TO PREVENT
# PICKUP CONFLICTS
#
mod_gzip_item_include text/*

# Uncomment this line to compress graphics
# when graphics compression is allowed...
#mod_gzip_item_include image/*


# Exclusions... MIME types and FILE types...
#
# The items listed here will be EXCLUDED from
# any attempt to apply compression...
#
mod_gzip_item_exclude .js
mod_gzip_item_exclude .css

# Exclusions... HTTP support levels...
#
# By specifying a certain minimum level of HTTP support
# certain older user agents ( browsers ) can be
# automatically excluded from receiving compressed data.
#
# The item value should be in the same HTTP numeric format
# that Apache uses to designate HTTP version levels.
#
# 1001 = HTTP/1.1
#
# So 'mod_gzip_min_http 1001' means that a requesting
# user agent ( browser ) must report a minimum HTTP support
# level of 1.1 or it will not receive any compressed data.
#
mod_gzip_min_http 1001

# Debugging...
#
# If your Apache 'LogLevel' is set to 'debug' then
# mod_gzip will add some diagnostic and compression
# information to your error.log file for each request
# that is processed by mod_gzip.
#
# LogLevel debug

# End of MOD_GZIP Configuration Directives

 * End of inline comments
 */

#include <stdlib.h>
/*
 * Apache headers...
 */

#define CORE_PRIVATE

#include "httpd.h"
#include "http_config.h"
#include "http_core.h"
#include "http_log.h"
#include "http_main.h"
#include "http_protocol.h"
#include "util_script.h"

/*
 * Add this header for ap_server_root[ MAX_STRING_LEN ] global...
 *
 * #include "http_conf_globals.h"
 *
 * ...or just include what we need from http_conf_globals.h
 * since that is, in fact, only 1 item at this time.
 */
extern API_VAR_EXPORT char ap_server_root[ MAX_STRING_LEN ];

/*
 * Add this header to get 'ap_update_mtime()' prototype...
 *
 * #include "http_request.h"
 *
 * ...or just include what we need from http_request.h since
 * that is, in fact, only 1 item at this time.
 */
extern API_EXPORT(time_t)
ap_update_mtime(request_rec *r, time_t dependency_mtime);

/*
 * Version information...
 *
 * Since this product is 'married' to the ASF Apache Web Server
 * the version numbers should always 'match' the changing
 * version numbers of Apache itself so users can be sure
 * they have the 'right' module. This allows us to move the
 * version numbers either backwards or forwards in case issues
 * arise which require specific versions of mod_gzip for
 * specific versions of Apache.
 *
 * The original code was first tested against the Apache 1.3.14
 * release but should be compatible with the entire 1.3.x series.
 * If earlier 1.3.x versions of Apache required special versions
 * then the mod_gzip version number will still match the Apache
 * version number ( As in... mod_gzip v1.3.12.1, if needed ).
 *
 * If a special version is required for Apache 2.0 then the
 * version number(s) will change to match release numbers in
 * that series. ( As in... mod_gzip v 2.0.1.1, etc. ).
 *
 * The first 3 numbers of the version are always the equivalent
 * Apache release numbers. The fourth number is always the actual
 * mod_gzip 'build' number for that version of Apache.
 */

char mod_gzip_version[] = "1.3.14.5"; /* Global version string */

/*
 * Declare the NAME by which this module will be known.
 * This is the NAME that will be used in LoadModule command(s).
 */
extern module MODULE_VAR_EXPORT gzip_module;

/*
 * Allow this module to 'read' config information from
 * ( and interact with ) the 'real' mod_cgi module...
 */
extern module cgi_module;

/*
 * Some compile-time code inclusion switches...
 */

/*
 * Turn MOD_GZIP_ALLOWS_INTERNAL_COMMANDS switch ON to allow
 * information requests to be sent via any standard browser.
 */

#define MOD_GZIP_ALLOWS_INTERNAL_COMMANDS

/*
 * Turn MOD_GZIP_USES_APACHE_LOGS switch ON to include the
 * code that can update Apache logs with compression information.
 */

#define MOD_GZIP_USES_APACHE_LOGS

 /*
  * Turn MOD_GZIP_USES_AP_SEND_MMAP switch ON to use the
  * ap_send_mmap() method for transmitting data. If this
  * switch is OFF then the default is to use ap_rwrite().
  * This might need to be platform specific at some point.
  */

#define MOD_GZIP_USES_AP_SEND_MMAP

/*
 * Turn MOD_GZIP_DEBUG1 switch ON for verbose diags.
 * This is normally OFF by default and should only be
 * used for diagnosing problems. The log output is
 * VERY detailed and the log files will be HUGE.
 */

/*
#define MOD_GZIP_DEBUG1
*/

/*
 * Some useful instance globals...
 */

#ifndef MOD_GZIP_MAX_PATH_LEN
#define MOD_GZIP_MAX_PATH_LEN 512
#endif

char mod_gzip_temp_dir[ MOD_GZIP_MAX_PATH_LEN + 2 ];

long mod_gzip_iusn = 0; /* Instance Unique Sequence Number */

long mod_gzip_maximum_inmem_size = 60000L;
long mod_gzip_minimum_file_size  = 300L;

#ifdef WIN32
char mod_gzip_dirsep[]="\\"; /* Dir separator is a backslash for Windows */
#else /* !WIN32 */
char mod_gzip_dirsep[]="/";  /* Dir separator is a forward slash for UNIX */
#endif /* WIN32 */

/*
 * The Compressed Object Cache control structure...
 */

#define MOD_GZIP_SEC_ONE_DAY 86400  /* Total seconds in one day */
#define MOD_GZIP_SEC_ONE_HR  3600   /* Total seconds in one hour */

#define MOD_GZIP_DEFAULT_CACHE_SPACE 5
#define MOD_GZIP_DEFAULT_CACHE_MAXEXPIRE MOD_GZIP_SEC_ONE_DAY
#define MOD_GZIP_DEFAULT_CACHE_EXPIRE    MOD_GZIP_SEC_ONE_HR
#define MOD_GZIP_DEFAULT_CACHE_LMFACTOR (0.1)
#define MOD_GZIP_DEFAULT_CACHE_COMPLETION (0.9)

struct mod_gzip_cache_conf {

    const char *root;             /* The location of the cache directory */
    off_t       space;            /* Maximum cache size (in 1024 bytes) */
    char        space_set;
    time_t      maxexpire;        /* Maximum time to keep cached files (secs) */
    char        maxexpire_set;
    time_t      defaultexpire;    /* Default time to keep cached file (secs) */
    char        defaultexpire_set;
    double      lmfactor;         /* Factor for estimating expires date */
    char        lmfactor_set;
    time_t      gcinterval;       /* Garbage collection interval (secs) */
    char        gcinterval_set;
    int         dirlevels;        /* Number of levels of subdirectories */
    char        dirlevels_set;
    int         dirlength;        /* Length of subdirectory names */
    char        dirlength_set;
};

/*
 * The Inclusion/Exclusion map item structure...
 */

#define MOD_GZIP_IMAP_MAXNAMES   256
#define MOD_GZIP_IMAP_MAXNAMELEN 90

#define MOD_GZIP_IMAP_ISMIME     1
#define MOD_GZIP_IMAP_ISEXT      2
#define MOD_GZIP_IMAP_ISHANDLER  3

#define MOD_GZIP_IMAP_STATIC1    9001
#define MOD_GZIP_IMAP_DYNAMIC1   9002
#define MOD_GZIP_IMAP_DECLINED1  9003

typedef struct {

    int  include; /* 1=Include 0=Exclude */
    int  type;    /* _ISMIME, _ISEXT, _ISHANDLER, etc. */
    int  action;  /* _STATIC1, _DYNAMIC1, etc. */

    char name[ MOD_GZIP_IMAP_MAXNAMELEN + 2 ];

} mod_gzip_imap;

/*
 * The primary module configuration record...
 */

typedef struct {

    struct mod_gzip_cache_conf cache; /* Compressed Object Cache control */

    int  req;                /* 1=mod_gzip handles requests 0=No */
    char req_set;            /* Mirrors the 'req' flag */
    int  do_static_files;    /* 1=Yes 0=No */
    int  do_cgi;             /* 1=Yes 0=No */
    int  keep_workfiles;     /* 1=Keep workfiles 0=No */
    int  min_http;           /* Minimum HTTP level ( 1001=HTTP/1.1 ) */
    long minimum_file_size;  /* Minimum size in bytes for compression attempt */
    long maximum_inmem_size; /* Maximum size in bytes for im-memory compress */

    /* Inclusion/Exclusion list(s)... */

    int imap_total_entries;

    mod_gzip_imap imap[ MOD_GZIP_IMAP_MAXNAMES + 1 ];

} mod_gzip_conf;

/*
 * The GZP request control structure...
 */

#define GZIP_FORMAT    (0)
#define DEFLATE_FORMAT (1)

typedef struct _GZP_CONTROL {

    int   decompress;  /* 0=Compress 1=Decompress */

    int   compression_format;  /* GZIP_FORMAT or DEFLATE_FORMAT */

    /* Input control... */

    int   input_ismem;         /* Input source is memory buffer, not file */
    char *input_ismem_ibuf;    /* Pointer to input memory buffer */
    long  input_ismem_ibuflen; /* Total length of input data */

    char  input_filename[ MOD_GZIP_MAX_PATH_LEN + 2 ]; /* Input file name */

    /* Output control... */

    int   output_ismem;         /* Output source is memory buffer, not file */
    char *output_ismem_obuf;    /* Pointer to output memory buffer */
    long  output_ismem_obuflen; /* Maximum length of output data buffer */

    char  output_filename[ MOD_GZIP_MAX_PATH_LEN + 2 ]; /* Output file name */

    /* Results... */

    int   result_code; /* Result code */
    long  bytes_out;   /* Total number of compressed output bytes */

} GZP_CONTROL;

/*
 * Forward prototypes for internal routines...
 */

int gzp_main( GZP_CONTROL *gzp ); /* Primary GZP API entry point */

int mod_gzip_request_handler( request_rec *r );
int mod_gzip_cgi_handler( request_rec *r );
int mod_gzip_static_file_handler( request_rec *r );
int mod_gzip_prepare_for_dynamic_call( request_rec *r );
int mod_gzip_imap_show_items( mod_gzip_conf *mgc );
int mod_gzip_get_action_flag( request_rec *r, mod_gzip_conf *conf );
int mod_gzip_ismatch( char *s1, char *s2, int len1, int haswilds );

FILE *mod_gzip_open_output_file(
request_rec *r,
char *output_filename,
int  *rc
);

int mod_gzip_create_unique_filename(
mod_gzip_conf *mgc,
char *target,
int targetmaxlen
);

int mod_gzip_encode_and_transmit(
request_rec *r,
char        *source,
int          source_is_a_file,
long         input_size,
int          nodecline
);


#ifdef MOD_GZIP_ALLOWS_INTERNAL_COMMANDS

int mod_gzip_send_html_command_response(
request_rec *r, /* Request record */
char *tmp,      /* Response to send */
char *ctype     /* Content type string */
);

#endif /* MOD_GZIP_ALLOWS_INTERNAL_COMMANDS */

/*
 * Module routines...
 */

#ifdef MOD_GZIP_DEBUG1

void mod_gzip_printf( const char *fmt, ... )
{
 int   l;
 FILE *log;

 va_list ap;

 char logname[256];
 char log_line[4096];

 /* Start... */

 /* If UNIX  then mod_gzip_dirsep = '/' Backward slash */
 /* If WIN32 then mod_gzip_dirsep = '\' Forward  slash */

 #ifdef FUTURE_USE
 /*
 For now we need both startup and runtime diags in the same
 log so it all goes to ServerRoot. 'mod_gzip_temp_dir' name
 isn't even valid until late in the startup process so we
 have to write to ServerRoot anyway until temp dir is known.
 */
 if ( strlen(mod_gzip_temp_dir) ) /* Use temp directory ( if specified )... */
   {
    sprintf( logname, "%s%smod_gzip.log", mod_gzip_temp_dir, mod_gzip_dirsep );
   }
 else /* Just use 'ap_server_root' Apache ServerRoot directory... */
   {
    sprintf( logname, "%s%smod_gzip.log", ap_server_root, mod_gzip_dirsep );
   }
 #endif /* FUTURE_USE */

 /* Just use ServerRoot for now... */
 sprintf( logname, "%s%smod_gzip.log", ap_server_root, mod_gzip_dirsep );

 log = fopen( logname,"a" );

 if ( !log ) /* Log file did not open... */
   {
    /* Just turn and burn... */

    return; /* Void return */
   }

 /* Get the variable parameter list... */

 va_start( ap, fmt );

 l = vsprintf(log_line, fmt, ap);

 /* See if we need to add LF... */

 if ( l > 0 )
   {
    if ( log_line[l-1] != '\n' )
      {
       log_line[l]='\n';
       l++;
      }

    log_line[l+1] = 0;
   }

 fprintf( log, "%s", log_line );

 fclose( log );

 va_end(ap); /* End session */

 return; /* Void return */

}/* End of log_d() */

void mod_gzip_hexdump( char *buffer, int buflen )
{
 int i,o1,o2,o3;

 int len1;
 int len2;

 char ch1;
 char ch2;
 char s[40];
 char l1[129];
 char l2[129];
 char l3[300];

 long offset1=0L;

 /* Start... */

 o1=0;
 o2=0;
 o3=0;

 l1[0] = 0;
 l2[0] = 0;
 l3[0] = 0;

 offset1 = 0;

 for ( i=0; i<buflen; i++ )
    {
     ch1 = (char) *buffer++;

     /*------------------------------------------------------------*/
     /* WARNING: UNIX hates anything non-printable. It can mess    */
     /*          up the terminal output by trying to use SLASH     */
     /*          ESCAPE substitutions...                           */
     /*------------------------------------------------------------*/
     /* DOUBLE WARNING!: We MUST mask the per-cent char (37 dec)   */
     /* and the 'backslash' char ( 92 decimal ) or the UNIX        */
     /* STDIO calls could CRASH. They are just brain-dead enough   */
     /* to actually try to respond to these chars in the output    */
     /* stream and convert them to HEX equivalents which could     */
     /* lengthen the output string(s) and CRASH the output buffer. */
     /*------------------------------------------------------------*/

     /* ASTERISK         = ASC 42 */
     /* LEFT APOSTROPHE  = ASC 96 */
     /* RIGHT APOSTROPHE = ASC 39 */
     /* PERIOD           = ASC 46 */
     /* CR DUMP SYMBOL   = ASC 67 ( The letter C ) */
     /* LF DUMP SYMBOL   = ASC 76 ( The letter L ) */

     #define DUMPIT_ASTERISK    42
     #define DUMPIT_LAPOSTROPHE 96
     #define DUMPIT_RAPOSTROPHE 39
     #define DUMPIT_PERIOD      46
     #define DUMPIT_CR          67
     #define DUMPIT_LF          76

     #ifdef MASK_ONLY_CERTAIN_CHARS
          if ( ch1 ==  0 ) ch2 = DUMPIT_PERIOD;
     else if ( ch1 == 13 ) ch2 = DUMPIT_CR;
     else if ( ch1 == 10 ) ch2 = DUMPIT_LF;
     else if ( ch1 ==  9 ) ch2 = DUMPIT_LAPOSTROPHE;
     else                  ch2 = ch1;
     #endif

     #define MASK_ALL_NON_PRINTABLE_CHARS
     #ifdef  MASK_ALL_NON_PRINTABLE_CHARS

     /* Mask all control chars and high ends chars for UNIX or */
     /* TTY console screws up... */

          if ( ch1 == 13 ) ch2 = DUMPIT_CR;
     else if ( ch1 == 10 ) ch2 = DUMPIT_LF;
     else if ( ch1 <  32 ) ch2 = DUMPIT_PERIOD;
     else if ( ch1 >  126) ch2 = DUMPIT_LAPOSTROPHE;
     else if ( ch1 == 37 ) ch2 = DUMPIT_ASTERISK; /* Mask PERCENT   for UNIX */
     else if ( ch1 == 92 ) ch2 = DUMPIT_ASTERISK; /* Mask BACKSLASH for UNIX */
     else                  ch2 = ch1;

     /* ENDIF on MASK_ALL_NON_PRINTABLE_CHARS */
     #endif

     l2[o2++] = ch2;

     sprintf( s, "%02X", ch1 );

     if ( strlen(s) > 2 ) s[2]=0; /* Prevent overflow */

     len1 = strlen(s);
     len2 = strlen(l1);

     if ( strlen(l1) < (sizeof(l1) - (len1+1)) )
       {
        strcat( l1, s   );
        strcat( l1, " " );
       }

     if ( o2 >= 16 )
       {
        l2[o2]=0;

        mod_gzip_printf( "%6lu| %-49.49s| %-16.16s |\n", offset1, l1, l2 );

        offset1 += o2;

        o1=0;
        o2=0;
        o3=0;

        l1[0] = 0;
        l2[0] = 0;
        l3[0] = 0;
       }

    }/* End 'for( i=0; i<buflen; i++ )' loop... */

 /* Print remainder ( if anything ) */

 if ( o2 > 0  )
   {
    l2[o2]=0;

    mod_gzip_printf( "%6lu| %-49.49s| %-16.16s |\n", offset1, l1, l2 );

    offset1 += o2;

    o1 = o2 = o3 = 0;

    l1[0] = 0;
    l2[0] = 0;
    l3[0] = 0;
   }

}/* End of mod_gzip_hexdump() */

#endif /* MOD_GZIP_DEBUG1 */

static void mod_gzip_init( server_rec *server, pool *p )
{
    /*
     * The module initialization procedure...
     */

    FILE *fh1;
    char filename[ 512 ];

    #ifdef MOD_GZIP_DEBUG1
    char cn[]="mod_gzip_init()";
    #endif

    mod_gzip_conf *mgc;

    /* Start... */

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: Entry...\n", cn );
    #endif

    /*
     * Set some instance specific globals...
     *
     * The default 'temp' dir, lacking an httpd.conf config file
     * override, is the Apache 'ServerRoot'. Don't assume that /logs
     * dir exists because some Apache installations just use syslog
     * or stderr as their log output target.
     *
     * On most Apache installations 'ServerRoot' is automatically
     * readable/writable by the Server while it is running.
     *
     * On systems where it is not there MUST be an override
     * in the httpd.conf file.
     *
     * See the comments regarding the 'mod_gzip_temp_dir' directive
     * in the httpd.conf configuration file.
     */

    mgc = ( mod_gzip_conf * )
    ap_get_module_config(server->module_config, &gzip_module);

    /* Make sure we can read/write the temp directory... */

    sprintf( filename, "%s%smod_gzip.id", mgc->cache.root, mod_gzip_dirsep );

    fh1 = fopen( filename, "wb" );

    if ( !fh1 ) /* Write an ERROR to console and to log(s)... */
      {
       fprintf( stderr, "mod_gzip: Cannot read/write dir/file [%s]\n",filename);
       fprintf( stderr, "mod_gzip: Make sure the directory exists and that the Server\n");
       fprintf( stderr, "mod_gzip: has read/write permission(s) for the directory.\n");
       fprintf( stderr, "mod_gzip: See the 'mod_gzip_temp_dir' configuration notes.\n");

       /* This is a startup ERROR and has to be fixed... */

       ap_log_error( "",0,APLOG_NOERRNO|APLOG_ERR, server,
       "mod_gzip: Cannot read/write dir/file [%s]", filename);
       ap_log_error( "",0,APLOG_NOERRNO|APLOG_ERR, server,
       "mod_gzip: Make sure the directory exists and that the Server");
       ap_log_error( "",0,APLOG_NOERRNO|APLOG_ERR, server,
       "mod_gzip: has read/write permission(s) for the directory.");
       ap_log_error( "",0,APLOG_NOERRNO|APLOG_ERR, server,
       "mod_gzip: See the 'mod_gzip_temp_dir' configuration notes.");
      }
    else /* File opened OK... just add some data and close it... */
      {
       /*
        * Since this is just a MARK file we could simply wipe
        * it out but might as well print the actual version
        * number into it and leave it there in case there is
        * any question about which version is actually running.
        */

       fprintf( fh1, "mod_gzip version %s\n", mod_gzip_version );
       fclose( fh1 );
      }

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_imap_show_items( (mod_gzip_conf *) mgc ); /* Show item list */
    mod_gzip_printf( "%s: Exit > return( void ) >\n", cn );
    mod_gzip_printf( "\n" ); /* Separator for log file */
    #endif

}/* End of mod_gzip_init() */

int mod_gzip_strnicmp( char *s1, char *s2, int len1 )
{
 /* Behaves just like strnicmp() but IGNORES differences */
 /* between FORWARD or BACKWARD slashes in a STRING...   */
 /* Also uses straight pointers and avoids stdlib calls. */

 int i;
 char ch1;
 char ch2;

 /* WARNING! We MUST have a check for 'NULL' on the pointer(s) */
 /*          themselves or we might GP ( like NETSCAPE does )  */
 /*          if a 'NULL' pointer is passed to this routine...  */

 if ( ( s1 == 0 ) || ( s2 == 0 ) )
   {
    /* SAFETY! If pointer itself if NULL       */
    /* don't enter LOOP or NETSCAPE will GP... */

    return( 1 ); /* Return '1' for NOMATCH...  */
   }

 for ( i=0; i<len1; i++ )
    {
     if ( ( *s1 == 0 ) || ( *s2 == 0 ) ) return( 1 ); /* No match! */

     ch1 = *s1;
     ch2 = *s2;

     if ( ch1 > 96 ) ch1 -= 32;
     if ( ch2 > 96 ) ch2 -= 32;

     if ( ch1 == '/' ) ch1 = '\\';
     if ( ch2 == '/' ) ch2 = '\\';

     if ( ch1 != ch2 ) return( 1 ); /* No match! */

     s1++;
     s2++;

    }/* End 'i' loop */

 /* If we make it to here then everything MATCHED! */

 return( 0 ); /* MATCH! */

}/* End mod_gzip_strnicmp() */

extern API_VAR_EXPORT module *top_module;

struct _table {
    array_header a;
#ifdef MAKE_TABLE_PROFILE
    void *creator;
#endif
};
typedef struct _table _table;

const char *mod_gzip_isscript( request_rec *r, _table *t, const char *key)
{
    /*
     * Get a 'handler' name for a MIME type right out of
     * the Apache 'Action' table(s)...
     *
     * Example:
     *
     * If "key" is "applications/x-httpd-php3"
     * then this search will return "/php3/php.exe"
     * or whatever the equivalent PHP executable
     * pathname is as specified by an 'Action' statement
     * in the httpd.conf configuration file.
     *
     * This pathname might still have 'aliases' in it
     * so we will have to consult with mod_alias
     * following this call and get any aliases converted.
     */

    table_entry *elts =
    (table_entry *) t->a.elts;
    int i;

    char cn[]="mod_gzip_isscript()";

    /*
     * Start...
     */

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: Entry...\n",cn);
    mod_gzip_printf( "%s: key=[%s]\n",cn,key );
    #endif

    if ( key == NULL )
      {
       #ifdef MOD_GZIP_DEBUG1
       mod_gzip_printf( "%s: 'key' has no length\n",cn);
       mod_gzip_printf( "%s: Exit > return( NULL ) >\n",cn);
       #endif

       if ( r->server->loglevel == APLOG_DEBUG )
         {
          ap_log_error( "",0,APLOG_NOERRNO|APLOG_DEBUG, r->server,
          "%s: Search key is NULL.",cn);
         }

       return NULL;
      }

    for (i = 0; i < t->a.nelts; ++i)
       {
        #ifdef MOD_GZIP_DEBUG1
        mod_gzip_printf(
        "%s: i=%4.4d Comparing [%s] with elts.key[%s].val[%s]\n",
        cn, i, key, elts[i].key, elts[i].val );
        #endif

        if ( r->server->loglevel == APLOG_DEBUG )
          {
           ap_log_error( "",0,APLOG_NOERRNO|APLOG_DEBUG, r->server,
           "%s: i=%4.4d Comparing [%s] with elts.key[%s].val[%s]",
           cn, i, key, elts[i].key, elts[i].val );
          }

        if (!strcasecmp(elts[i].key, key))
          {
           #ifdef MOD_GZIP_DEBUG1
           mod_gzip_printf( "%s: MATCH FOUND!",cn);
           mod_gzip_printf( "%s: Exit > return(%s) >\n",cn,elts[i].val);
           #endif

           if ( r->server->loglevel == APLOG_DEBUG )
             {
              ap_log_error( "",0,APLOG_NOERRNO|APLOG_DEBUG, r->server,
              "%s: MATCH FOUND...",cn);
             }

           return elts[i].val;
          }

       }/* End 'i' loop */

    if ( r->server->loglevel == APLOG_DEBUG )
      {
       ap_log_error( "",0,APLOG_NOERRNO|APLOG_DEBUG, r->server,
       "%s: NO MATCH FOUND...",cn);
      }

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: NO MATCH FOUND!\n",cn);
    mod_gzip_printf( "%s: Exit > return( NULL ) >\n",cn);
    #endif

    return NULL;

}/* End of 'mod_gzip_isscript()' */

typedef struct {
  table *action_types;       /* Added with Action... */
  char *scripted[METHODS];   /* Added with Script... */
  array_header *xmethods;    /* Added with Script -- extension methods */
} mod_actions_local_config;

int mod_gzip_run_mod_action( request_rec *r )
{
    module *modp;
    int count=0;
    int pass=0;

    mod_actions_local_config *mod_actions_conf;

    const char *t=0;
    const char *action=0;

    #ifdef MOD_GZIP_DEBUG1
    char cn[]="mod_gzip_run_mod_action()";
    #endif

    #ifdef MOD_GZIP_FUTURE_USE
    const handler_rec *handp;
    #endif

    /* Currently 9 possible 'event' handlers. */
    /* Actual content handler in a module is 'extra'. */
    #define MOD_GZIP_NMETHODS 9

    /*
     * Start...
     */

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: Entry...\n",cn);
    mod_gzip_printf( "%s: *IN: r->uri         =[%s]\n", cn, r->uri );
    mod_gzip_printf( "%s: *IN: r->unparsed_uri=[%s]\n", cn, r->unparsed_uri );
    mod_gzip_printf( "%s: *IN: r->filename    =[%s]\n", cn, r->filename );
    mod_gzip_printf( "%s: r->content_type     =[%s]\n", cn,r->content_type);
    mod_gzip_printf( "%s: r->handler          =[%s]\n", cn,r->handler);
    #endif

    for ( modp = top_module; modp; modp = modp->next )
       {
        /* modp->name list will look like this... */
        /*--------------------*/
        /* 00 [mod_gzip.c]    */
        /* 01 [mod_isapi.c]   */
        /* 02 [mod_setenv.c]  */
        /* 02 [mod_actions.c] */
        /*    ............... */
        /*    ............... */
        /* 18 [mod_so.c]      */
        /* 19 [http_core.c]   <- Always bottom of list (last one called) */
        /*--------------------*/

        #ifdef MOD_GZIP_DEBUG1
        mod_gzip_printf( "%s: count=%4.4d modp = %10.10ld modp->name=[%s]\n",
        cn,count,(long)modp, modp->name );
        #endif

        if ( mod_gzip_strnicmp( (char *) modp->name, "mod_actions.c", 13 ) == 0 )
        {

        /* Module information... */

        #ifdef MOD_GZIP_DEBUG1
        mod_gzip_printf( "%s: ++++++++++ MODULE FOUND!...\n",cn);
        mod_gzip_printf( "%s: ++++++++++ modp->module_index = %d\n",cn,(int)modp->module_index);
        #endif

        /* Get a pointer to the module configuration data... */

        mod_actions_conf = (mod_actions_local_config *)
        ap_get_module_config(r->per_dir_config, modp );

        /* Get script name... */

        /* Make 2 passes if necessary. If we don't find a   */
        /* program name associated with MIME type first     */
        /* then punt and look for a program name associated */
        /* with the r->handler name such as [php-script]    */

        for ( pass = 0; pass < 2; pass++ )
           {
            if ( pass == 0 ) /* Check r->content_type first */
              {
               /* This is the first pass... */

               /* Set 'action' search key to 'r->content_type' */
               /* so we search for [application/x-httpd-php3]  */

               action = r->content_type;
              }
            else if ( pass == 1 ) /* Try r->handler */
              {
               /* This is the second pass... */

               /* Set 'action' search key to 'r->handler' */
               /* so we search for [php-script]  */

               action = r->handler;
              }

            #ifdef MOD_GZIP_DEBUG1
            mod_gzip_printf( "%s: ++++++++++ pass            =%d\n",  cn,pass);
            mod_gzip_printf( "%s: ++++++++++ t               =[%s]\n",cn,t);
            mod_gzip_printf( "%s: ++++++++++ r->content_type =[%s]\n",cn,r->content_type);
            mod_gzip_printf( "%s: ++++++++++ r->handler      =[%s]\n",cn,r->handler);
            mod_gzip_printf( "%s: ++++++++++ action          =[%s]\n",cn,action);
            mod_gzip_printf( "%s: ++++++++++ r->filename     =[%s]\n",cn,r->filename);
            mod_gzip_printf( "%s: ++++++++++ r->uri          =[%s]\n",cn,r->uri);
            mod_gzip_printf( "%s: ++++++++++ Call mod_gzip_isscript()...\n",cn);
            #endif

            t =
            mod_gzip_isscript(
            r,
            (_table *) mod_actions_conf->action_types,
            action ? action : ap_default_type(r)
            );

            #ifdef MOD_GZIP_DEBUG1
            mod_gzip_printf( "%s: ++++++++++ Back mod_gzip_isscript()...\n",cn);
            mod_gzip_printf( "%s: ++++++++++ t               =[%s]\n",cn,t);
            mod_gzip_printf( "%s: ++++++++++ action          =[%s]\n",cn,action);
            #endif

            if ( t )
              {
               /*
                * If a program name was found then make it r->filename
                * and r->uri will become the input name for the program
                */

               r->filename = ap_pstrdup(r->pool,t);

               break;
              }

           }/* End 'for( pass )' loop */

        #ifdef MOD_GZIP_DEBUG1
        mod_gzip_printf( "%s: ++++++++++ r->filename=[%s]\n",cn,r->filename);
        mod_gzip_printf( "%s: ++++++++++ r->uri     =[%s]\n",cn,r->uri);
        #endif

        /* If a handler was found we are DONE... */

        if ( t )
          {
           #ifdef MOD_GZIP_DEBUG1
           mod_gzip_printf( "%s: Handler was found...\n",cn);
           mod_gzip_printf( "%s: Exit > return( OK ) >\n",cn);
           #endif

           return OK;
          }

        #ifdef MOD_GZIP_FUTURE_USE

        #ifdef MOD_GZIP_DEBUG1
        mod_gzip_printf( "%s: ++++++++++ METHODS\n",cn);
        mod_gzip_printf( "%s: ++++++++++ modp->translate_handler = %ld\n",cn,(long)modp->translate_handler);
        mod_gzip_printf( "%s: ++++++++++ modp->ap_check_user_id  = %ld\n",cn,(long)modp->ap_check_user_id);
        mod_gzip_printf( "%s: ++++++++++ modp->auth_checker      = %ld\n",cn,(long)modp->auth_checker);
        mod_gzip_printf( "%s: ++++++++++ modp->access_checker    = %ld\n",cn,(long)modp->access_checker);
        mod_gzip_printf( "%s: ++++++++++ modp->type_checker      = %ld\n",cn,(long)modp->type_checker);
        mod_gzip_printf( "%s: ++++++++++ modp->fixer_upper       = %ld\n",cn,(long)modp->fixer_upper);
        mod_gzip_printf( "%s: ++++++++++ modp->logger            = %ld\n",cn,(long)modp->logger);
        mod_gzip_printf( "%s: ++++++++++ modp->header_parser     = %ld\n",cn,(long)modp->header_parser);
        mod_gzip_printf( "%s: .......... CONTENT HANDLERS\n",cn);
        #endif /* MOD_GZIP_DEBUG1 */

        if ( !modp->handlers )
          {
           #ifdef MOD_GZIP_DEBUG1
           mod_gzip_printf( "%s: .......... NO CONTENT HANDLERS!\n",cn);
           #endif
          }
        else /* There are some handlers... */
          {
           for ( handp = modp->handlers; handp->content_type; ++handp )
              {
               #ifdef MOD_GZIP_DEBUG1
               mod_gzip_printf( "%s: .......... handp->content_type = [%s]\n",
               cn,handp->content_type);
               mod_gzip_printf( "%s: .......... handp->handler      = %ld\n",cn,(long)handp->handler);
               #endif

              }/* End 'handp' loop */

          }/* End 'else' */

        #endif /* MOD_GZIP_FUTURE_USE */

        #ifdef MOD_GZIP_DEBUG1
        mod_gzip_printf( "%s: No handler was found...\n",cn);
        mod_gzip_printf( "%s: Exit > return( DECLINED ) >\n",cn);
        #endif

        return DECLINED;

        }/* 'if ( mod_gzip_strnicmp( (char *) modp->name, "mod_actions.c", 13 ) == 0 )' */

        count++;

       }/* End 'modp' loop... */

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: No handler found...\n",cn);
    mod_gzip_printf( "%s: Exit > return( DECLINED ) > ERROR >\n",cn);
    #endif

    return DECLINED;

}/* End of mod_gzip_run_mod_action() */


int mod_gzip_run_mod_alias( request_rec *r )
{
    /*
     * This calls 'translate_alias_redir()' routine in mod_alias.c
     * which will search/replace keywords in the URI with the correct
     * 'ScriptAlias' value(s) from the httpd.conf configuration file.
     *
     * 'translate_alias_redir()' is the name of routine registered
     * by mod_alias.c module as the 'translate' hook.
     */

    module *modp;
    int count=0;
    int rc;

    #ifdef MOD_GZIP_DEBUG1
    char cn[]="mod_gzip_run_mod_alias()";
    #endif

    const handler_rec *handp;

    /* Currently 9 possible 'event' handlers. */
    /* Actual content handler in a module is 'extra'. */
    #define MOD_GZIP_NMETHODS 9

    char *save_filename     = 0;
    char *save_uri          = 0;

    char nothing[256];

    /*
     * Start...
     */

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: Entry...\n",cn);
    mod_gzip_printf( "%s: *IN: r->uri         =[%s]\n", cn, r->uri );
    mod_gzip_printf( "%s: *IN: r->unparsed_uri=[%s]\n", cn, r->unparsed_uri );
    mod_gzip_printf( "%s: *IN: r->filename    =[%s]\n", cn, r->filename );
    #endif

    for ( modp = top_module; modp; modp = modp->next )
       {
        /* modp->name list will look like this... */
        /*--------------------*/
        /* 00 [mod_gzip.c]    */
        /* 01 [mod_isapi.c]   */
        /* 02 [mod_setenv.c]  */
        /* 02 [mod_actions.c] */
        /*    ............... */
        /*    ............... */
        /* 18 [mod_so.c]      */
        /* 19 [http_core.c]   <- Always bottom of list (last one called) */
        /*--------------------*/

        #ifdef MOD_GZIP_DEBUG1
        mod_gzip_printf( "%s: count=%4.4d modp = %10.10ld modp->name=[%s]\n",
        cn,count,(long)modp, modp->name );
        #endif

        /*
        There are only 3 modules that normally have
        'translate' handlers registered...

        mod_alias
        mod_userdir
        http_core
        */

        if ( ( mod_gzip_strnicmp( (char *) modp->name, "mod_alias.c",   11 ) == 0 ) ||
             ( mod_gzip_strnicmp( (char *) modp->name, "mod_userdir.c", 13 ) == 0 ) ||
             ( mod_gzip_strnicmp( (char *) modp->name, "http_core.c",   11 ) == 0 ) )
        {

        /* Module information... */

        #ifdef MOD_GZIP_DEBUG1

        mod_gzip_printf( "%s: ++++++++++ MODULE FOUND!...\n",cn);
        mod_gzip_printf( "%s: ++++++++++ modp->module_index = %d\n",cn,(int)modp->module_index);

        mod_gzip_printf( "%s: ++++++++++ METHODS\n",cn);
        mod_gzip_printf( "%s: ++++++++++ modp->translate_handler = %ld\n",cn,(long)modp->translate_handler);
        mod_gzip_printf( "%s: ++++++++++ modp->ap_check_user_id  = %ld\n",cn,(long)modp->ap_check_user_id);
        mod_gzip_printf( "%s: ++++++++++ modp->auth_checker      = %ld\n",cn,(long)modp->auth_checker);
        mod_gzip_printf( "%s: ++++++++++ modp->access_checker    = %ld\n",cn,(long)modp->access_checker);
        mod_gzip_printf( "%s: ++++++++++ modp->type_checker      = %ld\n",cn,(long)modp->type_checker);
        mod_gzip_printf( "%s: ++++++++++ modp->fixer_upper       = %ld\n",cn,(long)modp->fixer_upper);
        mod_gzip_printf( "%s: ++++++++++ modp->logger            = %ld\n",cn,(long)modp->logger);
        mod_gzip_printf( "%s: ++++++++++ modp->header_parser     = %ld\n",cn,(long)modp->header_parser);
        mod_gzip_printf( "%s: .......... CONTENT HANDLERS\n",cn);

        #endif /* MOD_GZIP_DEBUG1 */

        if ( !modp->handlers )
          {
           #ifdef MOD_GZIP_DEBUG1
           mod_gzip_printf( "%s: .......... NO CONTENT HANDLERS!\n",cn);
           #endif
          }
        else /* There are some handlers... */
          {
           for ( handp = modp->handlers; handp->content_type; ++handp )
              {
               #ifdef MOD_GZIP_DEBUG1
               mod_gzip_printf( "%s: .......... handp->content_type = [%s]\n",
               cn,handp->content_type);
               mod_gzip_printf( "%s: .......... handp->handler      = %ld\n",cn,(long)handp->handler);
               #endif

              }/* End 'handp' loop */

          }/* End 'else' */

        if ( modp->translate_handler )
          {
           #ifdef MOD_GZIP_DEBUG1
           mod_gzip_printf( "%s: modp->translate_handler is VALID...\n",cn);
           #endif

           /*
           There are only 3 modules that normally have
           'translate' handlers registered...

           mod_alias     <- Will translate /php3/xxx to c:/php3017/xx
           mod_userdir
           http_core
           */

           /*
            * This calls 'translate_alias_redir()' routine in mod_alias.c
            * which will search/replace keywords in the URI with the correct
            * 'ScriptAlias' value(s) from the httpd.conf configuration file.
            *
            * 'translate_alias_redir()' is the name of routine registered
            * by mod_alias.c module as the 'translate' hook.
            *
            * The 'translate_alias_redir()' function in mod_alias.c
            * is really simple. All it does is check to make sure
            * that r->uri has some value and, if it does, it calls
            * another routine in mod_alias.c named 'try_alias_list()'
            * which replaces any 'ScriptAlias' phrases with their
            * real values and copies the result to r->filename.
            *
            * We must make sure the phrase we want translated is
            * in r->uri and check for results in r->filename.
            */

           /*
            * Calling mod_alias.c translate handler will correctly
            * translate 'ScriptAlias' phrases such as...
            *
            * URI value...
            * /php3/php3.exe
            * becomes...
            * c:/php3017/php3.exe
            */

           save_filename     = r->filename;
           save_uri          = r->uri;
           nothing[0]        = 0;

           r->filename       = nothing;
           r->uri            = save_filename; /* Phrase to translate */

           #ifdef MOD_GZIP_DEBUG1
           mod_gzip_printf( "%s: r->filename     = [%s]\n",cn,r->filename);
           mod_gzip_printf( "%s: r->uri          = [%s]\n",cn,r->uri);
           mod_gzip_printf( "%s: Call (modp->translate_handler)(r)...\n",cn);
           #endif

           /* Call the actual translate routine in mod_action module... */

           rc = (modp->translate_handler)( (request_rec *) r );

           #ifdef MOD_GZIP_DEBUG1
           mod_gzip_printf( "%s: Back (modp->translate_handler)(r)...\n",cn);
           mod_gzip_printf( "%s: r->filename     = [%s]\n",cn,r->filename);
           mod_gzip_printf( "%s: r->uri          = [%s]\n",cn,r->uri);
           #endif

           /*
            * If there was a successful translation then the return
            * code will be OK and the translated URI will be sitting
            * in r->filename. If there were no phrase replacements
            * then the return code will be DECLINED.
            */

           #ifdef MOD_GZIP_DEBUG1

           if ( rc == OK )
             {
              mod_gzip_printf( "%s: rc = %d = OK\n",cn, rc );
             }
           else if ( rc == DECLINED )
             {
              mod_gzip_printf( "%s: rc = %d = DECLINED\n",cn, rc );
             }
           else if ( rc == DONE ) /* -2 means 'totally done' */
             {
              mod_gzip_printf( "%s: rc = %d = DONE\n",cn, rc );
             }
           else /* Probably an HTTP ERROR value... */
             {
              mod_gzip_printf( "%s: rc = %d = HTTP_ERROR?\n",cn, rc );
             }

           #endif /* MOD_GZIP_DEBUG */

           /*
            * Evaluate the results...
            */

           if ( rc == OK ) /* There was a phrase translation... */
             {
              #ifdef MOD_GZIP_DEBUG1
              mod_gzip_printf( "%s: There was a phrase translation...\n",cn );
              mod_gzip_printf( "%s: Keeping new 'r->filename'\n",cn );
              #endif

              /* Do NOT restore 'r->filename' to original value... */
              /* Just fall-through and continue... */
             }
           else /* No phrases were replaced... */
             {
              #ifdef MOD_GZIP_DEBUG1
              mod_gzip_printf( "%s: There were NO phrases translated...\n",cn );
              mod_gzip_printf( "%s: Restoring 'r->filename' to original value...\n",cn );
              #endif

              /* Restore 'r->filename' to original value... */

              r->filename = save_filename;
             }

           /* Always 'restore' URI to original value... */

           r->uri = save_uri;

           /* Turn and burn... */

           #ifdef MOD_GZIP_DEBUG1
           mod_gzip_printf( "%s: Exit > return( rc=%d ) >\n",cn,rc);
           #endif

           return rc;
          }
        else /* modp->translate_handler is NULL... */
          {
           #ifdef MOD_GZIP_DEBUG1
           mod_gzip_printf( "%s: modp->translate_handler is NOT VALID.\n",cn);
           #endif
          }

        }/* 'if ( mod_gzip_strnicmp( (char *) modp->name, "mod_actions.c", 13 ) == 0 )' */

        count++;

       }/* End 'modp' loop... */

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: No handler found...\n",cn);
    mod_gzip_printf( "%s: Exit > return( DECLINED ) > ERROR >\n",cn);
    #endif

    return DECLINED;

}/* End of mod_gzip_run_mod_alias() */


static int mod_gzip_handler( request_rec *r )
{
    /*
     * The primary module request handler...
     */

    int  rc=0;
    char cn[]="mod_gzip_handler()";
    int access_status=0;
    int access_status2=0;

    /*
     * Start...
     */

    if ( r->server->loglevel == APLOG_DEBUG )
      {
       /*
        * If the user has 'LogLevel debug' set in httpd.conf then
        * it's ok to go ahead and strike some diagnostic information
        * to the Apache log(s).
        *
        * APLOG_MARK is what supplies __FILE__ and __LINE__ info and
        * it is actually defined in HTTP_LOG.H as...
        *
        * define APLOG_MARK  __FILE__,__LINE__
        *
        * Sometimes the original __FILE__ name is very long and is
        * fairly useless information cluttering up the logs when
        * there is only 1 possible source file name so
        * to NOT use it just supply 2 dummy parameters instead.
        *
        * The first parameter can be a custom message instead of
        * the __FILE__ string that would normally be substituted.
        */

        ap_log_error( "",0,APLOG_NOERRNO|APLOG_DEBUG, r->server,
        "%s: Entry point...",cn);

        ap_log_error( "",0,APLOG_NOERRNO|APLOG_DEBUG, r->server,
        "%s: r->the_request  = [%s]",cn,r->the_request);

        ap_log_error( "",0,APLOG_NOERRNO|APLOG_DEBUG, r->server,
        "%s: r->protocol     = [%s]",cn,r->protocol);

        ap_log_error( "",0,APLOG_NOERRNO|APLOG_DEBUG, r->server,
        "%s: r->proto_num    = %d",cn,(int)r->proto_num);

        ap_log_error( "",0,APLOG_NOERRNO|APLOG_DEBUG, r->server,
        "%s: r->filename     = [%s]",cn,r->filename);

        ap_log_error( "",0,APLOG_NOERRNO|APLOG_DEBUG, r->server,
        "%s: r->uri          = [%s]",cn,r->uri);

        ap_log_error( "",0,APLOG_NOERRNO|APLOG_DEBUG, r->server,
        "%s: r->content_type = [%s]",cn,r->content_type);

        ap_log_error( "",0,APLOG_NOERRNO|APLOG_DEBUG, r->server,
        "%s: r->handler      = [%s]",cn,r->handler);

       }/* End 'if( r->server->loglevel == APLOG_DEBUG )' */

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "\n" );
    mod_gzip_printf( "%s: ```` Entry...\n",cn);
    mod_gzip_printf( "%s: -------------------------------------------------------------\n",cn);
    mod_gzip_printf( "%s: *IN: r->uri                 =[%s]\n", cn, r->uri );
    mod_gzip_printf( "%s: *IN: r->unparsed_uri        =[%s]\n", cn, r->unparsed_uri );
    mod_gzip_printf( "%s: *IN: r->filename            =[%s]\n", cn, r->filename );
    mod_gzip_printf( "%s: *IN: r->path_info           =[%s]\n", cn, r->path_info );
    mod_gzip_printf( "%s: *IN: r->args                =[%s]\n", cn, r->args );
    mod_gzip_printf( "%s: *IN: r->header_only         =[%s]\n", cn, r->header_only );
    mod_gzip_printf( "%s: *IN: r->protocol            =[%s]\n", cn, r->protocol );
    mod_gzip_printf( "%s: *IN: r->proto_num           =%d\n",   cn, r->proto_num );
    mod_gzip_printf( "%s: *IN: r->hostname            =[%s]\n", cn, r->hostname );
    mod_gzip_printf( "%s: *IN: r->the_request         =[%s]\n", cn, r->the_request );
    mod_gzip_printf( "%s: *IN: r->assbackwards        =%d\n",   cn, r->assbackwards );
    mod_gzip_printf( "%s: *IN: r->status_line         =[%s]\n", cn, r->status_line );
    mod_gzip_printf( "%s: *IN: r->status              =%d\n",   cn, r->status );
    mod_gzip_printf( "%s: *IN: r->method              =[%s]\n", cn, r->method );
    mod_gzip_printf( "%s: *IN: r->method_number       =%d\n",   cn, r->method_number );
    mod_gzip_printf( "%s: *IN: r->content_type        =[%s]\n", cn, r->content_type );
    mod_gzip_printf( "%s: *IN: r->handler             =[%s]\n", cn, r->handler );
    mod_gzip_printf( "%s: *IN: r->content_encoding    =[%s]\n", cn, r->content_encoding );
    mod_gzip_printf( "%s: *IN: r->content_language    =[%s]\n", cn, r->content_language );
    mod_gzip_printf( "%s: -------------------------------------------------------------\n",cn);
    mod_gzip_printf( "%s: *IN: r->parsed_uri.scheme   =[%s]\n", cn, r->parsed_uri.scheme );
    mod_gzip_printf( "%s: *IN: r->parsed_uri.hostinfo =[%s]\n", cn, r->parsed_uri.hostinfo );
    mod_gzip_printf( "%s: *IN: r->parsed_uri.user     =[%s]\n", cn, r->parsed_uri.user );
    mod_gzip_printf( "%s: *IN: r->parsed_uri.password =[%s]\n", cn, r->parsed_uri.password );
    mod_gzip_printf( "%s: *IN: r->parsed_uri.hostname =[%s]\n", cn, r->parsed_uri.hostname );
    mod_gzip_printf( "%s: *IN: r->parsed_uri.port_str =[%s]\n", cn, r->parsed_uri.port_str );
    mod_gzip_printf( "%s: *IN: r->parsed_uri.port     =%u\n",   cn, r->parsed_uri.port );
    mod_gzip_printf( "%s: *IN: r->parsed_uri.path     =[%s]\n", cn, r->parsed_uri.path );
    mod_gzip_printf( "%s: *IN: r->parsed_uri.query    =[%s]\n", cn, r->parsed_uri.query );
    mod_gzip_printf( "%s: *IN: r->parsed_uri.fragment =[%s]\n", cn, r->parsed_uri.fragment );
    mod_gzip_printf( "%s: -------------------------------------------------------------\n",cn);

    #endif /* MOD_GZIP_DEBUG1 */

    /*
     * Call the real transaction handler....
     */

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: Call mod_gzip_request_handler()...\n", cn );
    #endif

    rc = mod_gzip_request_handler( (request_rec *) r );

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: Back mod_gzip_request_handler()... rc=%d\n",cn,rc);
    #endif

    if ( r->server->loglevel == APLOG_DEBUG )
      {
       /*
        * If LogLevel is 'debug' then show the final return code
        * value in the log(s)...
        */

       if ( rc == OK )
         {
          ap_log_error( "",0,APLOG_NOERRNO|APLOG_DEBUG, r->server,
          "%s: Exit: return( rc = %d = OK )", cn, rc );
         }
       else if ( rc == DECLINED )
         {
          ap_log_error( "",0,APLOG_NOERRNO|APLOG_DEBUG, r->server,
          "%s: Exit: return( rc = %d = DECLINED )", cn, rc );
         }
       else /* It's probably an HTTP error code... */
         {
          ap_log_error( "",0,APLOG_NOERRNO|APLOG_DEBUG, r->server,
          "%s: Exit: return( rc = %d = HTTP ERROR CODE? )", cn, rc );
         }

      }/* End 'if( r->server->loglevel == APLOG_DEBUG )' */

    #ifdef MOD_GZIP_DEBUG1

    if ( rc == OK )
      {
       mod_gzip_printf( "%s: rc = %d OK\n", cn, (int) rc);
      }
    else if ( rc == DECLINED )
      {
       mod_gzip_printf( "%s: rc = %d DECLINED\n", cn, (int) rc );
      }
    else /* It's probably an HTTP error code... */
      {
       mod_gzip_printf( "%s: rc = %d ( HTTP ERROR CODE? )\n", cn, (int) rc );
      }

    mod_gzip_printf( "%s: Exit > return( rc = %d ) >\n",cn,rc );

    #endif /* MOD_GZIP_DEBUG1 */

    return rc;

}/* End of mod_gzip_handler() */

typedef struct {
    table *action_types;       /* Added with Action... */
    char *scripted[METHODS];   /* Added with Script... */
    array_header *xmethods;    /* Added with Script -- extension methods */
} action_dir_config2;

extern module action_module;

int mod_gzip_request_handler( request_rec *r )
{
    /*
     * Process a new request...
     */

    int             rc                = 0;
    int             loglevel          = 0;
    int             do_command        = 0;
    int             process           = 0;
    int             action_flag       = 0;
    long            compression_ratio = 0;

    const char*     has_encoding      = 0;
    const char*     accept_encoding   = 0;

    #ifdef MOD_GZIP_ALLOWS_INTERNAL_COMMANDS
    char tmp[4096]; /* Scratch buffer for HTML output */
    #endif

    #ifdef MOD_GZIP_DEBUG1
    char cn[]="mod_gzip_request_handler()";
    const char* the_type = 0;
    #endif

    #ifdef MOD_GZIP_USES_APACHE_LOGS
    char log_info[40]; /* Scratch buffer */
    #endif

    void *modconf = r->server->module_config;

    mod_gzip_conf *conf = 0; /* Pointer to our own config data */

    /*
     * Start...
     *
     * Establish a local pointer to module configuration data...
     */

    conf = (mod_gzip_conf *)
    ap_get_module_config(modconf, &gzip_module);

    /*
     * Get the current Apache log level...
     */

    loglevel = r->server->loglevel;

    #ifdef MOD_GZIP_USES_APACHE_LOGS

    /*
     * If the MOD_GZIP_USES_APACHE_LOGS compile-time switch is ON
     * then the Apache log module interface code is being included.
     *
     * Reset the module 'notes' that are used by mod_gzip to
     * add entries to Apache standard log files...
     *
     * See the note farther below about how to add mod_gzip
     * compression information to any standard Apache log file.
     */

    /* Default for 'mod_result' message is 'DECLINED:NOP'... */

    ap_table_setn( r->notes,"mod_gzip_result",ap_pstrdup(r->pool,"DECLINED:NOP"));

    /* Default for in/out size is 'n/a'... 'Not available'...*/

    sprintf( log_info, "n/a" );

    ap_table_setn( r->notes,"mod_gzip_input_size",ap_pstrdup(r->pool,log_info));
    ap_table_setn( r->notes,"mod_gzip_output_size",ap_pstrdup(r->pool,log_info));

    /* Default for compression ratio is '0' percent... */

    ap_table_setn( r->notes,"mod_gzip_compression_ratio",ap_pstrdup(r->pool,"0"));

    #endif /* MOD_GZIP_USES_APACHE_LOGS */

    #ifdef MOD_GZIP_DEBUG1

    /* Request info... */

    mod_gzip_printf( "%s: Entry...\n",cn);
    mod_gzip_printf( "%s: mod_gzip_version    =[%s]\n", cn, mod_gzip_version);
    mod_gzip_printf( "%s: conf->req           = %d\n",  cn, (int) conf->req);
    mod_gzip_printf( "%s: conf->cache.root    =[%s]\n", cn, conf->cache.root);
    mod_gzip_printf( "%s: *IN: r->uri         =[%s]\n", cn, r->uri);
    mod_gzip_printf( "%s: *IN: r->unparsed_uri=[%s]\n", cn, r->unparsed_uri);
    mod_gzip_printf( "%s: *IN: r->filename    =[%s]\n", cn, r->filename);
    mod_gzip_printf( "%s: *IN: r->handler     =[%s]\n", cn, r->handler);
    mod_gzip_printf( "%s: r->finfo.st_size    = %ld\n", cn, (long) r->finfo.st_size);

    /* NOTE: The r->headers_out content type value has not normally */
    /* been set at this point but grab a pointer to it and show */
    /* it just to make sure. The r->content_type value, however, */
    /* normally WILL have some value at this point. */

    the_type = ap_table_get( r->headers_out,"Content-type" );

    mod_gzip_printf( "%s: r->headers_out, Content-type = [%s]\n",cn,the_type);
    mod_gzip_printf( "%s: r->content_type = [%s]\n",cn,r->content_type );

    /* The r->handler ASCII name string is the all-important */
    /* jump table name for the module that will handle the */
    /* transaction. If this is a CGI jump then it will normally */
    /* have a value of 'cgi-script' at this point. */

    mod_gzip_printf( "%s: r->handler      = [%s]\n",cn,r->handler );

    /* Server info... */

    mod_gzip_printf( "%s: r->server->path            = [%s]\n",cn,r->server->path );
    mod_gzip_printf( "%s: r->server->pathlen         = %d\n",  cn,r->server->pathlen);
    mod_gzip_printf( "%s: r->server->server_admin    = [%s]\n",cn,r->server->server_admin);
    mod_gzip_printf( "%s: r->server->server_hostname = [%s]\n",cn,r->server->server_hostname);
    mod_gzip_printf( "%s: r->server->error_fname     = [%s]\n",cn,r->server->error_fname);

    /* Environment info... */

    mod_gzip_printf( "%s: DOCUMENT_ROOT = [%s]\n",cn,ap_document_root(r));

    #endif /* MOD_GZIP_DEBUG1 */

    /*
     * Check the 'master' request control switch and see if mod_gzip
     * is ON (ENABLED) or OFF (DISABLED)...
     */

    if ( conf->req != 1 )
      {
       /* mod_gzip is currently DISABLED so DECLINE the processing... */

       #ifdef MOD_GZIP_DEBUG1
       mod_gzip_printf( "%s: conf->req = %d = OFF\n",cn,conf->req);
       mod_gzip_printf( "%s: The module is currently DISABLED\n",cn);
       mod_gzip_printf( "%s: Exit > return( DECLINED ) >\n",cn);
       #endif

       #ifdef MOD_GZIP_USES_APACHE_LOGS

       /* Each 'DECLINE' condition provides a short ':WHYTAG' for logs */

       ap_table_setn(
       r->notes,"mod_gzip_result",ap_pstrdup(r->pool,"DECLINED:DISABLED"));

       #endif /* MOD_GZIP_USES_APACHE_LOGS */

       return DECLINED;

      }/* End 'if( conf->req != 1 )' */

    /*
     * Check for a default HTTP support level ( if used ).
     * If no value for conf->min_http was supplied in the
     * httpd.conf file then the default value will be 0
     * so that ALL levels of HTTP will be OK...
     */

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: *HTTP CHECK:conf->min_http = %d\n",   cn, conf->min_http );
    mod_gzip_printf( "%s: *HTTP CHECK:r->proto_num   = %d\n",   cn, r->proto_num );
    mod_gzip_printf( "%s: *HTTP CHECK:r->protocol    = [%s]\n", cn, r->protocol );
    #endif

    if ( r->proto_num < conf->min_http )
      {
       /* The HTTPx/x version number does not meet the minimum requirement */

       #ifdef MOD_GZIP_DEBUG1
       mod_gzip_printf( "%s: Request HTTP level does not meet minimum requirement\n",cn);
       mod_gzip_printf( "%s: Exit > return( DECLINED ) >\n",cn);
       #endif

       #ifdef MOD_GZIP_USES_APACHE_LOGS

       /* Each 'DECLINE' condition provides a short ':WHYTAG' for logs */

       sprintf( log_info, "DECLINED:%s:%d", r->protocol, r->proto_num );

       ap_table_setn(
       r->notes,"mod_gzip_result",ap_pstrdup(r->pool,log_info));

       #endif /* MOD_GZIP_USES_APACHE_LOGS */

       return DECLINED;

      }/* End 'if ( r->proto_num < conf->min_http )' */

    else /* Protocol level is OK... */
      {
       #ifdef MOD_GZIP_DEBUG1
       mod_gzip_printf( "%s: Request HTTP level is OK...\n",cn);
       #endif
      }

    #ifdef MOD_GZIP_ALLOWS_INTERNAL_COMMANDS

    /*
     * Internal command pickups...
     *
     * If this module was compiled with the
     * MOD_GZIP_ALLOWS_INTERNAL_COMMANDS switch ON
     * then the first thing we do is check for valid
     * URL-based internal commands.
     *
     * Rather than check for all possible commands each time
     * just do 1 quick check for the command prefix and set
     * a flag to indicate if there is any need to enter the
     * actual command handler...
     */

    if ( strstr( r->filename, "mod_gzip_command_" ) )
      {
       do_command = 1; /* Process the command */
      }

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: do_command = %d\n",cn,do_command);
    #endif

    if ( do_command )
      {
       /* Determine the exact command and respond... */

       if ( strstr( r->filename, "mod_gzip_command_version" ) )
         {
          /*------------------------------------------------------*/
          /* Command: 'mod_gzip_command_version'                  */
          /* Purpose: Return the current mod_gzip version number. */
          /* Comment: Allows anyone to query any Apache Server at */
          /*          any URL with a browser and discover if      */
          /*          mod_gzip is in use at that site.            */
          /*------------------------------------------------------*/

          #ifdef MOD_GZIP_DEBUG1
          mod_gzip_printf( "%s: 'mod_gzip_command_version' seen...\n",cn);
          #endif

          /* NOTE: mod_gzip command results are not sent compressed */

          /* Build the response buffer... */

          sprintf( tmp,
          "<html><body><pre>"
          "mod_gzip is available on this Server\r\n"
          "mod_gzip version = %s\r\n"
          "</pre></body></html>",
          mod_gzip_version
          );

          /* For all mod_gzip commands that are intercepted we */
          /* simply return OK. */

          return( mod_gzip_send_html_command_response( r, tmp, "text/html" ));
         }
       else if ( strstr( r->filename, "mod_gzip_command_showstats" ) )
         {
          /*------------------------------------------------------*/
          /* Command: 'mod_gzip_command_showstats'                */
          /* Purpose: Display compression statistics.             */
          /* Comment: Allows anyone to query any Apache Server at */
          /*          any URL with a browser and get a report     */
          /*          about compression results.                  */
          /*------------------------------------------------------*/

          #ifdef MOD_GZIP_DEBUG1
          mod_gzip_printf( "%s: 'mod_gzip_command_showstats' seen...\n",cn);
          #endif

          /* NOTE: mod_gzip command results are not sent compressed */

          /* Build the response buffer... */

          /* NOTE: This command has been temporarily removed */

          sprintf( tmp,
          "<html><body><pre>"
          "mod_gzip is available on this Server\r\n"
          "mod_gzip version = %s\r\n\r\n"
          "The 'mod_gzip_command_showstats' command has been temporarily removed.\r\n"
          "</pre></body></html>",
          mod_gzip_version
          );

          /* For all mod_gzip commands that are intercepted we */
          /* simply return OK. */

          return( mod_gzip_send_html_command_response( r, tmp, "text/html" ));
         }
       else if ( strstr( r->filename, "mod_gzip_command_resetstats" ) )
         {
          /*------------------------------------------------------*/
          /* Command: 'mod_gzip_command_resetstats'               */
          /* Purpose: Resets the compression statistics.          */
          /* Comment: Allows the compression statistics to be     */
          /*          reset using only a browser.                 */
          /*------------------------------------------------------*/

          #ifdef MOD_GZIP_DEBUG1
          mod_gzip_printf( "%s: 'mod_gzip_command_resetstats' seen...\n",cn);
          #endif

          /* NOTE: mod_gzip command results are not sent compressed */

          /* Build the response buffer... */

          /* NOTE: This command has been temporarily removed */

          sprintf( tmp,
          "<html><body><pre>"
          "mod_gzip is available on this Server\r\n"
          "mod_gzip version = %s\r\n\r\n"
          "The 'mod_gzip_command_resetstats' command has been temporarily removed.\r\n"
          "</pre></body></html>",
          mod_gzip_version
          );

          /* For all mod_gzip commands that are intercepted we */
          /* simply return OK. */

          return( mod_gzip_send_html_command_response( r, tmp, "text/html" ));
         }
       else /* Unrecognized command... */
         {
          /* The command prefix was 'seen' and the 'do_command' flag */
          /* was TRUE but either the command was mis-typed or there */
          /* is no such command available. This is not an ERROR and */
          /* we should simply fall-through and assume that the URL */
          /* is valid for the local Server. A 404 will be returned */
          /* if there is no object that actually matches the name. */
         }

      }/* End 'if( do_command )' */

    #endif /* MOD_GZIP_ALLOWS_INTERNAL_COMMANDS */

    /*
     * Sanity checks...
     */

    /*
     * If the requested file already contains the .gz designation
     * then we must assume it is pre-compressed and let the
     * default logic take care of sending the file. This module
     * doesn't really care if a .gz file was actually requested
     * or if it is the source target because of a successful
     * Server side 'negotiation'. Doesn't matter.
     */

    if ( ( r->filename ) && ( strstr( r->filename, ".gz" ) ) )
      {
       #ifdef MOD_GZIP_DEBUG1
       mod_gzip_printf( "%s: r->filename already contains '.gz'.\n",cn);
       mod_gzip_printf( "%s: Pre-compression is assumed.\n",cn);
       mod_gzip_printf( "%s: Exit > return( DECLINED ) >\n",cn);
       #endif

       #ifdef MOD_GZIP_USES_APACHE_LOGS

       /* Each 'DECLINE' condition provides a short ':WHYTAG' for logs */

       ap_table_setn(
       r->notes,"mod_gzip_result",ap_pstrdup(r->pool,"DECLINED:HAS.GZ"));

       if ( r->server->loglevel == APLOG_DEBUG )
         {
          ap_log_error( "",0,APLOG_NOERRNO|APLOG_DEBUG, r->server,
          "mod_gzip: Files with .gz file extension are skipped.");
         }

       #endif /* MOD_GZIP_USES_APACHE_LOGS */

       return DECLINED;
      }
    else /* r->filename doesn not contain '.gz' designator... */
      {
       #ifdef MOD_GZIP_DEBUG1
       mod_gzip_printf( "%s: r->filename does NOT contain '.gz'.\n",cn);
       mod_gzip_printf( "%s: Assuming OK to proceed...\n",cn);
       #endif
      }

    /*
     * For now just block all attempts to compress 'image/*' MIME
     * type even if user is trying to do so. Too many issues with
     * broken browsers when it comes to decoding compressed images.
     *
     * WARNING: Don't submit r->content_type to strstr() it if is
     * NULL or the API call will GP fault. Go figure.
     */

    if ( ( r->content_type ) && ( strstr( r->content_type, "image/" ) ) )
      {
       #ifdef MOD_GZIP_DEBUG1
       mod_gzip_printf( "%s: r->content_type contains 'image/'.\n",cn);
       mod_gzip_printf( "%s: Image compression is temporaily BLOCKED\n",cn);
       mod_gzip_printf( "%s: Exit > return( DECLINED ) >\n",cn);
       #endif

       #ifdef MOD_GZIP_USES_APACHE_LOGS

       /* Each 'DECLINE' condition provides a short ':WHYTAG' for logs */

       ap_table_setn(
       r->notes,"mod_gzip_result",ap_pstrdup(r->pool,"DECLINED:IMAGE"));

       if ( r->server->loglevel == APLOG_DEBUG )
         {
          ap_log_error( "",0,APLOG_NOERRNO|APLOG_DEBUG, r->server,
          "mod_gzip: Graphics image compression option is temporarily disabled.");
         }

       #endif /* MOD_GZIP_USES_APACHE_LOGS */

       return DECLINED;
      }

    /*
     * Safeguard against situations where some other module or
     * filter has gotten to this request BEFORE us and has already
     * added the 'Content-encoding: gzip' field to the output header.
     * It must be assumed that whoever added the header prior to this
     * point also took care of the compression itself.
     *
     * If the output header already contains "Content-encoding: gzip"
     * then simply DECLINE the processing and let the default chain
     * take care of it...
     */

    has_encoding = ap_table_get( r->headers_out, "Content-encoding" );

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: has_encoding = [%s]\n",cn,has_encoding);
    #endif

    if ( has_encoding ) /* 'Content-encoding' field is present... */
      {
       #ifdef MOD_GZIP_DEBUG1
       mod_gzip_printf( "%s: Output header already contains 'Content-encoding:' field\n",cn);
       mod_gzip_printf( "%s: Checking for 'gzip' designator...\n",cn);
       #endif

       if ( strstr( has_encoding, "gzip" ) ||
			strstr( has_encoding, "deflate" ) )
         {
          #ifdef MOD_GZIP_DEBUG1
          mod_gzip_printf( "%s: 'Content-encoding:' field contains 'gzip' or 'deflate' designator...\n",cn);
          mod_gzip_printf( "%s: Pre-compression is assumed.\n",cn);
          mod_gzip_printf( "%s: Exit > return( DECLINED ) >\n",cn);
          #endif

          #ifdef MOD_GZIP_USES_APACHE_LOGS

          /* Each 'DECLINE' condition provides a short ':WHYTAG' for logs */

          ap_table_setn(
          r->notes,"mod_gzip_result",ap_pstrdup(r->pool,"DECLINED:HAS_CE:GZIP"));

          if ( r->server->loglevel == APLOG_DEBUG )
            {
             ap_log_error( "",0,APLOG_NOERRNO|APLOG_DEBUG, r->server,
             "mod_gzip: Header already has 'Content-encoding: gzip'");
            }

          #endif /* MOD_GZIP_USES_APACHE_LOGS */

          return DECLINED;
         }
       else /* 'gzip' designator not found... */
         {
          #ifdef MOD_GZIP_DEBUG1
          mod_gzip_printf( "%s: 'Content-encoding:' field does NOT contain 'gzip' or 'deflate' designator...\n",cn);
          mod_gzip_printf( "%s: Assuming OK to proceed...\n",cn);
          #endif
         }

      }/* End 'if( has_encoding )' */

    else /* Output header does NOT contain 'Content-encoding:' field... */
      {
       #ifdef MOD_GZIP_DEBUG1
       mod_gzip_printf( "%s: Output header does NOT contain 'Content-encoding:' field.\n",cn);
       mod_gzip_printf( "%s: Assuming OK to proceed...\n",cn);
       #endif
      }

    /*
     * Basic sanity checks completed and we are still here.
     *
     * Now we must determine if the User-Agent is capable of receiving
     * compressed data...
     *
     * There are, currently, many reasons why it is actually never
     * enough to simply trust the 'Accept-encoding: foo, bar'
     * request header field when it comes to actually determining
     * if a User-agent is capable of receiving content or transfer
     * encodings.
     *
     * Some of them are...
     *
     * 1. There have been several major releases of popular browsers
     *    that actually send the 'Accept-encoding:' request field but
     *    are, in reality, unable to perform the specified decoding(s).
     *    In some cases the result will be that the browser screen
     *    simply fills with garbage ( the binary compressed data
     *    itself ) but in some cases the browser will actually crash.
     *
     * 2. There have been other major releases of browsers that are
     *    specifying multiple 'Accept-encoding' techniques with no
     *    'Q' values whatsoever and they are actually only able to
     *    handle one of the multiple types specified. There is no
     *    way to know which type is 'real' other than by using other
     *    empiricial data extracted from the 'User-agent' field
     *    or other inbound request headers.
     *
     * 3. Same as 1 and 2 but relates to SIZE. Some major browser
     *    releases can handle the encoded content but only up to
     *    a certain 'SIZE' limit and then they will fail. There
     *    is no way for a User-agent to specify this limitation
     *    via HTTP so empirical header analysis is the only option.
     *
     * 4. The HTTP specification has no way for a Server to distinguish
     *    from the 'Accept encoding: foo, bar' input request field
     *    whether the user agent can only support the specified encodings
     *    as either a Content-encoding OR a Transfer-encoding, but
     *    not both. There is also no way of knowing if the user
     *    agent is able to handle any of the specified types being
     *    used as both a Content-encoding AND a Transfer-encoding
     *    for the same message body. All the Server can do is assume
     *    that the encodings are valid in any/all combinations
     *    and that the user agent can 'Accept' them as either
     *    'Content' encodings and/or 'Transfer' encodings under
     *    any and all circumstances. This blanket assumption will
     *    cause problems with some release versions of some browsers
     *    because the assumed 'do all' capability is simply not a
     *    reality.
     *
     * 5. Many browsers ( such as Netscape 4.75 for UNIX ) are unable
     *    to handle Content-encoding only for specific kinds of HTML
     *    transactions such as Style Sheets even though the browser
     *    says it is HTTP 1.1 compliant and is suppying the standard
     *    'Accept-encoding: gzip' field. According to the IETF
     *    specifications any user-agent that says it can accept
     *    encodings should be able to do so for all types of HTML
     *    transactions but this is simply not the current reality.
     *    Some will, some won't... even if they say they can.
     *
     * This version of this module takes the 'What, me worry' approach
     * and simply uses the accepted method of relying solely on the
     * 'Accept-encoding: foo, bar' field and also assumes this means
     * that the User-agent can accept the specified encodings as
     * either Content-encodings (CE) and/or Transfer-encodings (TE)
     * under all circumstances and in any combinations that the
     * Server decides to send.
     *
     * It also assumes that the caller has no preference and should
     * be able to decode any of the specified encodings equally well.
     * Most user-agents sending the 'Accept-encoding:' field do NOT
     * supply any 'Q' values to help with determining preferences.
     */

    accept_encoding = ap_table_get( r->headers_in, "Accept-Encoding" );

    #ifdef MOD_GZIP_DEBUG1

    if ( accept_encoding )
      {
       mod_gzip_printf( "%s: 'Accept Encoding:' field seen.\n",cn);
      }
    else
      {
       mod_gzip_printf( "%s: 'Accept Encoding' field NOT seen.\n",cn);
      }

    #endif /* MOD_GZIP_DEBUG1 */

    /* If Accept-Encoding is applicable to this request...*/

    if ( accept_encoding )
      {
       /* ...and if it has the right 'gzip' indicator... */
	   /* We record the compression format in a request note, so we
        * can get it again later, and so it can potentially be logged.
        */
       if ( strstr( accept_encoding, "gzip" ) )
         {
          process = 1; /* ...set the 'process' flag TRUE */
          ap_table_setn( r->notes,"mod_gzip_compression_format",
						 ap_pstrdup(r->pool,"gzip"));

         }
       else if ( strstr( accept_encoding, "deflate" ) )
         {
          process = 1; /* ...set the 'process' flag TRUE */
          ap_table_setn( r->notes,"mod_gzip_compression_format",
						 ap_pstrdup(r->pool,"deflate"));
         }

      }/* End 'if( accept_encoding )' */

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: 'process' flag = %d\n",cn,process);
    #endif

    if ( !process ) /* Request does not meet criteria for processing... */
      {
       #ifdef MOD_GZIP_DEBUG1
       mod_gzip_printf( "%s: No 'gzip' capability specified by user-agent.\n",cn);
       mod_gzip_printf( "%s: 'process' flag is FALSE.\n",cn);
       mod_gzip_printf( "%s: This request will not be processed.\n",cn);
       mod_gzip_printf( "%s: Exit > return( DECLINED ) >\n",cn);
       #endif

       #ifdef MOD_GZIP_USES_APACHE_LOGS

       /* Each 'DECLINE' condition provides a short ':WHYTAG' for logs */

       ap_table_setn(
       r->notes,"mod_gzip_result",ap_pstrdup(r->pool,"DECLINED:NO_GZIP"));

       if ( r->server->loglevel == APLOG_DEBUG )
         {
          ap_log_error( "",0,APLOG_NOERRNO|APLOG_DEBUG, r->server,
          "mod_gzip: The inbound request header does not have 'Accept-encoding: gzip'");
         }

       #endif /* MOD_GZIP_USES_APACHE_LOGS */

       return DECLINED;
      }
    else /* 'gzip' designator was seen in 'Accept-Encoding:' field */
      {
       #ifdef MOD_GZIP_DEBUG1
       mod_gzip_printf( "%s: 'gzip' or 'deflate' capability specified by user-agent.\n",cn);
       mod_gzip_printf( "%s: Assuming OK to proceed...\n",cn);
       #endif
      }

    /*
     * Handle the transaction...
     *
     * At this point the inbound header analysis has been completed
     * and we are assuming that the user agent is capable of accepting
     * the content encodings we can provide.
     *
     * We must now 'do the right thing' based on what type of
     * request it actually is...
     */

     #ifdef MOD_GZIP_DEBUG1
     mod_gzip_printf( "%s: r->handler      = [%s]\n",cn,r->handler);
     mod_gzip_printf( "%s: r->content_type = [%s]\n",cn,r->content_type);
     mod_gzip_printf( "%s: Call mod_gzip_get_action_flag()...\n",cn);
     #endif

     action_flag =
     mod_gzip_get_action_flag(
     (request_rec   *) r,
     (mod_gzip_conf *) conf
     );

     #ifdef MOD_GZIP_DEBUG1
     mod_gzip_printf( "%s: Back mod_gzip_get_action_flag()...\n",cn);
     mod_gzip_printf( "%s: action_flag           = %d\n",cn,action_flag);
     mod_gzip_printf( "%s: conf->do_static_files = %d\n",cn,(int)conf->do_static_files);
     mod_gzip_printf( "%s: conf->do_cgi          = %d\n",cn,(int)conf->do_cgi);
     #endif

     /*
      * Perform the right 'action' for this transaction...
      */

     if ( action_flag == MOD_GZIP_IMAP_DECLINED1 )
       {
        /*
         * If the transaction is to be DECLINED then just set the final
         * return code to DECLINED, fall through, and return.
         */

        #ifdef MOD_GZIP_DEBUG1
        mod_gzip_printf( "%s: action_flag = MOD_GZIP_IMAP_DECLINED1\n",cn);
        #endif

        if ( r->server->loglevel == APLOG_DEBUG )
          {
           ap_log_error( "",0,APLOG_NOERRNO|APLOG_DEBUG, r->server,
           "mod_gzip: action_flag = MOD_GZIP_IMAP_DECLINED1 ");
          }

        rc = DECLINED;
       }
     else if ( action_flag == MOD_GZIP_IMAP_DYNAMIC1 )
       {
        #ifdef MOD_GZIP_DEBUG1
        mod_gzip_printf( "%s: action_flag = MOD_GZIP_IMAP_DYNAMIC1\n",cn);
        #endif

        if ( r->server->loglevel == APLOG_DEBUG )
          {
           ap_log_error( "",0,APLOG_NOERRNO|APLOG_DEBUG, r->server,
           "mod_gzip: action_flag = MOD_GZIP_IMAP_DYNAMIC1 ");
          }

        /*
         * Check the flag that can control whether or not the
         * CGI dynamic output handler is ever called...
         */

        if ( conf->do_cgi != 1 ) /* CGI handler is OFF for now... */
          {
           if ( r->server->loglevel == APLOG_DEBUG )
             {
              ap_log_error( "",0,APLOG_NOERRNO|APLOG_DEBUG, r->server,
              "mod_gzip: Calls to CGI handler currently DISABLED ");
             }

           #ifdef MOD_GZIP_USES_APACHE_LOGS
           /* Update the result string for Apache log(s)... */
           ap_table_setn(
           r->notes,"mod_gzip_result",ap_pstrdup(r->pool,"DECLINED:CGI_OFF"));
           #endif

           rc = DECLINED; /* Just set final return code and fall through */

          }/* End 'if( conf->do_cgi == 0 )' */

        else /* It's OK to call the handler... */
          {
           if ( r->server->loglevel == APLOG_DEBUG )
             {
              ap_log_error( "",0,APLOG_NOERRNO|APLOG_DEBUG, r->server,
              "mod_gzip: Calling cgi_handler for r->uri=[%s]",r->uri);
             }

           /* Take care of some business BEFORE calling the */
           /* dynamic handler... */

           mod_gzip_prepare_for_dynamic_call( r );

           /* PHP NOTE */
           /* r->path_info must be set before ap_add_cgi_vars() */
           /* is called from within the upcoming hander or we */
           /* won't get PATH_INFO or PATH_TRANSLATED environment */
           /* variables set and PHP.EXE will return 'No input file' */
           /* error message since it depends on both of these being */
           /* set. r->path_info must be set to r->uri */

           #ifdef MOD_GZIP_DEBUG1
           mod_gzip_printf( "%s: 1 r->uri       = [%s]\n", cn, r->uri );
           mod_gzip_printf( "%s: 1 r->path_info = [%s]\n", cn, r->path_info );
           mod_gzip_printf( "%s: Setting r->path_info to r->uri for CGI...\n", cn );
           #endif

           r->path_info = r->uri;

           #ifdef MOD_GZIP_DEBUG1
           mod_gzip_printf( "%s: 2 r->uri       = [%s]\n", cn, r->uri );
           mod_gzip_printf( "%s: 2 r->path_info = [%s]\n", cn, r->path_info );
           #endif

           /* Call the actual handler... */

           #ifdef MOD_GZIP_DEBUG1
           mod_gzip_printf( "%s: Call mod_gzip_cgi_handler()...\n",cn);
           #endif

           rc = mod_gzip_cgi_handler( (request_rec *) r );

           #ifdef MOD_GZIP_DEBUG1
           mod_gzip_printf( "%s: Back mod_gzip_cgi_handler()... rc=%d\n",cn,rc);
           #endif

          }/* End 'else' - OK to call handler */
       }
     else if ( action_flag == MOD_GZIP_IMAP_STATIC1 )
       {
        #ifdef MOD_GZIP_DEBUG1
        mod_gzip_printf( "%s: action_flag = MOD_GZIP_IMAP_STATIC1\n",cn);
        #endif

        if ( r->server->loglevel == APLOG_DEBUG )
          {
           ap_log_error( "",0,APLOG_NOERRNO|APLOG_DEBUG, r->server,
           "mod_gzip: action_flag = MOD_GZIP_IMAP_STATIC1 ");
          }

        /*
         * Check the flag that can control whether or not the
         * static handler is ever called...
         */

        if ( conf->do_static_files != 1 ) /* Static handler is OFF for now... */
          {
           if ( r->server->loglevel == APLOG_DEBUG )
             {
              ap_log_error( "",0,APLOG_NOERRNO|APLOG_DEBUG, r->server,
              "mod_gzip: Calls to static handler currently DISABLED ");
             }

           #ifdef MOD_GZIP_USES_APACHE_LOGS
           /* Update the result string for Apache log(s)... */
           ap_table_setn(
           r->notes,"mod_gzip_result",ap_pstrdup(r->pool,"DECLINED:STATIC_OFF"));
           #endif

           rc = DECLINED; /* Just set final return code and fall through */

          }/* End 'if( conf->do_static == 0 )' */

        else /* It's OK to call the handler... */
          {
           if ( r->server->loglevel == APLOG_DEBUG )
             {
              ap_log_error( "",0,APLOG_NOERRNO|APLOG_DEBUG, r->server,
              "mod_gzip: Calling static_handler for r->uri=[%s]",r->uri);
             }

           #ifdef MOD_GZIP_DEBUG1
           mod_gzip_printf( "%s: Call mod_gzip_static_file_handler()...\n",cn);
           #endif

           rc = mod_gzip_static_file_handler( (request_rec *) r );

           #ifdef MOD_GZIP_DEBUG1
           mod_gzip_printf( "%s: Back mod_gzip_static_file_handler()... rc=%d\n",cn,rc);
           #endif

          }/* End 'else' - OK to call the handler */
       }
     else /* Safety catch... No pickup for the 'action' flag... */
       {
        if ( r->server->loglevel == APLOG_DEBUG )
          {
           ap_log_error( "",0,APLOG_NOERRNO|APLOG_DEBUG, r->server,
           "mod_gzip: action_flag = MOD_GZIP_IMAP_????? Unknown value");
          }

        if ( r->server->loglevel == APLOG_DEBUG )
          {
           ap_log_error( "",0,APLOG_NOERRNO|APLOG_DEBUG, r->server,
           "mod_gzip: No pickup for specified 'action' flag.");
          }

        #ifdef MOD_GZIP_DEBUG1
        mod_gzip_printf( "%s: action_flag = MOD_GZIP_??? Unknown value\n",cn);
        #endif

        rc = DECLINED;
       }

     /*
      * Record results to logs, if applicable, and return...
      *
      * The 'r->notes' values that can be used to disply result
      * information in the standard Apache log files should have
      * already been updated by the handler that was actually
      * used to process the transaction.
      */

     #ifdef MOD_GZIP_DEBUG1

     if ( rc == OK )
       {
        mod_gzip_printf( "%s: Exit > return( rc=%d OK ) >\n",cn,rc);
       }
     else if ( rc == DECLINED )
       {
        mod_gzip_printf( "%s: Exit > return( rc=%d DECLINED ) >\n",cn,rc);
       }
     else /* HTTP ERROR VALUE... */
       {
        mod_gzip_printf( "%s: Exit > return( rc=%d HTTP_ERROR ) >\n",cn,rc);
       }

     #endif /* MOD_GZIP_DEBUG1 */

     return rc; /* Could be OK or DECLINED or HTTP_ERROR */

}/* End of mod_gzip_request_handler() */

int mod_gzip_prepare_for_dynamic_call( request_rec *r )
{
    int rc;

    #ifdef MOD_GZIP_DEBUG1
    char cn[]="mod_gzip_prepare_for_dynamic_call()";
    #endif

    /*
     * Start...
     */

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: Entry...\n",cn);
    #endif

    /*
     * mod_gzip can run other modules directly...
     */

    /*
     * First run mod_action and see it there's a SCRIPT
     * for this mime type...
     */

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: 1 ***: r->uri         =[%s]\n", cn, r->uri );
    mod_gzip_printf( "%s: 1 ***: r->unparsed_uri=[%s]\n", cn, r->unparsed_uri );
    mod_gzip_printf( "%s: 1 ***: r->filename    =[%s]\n", cn, r->filename );
    mod_gzip_printf( "%s: 1 ***: r->content_type=[%s]\n", cn, r->content_type );
    mod_gzip_printf( "%s: 1 ***: r->handler     =[%s]\n", cn, r->handler );
    mod_gzip_printf( "%s: Call mod_gzip_run_mod_action(r)...\n",cn);
    #endif

    rc = mod_gzip_run_mod_action( (request_rec *) r  );

    #ifdef MOD_GZIP_DEBUG1

    mod_gzip_printf( "%s: Back mod_gzip_run_mod_action(r)...\n",cn);

    if ( rc == OK )
      {
       mod_gzip_printf( "%s: rc = %d = OK\n",cn, rc );
      }
    else if ( rc == DECLINED )
      {
       mod_gzip_printf( "%s: rc = %d = DECLINED\n",cn, rc );
      }
    else if ( rc == DONE ) /* -2 means 'totally done' */
      {
       mod_gzip_printf( "%s: rc = %d = DONE\n",cn, rc );
      }
    else /* Probably an HTTP ERROR value... */
      {
       mod_gzip_printf( "%s: rc = %d = HTTP_ERROR?\n",cn, rc );
      }

    mod_gzip_printf( "%s: 2 ***: r->uri         =[%s]\n", cn, r->uri );
    mod_gzip_printf( "%s: 2 ***: r->unparsed_uri=[%s]\n", cn, r->unparsed_uri );
    mod_gzip_printf( "%s: 2 ***: r->filename    =[%s]\n", cn, r->filename );
    mod_gzip_printf( "%s: 2 ***: r->content_type=[%s]\n", cn, r->content_type );
    mod_gzip_printf( "%s: 2 ***: r->handler     =[%s]\n", cn, r->handler );

    #endif /* MOD_GZIP_DEBUG1 */

    /*
     * Now run mod_alias and get any aliases converted
     * to real pathnames...
     */

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: Call mod_gzip_run_mod_alias(r)...\n",cn);
    #endif

    rc = mod_gzip_run_mod_alias( (request_rec *) r  );

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: Back mod_gzip_run_mod_alias(r)...\n",cn);

    if ( rc == OK )
      {
       mod_gzip_printf( "%s: rc = %d = OK\n",cn, rc );
      }
    else if ( rc == DECLINED )
      {
       mod_gzip_printf( "%s: rc = %d = DECLINED\n",cn, rc );
      }
    else if ( rc == DONE ) /* -2 means 'totally done' */
      {
       mod_gzip_printf( "%s: rc = %d = DONE\n",cn, rc );
      }
    else /* Probably an HTTP ERROR value... */
      {
       mod_gzip_printf( "%s: rc = %d = HTTP_ERROR?\n",cn, rc );
      }

    mod_gzip_printf( "%s: 3 ***: r->uri         =[%s]\n", cn, r->uri );
    mod_gzip_printf( "%s: 3 ***: r->unparsed_uri=[%s]\n", cn, r->unparsed_uri );
    mod_gzip_printf( "%s: 3 ***: r->filename    =[%s]\n", cn, r->filename );
    mod_gzip_printf( "%s: 3 ***: r->content_type=[%s]\n", cn, r->content_type );
    mod_gzip_printf( "%s: 3 ***: r->handler     =[%s]\n", cn, r->handler );

    #endif /* MOD_GZIP_DEBUG1 */

    return OK;

}/* End of mod_gzip_prepare_for_dynamic_call() */


int mod_gzip_static_file_handler( request_rec *r )
{
    int             rc         = 0;
    long            input_size = 0;
    FILE*           ifh1       = 0;

    #ifdef MOD_GZIP_DEBUG1
    char cn[]="mod_gzip_static_file_handler()";
    #endif

    /*
     * Start...
     */

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: Processing file [%s]\n",cn,r->filename);
    mod_gzip_printf( "%s: r->finfo.st_size = %ld\n",
               cn, (long) r->finfo.st_size);
    #endif

    /*
     * If there is a valid file size already associated with
     * the request then we can assume the core stat() call succeeded
     * and that r->filename actually exists. We shouldn't need
     * to waste a call to 'fopen()' just to find out for ourselves
     * if the file exists.
     *
     * If the inbound file size was '0' then we need to do some
     * verifications of our own before we give up since the
     * absence of size might just be a simple bug in the parent code.
     */

    if ( r->finfo.st_size > 0 )
      {
       #ifdef MOD_GZIP_DEBUG1
       mod_gzip_printf( "%s: Source file length already known...\n",cn);
       #endif

       input_size = (long) r->finfo.st_size;
      }
    else /* Do our own checking... */
      {
       /*
        * See if the requested source file exists...
        * Be SURE to open the file in BINARY mode...
        */

       ifh1 = fopen( r->filename, "rb" );

       if ( !ifh1 ) /* The file cannot be found or opened... */
         {
          #ifdef MOD_GZIP_DEBUG1
          mod_gzip_printf( "%s: The requested source file was NOT FOUND.\n",cn);
          mod_gzip_printf( "%s: Exit > return( HTTP_NOT_FOUND ) >\n",cn);
          #endif

          #ifdef MOD_GZIP_USES_APACHE_LOGS

          /* HTTP ERROR conditions provides a short ':WHYTAG' for logs */

          ap_table_setn(
          r->notes,"mod_gzip_result",ap_pstrdup(r->pool,"DECLINED:HTTP_NOT_FOUND"));

          #endif /* MOD_GZIP_USES_APACHE_LOGS */

          return HTTP_NOT_FOUND;
         }
       else /* The file was found and opened OK... */
         {
          #ifdef MOD_GZIP_DEBUG1
          mod_gzip_printf( "%s: The requested source file is now OPEN...\n",cn);
          #endif
         }

       /*
        * Move the current file pointer to the end of the file...
        */

       if ( fseek( ifh1, 0, SEEK_END ) )
         {
          #ifdef MOD_GZIP_DEBUG1
          mod_gzip_printf( "%s: ERROR: fseek() call failed...\n",cn);
          #endif

          fclose( ifh1 ); /* FILE is still open so CLOSE it... */

          /* fseek() failure could be a platform issue so log the event... */

          ap_log_error( APLOG_MARK,APLOG_NOERRNO|APLOG_ERR, r->server,
          "mod_gzip: fseek() failed for r->filename=[%s]",r->filename );

          /* Return DECLINED and let default logic finish the request... */

          #ifdef MOD_GZIP_DEBUG1
          mod_gzip_printf( "%s: Exit > return( DECLINED ) >\n",cn);
          #endif

          #ifdef MOD_GZIP_USES_APACHE_LOGS

          /* Each 'DECLINE' condition provides a short ':WHYTAG' for logs */

          ap_table_setn(
          r->notes,"mod_gzip_result",ap_pstrdup(r->pool,"DECLINED:FSEEK_FAIL"));

          #endif /* MOD_GZIP_USES_APACHE_LOGS */

          return DECLINED;
         }

       /*
        * Get the current SIZE of the requested file...
        */

       input_size = (long) ftell( ifh1 );

       if ( input_size == -1l )
         {
          #ifdef MOD_GZIP_DEBUG1
          mod_gzip_printf( "%s: ERROR: ftell() call failed...\n",cn);
          #endif

          fclose( ifh1 ); /* FILE is still open so CLOSE it... */

          /* ftell() failure could be a platform issue so log the event... */

          ap_log_error( APLOG_MARK,APLOG_NOERRNO|APLOG_ERR, r->server,
          "mod_gzip: ftell() failed for r->filename=[%s]", r->filename );

          /* Return DECLINED and let default logic finish the request... */

          #ifdef MOD_GZIP_DEBUG1
          mod_gzip_printf( "%s: Exit > return( DECLINED ) >\n",cn);
          #endif

          #ifdef MOD_GZIP_USES_APACHE_LOGS

          /* Each 'DECLINE' condition provides a short ':WHYTAG' for logs */

          ap_table_setn(
          r->notes,"mod_gzip_result",ap_pstrdup(r->pool,"DECLINED:FTELL_FAIL"));

          #endif /* MOD_GZIP_USES_APACHE_LOGS */

          return DECLINED;
         }

       /*
        * Once we have the length just close the file...
        */

       if ( fclose( ifh1 ) == EOF )
         {
          #ifdef MOD_GZIP_DEBUG1
          mod_gzip_printf( "%s: ERROR: fclose() following ftell() call failed...\n",cn);
          #endif

          /* fclose() failure could be a platform issue so log the event... */

          ap_log_error( APLOG_MARK,APLOG_NOERRNO|APLOG_ERR, r->server,
          "mod_gzip: fclose() failed for r->filename=[%s]",r->filename );

          /* Return DECLINED and let default logic finish the request... */

          #ifdef MOD_GZIP_DEBUG1
          mod_gzip_printf( "%s: Exit > return( DECLINED ) >\n",cn);
          #endif

          #ifdef MOD_GZIP_USES_APACHE_LOGS

          /* Each 'DECLINE' condition provides a short ':WHYTAG' for logs */

          ap_table_setn(
          r->notes,"mod_gzip_result",ap_pstrdup(r->pool,"DECLINED:FCLOSE_FAIL"));

          #endif /* MOD_GZIP_USES_APACHE_LOGS */

          return DECLINED;
         }

      }/* End 'else' */

    /*
     * We have the static filename and the length.
     * That's pretty much all we need at this point so
     * go ahead and encode/transmit the object...
     */

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: Call mod_gzip_encode_and_transmit()...\n",cn);
    #endif

    rc =
    mod_gzip_encode_and_transmit(
    (request_rec *) r,           /* request_rec */
    (char        *) r->filename, /* source ( Filename or Memory buffer ) */
    (int          ) 1,           /* 1=Source is a file 0=Memory buffer */
    (long         ) input_size,  /* input_size */
    (int          ) 0            /* nodecline flag 0=Ok to DECLINE 1=No */
    );

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: Back mod_gzip_encode_and_transmit()...\n",cn);
    #endif

    /*
     * The encode/transmit path should have already updated
     * any relevant 'r->note' values ( if used ) for the transaction
     * to reflect the results of the operation.
     *
     * Just return the result code and finish the transaction.
     */

    #ifdef MOD_GZIP_DEBUG1
    if ( rc == OK )
      {
       mod_gzip_printf( "%s: Exit > return( rc = %d OK ) >\n",cn,rc);
      }
    else if ( rc == DECLINED )
      {
       mod_gzip_printf( "%s: Exit > return( rc = %d DECLINED ) >\n",cn,rc);
      }
    else /* HTTP ERROR */
      {
       mod_gzip_printf( "%s: Exit > return( rc = %d HTTP_ERROR ) >\n",cn,rc);
      }
    #endif /* MOD_GZIP_DEBUG1 */

    return( rc );

}/* End of mod_gzip_static_file_handler() */

int mod_gzip_create_unique_filename(
mod_gzip_conf *mgc,
char *target,
int targetmaxlen
)
{
 /*
  * Creates a unique work file name.
  */

 long  process_id = 0;  /* Current Process ID */
 long  thread_id  = 0;  /* Current thread  ID */

 #ifdef MOD_GZIP_DEBUG1
 char cn[]="mod_gzip_create_unique_filename()";
 #endif

 /* Start... */

 #ifdef WIN32
 process_id = (long) GetCurrentProcessId();
 thread_id  = (long) GetCurrentThreadId();
 #else /* !WIN32 */
 process_id = (long) getpid();
 thread_id  = (long) process_id; /* TODO: Add pthreads call */
 #endif /* WIN32 */

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: Entry...\n",cn );
 mod_gzip_printf( "%s: target            = %ld\n",cn,(long)target);
 mod_gzip_printf( "%s: targetmaxlen      = %ld\n",cn,(long)targetmaxlen);
 mod_gzip_printf( "%s: process_id        = %ld\n",cn,(long)process_id );
 mod_gzip_printf( "%s: thread_id         = %ld\n",cn,(long)thread_id  );
 mod_gzip_printf( "%s: mod_gzip_iusn     = %ld\n",cn,(long)mod_gzip_iusn );
 #endif

 /*
  * Sanity checks...
  */

 if ( ( !target ) || ( targetmaxlen == 0 ) )
   {
    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: Invalid target or targetmaxlen value.\n",cn);
    mod_gzip_printf( "%s: Exit > return( 1 ) > ERROR >\n",cn );
    #endif

    return 1;
   }

 /*
  * Use the PROCESS + THREAD ID's and the current IUSN
  * ( Instance Unique Sequence Number ) transaction ID to
  * create a one-time only unique output workfile name...
  */

 sprintf(
 target,
 "%s%s_%ld_%ld_%ld.wrk",
 mgc->cache.root,     /* Either ServerRoot or Config specified dir. */
 mod_gzip_dirsep,     /* Forward slash for UNIX, backslash for WIN32 */
 (long) process_id,   /* Current process ID */
 (long) thread_id,    /* Current thread  ID */
 (long) mod_gzip_iusn /* Instance Unique Sequence Number */
 );

 mod_gzip_iusn++; /* Increment Instance Unique Sequence Number */

 if ( mod_gzip_iusn > 999999999L ) mod_gzip_iusn = 1; /* Wrap */

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: target = [%s]\n",cn,target);
 mod_gzip_printf( "%s: Exit > return( 0 ) >\n",cn );
 #endif

 return 0;

}/* End of mod_gzip_create_unique_filename() */


#ifdef MOD_GZIP_ALLOWS_INTERNAL_COMMANDS

int mod_gzip_send_html_command_response(
request_rec *r, /* Request record */
char *tmp,      /* Response to send */
char *ctype     /* Content type string */
)
{
 /* Generic command response transmitter... */

 int  tmplen=0;
 char content_length[20];

 #ifdef MOD_GZIP_DEBUG1
 char cn[]="mod_gzip_send_html_command_response()";
 #endif

 /* Start... */

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: Entry...\n",cn);
 mod_gzip_printf( "%s: ctype=[%s]\n",cn,ctype);
 #endif

 /* Add the length of the response to the output header... */
 /* The third parameter to ap_table_set() MUST be a string. */

 tmplen = strlen( tmp );

 sprintf( content_length, "%d", tmplen );

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: content_length = [%s]\n",cn,content_length);
 #endif

 ap_table_set( r->headers_out, "Content-Length", content_length );

 /* Make sure the content type matches this response... */

 r->content_type = ctype; /* Actual type passed by caller */

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: r->content_type = [%s]\n",cn,r->content_type);
 #endif

 /* Start a timer... */

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: Call ap_soft_timeout()...\n",cn);
 #endif

 ap_soft_timeout( "mod_gzip_send_html_command", r );

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: Back ap_soft_timeout()...\n",cn);
 #endif
    
 #ifdef MOD_GZIP_COMMANDS_USE_LAST_MODIFIED

 /* Be sure to update the modifcation 'time' to current */
 /* time before calling 'ap_set_last_modified()'. All that */
 /* call does is set the r->xxxx value into the output */
 /* header. It doesn't actually update the time itself. */

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: Call ap_update_mtime(r,r-finfo.st_mtime)...\n",cn);
 #endif

 ap_update_mtime( r, r->finfo.st_mtime );

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: Back ap_update_mtime(r,r-finfo.st_mtime)...\n",cn);
 #endif

 /* Update the 'Last modified' stamp in output header... */

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: Call ap_set_last_modified()...\n",cn);
 #endif

 ap_set_last_modified(r);

 /* TODO: Add 'no-cache' option(s) to mod_gzip command responses */
 /* so user doesn't have hit reload to get fresh data. */

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: Back ap_set_last_modified()...\n",cn);
 #endif

 #endif /* MOD_GZIP_COMMANDS_USE_LAST_MODIFIED */

 /* Send the HTTP response header... */

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: Call ap_send_http_header()...\n",cn);
 #endif

 ap_send_http_header(r);

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: Back ap_send_http_header()...\n",cn);
 #endif

 /* Send the response BODY... */

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: Sending response...\n%s\n",cn,tmp);
 #endif

 #ifdef MOD_GZIP_USES_AP_SEND_MMAP

 /* Use ap_send_mmap() call to send the data... */

 ap_send_mmap( tmp, r, 0, tmplen );

 #else /* !MOD_GZIP_USES_AP_SEND_MMAP */

 /* Use ap_rwrite() call to send the data... */

 ap_rwrite( tmp, tmplen, r );

 #endif /* MOD_GZIP_USES_AP_SEND_MMAP */

 /* Clean up and exit... */

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: Call ap_kill_timeout()...\n",cn);
 #endif

 ap_kill_timeout(r);

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: Back ap_kill_timeout()...\n",cn);
 mod_gzip_printf( "%s: Exit > return( OK ) >\n",cn);
 #endif

 return OK;

}/* End of mod_gzip_send_html_command_response() */

#endif /* MOD_GZIP_ALLOWS_INTERNAL_COMMANDS */

static void *
mod_gzip_create_config( pool *p, server_rec *s )
{
    int i;

    mod_gzip_conf *ps = 0;

    #ifdef MOD_GZIP_DEBUG1
    char cn[]="mod_gzip_create_config()";
    #endif

    /*
     * Set all the configuration default values...
     */

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: Entry\n", cn );
    #endif

    /*
     * Allocate a new config structure...
     */

    ps = ( mod_gzip_conf * ) ap_pcalloc( p, sizeof( mod_gzip_conf ) );

    /*
     * Set all default values...
     */

    ps->req                = 1; /* Default is ON */
    ps->req_set            = 1; /* Default is ON */
    ps->do_static_files    = 1; /* Default is ON */
    ps->do_cgi             = 1; /* Default is ON */
    ps->keep_workfiles     = 0; /* 1=Keep workfiles 0=No */
    ps->min_http           = 0; /* 1001=HTTP/1.1 Default=All HTTP levels */

    ps->minimum_file_size  = (long) mod_gzip_minimum_file_size;
                             /* Minimum file size in bytes */
    ps->maximum_inmem_size = (long) mod_gzip_maximum_inmem_size;
                             /* Maximum size for in-memory compression */

    /* Compressed object cache control variables... */

    /* Using these default values the compressed object cache
    /* can have 2^18 directories (256,000) */

    ps->cache.root = ap_server_root; /* Default DIR is ServerRoot */

    ps->cache.space                = MOD_GZIP_DEFAULT_CACHE_SPACE;
    ps->cache.space_set            = 0;
    ps->cache.maxexpire            = MOD_GZIP_DEFAULT_CACHE_MAXEXPIRE;
    ps->cache.maxexpire_set        = 0;
    ps->cache.defaultexpire        = MOD_GZIP_DEFAULT_CACHE_EXPIRE;
    ps->cache.defaultexpire_set    = 0;
    ps->cache.lmfactor             = MOD_GZIP_DEFAULT_CACHE_LMFACTOR;
    ps->cache.lmfactor_set         = 0;
    ps->cache.gcinterval           = -1;
    ps->cache.gcinterval_set       = 0;
    ps->cache.dirlevels            = 3;
    ps->cache.dirlevels_set        = 0;
    ps->cache.dirlength            = 1;
    ps->cache.dirlength_set        = 0;

    /* Initialize the include/exclude item map list... */

    /* For now all init values are ZERO but don't use */
    /* memset() since this may not always be the case. */

    ps->imap_total_entries = 0;

    for ( i=0; i<MOD_GZIP_IMAP_MAXNAMES; i++ )
       {
        ps->imap[i].include = 0;
        ps->imap[i].type    = 0;
        ps->imap[i].action  = 0;
        ps->imap[i].name[0] = 0;

       }/* End 'i' loop */

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: ps->imap_total_entries = %d\n", cn, ps->imap_total_entries );
    mod_gzip_printf( "%s: Exit > return( ps ) >\n", cn );
    #endif

    return ps;

}/* End of mod_gzip_create_config() */

static void *
mod_gzip_merge_config( pool *p, void *basev, void *overridesv )
{
    mod_gzip_conf *ps        = ap_pcalloc(p, sizeof(mod_gzip_conf));
    mod_gzip_conf *base      = (mod_gzip_conf *) basev;
    mod_gzip_conf *overrides = (mod_gzip_conf *) overridesv;

    ps->req                  = (overrides->req_set == 0) ? base->req : overrides->req;
    ps->cache.root           = (overrides->cache.root == NULL) ? base->cache.root : overrides->cache.root;
    ps->cache.space          = (overrides->cache.space_set == 0) ? base->cache.space : overrides->cache.space;
    ps->cache.maxexpire      = (overrides->cache.maxexpire_set == 0) ? base->cache.maxexpire : overrides->cache.maxexpire;
    ps->cache.defaultexpire  = (overrides->cache.defaultexpire_set == 0) ? base->cache.defaultexpire : overrides->cache.defaultexpire;
    ps->cache.lmfactor       = (overrides->cache.lmfactor_set == 0) ? base->cache.lmfactor : overrides->cache.lmfactor;
    ps->cache.gcinterval     = (overrides->cache.gcinterval_set == 0) ? base->cache.gcinterval : overrides->cache.gcinterval;
    ps->cache.dirlevels      = (overrides->cache.dirlevels_set == 0) ? base->cache.dirlevels : overrides->cache.dirlevels;
    ps->cache.dirlength      = (overrides->cache.dirlength_set == 0) ? base->cache.dirlength : overrides->cache.dirlength;

    return ps;

}/* End of mod_gzip_merge_config() */

/*
 * Module configuration directive handlers...
 */

static const char *
mod_gzip_set_on(cmd_parms *parms, void *dummy, char *arg)
{
    mod_gzip_conf *mgc;

    #ifdef MOD_GZIP_DEBUG1
    char cn[]="mod_gzip_set_on()";
    #endif

    /* Start... */

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: Entry\n", cn );
    mod_gzip_printf( "%s: arg=[%s]\n", cn, arg );
    #endif

    mgc = ( mod_gzip_conf * )
    ap_get_module_config(parms->server->module_config, &gzip_module);

    if ( ( arg[0] == 'Y' )||( arg[0] == 'y' ) )
      {
       /* Set the master 'request control' switches ON... */

       mgc->req     = 1; /* Yes */
       mgc->req_set = 1; /* Yes */
      }
    else /* Set the master 'request control' switches OFF... */
      {
       mgc->req     = 0; /* No */
       mgc->req_set = 0; /* No */
      }

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: mgc->req     = %ld\n", cn, (long) mgc->req );
    mod_gzip_printf( "%s: mgc->req_set = %ld\n", cn, (long) mgc->req_set );
    #endif

    return NULL;
}

static const char *
mod_gzip_set_keep_workfiles(cmd_parms *parms, void *dummy, char *arg)
{
    mod_gzip_conf *mgc;

    #ifdef MOD_GZIP_DEBUG1
    char cn[]="mod_gzip_set_keep_workfiles()";
    #endif

    /* Start... */

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: Entry\n", cn );
    mod_gzip_printf( "%s: arg=[%s]\n", cn, arg );
    #endif

    mgc = ( mod_gzip_conf * )
    ap_get_module_config(parms->server->module_config, &gzip_module);

    if ( ( arg[0] == 'Y' )||( arg[0] == 'y' ) )
      {
       mgc->keep_workfiles = 1; /* Yes */
      }
    else
      {
       mgc->keep_workfiles = 0; /* No */
      }

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: mgc->keep_workfiles = %ld\n", cn,
                   (long) mgc->keep_workfiles );
    #endif

    return NULL;
}

static const char *
mod_gzip_set_min_http(cmd_parms *parms, void *dummy, char *arg)
{
    mod_gzip_conf *mgc;

    #ifdef MOD_GZIP_DEBUG1
    char cn[]="mod_gzip_set_min_http()";
    #endif

    /* Start... */

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: Entry\n", cn );
    mod_gzip_printf( "%s: arg=[%s]\n", cn, arg );
    #endif

    mgc = ( mod_gzip_conf * )
    ap_get_module_config(parms->server->module_config, &gzip_module);

    mgc->min_http = (int) atoi( arg );

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: mgc->min_http = %ld\n", cn,
                   (long) mgc->min_http );
    #endif

    return NULL;
}


static const char *
mod_gzip_imap_add_item( mod_gzip_conf *mgc, char *arg, int flag1 )
{
    int  x;
    char *p1;
    int  ch1;
    int  this_type=0;
    int  this_action=0;
    int  this_include=flag1;

    #ifdef MOD_GZIP_DEBUG1
    char cn[]="mod_gzip_imap_add_item()";
    #endif

    /* Start... */

    #ifdef MOD_GZIP_DEBUG1

    mod_gzip_printf( "%s: Entry\n", cn );
    mod_gzip_printf( "%s: 1 arg=[%s]\n", cn, arg );

    if ( flag1 == 1 )
      {
       mod_gzip_printf( "%s: flag1 = %d = INCLUDE\n", cn, flag1 );
      }
    else if ( flag1 == 0 )
      {
       mod_gzip_printf( "%s: flag1 = %d = EXCLUDE\n", cn, flag1 );
      }
    else
      {
       mod_gzip_printf( "%s: flag1 = %d = ??? Unknown value\n", cn, flag1 );
      }

    mod_gzip_printf( "%s: MOD-GZIP_IMAP_MAXNAMES  = %d\n",
                      cn, MOD_GZIP_IMAP_MAXNAMES );

    mod_gzip_printf( "%s: mgc->imap_total_entries = %d\n",
                      cn, mgc->imap_total_entries );

    #endif /* MOD_GZIP_DEBUG1 */

    /*
     * Parse the config line...
     */

    p1 = arg;
    while((*p1!=0)&&(*p1<33)) p1++;
    ch1 = *p1;

    this_type   = MOD_GZIP_IMAP_ISHANDLER;
    this_action = MOD_GZIP_IMAP_DYNAMIC1;

    if ( ch1 == '!' )
      {
       arg++;
       p1 = arg;
       while((*p1!=0)&&(*p1<33)) p1++;
       ch1 = *p1;
      }
    else
      {
       this_action = MOD_GZIP_IMAP_STATIC1;
      }

    if ( ch1 == '.' )
      {
       this_type = MOD_GZIP_IMAP_ISEXT;
      }
    else
      {
       p1 = arg;
       while (*p1!=0)
         {
          if ( *p1 == '/' )
            {
             this_type = MOD_GZIP_IMAP_ISMIME;
            }
          p1++;
         }
      }

    /*
     * Safety checks...
     */

    if ( ( this_type != MOD_GZIP_IMAP_ISMIME    ) &&
         ( this_type != MOD_GZIP_IMAP_ISEXT     ) &&
         ( this_type != MOD_GZIP_IMAP_ISHANDLER ) )
      {
       #ifdef MOD_GZIP_DEBUG1
       mod_gzip_printf( "%s: this_type = %d = MOD_GZIP_IMAP_IS??? Unknown type\n",cn,this_type);
       mod_gzip_printf( "%s: return( mod_gzip: ERROR: Unrecognized item 'type'\n",cn);
       #endif

       return( "mod_gzip: ERROR: Unrecognized item 'type'" );
      }

    if ( ( this_action != MOD_GZIP_IMAP_DYNAMIC1 ) &&
         ( this_action != MOD_GZIP_IMAP_STATIC1  ) )
      {
       #ifdef MOD_GZIP_DEBUG1
       mod_gzip_printf( "%s: this_action = %d = MOD_GZIP_IMAP_??? Unknown action\n",cn,this_action);
       mod_gzip_printf( "%s: return( mod_gzip: ERROR: Unrecognized item 'action'\n",cn);
       #endif

       return( "mod_gzip: ERROR: Unrecognized item 'action'" );
      }

    /*
     * Wildcards...
     */

     if ( this_type != MOD_GZIP_IMAP_ISMIME )
       {
        /*
         * Wildcards are only allowed in MIME strings such as 'image/*'
         */

        p1 = arg;
        while (*p1!=0)
          {
           if ( *p1 == '*' )
             {
              return( "mod_gzip: ERROR: Wildcards are only allowed in MIME strings." );
             }
           p1++;
          }
       }

    /*
     * If there is room for a new record then add it...
     */

    if ( mgc->imap_total_entries < MOD_GZIP_IMAP_MAXNAMES )
      {
       if ( strlen( arg ) < MOD_GZIP_IMAP_MAXNAMELEN )
         {
          x = mgc->imap_total_entries;

          p1 = arg;
          while((*p1!=0)&&(*p1<33)) p1++;

          strcpy( mgc->imap[x].name, p1 );

          mgc->imap[x].include = this_include;
          mgc->imap[x].type    = this_type;
          mgc->imap[x].action  = this_action;

          mgc->imap_total_entries++; /* Increase onboard items */
         }
       else /* ERROR: Name is too long */
         {
          #ifdef MOD_GZIP_DEBUG1
          mod_gzip_printf( "%s: return( mod_gzip: ERROR: Item name is too long\n",cn);
          #endif

          return( "mod_gzip: ERROR: Item name is too long" );
         }
      }
    else /* ERROR: INDEX is FULL */
      {
       #ifdef MOD_GZIP_DEBUG1
       mod_gzip_printf( "%s: return( mod_gzip: ERROR: Item index is full\n",cn);
       #endif

       return( "mod_gzip: ERROR: Item index is full" );
      }

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: Exit > return( NULL ) >\n",cn);
    #endif

    return NULL;

}/* End of mod_gzip_imap_add_item() */

#ifdef MOD_GZIP_DEBUG1

int mod_gzip_imap_show_items( mod_gzip_conf *mgc )
{
    /*
     * DEBUG only. Show the complete include/exclude list.
     * This is normally called from mod_gzip_init()
     * after all the configuration routines have executed.
     */

    int  i;
    int  x;
    char cn[]="mod_gzip_imap_show_items()";

    /* Start... */

    mod_gzip_printf( "\n");
    mod_gzip_printf( "%s: Entry\n", cn );

    mod_gzip_printf( "%s: mgc->imap_total_entries= %d\n", cn,
                   (long) mgc->imap_total_entries );

    for ( i=0; i<mgc->imap_total_entries; i++ )
       {
        x = i; /* Work variable */

        mod_gzip_printf( "\n");
        mod_gzip_printf( "%s: mgc->imap[%3.3d].include = %d\n",  cn,x,mgc->imap[x].include);
        mod_gzip_printf( "%s: mgc->imap[%3.3d].type    = %d\n",  cn,x,mgc->imap[x].type);

        if ( mgc->imap[x].type == MOD_GZIP_IMAP_ISMIME )
          {
           mod_gzip_printf( "%s: mgc->imap[%3.3d].type    = MOD_GZIP_IMAP_ISMIME\n",cn,x);
          }
        else if ( mgc->imap[x].type == MOD_GZIP_IMAP_ISEXT )
          {
           mod_gzip_printf( "%s: mgc->imap[%3.3d].type    = MOD_GZIP_IMAP_ISEXT\n",cn,x);
          }
        else if ( mgc->imap[x].type == MOD_GZIP_IMAP_ISHANDLER )
          {
           mod_gzip_printf( "%s: mgc->imap[%3.3d].type    = MOD_GZIP_IMAP_ISHANDLER\n",cn,x);
          }
        else /* Unrecognized item type... */
          {
           mod_gzip_printf( "%s: mgc->imap[%3.3d].type    = MOD_GZIP_IMAP_IS??? Unknown type\n",cn,x);
          }

        mod_gzip_printf( "%s: mgc->imap[%3.3d].action  = %d\n",  cn,x,mgc->imap[x].action);

        if ( mgc->imap[x].action == MOD_GZIP_IMAP_DYNAMIC1 )
          {
           mod_gzip_printf( "%s: mgc->imap[%3.3d].action  = MOD_GZIP_IMAP_DYNAMIC1\n",cn,x);
          }
        else if ( mgc->imap[x].action == MOD_GZIP_IMAP_STATIC1 )
          {
           mod_gzip_printf( "%s: mgc->imap[%3.3d].action  = MOD_GZIP_IMAP_STATIC1\n",cn,x);
          }
        else /* Unrecognized action type... */
          {
           mod_gzip_printf( "%s: mgc->imap[%3.3d].action  = MOD_GZIP_IMAP_??? Unknown action\n",cn,x);
          }

        mod_gzip_printf( "%s: mgc->imap[%3.3d].name    = [%s]\n",cn,x,mgc->imap[x].name);

       }/* End 'i' loop */

    mod_gzip_printf( "\n");

    return 0;

}/* End of mod_gzip_imap_show_items() */

#endif /* MOD_GZIP_DEBUG1 */

static const char *
mod_gzip_set_item_include(cmd_parms *parms, void *dummy, char *arg)
{
    mod_gzip_conf *mgc;

    #ifdef MOD_GZIP_DEBUG1
    char cn[]="mod_gzip_set_item_include()";
    #endif

    /* Start... */

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: Entry\n", cn );
    mod_gzip_printf( "%s: arg=[%s]\n", cn, arg );
    #endif

    mgc = ( mod_gzip_conf * )
    ap_get_module_config(parms->server->module_config, &gzip_module);

    /* Pass pre-determined pointer to config structure... */
    /* Pass '1' for parm 3 to INCLUDE this item... */

    return( mod_gzip_imap_add_item( mgc, arg, 1 ) );
}

static const char *
mod_gzip_set_item_exclude(cmd_parms *parms, void *dummy, char *arg)
{
    mod_gzip_conf *mgc;

    #ifdef MOD_GZIP_DEBUG1
    char cn[]="mod_gzip_set_item_exclude()";
    #endif

    /* Start... */

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: Entry\n", cn );
    mod_gzip_printf( "%s: arg=[%s]\n", cn, arg );
    #endif

    mgc = ( mod_gzip_conf * )
    ap_get_module_config(parms->server->module_config, &gzip_module);

    /* Pass pre-determined pointer to config structure... */
    /* Pass '0' for parm 3 to EXCLUDE this item... */

    return( mod_gzip_imap_add_item( mgc, arg, 0 ) );
}

static const char *
mod_gzip_set_temp_dir(cmd_parms *parms, void *dummy, char *arg)
{
    mod_gzip_conf *mgc;

    char cn[]="mod_gzip_set_temp_dir()";

    /* Start... */

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: Entry\n", cn );
    mod_gzip_printf( "%s: arg=[%s]\n", cn, arg );
    #endif

    mgc = ( mod_gzip_conf * )
    ap_get_module_config(parms->server->module_config, &gzip_module);

    mgc->cache.root = arg; /* For now temp dir is used as cache root */

    strcpy( mod_gzip_temp_dir, arg );
    mgc->cache.root = mod_gzip_temp_dir;

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: mgc->cache.root=[%s]\n", cn, mgc->cache.root );
    #endif

    return NULL;
}

static const char *
mod_gzip_set_minimum_file_size(cmd_parms *parms, void *dummy, char *arg)
{
    mod_gzip_conf *mgc;
    long lval;

    #ifdef MOD_GZIP_DEBUG1
    char cn[]="mod_gzip_set_minimum_file_size()";
    #endif

    /* Start... */

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: Entry\n", cn );
    mod_gzip_printf( "%s: arg=[%s]\n", cn, arg );
    #endif

    mgc = ( mod_gzip_conf * )
    ap_get_module_config(parms->server->module_config, &gzip_module);

    lval = (long) atol(arg);

    /* 300 bytes is the minimum at all times */
    if ( lval < 300L ) lval = 300L;

        mgc->minimum_file_size = (long) lval; /* Set config */
    mod_gzip_minimum_file_size = (long) lval; /* Set global */

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: ....mgc->minimum_file_size = %ld\n", cn,
                   (long)     mgc->minimum_file_size );
    mod_gzip_printf( "%s: mod_gzip_minimum_file_size = %ld\n", cn,
                   (long) mod_gzip_minimum_file_size );
    #endif

    return NULL;
}

static const char *
mod_gzip_set_maximum_inmem_size(cmd_parms *parms, void *dummy, char *arg)
{
    mod_gzip_conf *mgc;
    long lval=0;

    #ifdef MOD_GZIP_DEBUG1
    char cn[]="mod_gzip_set_maximum_inmem_size()";
    #endif

    /* Start... */

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: Entry\n", cn );
    mod_gzip_printf( "%s: arg=[%s]\n", cn, arg );
    #endif

    mgc = ( mod_gzip_conf * )
    ap_get_module_config(parms->server->module_config, &gzip_module);

    lval = (long) atol(arg);

    /* 60000 bytes is the current maximum since a malloc() call is used */
    if ( lval > 60000L ) lval = 60000L;

        mgc->maximum_inmem_size = (long) lval; /* Set config */
    mod_gzip_maximum_inmem_size = (long) lval; /* Set global */

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: ....mgc->maximum_inmem_size = %ld\n", cn,
                   (long)     mgc->maximum_inmem_size );
    mod_gzip_printf( "%s: mod_gzip_maximum_inmem_size = %ld\n", cn,
                   (long) mod_gzip_maximum_inmem_size );
    #endif

    return NULL;
}

static const char *
mod_gzip_set_do_static_files(cmd_parms *parms, void *dummy, char *arg)
{
    mod_gzip_conf *mgc;

    #ifdef MOD_GZIP_DEBUG1
    char cn[]="mod_gzip_set_do_static_files()";
    #endif

    /* Start... */

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: Entry\n", cn );
    mod_gzip_printf( "%s: arg=[%s]\n", cn, arg );
    #endif

    mgc = ( mod_gzip_conf * )
    ap_get_module_config(parms->server->module_config, &gzip_module);

    if ( ( arg[0] == 'Y' )||( arg[0] == 'y' ) )
      {
       mgc->do_static_files = 1; /* Yes */
      }
    else
      {
       mgc->do_static_files = 0; /* No */
      }

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: mgc->do_static_files = %ld\n", cn,
                   (long) mgc->do_static_files );
    #endif

    return NULL;
}

static const char *
mod_gzip_set_do_cgi(cmd_parms *parms, void *dummy, char *arg)
{
    mod_gzip_conf *mgc;

    #ifdef MOD_GZIP_DEBUG1
    char cn[]="mod_gzip_set_do_cgi()";
    #endif

    /* Start... */

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: Entry\n", cn );
    mod_gzip_printf( "%s: arg=[%s]\n", cn, arg );
    #endif

    mgc = ( mod_gzip_conf * )
    ap_get_module_config(parms->server->module_config, &gzip_module);

    if ( ( arg[0] == 'Y' )||( arg[0] == 'y' ) )
      {
       mgc->do_cgi = 1; /* Yes */
      }
    else
      {
       mgc->do_cgi = 0; /* No */
      }

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: mgc->do_cgi = %ld\n", cn,
                   (long) mgc->do_cgi );
    #endif

    return NULL;
}

static const handler_rec mod_gzip_handlers[] =
{
    /*
     * This is where we associate an ASCII NAME for our 'handler'
     * which is what gets set into the r->handler field for a
     * request and allows the function name associated with the
     * ASCII name to be called and handle the request...
     */

    /* Add a 'name' and some types to our handler... */

    {"mod_gzip_handler", mod_gzip_handler},
    {CGI_MAGIC_TYPE,     mod_gzip_handler},
    {"cgi-script",       mod_gzip_handler},
    {"*",                mod_gzip_handler},
    {NULL}
};


static const command_rec mod_gzip_cmds[] =
{
    /*
     * Define our httpd.conf configuration diectives and
     * the local routines that are responsible for processing
     * those directives when the time comes...
     */

    {"mod_gzip_on", mod_gzip_set_on, NULL, RSRC_CONF, TAKE1,
     "Yes=mod_gzip will handle requests No=mod_gzip runs in 'passthrough' mode"},

    {"mod_gzip_do_static_files", mod_gzip_set_do_static_files, NULL, RSRC_CONF, TAKE1,
     "'Yes' means mod_gzip will compress static files."},

    {"mod_gzip_do_cgi", mod_gzip_set_do_cgi, NULL, RSRC_CONF, TAKE1,
     "'Yes' means mod_gzip will compress dynamic CGI script output."},

    {"mod_gzip_keep_workfiles", mod_gzip_set_keep_workfiles, NULL, RSRC_CONF, TAKE1,
     "On=Keep work files Off=No"},

    {"mod_gzip_min_http", mod_gzip_set_min_http, NULL, RSRC_CONF, TAKE1,
     "Minimum HTTP support level to receive compression. 1001=HTTP/1.1"},

    {"mod_gzip_minimum_file_size", mod_gzip_set_minimum_file_size, NULL, RSRC_CONF, TAKE1,
     "The minimum size ( in bytes ) before compression will be attempted"},

    {"mod_gzip_maximum_inmem_size", mod_gzip_set_maximum_inmem_size, NULL, RSRC_CONF, TAKE1,
     "The maximum size ( in bytes ) to use for in-memory compression."},

    {"mod_gzip_temp_dir", mod_gzip_set_temp_dir, NULL, RSRC_CONF, TAKE1,
     "The directory to use for work files and compression cache"},

    {"mod_gzip_item_include", mod_gzip_set_item_include, NULL, RSRC_CONF, TAKE1,
     "Add the item the inclusion list"},

    {"mod_gzip_item_exclude", mod_gzip_set_item_exclude, NULL, RSRC_CONF, TAKE1,
     "Add the item the exclusion list"},

    {NULL}
};

/*
 * The actual module 'jump' table...
 *
 * If one of the fixed 'call' points has a valid function
 * address then Apache will 'call' into it at the appropriate time.
 *
 * When the compressed object cache is engaged we will need to
 * simply add some handlers for the URI detection and translation
 * call point(s).
 */

module MODULE_VAR_EXPORT gzip_module =
{
    STANDARD_MODULE_STUFF,
    mod_gzip_init,          /* initializer */
    NULL,                   /* create per-directory config structure */
    NULL,                   /* merge per-directory config structures */
    mod_gzip_create_config, /* create per-server config structure */
    mod_gzip_merge_config,  /* merge per-server config structures */
    mod_gzip_cmds,          /* command table */
    mod_gzip_handlers,      /* handlers */
    NULL,                   /* translate_handler */
    NULL,                   /* check_user_id */
    NULL,                   /* check auth */
    NULL,                   /* check access */
    NULL,                   /* type_checker */
    NULL,                   /* pre-run fixups */
    NULL,                   /* logger */
    NULL,                   /* header parser */
    NULL,                   /* child_init */
    NULL,                   /* child_exit */
    NULL                    /* post read-request */
};

#ifdef NETWARE
int main(int argc, char *argv[]) 
{
    ExitThread(TSR_THREAD, 0);
}
#endif

FILE *mod_gzip_open_output_file(
request_rec *r,
char *output_filename,
int  *rc
)
{
 FILE *ifh;

 #ifdef MOD_GZIP_DEBUG1
 char cn[]="mod_gzip_open_output_file():::";
 #endif

 /*
  * Start...
  */

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: Entry...\n",cn);
 mod_gzip_printf( "%s: output_filename=[%s]\n",cn,output_filename);
 #endif

 ifh = fopen( output_filename, "rb" ); /* Open in BINARY mode */

 if ( !ifh ) /* The file failed to open... */
   {
    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: ERROR: Cannot open file [%s]\n",
                      cn,output_filename);
    #endif

    /*
     * The workfile was created OK but now will not re-open.
     * This is worth a strike in the ERROR log.
     */

    ap_log_error( APLOG_MARK,APLOG_NOERRNO|APLOG_ERR, r->server,
    "mod_gzip: Cannot re-open output_filename=[%s]",
    output_filename );

    /* Return DECLINED and let default logic finish the request... */

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: Exit > return( NULL ) >\n",cn);
    #endif

    #ifdef MOD_GZIP_USES_APACHE_LOGS

    /* Each 'DECLINE' condition provides a short ':WHYTAG' for logs */

    ap_table_setn(
    r->notes,"mod_gzip_result",ap_pstrdup(r->pool,"DECLINED:WORK_OPENFAIL"));

    #endif /* MOD_GZIP_USES_APACHE_LOGS */

    *rc = DECLINED; /* Update caller's result code... */

    return NULL;

   }/* End 'if ( !ifh )' */

 #ifdef MOD_GZIP_DEBUG1
 mod_gzip_printf( "%s: File is now open...\n",cn);
 mod_gzip_printf( "%s: Exit > return( FILE *ifh ) >\n",cn);
 #endif

 *rc = OK; /* Update caller's result code */

 return ifh; /* Return the file handle */

}/* End of mod_gzip_open_output_file() */

int mod_gzip_encode_and_transmit(
request_rec *r,
char        *source,
int          source_is_a_file,
long         input_size,
int          nodecline
)
{
    GZP_CONTROL   gzc;
    GZP_CONTROL*  gzp = &gzc;

    int             rc                = 0;
    FILE           *ifh               = 0;
    int             bytesread         = 0;
    long            output_size       = 0;
    long            compression_ratio = 0;
    char*           gz1_ismem_obuf    = 0;
    int             finalize_stats    = 1;

    int             gz1_ismem_obuf_was_allocated = 0;

    char content_length[20]; /* For Content-length updates */

    #define MOD_GZIP_LARGE_BUFFER_SIZE 8192

    char tmp[ MOD_GZIP_LARGE_BUFFER_SIZE + 2 ]; /* Scratch buffer */

    char *actual_content_encoding_name = "gzip"; /* Adjustable */
	const char *compression_format;

    #ifdef MOD_GZIP_DEBUG1
    char cn[]="mod_gzip_encode_and_transmit()";
    #endif

    void *modconf = r->server->module_config;

    #ifdef MOD_GZIP_USES_APACHE_LOGS
    char log_info[40]; /* Scratch buffer */
    #endif

    /*
     * Start...
     *
     * Establish a local pointer to module configuration data...
     */

    mod_gzip_conf *conf =
    (mod_gzip_conf *) ap_get_module_config( modconf, &gzip_module );

    #ifdef MOD_GZIP_DEBUG1

    mod_gzip_printf( "%s: Entry...\n", cn);
    mod_gzip_printf( "%s: source_is_a_file = %d\n", cn, source_is_a_file);
    mod_gzip_printf( "%s: nodecline        = %d\n", cn, nodecline);

    if ( source_is_a_file ) /* Show the filename... */
      {
       mod_gzip_printf( "%s: source = [%s]\n", cn, source);
      }
    else /* Don't try to print the memory buffer... */
      {
       mod_gzip_printf( "%s: source = MEMORY BUFFER\n", cn );
      }

    mod_gzip_printf( "%s: input_size = %ld\n", cn,(long)input_size);

    #endif /* MOD_GZIP_DEBUG1 */


    #ifdef MOD_GZIP_USES_APACHE_LOGS

    /* This routine 'assumes' that the final result is 'OK' */
    /* and lets the remainder of the processing set the result */
    /* string to some other value, if necessary. */

    /* Since we are now using the 'nodecline' flag and might */
    /* have to 'stand and deliver' then this allows the right */
    /* result code to appear in the log files even if we */
    /* cannot DECLINE the processing. */

    ap_table_setn(
    r->notes,"mod_gzip_result",ap_pstrdup(r->pool,"OK"));

    /* We can also update the 'input' size right away since it is known */

    sprintf( log_info,"%d", (int) input_size );
    ap_table_setn( r->notes,"mod_gzip_input_size",ap_pstrdup(r->pool,log_info));

    #endif /* MOD_GZIP_USES_APACHE_LOGS */

    /*
     * If the source has no length then DECLINE the processing...
     */

    if ( input_size < 1 )
      {
       #ifdef MOD_GZIP_DEBUG1
       mod_gzip_printf( "%s: ERROR: Input source has no valid length.\n",cn);
       mod_gzip_printf( "%s: This request will not be processed...\n",cn);
       #endif

       /* An existing request object with no length is worth a warning... */

       ap_log_error( APLOG_MARK,APLOG_NOERRNO|APLOG_WARNING, r->server,
       "mod_gzip: r->filename=[%s] has no length",r->filename );

       #ifdef MOD_GZIP_DEBUG1
       mod_gzip_printf( "%s: Exit > return( DECLINED ) >\n",cn);
       #endif

       #ifdef MOD_GZIP_USES_APACHE_LOGS

       /* Each 'DECLINE' condition provides a short ':WHYTAG' for logs */

       ap_table_setn(
       r->notes,"mod_gzip_result",ap_pstrdup(r->pool,"DECLINED:NO_I_LEN"));

       #endif /* MOD_GZIP_USES_APACHE_LOGS */

       return DECLINED;
      }

	/*
     * If we're only supposed to send header information (HEAD request)
     * then all we need to do is call ap_send_http_header() at this point
     * and then return 'OK'...
     */

    if ( r->header_only )
      {
       #ifdef MOD_GZIP_DEBUG1
       mod_gzip_printf( "%s: HEAD request only... ignore body data...\n",cn);
       #endif

       /*
        * Set outbound response header fields...
        *
        * NOTE: If this is just a HEAD request then
        * there is no need to make the API call...
        *
        * ap_update_mtime( r, r->finfo.st_mtime );
        *
        * ...and update the actual time. Use the time
        * that's currently associated with the object.
        */

       ap_set_last_modified(r);
       ap_set_etag(r);
       ap_table_setn(r->headers_out, "Accept-Ranges", "bytes");

       /* Start a timer for this transaction... */

       ap_soft_timeout( "mod_gzip: HEAD request handler", r );

       #ifdef MOD_GZIP_DEBUG1
       mod_gzip_printf( "%s: r->content_type=[%s]\n",cn,r->content_type);
       mod_gzip_printf( "%s: Call ap_send_http_header()...\n",cn);
       #endif

       ap_send_http_header(r);

       #ifdef MOD_GZIP_DEBUG1
       mod_gzip_printf( "%s: Back ap_send_http_header()...\n",cn);
       mod_gzip_printf( "%s: Call ap_kill_timeout()...\n",cn);
       #endif

       ap_kill_timeout(r);

       #ifdef MOD_GZIP_DEBUG1
       mod_gzip_printf( "%s: Back ap_kill_timeout()...\n",cn);
       mod_gzip_printf( "%s: Exit > return( OK ) >\n",cn);
       #endif

       #ifdef MOD_GZIP_USES_APACHE_LOGS

       /* Return OK but distinguish it from a 'GET' request in logs...  */

       ap_table_setn(
       r->notes,"mod_gzip_result",ap_pstrdup(r->pool,"OK:HEAD_ONLY"));

       #endif /* MOD_GZIP_USES_APACHE_LOGS */

       return OK;

      }/* End 'if( r->header_only )' */

    /*
     * See if the source meets the MINUMUM SIZE requirement...
     *
     * Default to 300 bytes as a minimum size requirement for it
     * to even be worth a compression attempt. This works well as a
     * minimum for both GZIP and ZLIB which are both LZ77 based and,
     * as such, always have the potential to actually increase the
     * size of the file.
     *
     * The value is a module global that can be adjusted 'on the fly'
     * as load conditions change or as required for other reasons.
     */

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: conf->minimum_file_size = %ld\n",
               cn, (long) conf->minimum_file_size );
    #endif

    if ( input_size < (long) conf->minimum_file_size )
      {
       #ifdef MOD_GZIP_DEBUG1
       mod_gzip_printf( "%s: Source does not meet the minimum size requirement...\n",cn);
       mod_gzip_printf( "%s: nodecline = %d\n",cn,nodecline);
       #endif

       /* Set the 'mod_gzip_result' note value to something */
       /* that indicates this was too small... */

       #ifdef MOD_GZIP_USES_APACHE_LOGS

       /* Each 'DECLINE' condition provides a short ':WHYTAG' for logs */

       ap_table_setn(
       r->notes,"mod_gzip_result",ap_pstrdup(r->pool,"DECLINED:TOO_SMALL"));

       #endif /* MOD_GZIP_USES_APACHE_LOGS */

       /* Is it OK to DECLINE?... */

       if ( nodecline ) /* We have been told NOT to DECLINE */
         {
          #ifdef MOD_GZIP_DEBUG1
          mod_gzip_printf( "%s: DECLINE is NOT allowed...\n",cn);
          #endif

          /* Skip the compression phase and just set the output */
          /* control skid up to send the real input data... */

          output_size = input_size;

          if ( source_is_a_file ) /* Source is a workfile... */
            {
             #ifdef MOD_GZIP_DEBUG1
             mod_gzip_printf( "%s: Force send - source = FILE[%s]\n",
                              cn,source);
             #endif

             strcpy( gzp->output_filename, source );
             gzp->output_ismem         = 0; /* Output is a disk file */
             gz1_ismem_obuf            = 0; /* Make sure this is NULL */
             gzp->output_ismem_obuf    = 0; /* Not used for this method */
             gzp->output_ismem_obuflen = 0; /* Not used for this method */

             ifh = mod_gzip_open_output_file( r, gzp->output_filename, &rc );

             if ( !ifh ) /* The file failed to open... */
               {
                /* We really MUST decline... */
                /* Logs have already been updated... */

                return( rc );
               }
            }
          else /* Source is just a memory buffer... */
            {
             #ifdef MOD_GZIP_DEBUG1
             mod_gzip_printf( "%s: Force send - source = MEMORY BUFFER\n",cn);
             #endif

             gzp->output_ismem = 1;
             gz1_ismem_obuf    = source;

             gz1_ismem_obuf_was_allocated = 0; /* No 'free' is required */
            }

          #ifdef MOD_GZIP_DEBUG1
          mod_gzip_printf( "%s: No compression attempt was made.\n",cn);
          mod_gzip_printf( "%s: Advancing directly to transmit phase...\n",cn);
          #endif

          goto mod_gzip_encode_and_transmit_send_start; /* Jump */
         }
       else /* It's OK to DECLINE the processing... */
         {
          #ifdef MOD_GZIP_DEBUG1
          mod_gzip_printf( "%s: DECLINE is allowed...\n",cn);
          mod_gzip_printf( "%s: Exit > return( DECLINED ) >\n",cn);
          #endif

          return DECLINED;
         }
      }
    else /* The source is larger than the minimum size requirement... */
      {
       #ifdef MOD_GZIP_DEBUG1
       mod_gzip_printf( "%s: Source meets the minimum size requirement.\n",cn);
       mod_gzip_printf( "%s: Assuming OK to proceed...\n",cn);
       #endif
      }

    /*
     * We must now encode the requested object...
     *
     * Statistically speaking, most 'text/*' pages are
     * less than 60k. XML documents are an exception.
     *
     * If the size of the requested object is less than 60k
     * then go ahead and compress the source directly to a
     * small memory buffer. If the requested object is greater
     * than 60k then go ahead and swap the results to an output
     * disk file and then send the contents of the result file.
     *
     * We can't ever allocate all the memory we want inside of
     * a Server task thread so there must always be this kind
     * of 'decision' making about when we can compress to
     * a memory buffer ( Less than 60k ) and when we must
     * compress to DISK. ( Greater than 60k ).
     *
     * There is a trade-off here between running the risk of
     * too many tasks stealing away all the heap space and
     * still maintaining performance. Given all the variables
     * involved such as the true efficiency of the compression
     * algorithm(s) and the speed of the CPU and the amount of
     * memory/disk space available there is no 'real' answer to
     * this dilemma other than relying on statistical data
     * and empirical observations. The 60k limit on in-memory
     * compression seems to strike a good balance and performs
     * incredibly well under the heaviest of loads.
     *
     * At all times, the greatest benefit being gained is the
     * appreciable reduction of data that must actually be
     * sent by the TCP/IP sub-system and the reduced usage
     * of those resources to perform the transmission task(s),
     *
     * The key, then, is to always strive for a balance where
     * the time and resource usage it takes to compress a
     * deliverable object will always be less than the processor
     * burden that would otherwise be realized by handing the
     * full, uncompressed object to the TCP/IP sub-system which
     * always extend the time that the thread and all its
     * locked resources must be maintained as well as the
     * overhead for keeping a connection active any longer
     * than is absolutely necessary.
     *
     * As long as the resource usage it takes to accomplish
     * a significant reduction in the amount of data that
     * must actually be processed by the remainder of the
     * HTTP task thread and the TCP/IP sub-system itself
     * is always less than the processor burden seen by
     * NOT doing so then we are always 'ahead of the game'.
     */

    /*
     * See if the object size exceeds the current MAXIMUM size
     * to use for in-memory compression...
     *
     * See notes above about a range of 60k or so being the best
     * value for heavy load conditions.
     *
     * This number is currently a global so it can be changed
     * 'on the fly' and can 'breathe' as the load changes.
     * It should probably become a thread specific variable
     * so each task can have its 'own' max value depending
     * on current load conditions.
     */

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: conf->maximum_inmem_size = %ld\n",
               cn, (long) conf->maximum_inmem_size );
    #endif

    /*
     * Set up the INPUT target...
     */

    /* The size and type of the input source is always known */
    /* and was passed by the caller... */

    if ( source_is_a_file )
      {
       #ifdef MOD_GZIP_DEBUG1
       mod_gzip_printf( "%s: Input source is file[%s]\n",cn,source);
       #endif

       strcpy( gzp->input_filename, source );

       gzp->input_ismem         = 0; /* Input is a disk file */
       gzp->input_ismem_ibuf    = 0; /* Source buffer */
       gzp->input_ismem_ibuflen = 0; /* Length of data */
      }
    else
      {
       #ifdef MOD_GZIP_DEBUG1
       mod_gzip_printf( "%s: Input source is a MEMORY BUFFER\n",cn);
       #endif

       *gzp->input_filename = 0; /* Not used */

       gzp->input_ismem         = 1;          /* Input is a memory buffer */
       gzp->input_ismem_ibuf    = source;     /* Source buffer */
       gzp->input_ismem_ibuflen = input_size; /* Length of data */
      }

    /*
     * Set up the OUTPUT target...
     */

    gzp->decompress = 0; /* Perform encoding */

	/* Recover the compression format we're supposed to use. */
	compression_format = ap_table_get(r->notes, "mod_gzip_compression_format");
	if (compression_format && strcmp(compression_format, "deflate") == 0)
	  {
	   actual_content_encoding_name = "deflate";
	   gzp->compression_format = DEFLATE_FORMAT;
      }
    else
	  {
	   gzp->compression_format = GZIP_FORMAT;
      }

    if ( input_size <= (long) conf->maximum_inmem_size )
      {
       /* The input source is small enough to compress directly */
       /* to an in-memory output buffer... */

       #ifdef MOD_GZIP_DEBUG1
       mod_gzip_printf( "%s: Input source is small enough for in-memory compression.\n",cn);
       #endif

       *gzp->output_filename = 0; /* Not used */
        gzp->output_ismem    = 1; /* Output is a memory buffer */

       /*
        * Allocate a memory buffer to hold compressed output.
        *
        * For now this is borrowed from the heap for only
        * the lifetime of this function call. If the stack
        * can handle the current in-memory MAXSIZE then
        * that will work just as well.
        *
        * Add at least 1000 bytes in case the compression
        * algorithm(s) actually expands the source ( which is
        * not likely but is always a possibility when using
        * any LZ77 based compression such as GZIP or ZLIB )
        */

       gz1_ismem_obuf = (char *) malloc( input_size + 1000 );

       if ( !gz1_ismem_obuf )
         {
          /*
           * There wasn't enough memory left for another
           * in-memory compression buffer so default to using
           * an output disk file instead...
           */

          #ifdef MOD_GZIP_DEBUG1
          mod_gzip_printf( "%s: ERROR: Cannot allocate GZP memory...\n",cn);
          mod_gzip_printf( "%s: Defaulting to output file method... \n",cn);
          #endif

          gzp->output_ismem = 0; /* Switch to using a disk file */
         }

       else /* We got the memory we need for in-memory compression... */
         {
          /* Set the local flag which tells the exit logic */
          /* that 'gz1_ismem_obuf' was actually allocated */
          /* and not simply set to 'source' so that the */
          /* allocation can be 'freed' on exit... */

          gz1_ismem_obuf_was_allocated = 1; /* 'free' is required */

          /* Compression codecs require a 'clean' buffer so */
          /* we need to spend the cycles for a memset() call. */

          memset( gz1_ismem_obuf, 0, ( input_size + 1000 ) );

          /* Set OUTPUT buffer control variables... */

          gzp->output_ismem_obuf    = gz1_ismem_obuf;
          gzp->output_ismem_obuflen = input_size + 1000;
         }

      }/* End 'if ( input_size <= conf->maximum_inmem_size )' */

    /*
     * If we are unable ( or it is unadvisable ) to use
     * an in-memory output buffer at this time then the
     * 'gzp->output_ismem' flag will still be ZERO at this point.
     */

    if ( gzp->output_ismem != 1 )
      {
       /*
        * The input source is NOT small enough to compress to an
        * in-memory output buffer or it is unadvisable to do
        * so at this time so just use an output file...
        */

       #ifdef MOD_GZIP_DEBUG1
       mod_gzip_printf( "%s: Input source too big for in-memory compression.\n",cn);
       #endif

       /*
        * Create the GZP output target name...
        */

       mod_gzip_create_unique_filename(
       (mod_gzip_conf *) conf,
       (char *) gzp->output_filename,
       MOD_GZIP_MAX_PATH_LEN
       );

       /*
        * COMPRESSION OBJECT CACHE
        *
        * TODO: Obviously one place to add the compression cache
        * logic is right here. If there is already a pre-compressed
        * version of the requested entity sitting in the special
        * compression cache and it is 'fresh' then go ahead and
        * send it as the actual response. Add a CRC/MD5 checksum
        * to the stored compression object(s) so we can quickly
        * determine if the compressed object is 'fresh'. Relying
        * on Content-length and/or modification time/date won't handle
        * all possible expiration scenarios for compressed objects.
        */

       gzp->output_ismem = 0; /* Output is a disk file */

       gz1_ismem_obuf    = 0; /* Make sure this is NULL */

       /* Set OUTPUT buffer control variables... */

       gzp->output_ismem_obuf    = 0; /* Not used for this method */
       gzp->output_ismem_obuflen = 0; /* Not used for this method */

      }/* End 'else' */

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: gzp->decompress      = %d\n"  ,cn,gzp->decompress);
    mod_gzip_printf( "%s: gzp->compression_format = %d\n",cn,gzp->compression_format);
    mod_gzip_printf( "%s: gzp->input_ismem     = %d\n",  cn,gzp->input_ismem);
    mod_gzip_printf( "%s: gzp->output_ismem    = %d\n",  cn,gzp->output_ismem);
    mod_gzip_printf( "%s: gzp->input_filename  = [%s]\n",cn,gzp->input_filename);
    mod_gzip_printf( "%s: gzp->output_filename = [%s]\n",cn,gzp->output_filename);
    mod_gzip_printf( "%s: Call gzp_main()...\n",cn);
    #endif

    rc = gzp_main( gzp ); /* Perform the compression... */

    output_size = (long) gzp->bytes_out;

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: Back gzp_main()...\n",cn);
    mod_gzip_printf( "%s: input_size     = %ld\n",cn,(long)input_size);
    mod_gzip_printf( "%s: output_size    = %ld\n",cn,(long)output_size);
    mod_gzip_printf( "%s: gzp->bytes_out = %ld\n",cn,(long)gzp->bytes_out);
    mod_gzip_printf( "%s: Bytes saved    = %ld\n",cn,
                     (long)input_size-gzp->bytes_out );
    #endif

    /* Compute the compresion ratio for access.log and */
    /* internal statistics update... */

    compression_ratio = 0; /* Reset */

    /* Prevent 'Divide by zero' error... */

    if ( ( input_size  > 0 ) &&
         ( output_size > 0 ) )
      {
       compression_ratio = 100 - (int)
       ( output_size * 100L / input_size );
      }

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: Compression ratio = %ld percent\n",cn,
             (long) compression_ratio );
    #endif

    /*
     * Update the logs with output size information
     * as soon as it is known in case there was an
     * error or we must DECLINE. At least the logs
     * will then show the sizes and the results.
     */

    #ifdef MOD_GZIP_USES_APACHE_LOGS

    sprintf( log_info,"%d", (int) output_size );
    ap_table_setn( r->notes,"mod_gzip_output_size",ap_pstrdup(r->pool,log_info));

    sprintf( log_info,"%d", (int) compression_ratio );
    ap_table_setn( r->notes,"mod_gzip_compression_ratio",ap_pstrdup(r->pool,log_info));

    #endif /* MOD_GZIP_USES_APACHE_LOGS */

    /*
     * Evaluate the compression result(s)...
     *
     * If the compression pass failed then the output length
     * will be ZERO bytes...
     */

    if ( output_size < 1 )
      {
       #ifdef MOD_GZIP_DEBUG1
       mod_gzip_printf( "%s: Compressed version has no length.\n",cn);
       mod_gzip_printf( "%s: Sending the original version uncompressed...\n",cn);
       #endif

       finalize_stats = 0; /* Don't update stats again */

       if ( r->server->loglevel == APLOG_DEBUG )
         {
          ap_log_error( "",0,APLOG_NOERRNO|APLOG_DEBUG, r->server,
          "mod_gzip: gzp_main(ERR): r->uri=[%s] input_size=%ld output_size=%ld gzp->output_filename=[%s]",
           r->uri,(long)input_size,(long)output_size,gzp->output_filename);
         }

       /*
        * NOTE: It's perfectly possible that we have made it all
        * the way to here and the straight execution of the
        * compressor is the first time there has been a check for
        * the actual existence of the requested object. This will
        * be especially true for STATIC requests.
        *
        * The compressor itself will fail if/when it can't find
        * the input target so 'DECLINED:NO_O_LEN' could simply
        * means the file was not found. In these cases the Apache
        * logs should also contain the correct '404 Not Found' code.
        */

       #ifdef MOD_GZIP_USES_APACHE_LOGS

       /* Each 'DECLINE' condition provides a short ':WHYTAG' for logs */

       ap_table_setn(
       r->notes,"mod_gzip_result",ap_pstrdup(r->pool,"DECLINED:NO_O_LEN"));

       #endif /* MOD_GZIP_USES_APACHE_LOGS */

       /* Is it OK to DECLINE?... */

       if ( nodecline ) /* We have been told NOT to DECLINE... */
         {
          #ifdef MOD_GZIP_DEBUG1
          mod_gzip_printf( "%s: DECLINE is NOT allowed...\n",cn);
          #endif

          /* Just set the output control skid */
          /* to send the real input data... */

          output_size = input_size;

          if ( source_is_a_file ) /* Source is a workfile... */
            {
             strcpy( gzp->output_filename, source );

             gzp->output_ismem         = 0; /* Output is a disk file */
             gz1_ismem_obuf            = 0; /* Make sure this is NULL */
             gzp->output_ismem_obuf    = 0; /* Not used for this method */
             gzp->output_ismem_obuflen = 0; /* Not used for this method */

             ifh = mod_gzip_open_output_file( r, gzp->output_filename, &rc );

             if ( !ifh ) /* We really must DECLINE... */
               {
                return( rc );
               }
            }
          else /* Source is just a memory buffer... */
            {
             gzp->output_ismem = 1;
             gz1_ismem_obuf    = source;
            }

          #ifdef MOD_GZIP_DEBUG1
          mod_gzip_printf( "%s: Advancing directly to transmit phase...\n",cn);
          #endif

          goto mod_gzip_encode_and_transmit_send_start; /* Jump */
         }
       else /* It's OK to DECLINE the processing... */
         {
          #ifdef MOD_GZIP_DEBUG1
          mod_gzip_printf( "%s: DECLINE is allowed...\n",cn);
          mod_gzip_printf( "%s: Exit > return( DECLINED ) >\n",cn);
          #endif

          /* Free the local memory buffer allocation ( if necessary )... */

          if ( gz1_ismem_obuf )
            {
             /* The pointer may have been 'borrowed' and was */
             /* not actually 'allocated' so check the flag... */

             if ( gz1_ismem_obuf_was_allocated )
               {
                free( gz1_ismem_obuf );

                gz1_ismem_obuf = 0;
                gz1_ismem_obuf_was_allocated = 0;

               }/* End 'if( gz1_ismem_obuf_was_allocated )' */

            }/* End 'if( gz1_ismem_obuf )' */

          /* Return... */

          return DECLINED;
         }

      }/* End 'if( output_size < 1 )' */

    /*
     * If we reach this point then the compressed version has
     * a valid length. Time to see if it it's worth sending.
     *
     * If the original source is SMALLER than the COMPRESSED
     * version ( not likely but possible with LZ77 ) then
     * just punt and send the original source...
     */

    if ( output_size > input_size )
      {
       #ifdef MOD_GZIP_DEBUG1
       mod_gzip_printf( "%s: Compressed version is larger than original.\n",cn);
       mod_gzip_printf( "%s: Sending the original version uncompressed...\n",cn);
       #endif

       finalize_stats = 0; /* Don't update stats again */

       #ifdef MOD_GZIP_USES_APACHE_LOGS

       /* Each 'DECLINE' condition provides a short ':WHYTAG' for logs */

       ap_table_setn(
       r->notes,"mod_gzip_result",ap_pstrdup(r->pool,"DECLINED:ORIGINAL_SMALLER"));

       #endif /* MOD_GZIP_USES_APACHE_LOGS */

       /* Is it OK to DECLINE?... */

       if ( nodecline ) /* We have been told NOT to DECLINE... */
         {
          #ifdef MOD_GZIP_DEBUG1
          mod_gzip_printf( "%s: DECLINE is NOT allowed...\n",cn);
          #endif

          /* Just set the output control skid */
          /* to send the real input data... */

          output_size = input_size;

          if ( source_is_a_file ) /* Source is a workfile... */
            {
             strcpy( gzp->output_filename, source );

             gzp->output_ismem         = 0; /* Output is a disk file */
             gz1_ismem_obuf            = 0; /* Make sure this is NULL */
             gzp->output_ismem_obuf    = 0; /* Not used for this method */
             gzp->output_ismem_obuflen = 0; /* Not used for this method */

             ifh = mod_gzip_open_output_file( r, gzp->output_filename, &rc );

             if ( !ifh ) /* We really must DECLINE... */
               {
                return( rc );
               }
            }
          else /* Source is just a memory buffer... */
            {
             gzp->output_ismem = 1;
             gz1_ismem_obuf    = source;

             gz1_ismem_obuf_was_allocated = 0; /* No 'free' is required */
            }

          #ifdef MOD_GZIP_DEBUG1
          mod_gzip_printf( "%s: Advancing directly to transmit phase...\n",cn);
          #endif

          goto mod_gzip_encode_and_transmit_send_start; /* Jump */
         }
       else /* It's OK to DECLINE the processing... */
         {
          #ifdef MOD_GZIP_DEBUG1
          mod_gzip_printf( "%s: DECLINE is allowed...\n",cn);
          mod_gzip_printf( "%s: Exit > return( DECLINED ) >\n",cn);
          #endif

          /* Free the local memory buffer allocation ( if necessary )... */

          if ( gz1_ismem_obuf )
            {
             /* The pointer may have been 'borrowed' and was */
             /* not actually 'allocated' so check the flag... */

             if ( gz1_ismem_obuf_was_allocated )
               {
                free( gz1_ismem_obuf );

                gz1_ismem_obuf = 0;
                gz1_ismem_obuf_was_allocated = 0;

               }/* End 'if( gz1_ismem_obuf_was_allocated )' */

            }/* End 'if( gz1_ismem_obuf )' */

          /* Return... */

          return DECLINED;
         }
      }
    else /* Compressed version is smaller than original... */
      {
       #ifdef MOD_GZIP_DEBUG1
       mod_gzip_printf( "%s: Compressed version is smaller than original.\n",cn);
       mod_gzip_printf( "%s: Sending the compressed version...\n",cn);
       #endif
      }

    /*
     * If an output workfile was used then make SURE it is going
     * to reopen before beginning the transmit phase.
     *
     * If we begin the transmit phase before discovering a problem
     * re-opening the workfile then we have lost the chance to
     * DECLINE the processing and allow the default logic to
     * deliver the requested object.
     *
     * This only matters for 'static' files or times when the
     * 'nodecline' flag is FALSE and it is actually OK to DECLINE.
     */

    if ( !gzp->output_ismem ) /* Workfile was used... */
      {
       #ifdef MOD_GZIP_DEBUG1
       mod_gzip_printf( "%s: Opening compressed output file [%s]...\n",
                cn, gzp->output_filename );
       #endif

       ifh = mod_gzip_open_output_file( r, gzp->output_filename, &rc );

       if ( !ifh ) /* The file failed to open... */
         {
          #ifdef MOD_GZIP_DEBUG1
          mod_gzip_printf( "%s: ERROR: Cannot re-open file [%s]\n",
                   cn,gzp->output_filename);
          #endif

          /* We really must DECLINE... */
          /* Logs have already been updated... */

          #ifdef MOD_GZIP_DEBUG1
          mod_gzip_printf( "%s: Exit > return( DECLINED ) >\n",cn);
          #endif

          return DECLINED;

         }/* End 'if ( !ifh )' */

       #ifdef MOD_GZIP_DEBUG1
       mod_gzip_printf( "%s: Workile re-opened OK...\n",cn);
       #endif

      }/* End 'if ( !gzp->output_ismem )' */

    /*
     * IMPORTANT
     *
     * If we have made it to here then all is well and only
     * now can we set the encoding for this response...
     *
     * We must do this 'above' any jump points that might
     * be sending the 'untouched' data or the browser will
     * get confused regarding the actual content.
     */

    r->content_encoding = actual_content_encoding_name;

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: r->content_encoding is now [%s]\n",
                      cn, r->content_encoding );
    #endif

    /*
     * Begin the transmission phase...
     *
     * Even if the 'nodecline' flag is TRUE if we encounter
     * any fatal errors at this point we must 'DECLINE'.
     */

    mod_gzip_encode_and_transmit_send_start: ; /* <<-- Jump point */

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: Starting transmit phase...\n",cn);
    #endif

    /*
     * We are ready to send content so update the "Content-length:"
     * response field and send the HTTP header. We don't need to
     * worry about setting the "Content-type:" field since we are
     * simply accepting the value that was passed to us as indicated
     * by the inbound r->content_type string. The "Content-type:"
     * field never changes even when multiple encodings have been
     * applied to the content itself.
     *
     * This version does not make any attempt to use 'Chunked'
     * transfer encoding since there are so many user agents that
     * do not support it and when Content-length is known prior
     * to header transmission ( as is always the case with this
     * code ) then there is simply no reason to even think about
     * using the slower and more problematic 'Chunked' encoding
     * transfer method.
     */

    /*
     * Set relevant outbound response header fields...
     *
     * Be sure to call ap_update_mtime() before calling
     * ap_set_last_modified() to be sure the 'current'
     * time is actually updated in outbound response header.
     */

    ap_update_mtime( r, r->finfo.st_mtime );
    ap_set_last_modified(r);
    ap_set_etag(r);
    ap_table_setn(r->headers_out, "Accept-Ranges", "bytes");

    /*
     * Start a timer for this transaction...
     */

    ap_soft_timeout( "mod_gzip: Encoded data transmit", r );

    /*
     * Return the length of the compressed output in
     * the response header.
     *
     * See notes above about there never being a requirement
     * to use 'Chunked' transfer encoding since the content
     * length is always 'known' prior to transmission.
     */

    sprintf( content_length, "%ld", output_size );
    ap_table_set (r->headers_out, "Content-Length", content_length );

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: output_size     = %ld\n",cn,(long)output_size);
    mod_gzip_printf( "%s: r->content_type = [%s]\n",cn,r->content_type);
    mod_gzip_printf( "%s: Call ap_send_http_header()...\n",cn);
    #endif

    ap_send_http_header(r);

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: Back ap_send_http_header()...\n",cn);
    #endif

	/*
     * Send the response...
     *
     * If the requested object was small enough to fit into
     * our special in-memory output space then send the result
     * directly from memory. If the requested object exceeded
     * the minimum size for in-memory compression then an output
     * file was used so re-open and send the results file...
     */

    if ( gzp->output_ismem )
      {
       /* Send the in-memory output buffer... */

       #ifdef MOD_GZIP_DEBUG1

       mod_gzip_printf( "%s: Sending the in-memory output buffer...\n",cn);
       mod_gzip_printf( "%s: output_size = %ld\n",cn,(long)output_size);

       /* Turn this 'on' for VERY verbose diagnostics...
       #define MOD_GZIP_DUMP_JUST_BEFORE_SENDING
       */
       #ifdef  MOD_GZIP_DUMP_JUST_BEFORE_SENDING
       mod_gzip_hexdump( gz1_ismem_obuf, output_size );
       #endif

       #endif /* MOD_GZIP_DEBUG1 */

       /* This module can use either ap_send_mmap() or ap_rwrite()... */

       #ifdef MOD_GZIP_USES_AP_SEND_MMAP

       /* Use ap_send_mmap() call to send the data... */

       #ifdef MOD_GZIP_DEBUG1
       mod_gzip_printf( "%s: Call ap_send_mmap( gz1_ismem_obuf, bytes=%ld )...\n",
                cn, (long)output_size );
       #endif

       ap_send_mmap( gz1_ismem_obuf, r, 0, output_size );

       #ifdef MOD_GZIP_DEBUG1
       mod_gzip_printf( "%s: Back ap_send_mmap( gz1_ismem_obuf, bytes=%ld )...\n",
                cn, (long)output_size );
       #endif

       #else /* !MOD_GZIP_USES_AP_SEND_MMAP */

       /* Use ap_rwrite() call to send the data... */

       #ifdef MOD_GZIP_DEBUG1
       mod_gzip_printf( "%s: Call ap_rwrite( gz1_ismem_obuf, bytes=%ld )...\n",
                cn, (long)output_size );
       #endif

       ap_rwrite( gz1_ismem_obuf, output_size, r );

       #ifdef MOD_GZIP_DEBUG1
       mod_gzip_printf( "%s: Back ap_rwrite( gz1_ismem_obuf, bytes=%ld )...\n",
                cn, (long)output_size );
       #endif

       #endif /* MOD_GZIP_USES_AP_SEND_MMAP */

       /* Stop the timer... */

       #ifdef MOD_GZIP_DEBUG1
       mod_gzip_printf( "%s: Call ap_kill_timeout()...\n",cn);
       #endif

       ap_kill_timeout(r);

       #ifdef MOD_GZIP_DEBUG1
       mod_gzip_printf( "%s: Back ap_kill_timeout()...\n",cn);
       #endif

       /* Free the local memory buffer allocation ( if necessary )... */

       if ( gz1_ismem_obuf )
         {
          /* The pointer may have been 'borrowed' and was */
          /* not actually 'allocated' so check the flag... */

          if ( gz1_ismem_obuf_was_allocated )
            {
             free( gz1_ismem_obuf );

             gz1_ismem_obuf = 0;
             gz1_ismem_obuf_was_allocated = 0;

            }/* End 'if( gz1_ismem_obuf_was_allocated )' */

         }/* End 'if( gz1_ismem_obuf )' */
      }
    else /* Output workfile was used so send the contents... */
      {
       /*
        * NOTE: The workfile was already 're-opened' up above
        * before the transmit phase began so that we still had
        * the chance to return DECLINED if, for some reason, the
        * workfile could not be re-opened.
        */

       #ifdef MOD_GZIP_DEBUG1
       mod_gzip_printf( "%s: sizeof( tmp )        = %d\n",cn,sizeof(tmp));
       mod_gzip_printf( "%s: Transmit buffer size = %d\n",cn,sizeof(tmp));
       mod_gzip_printf( "%s: Sending compressed output file...\n",cn);
       #endif

       for (;;)
          {
           bytesread = fread( tmp, 1, sizeof( tmp ), ifh );

           #ifdef MOD_GZIP_DEBUG1
           mod_gzip_printf( "%s: Back fread(): bytesread=%d\n",cn,bytesread);
           #endif

           if ( bytesread < 1 ) break; /* File is exhausted... We are done...*/

           /* This module can use either ap_send_mmap() or ap_rwrite()... */

           #ifdef MOD_GZIP_USES_AP_SEND_MMAP

           /* Use ap_send_mmap() call to send the data... */

           ap_send_mmap( tmp, r, 0, bytesread );

           #else /* !MOD_GZIP_USES_AP_SEND_MMAP */

           /* Use ap_rwrite() call to send the data... */

           ap_rwrite( tmp, bytesread, r );

           #endif /* MOD_GZIP_USES_AP_SEND_MMAP */
          }

       #ifdef MOD_GZIP_DEBUG1
       mod_gzip_printf( "%s: Done Sending compressed output file...\n",cn);
       mod_gzip_printf( "%s: Closing workfile [%s]...\n",
                cn, gzp->output_filename );
       #endif

       fclose( ifh ); /* Close the input file */

       /* Stop the timer before attempting to delete the workfile... */

       #ifdef MOD_GZIP_DEBUG1
       mod_gzip_printf( "%s: Call ap_kill_timeout()...\n",cn);
       #endif

       ap_kill_timeout(r);

       #ifdef MOD_GZIP_DEBUG1
       mod_gzip_printf( "%s: Back ap_kill_timeout()...\n",cn);
       #endif

       /* Delete the workfile if 'keep' flag is OFF... */

       #ifdef MOD_GZIP_DEBUG1
       mod_gzip_printf( "%s: conf->keep_workfiles = %d\n",
                         cn, conf->keep_workfiles );
       #endif

       if ( !conf->keep_workfiles ) /* Default is OFF */
         {
          #ifdef MOD_GZIP_DEBUG1
          mod_gzip_printf( "%s: Deleting workfile [%s]...\n",
                   cn, gzp->output_filename );
          #endif

          #ifdef WIN32
          DeleteFile( gzp->output_filename );
          #else /* !WIN32 */
          unlink( gzp->output_filename );
          #endif /* WIN32 */
         }
       else /* Keep all work files... */
         {
          #ifdef MOD_GZIP_DEBUG1
          mod_gzip_printf( "%s: Keeping workfile [%s]...\n",
                   cn, gzp->output_filename );
          #endif
         }

      }/* End 'else' that sends compressed workfile */

    /*
     * The compressed object has been sent...
     */

    #ifdef MOD_GZIP_USES_APACHE_LOGS

    if ( finalize_stats )
      {
       sprintf( log_info,"%d", (int) output_size );
       ap_table_setn( r->notes,"mod_gzip_output_size",ap_pstrdup(r->pool,log_info));

       sprintf( log_info,"%d", (int) compression_ratio );
       ap_table_setn( r->notes,"mod_gzip_compression_ratio",ap_pstrdup(r->pool,log_info));

      }
    #endif /* MOD_GZIP_USES_APACHE_LOGS */

    if ( r->server->loglevel == APLOG_DEBUG )
      {
       /*
        * If LogLevel is 'debug' then show the compression results
        * in the log(s)...
        */

       ap_log_error( "",0,APLOG_NOERRNO|APLOG_DEBUG, r->server,
       "mod_gzip: r->uri=[%s] OK: Bytes In:%ld Out:%ld Compression: %ld pct.",
       r->uri,
       (long) input_size,
       (long) output_size,
       (long) compression_ratio
       );

      }/* End 'if( r->server->loglevel == APLOG_DEBUG )' */

    /*
     * Return OK to the Server to indicate SUCCESS...
     */

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: Exit > return( OK ) >\n",cn);
    #endif

    return OK;

}/* End of mod_gzip_encode_and_transmit() */

int mod_gzip_ismatch( char *s1, char *s2, int len1, int haswilds )
{
 /* Behaves just like strncmp() but IGNORES differences     */
 /* between FORWARD or BACKWARD slashes in a STRING, allows */
 /* wildcard matches, and can ignore length value.          */
 /* It uses pointers and is faster than using lib calls.    */

 /* Unlike strncmp() this routine returns TRUE (1) if the   */
 /* strings match and FALSE (0) if they do not...           */

 int  i;
 int  l1;
 int  l2;
 int  distance;
 char ch1;
 char ch2;

 /* WARNING! We MUST have a check for 'NULL' on the pointer(s) */
 /*          themselves or we might GP */

 if ( ( s1 == 0 ) || ( s2 == 0 ) )
   {
    /* SAFETY! If pointer itself if NULL */
    /* don't enter LOOP... */

    return( 0 ); /* Return FALSE for NOMATCH...  */
   }

 distance = len1; /* Default to value passed... */

 /* If no length was given then the 2 strings must already */
 /* have the same length or this is a 'no match'... */
 /* Exception to this is if wildcards are present. */

 if ( len1 == 0 )
   {
    l1 = strlen( s1 );
    l2 = strlen( s2 );

    /* If either string had a valid pointer but is EMPTY */
    /* then this is an automatic 'no match'... */

    if ((l1==0)||(l2==0))
      {
       return( 0 ); /* Return FALSE for NOMATCH...  */
      }

    if ( l1 != l2  )
      {
       if ( haswilds == 0 )
         {
          return( 0 ); /* Return FALSE for NOMATCH...  */
         }
      }

    /* If the lengths ARE equal then this is a possible */
    /* match. Use the smaller of the 2 values for scan...*/

    if ( l1 < l2 ) distance = l1;
    else           distance = l2;
   }

 /* Final check... if distance is still 0 then this */
 /* is an automatic 'no match'... */

 if ( distance == 0 )
   {
    return( 0 ); /* Return FALSE for NOMATCH...  */
   }

 /* Do the deed... */

 for ( i=0; i<distance; i++ )
    {
     /* If we encounter a null in either string before we */
     /* have 'gone the distance' then the strings don't match... */

     if ( ( *s1 == 0 ) || ( *s2 == 0 ) ) return( 0 ); /* No match! */

     ch1 = *s1;
     ch2 = *s2;

     if ( ( ch1 == '*' ) || ( ch2 == '*' ) )
       {
        /* If we are still here and wildcards are allowed */
        /* then the first one seen in either string causes */
        /* us to return SUCCESS... */

        if ( haswilds )
          {
           return( 1 ); /* Wildcard match was OK this time... */
          }
       }

     if ( ch1 == '/' ) ch1 = '\\';
     if ( ch2 == '/' ) ch2 = '\\';

     if ( ch1 != ch2 ) return( 0 ); /* No match! */

     s1++;
     s2++;

    }/* End 'i' loop */

 /* If we make it to here then everything MATCHED! */

 return( 1 ); /* MATCH! */

}/* End mod_gzip_ismatch() */

int mod_gzip_get_action_flag( request_rec *r, mod_gzip_conf *mgc )
{
    int   x    = 0;
    int   pass = 0;
    int   clen = 0;
    int   hlen = 0;
    int   flen = 0;

    int   pass_result        = 0;
    int   filter_value       = 0;
    int   item_is_included   = 0;
    int   item_is_excluded   = 0;
    int   action_flag        = 0;
    int   this_type          = 0;
    int   this_action        = 0;
    char *this_name          = 0;
    char *file_extension     = 0;
    int   file_extension_len = 0;
    char *p1                 = 0;

    #ifdef MOD_GZIP_DEBUG1
    char cn[]="mod_gzip_get_action_flag()";
    #endif

    /*
     * Start...
     */

    if ( r->content_type ) clen = strlen( r->content_type );
    if ( r->handler      ) hlen = strlen( r->handler );

    if ( r->filename )
      {
       flen = strlen( r->filename );
       p1   = r->filename;
       while(*p1!=0){if (*p1=='.') file_extension=p1; p1++;}
       if ( file_extension ) file_extension_len = strlen( file_extension );
      }

    #ifdef MOD_GZIP_DEBUG1

    mod_gzip_printf( "%s: Entry...\n",cn);
    mod_gzip_printf( "%s: r->content_type    = [%s]\n",cn,r->content_type);
    mod_gzip_printf( "%s: clen               = %d\n",  cn,clen);
    mod_gzip_printf( "%s: r->handler         = [%s]\n",cn,r->handler);
    mod_gzip_printf( "%s: hlen               = %d\n",  cn,hlen);
    mod_gzip_printf( "%s: r->filename        = [%s]\n",cn,r->filename);
    mod_gzip_printf( "%s: flen               = %d\n",  cn,flen);
    mod_gzip_printf( "%s: file_extension     = [%s]\n",cn,file_extension);
    mod_gzip_printf( "%s: file_extension_len = %d\n",  cn,file_extension_len);

    #endif /* MOD_GZIP_DEBUG1 */

    /*
     * Sanity checks...
     */

    if ( ( hlen == 0 ) && ( clen == 0 ) )
      {
       /*
        * If the header analysis and/or negotiation phase has
        * determined this to be a CGI script then the r->content_type
        * field will be (null) but r->handler will contain "cgi-script".
        * or "php-script" or the like.
        *
        * If the analysis has determined this is a static file
        * then r->handler will be (null) but the r->content_type
        * field will be "text/html" or "text/plain" or whatever.
        *
        * Both the r->content_type field and the r->handler
        * field are empty. Ignore this one...
        */

       #ifdef MOD_GZIP_DEBUG1
       mod_gzip_printf( "%s: Both hlen and clen are ZERO...\n",cn);
       mod_gzip_printf( "%s: Exit > return( MOD_GZIP_IMAP_DECLINED1 ) >\n",cn);
       #endif

       if ( r->server->loglevel == APLOG_DEBUG )
         {
          ap_log_error( "",0,APLOG_NOERRNO|APLOG_DEBUG, r->server,
          "mod_gzip: There is no valid r->handler or r->content_length ");
         }

       return( MOD_GZIP_IMAP_DECLINED1 );
      }

    /*
     * Perform 2 passes at the Include/Exclude list...
     *
     * The first  pass is the higher-priority EXCLUSION check.
     * The second pass is the lower-priority  INCLUSION check.
     */

    for ( pass=0; pass<2; pass++ )
    {

    pass_result = 0; /* Reset result */

    if ( pass == 0 ) /* EXCLUSION CHECK */
      {
       filter_value = 0;

       #ifdef MOD_GZIP_DEBUG1
       mod_gzip_printf( "%s: EXCLUSION CHECK...\n",cn);
       #endif
      }
    else if ( pass == 1 ) /* INCLUSION CHECK */
      {
       filter_value = 1;

       #ifdef MOD_GZIP_DEBUG1
       mod_gzip_printf( "%s: INCLUSION CHECK...\n",cn);
       #endif
      }

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: pass = %d\n", cn, pass );
    mod_gzip_printf( "%s: filter_value = %d\n", cn, filter_value );
    mod_gzip_printf( "%s: mgc->imap_total_entries = %d\n", cn,
                    (int) mgc->imap_total_entries );
    #endif

    for ( x=0; x<mgc->imap_total_entries; x++ )
       {
        if ( r->server->loglevel == APLOG_DEBUG )
          {
           /* Show the lookups in the Apache ERROR log if DEBUG is on */

           ap_log_error( "",0,APLOG_NOERRNO|APLOG_DEBUG, r->server,
           "mod_gzip: mgc->imap[%3.3d] = i%2.2d t%4.4d a%4.4d n[%s]",
            x,
            mgc->imap[x].include,
            mgc->imap[x].type,
            mgc->imap[x].action,
            mgc->imap[x].name
            );
          }

        #ifdef MOD_GZIP_DEBUG1

        mod_gzip_printf( "%s: --------------------------------------------\n",cn);
        mod_gzip_printf( "%s: r->handler      = [%s]\n",cn,r->handler);
        mod_gzip_printf( "%s: r->content_type = [%s]\n",cn,r->content_type);
        mod_gzip_printf( "%s: r->filename     = [%s]\n",cn,r->filename);
        mod_gzip_printf( "%s: file_extension  = [%s]\n",cn,file_extension);
        mod_gzip_printf( "%s: mgc->imap[%3.3d].include = %d\n",cn,x,mgc->imap[x].include);
        mod_gzip_printf( "%s: mgc->imap[%3.3d].type    = %d\n",cn,x,mgc->imap[x].type);

        if ( mgc->imap[x].type == MOD_GZIP_IMAP_ISMIME )
          {
           mod_gzip_printf( "%s: mgc->imap[%3.3d].type    = MOD_GZIP_IMAP_ISMIME\n",cn,x);
          }
        else if ( mgc->imap[x].type == MOD_GZIP_IMAP_ISEXT )
          {
           mod_gzip_printf( "%s: mgc->imap[%3.3d].type    = MOD_GZIP_IMAP_ISEXT\n",cn,x);
          }
        else if ( mgc->imap[x].type == MOD_GZIP_IMAP_ISHANDLER )
          {
           mod_gzip_printf( "%s: mgc->imap[%3.3d].type    = MOD_GZIP_IMAP_ISHANDLER\n",cn,x);
          }
        else /* Unrecognized item type... */
          {
           mod_gzip_printf( "%s: mgc->imap[%3.3d].type    = MOD_GZIP_IMAP_IS??? Unknown type\n",cn,x);
          }

        mod_gzip_printf( "%s: mgc->imap[%3.3d].action  = %d\n",  cn,x,mgc->imap[x].action);

        if ( mgc->imap[x].action == MOD_GZIP_IMAP_DYNAMIC1 )
          {
           mod_gzip_printf( "%s: mgc->imap[%3.3d].action  = MOD_GZIP_IMAP_DYNAMIC1\n",cn,x);
          }
        else if ( mgc->imap[x].action == MOD_GZIP_IMAP_STATIC1 )
          {
           mod_gzip_printf( "%s: mgc->imap[%3.3d].action  = MOD_GZIP_IMAP_STATIC1\n",cn,x);
          }
        else /* Unrecognized action type... */
          {
           mod_gzip_printf( "%s: mgc->imap[%3.3d].action  = MOD_GZIP_IMAP_??? Unknown action\n",cn,x);
          }

        mod_gzip_printf( "%s: mgc->imap[%3.3d].name    = [%s]\n",cn,x,mgc->imap[x].name);

        #endif /* MOD_GZIP_DEBUG1 */

        /* 'filter_value' mirrors 'pass' value for now but this might */
        /* not always be true. First pass is EXCLUDE and second is INCLUDE */

        if ( mgc->imap[x].include == filter_value )
          {
           #ifdef MOD_GZIP_DEBUG1
           mod_gzip_printf( "%s: This record matches filter_value %d\n",
                             cn, filter_value );
           mod_gzip_printf( "%s: The record will be checked...\n",cn);
           #endif

           /*
            * Set work values for this record...
            */

           this_type   = mgc->imap[x].type;
           this_action = mgc->imap[x].action;
           this_name   = mgc->imap[x].name;

           /*
            * If the header analysis and/or negotiation phase has
            * determined this to be a CGI script then the r->content_type
            * field will be (null) but r->handler will contain "cgi-script".
            *
            * If the analysis has determined this is a static file
            * then r->handler will be (null) but the r->content_type
            * field will be "text/html" or "text/plain" or whatever.
            */

           if ( hlen > 0 ) /* r->handler field has a value... */
             {
              #ifdef MOD_GZIP_DEBUG1
              mod_gzip_printf( "%s: hlen has value...\n",cn);
              #endif

              if ( this_type == MOD_GZIP_IMAP_ISHANDLER )
                {
                 #ifdef MOD_GZIP_DEBUG1
                 mod_gzip_printf( "%s: this_type = ISHANDLER\n",cn);
                 mod_gzip_printf( "%s: Call mod_gzip_ismatch(%s,%s,0,0)...\n",
                                   cn, this_name, r->handler );
                 #endif

                 /* mod_gzip_ismatch()... */

                 /* The 2 strings must match exactly so  */
                 /* pass '0' for parm 3...               */

                 /* Wildcard matches are not allowed for */
                 /* handler strings like 'cgi-script' so */
                 /* Fourth parm should be 0..            */

                 if ( mod_gzip_ismatch(
                      this_name, (char *)r->handler,0,0) )
                   {
                    pass_result = 1;           /* We found a match */
                    action_flag = this_action; /* What to do */
                    break;                     /* Stop now */
                   }

                }/* End 'if ( this_type == MOD_GZIP_IMAP_ISHANDLER )' */

             }/* End 'if( hlen > 0 )' */

           if ( clen > 0 ) /* r->content_type field has a value... */
             {
              #ifdef MOD_GZIP_DEBUG1
              mod_gzip_printf( "%s: clen has value...\n",cn);
              #endif

              if ( this_type == MOD_GZIP_IMAP_ISMIME )
                {
                 #ifdef MOD_GZIP_DEBUG1
                 mod_gzip_printf( "%s: this_type = ISMIME\n",cn);
                 mod_gzip_printf( "%s: Wildcards matches are OK for MIME types.\n",cn);
                 mod_gzip_printf( "%s: Call mod_gzip_ismatch(%s,%s,0,1)...\n",
                                   cn, this_name, r->content_type );
                 #endif

                 /* mod_gzip_ismatch()... */

                 /* Wildcard matches are ALLOWED for    */
                 /* MIME type strings like 'cgi-script' */
                 /* so fourth parm should be 1...       */

                 if ( mod_gzip_ismatch(
                      this_name, (char *)r->content_type, 0, 1 ) )
                   {
                    pass_result = 1;           /* We found a match */
                    action_flag = this_action; /* What to do */
                    break;                     /* Stop now */
                   }

                }/* End 'if ( this_type == MOD_GZIP_IMAP_ISMIME )' */

             }/* End 'if( clen > 0 )' */

           if ( flen > 0 ) /* r->filename field has a value... */
             {
              #ifdef MOD_GZIP_DEBUG1
              mod_gzip_printf( "%s: flen has value...\n",cn);
              #endif

              if ( this_type == MOD_GZIP_IMAP_ISEXT )
                {
                 #ifdef MOD_GZIP_DEBUG1
                 mod_gzip_printf( "%s: this_type = ISEXT\n",cn);
                 #endif

                 if ( file_extension_len > 0 ) /* There is a file extension */
                   {
                    #ifdef MOD_GZIP_DEBUG1
                    mod_gzip_printf( "%s: file_extension_len has value...\n",cn);
                    mod_gzip_printf( "%s: Call mod_gzip_ismatch(%s,%s,0,0)...\n",
                                      cn, this_name, file_extension );
                    #endif

                    /* mod_gzip_ismatch()... */

                    /* The 2 strings must match exactly so  */
                    /* pass '0' for parm 3...               */

                    /* Wildcard matches are not allowed for */
                    /* file extensions like '.html' so      */
                    /* Fourth parm should be 0..            */

                    if ( mod_gzip_ismatch(
                         this_name, file_extension, 0, 0 ) )
                      {
                       pass_result = 1;           /* We found a match */
                       action_flag = this_action; /* What to do */
                       break;                     /* Stop now */
                      }

                   }/* End 'if( file_extension_len > 0 )' */

                }/* End 'if( this_type == MOD_GZIP)IMAP_ISEXT )' */

             }/* End 'if( flen > 0 )' */

          }/* End 'if ( mgc->imap[x].include == filter )' */

        else /* The record did not match the current 'filter' value... */
          {
           #ifdef MOD_GZIP_DEBUG1
           mod_gzip_printf( "%s: This record does NOT match filter_value %d\n",
                             cn, filter_value );
           mod_gzip_printf( "%s: The record has been SKIPPED...\n",cn);
           #endif
          }

       }/* End 'x' loop that looks at 'filtered' records... */

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: --------------------------------------------\n",cn);
    mod_gzip_printf( "%s: pass_result = %d\n",cn,pass_result);
    #endif

    if ( pass_result ) /* We are done... */
      {
       if ( pass == 0 ) item_is_excluded = 1;
       else             item_is_included = 1;

       break; /* Break out of 'pass' loop now... */
      }

    }/* End 'pass' loop */

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: item_is_excluded = %d\n",cn,item_is_excluded);
    mod_gzip_printf( "%s: item_is_included = %d\n",cn,item_is_included);
    mod_gzip_printf( "%s: action_flag      = %d\n",cn,action_flag);
    #endif

    if ( item_is_excluded )
      {
       #ifdef MOD_GZIP_DEBUG1
       mod_gzip_printf( "%s: The item is excluded...\n",cn);
       mod_gzip_printf( "%s: Exit > return( MOD_GZIP_IMAP_DECLINED1 ) >\n",cn);
       #endif

       if ( r->server->loglevel == APLOG_DEBUG )
         {
          ap_log_error( "",0,APLOG_NOERRNO|APLOG_DEBUG, r->server,
          "mod_gzip: This item is EXCLUDED as per httpd.conf");
         }

       return( MOD_GZIP_IMAP_DECLINED1 );
      }

    else if ( item_is_included )
      {
       #ifdef MOD_GZIP_DEBUG1
       mod_gzip_printf( "%s: The item is included...\n",cn);
       mod_gzip_printf( "%s: Exit > return( action_flag = %d ) >\n",cn,action_flag);
       #endif

       if ( r->server->loglevel == APLOG_DEBUG )
         {
          ap_log_error( "",0,APLOG_NOERRNO|APLOG_DEBUG, r->server,
          "mod_gzip: This item is INCLUDED as per httpd.conf");
         }

       return( action_flag ); /* STATIC1 or DYNAMIC1 */
      }

    /*
     * Default action is to DECLINE processing...
     */

    #ifdef MOD_GZIP_DEBUG1
    mod_gzip_printf( "%s: Exit > return( MOD_GZIP_IMAP_DECLINED1 ) >\n",cn);
    #endif

    if ( r->server->loglevel == APLOG_DEBUG )
      {
       ap_log_error( "",0,APLOG_NOERRNO|APLOG_DEBUG, r->server,
       "mod_gzip: This item was NOT FOUND in any mod_gzip httpd item record.");
       ap_log_error( "",0,APLOG_NOERRNO|APLOG_DEBUG, r->server,
       "mod_gzip: This item will NOT be processed.");
      }

    return( MOD_GZIP_IMAP_DECLINED1 );

}/* End of mod_gzip_get_action_flag() */

/*--------------------------------------------------------------------------*/
/* ALL SOURCE CODE BELOW THIS POINT IS COMPRESSION SPECIFIC...              */
/*--------------------------------------------------------------------------*/

#define USE_GATHER
extern MODULE_VAR_EXPORT int ap_suexec_enabled;
extern API_EXPORT(void)
ap_internal_redirect_handler(const char *new_uri, request_rec *);
long mod_gzip_ap_send_fb(
BUFF *fb,
request_rec *r,
int *final_return_code
);
long mod_gzip_ap_send_fb_length(
BUFF *fb,
request_rec *r,
long length,
int *final_return_code
);
#define DEFAULT_LOGBYTES 10385760
#define DEFAULT_BUFBYTES 1024
static int mod_gzip_cgi_child(void *child_stuff, child_info *pinfo);
typedef struct {
    char *logname;
    long logbytes;
    int bufbytes;
} cgi_server_conf;
struct mod_gzip_cgi_child_stuff {
#ifdef TPF
    TPF_FORK_CHILD t;
#endif
    request_rec *r;
    int nph;
    int debug;
    char *argv0;
};
static int is_scriptaliased( request_rec *r )
{
 const char *t = ap_table_get(r->notes, "alias-forced-type");
 return t && (!strcasecmp(t, "cgi-script"));
}
static int log_scripterror(request_rec *r, cgi_server_conf * conf, int ret,
           int show_errno, char *error)
{
    FILE *f;
    struct stat finfo;
    ap_log_rerror(APLOG_MARK, show_errno|APLOG_ERR, r, 
		"%s: %s", error, r->filename);
    if (!conf->logname ||
	((stat(ap_server_root_relative(r->pool, conf->logname), &finfo) == 0)
	 &&   (finfo.st_size > conf->logbytes)) ||
         ((f = ap_pfopen(r->pool, ap_server_root_relative(r->pool, conf->logname),
		      "a")) == NULL)) {
	return ret;
    }
    fprintf(f, "%%%% [%s] %s %s%s%s %s\n", ap_get_time(), r->method, r->uri,
	    r->args ? "?" : "", r->args ? r->args : "", r->protocol);
    fprintf(f, "%%%% %d %s\n", ret, r->filename);
    fprintf(f, "%%error\n%s\n", error);
    ap_pfclose(r->pool, f);
    return ret;
}
static int log_script(request_rec *r, cgi_server_conf * conf, int ret,
		  char *dbuf, const char *sbuf, BUFF *script_in, BUFF *script_err)
{
    array_header *hdrs_arr = ap_table_elts(r->headers_in);
    table_entry *hdrs = (table_entry *) hdrs_arr->elts;
    char argsbuffer[HUGE_STRING_LEN];
    FILE *f;
    int i;
    struct stat finfo;
    if (!conf->logname ||
	((stat(ap_server_root_relative(r->pool, conf->logname), &finfo) == 0)
	 &&   (finfo.st_size > conf->logbytes)) ||
         ((f = ap_pfopen(r->pool, ap_server_root_relative(r->pool, conf->logname),
		      "a")) == NULL)) {
	while (ap_bgets(argsbuffer, HUGE_STRING_LEN, script_in) > 0)
	    continue;
#if defined(WIN32) || defined(NETWARE)
        while (ap_bgets(argsbuffer, HUGE_STRING_LEN, script_err) > 0) {
            ap_log_rerror(APLOG_MARK, APLOG_ERR | APLOG_NOERRNO, r, 
                          "%s", argsbuffer);            
        }
#else
	while (ap_bgets(argsbuffer, HUGE_STRING_LEN, script_err) > 0)
	    continue;
#endif
	return ret;
    }
    fprintf(f, "%%%% [%s] %s %s%s%s %s\n", ap_get_time(), r->method, r->uri,
	    r->args ? "?" : "", r->args ? r->args : "", r->protocol);
    fprintf(f, "%%%% %d %s\n", ret, r->filename);
    fputs("%request\n", f);
    for (i = 0; i < hdrs_arr->nelts; ++i) {
	if (!hdrs[i].key)
	    continue;
	fprintf(f, "%s: %s\n", hdrs[i].key, hdrs[i].val);
    }
    if ((r->method_number == M_POST || r->method_number == M_PUT)
	&& *dbuf) {
	fprintf(f, "\n%s\n", dbuf);
    }
    fputs("%response\n", f);
    hdrs_arr = ap_table_elts(r->err_headers_out);
    hdrs = (table_entry *) hdrs_arr->elts;
    for (i = 0; i < hdrs_arr->nelts; ++i) {
	if (!hdrs[i].key)
	    continue;
	fprintf(f, "%s: %s\n", hdrs[i].key, hdrs[i].val);
    }
    if (sbuf && *sbuf)
	fprintf(f, "%s\n", sbuf);
    if (ap_bgets(argsbuffer, HUGE_STRING_LEN, script_in) > 0) {
	fputs("%stdout\n", f);
	fputs(argsbuffer, f);
	while (ap_bgets(argsbuffer, HUGE_STRING_LEN, script_in) > 0)
	    fputs(argsbuffer, f);
	fputs("\n", f);
    }
    if (ap_bgets(argsbuffer, HUGE_STRING_LEN, script_err) > 0) {
	fputs("%stderr\n", f);
	fputs(argsbuffer, f);
	while (ap_bgets(argsbuffer, HUGE_STRING_LEN, script_err) > 0)
	    fputs(argsbuffer, f);
	fputs("\n", f);
    }
    ap_bclose( script_in  );
    ap_bclose( script_err );
    ap_pfclose(r->pool, f);
    return ret;
}
int mod_gzip_cgi_handler( request_rec *r )
{
    int bytesread;
    int retval, nph, dbpos = 0;
    char *argv0, *dbuf = NULL;
    BUFF *script_out, *script_in, *script_err;
    char argsbuffer[HUGE_STRING_LEN];
    int is_included = !strcmp(r->protocol, "INCLUDED");
    void *sconf = r->server->module_config;
    int final_result = DECLINED;
    #define MOD_GZIP_ENGAGED
    #ifdef  MOD_GZIP_ENGAGED
    cgi_server_conf conf_local;
    cgi_server_conf *conf = &conf_local;
    char cgi_logname[]="";
    #else
    cgi_server_conf *conf =
    (cgi_server_conf *) ap_get_module_config(sconf, &cgi_module);
    #endif
    const char *location;
    struct mod_gzip_cgi_child_stuff cld;
    #ifdef MOD_GZIP_ENGAGED
    conf->logname  = cgi_logname;
    conf->logbytes = (long) 60000L;
    conf->bufbytes = (int)  20000;
    #endif 
    if ( r->method_number == M_OPTIONS )
      {
       r->allowed |= (1 << M_GET);
       r->allowed |= (1 << M_POST);
       return DECLINED;
      }
    if ((argv0 = strrchr(r->filename, '/')) != NULL)
      {
       argv0++;
      }
    else
      {
       argv0 = r->filename;
      }
    nph = !(strncmp(argv0, "nph-", 4));
    if ( !(ap_allow_options(r) & OPT_EXECCGI) && !is_scriptaliased(r) )
      {
       return log_scripterror(r, conf, FORBIDDEN, APLOG_NOERRNO,
              "Options ExecCGI is off in this directory");
      }
    if ( nph && is_included )
      {
       return log_scripterror(r, conf, FORBIDDEN, APLOG_NOERRNO,
              "attempt to include NPH CGI script");
      }
    #if defined(OS2) || defined(WIN32)
    if ( r->finfo.st_mode == 0 )
      {
       struct stat statbuf;
       char *newfile;
       newfile = ap_pstrcat(r->pool, r->filename, ".EXE", NULL);
       if ((stat(newfile, &statbuf) != 0) || (!S_ISREG(statbuf.st_mode)))
         {
          return log_scripterror(r, conf, NOT_FOUND, 0,
                 "script not found or unable to stat");
         }
       else 
         {
          r->filename = newfile;
         }
      }
    #else 
    if ( r->finfo.st_mode == 0 )
      {
       return log_scripterror(r, conf, NOT_FOUND, APLOG_NOERRNO,
              "script not found or unable to stat");
      }
    #endif 
    if ( S_ISDIR( r->finfo.st_mode ) )
      {
       return log_scripterror(r, conf, FORBIDDEN, APLOG_NOERRNO,
              "attempt to invoke directory as script");
      }
    if ( !ap_suexec_enabled )
      {
       if ( !ap_can_exec( &r->finfo ) )
         {
          return log_scripterror(r, conf, FORBIDDEN, APLOG_NOERRNO,
                 "file permissions deny server execution");
         }
      }
    if ((retval = ap_setup_client_block(r, REQUEST_CHUNKED_ERROR)))
      {
       return retval;
      }
    ap_add_common_vars(r);
    cld.argv0 = argv0;
    cld.r     = r;
    cld.nph   = nph;
    cld.debug = conf->logname ? 1 : 0;
    #ifdef TPF
    cld.t.filename       = r->filename;
    cld.t.subprocess_env = r->subprocess_env;
    cld.t.prog_type      = FORK_FILE;
    #endif 
    #ifdef CHARSET_EBCDIC
    ap_bsetflag( r->connection->client, B_EBCDIC2ASCII, 1 );
    #endif 
    if ( !ap_bspawn_child(
          r->main ? r->main->pool : r->pool, 
          mod_gzip_cgi_child, 
          (void *) &cld,      
          kill_after_timeout, 
          &script_out, 
          &script_in,  
          &script_err  
          )
       )
      {
       ap_log_rerror(APLOG_MARK, APLOG_ERR, r,
       "couldn't spawn child process: %s", r->filename);
       return HTTP_INTERNAL_SERVER_ERROR;
      }
    else 
      {
      }
    if ( ap_should_client_block(r) )
      {
       int dbsize, len_read;
       if ( conf->logname )
         {
          dbuf  = ap_pcalloc( r->pool, conf->bufbytes + 1 );
          dbpos = 0; 
         }
       ap_hard_timeout("copy script args", r);
       for (;;)
          {
           len_read =
           ap_get_client_block( r, argsbuffer, HUGE_STRING_LEN );
           if ( len_read < 1 ) 
             {
              break; 
             }
           if (conf->logname)
             {
              if ((dbpos + len_read) > conf->bufbytes)
                {
                 dbsize = conf->bufbytes - dbpos;
                }
              else
                {
                 dbsize = len_read;
                }
              memcpy(dbuf + dbpos, argsbuffer, dbsize);
              dbpos += dbsize;
             }
           ap_reset_timeout(r);
           if ( ap_bwrite(script_out, argsbuffer, len_read) < len_read )
             {
              while ( len_read=
                      ap_get_client_block(r, argsbuffer, HUGE_STRING_LEN) > 0)
                {
                }
              break;
             }
           else 
             {
             }
          }
       ap_bflush( script_out );
       ap_kill_timeout(r);
      }
    else 
      {
      }
    ap_bclose( script_out );
    if ( script_in && !nph )
      {
       char sbuf[MAX_STRING_LEN];
       int ret;
       if ((ret = ap_scan_script_header_err_buff(r, script_in, sbuf)))
         {
          return log_script(r, conf, ret, dbuf, sbuf, script_in, script_err);
         }
       #ifdef CHARSET_EBCDIC
       ap_checkconv(r);
       #endif 
       location = ap_table_get( r->headers_out, "Location" );
       if ( location && location[0] == '/' && r->status == 200 )
         {
          ap_hard_timeout("read from script", r);
          while ( ap_bgets(argsbuffer, HUGE_STRING_LEN, script_in) > 0 )
            {
             continue;
            }
          while (ap_bgets(argsbuffer, HUGE_STRING_LEN, script_err) > 0)
            {
             continue;
            }
          ap_kill_timeout(r);
          r->method = ap_pstrdup(r->pool, "GET");
          r->method_number = M_GET;
          ap_table_unset( r->headers_in, "Content-Length" );
          ap_internal_redirect_handler( location, r );
          return OK;
         }
       else if ( location && r->status == 200 )
         {
          return REDIRECT;
         }
       #ifdef USE_GATHER
       if ( r->header_only )
         {
          ap_send_http_header(r);
         }
       else
         {
         }
       #else /* !USE_GATHER */
       ap_send_http_header(r);
       #endif /* USE_GATHER */
       if (!r->header_only)
         {
          mod_gzip_ap_send_fb( script_in, r, &final_result );
         }
       ap_bclose( script_in );
       ap_soft_timeout("soaking script stderr", r);
       for (;;)
          {
           bytesread = ap_bgets( argsbuffer, HUGE_STRING_LEN, script_err );
           if ( bytesread < 1 )
             {
              break;
             }
          }
       ap_kill_timeout(r);
       ap_bclose( script_err );
      }
    else 
      {
      }
    if ( script_in && nph )
      {
       #ifdef RUSSIAN_APACHE
       if (ra_charset_active(r))
         {
          r->ra_codep=NULL;
         }
       #endif 
       mod_gzip_ap_send_fb( script_in, r, &final_result );
      }
    else 
      {
      }
    #ifdef ORIGINAL
    return OK; 
    #endif
    return final_result;
}
static int mod_gzip_cgi_child(void *child_stuff, child_info *pinfo)
{
    struct mod_gzip_cgi_child_stuff *cld = (struct mod_gzip_cgi_child_stuff *) child_stuff;
    request_rec *r = cld->r;
    char *argv0 = cld->argv0;
    int child_pid;

/* WARNING! If the following DEBUG_CGI switch is ON you may need to */
/* run Apache with the -X switch or the dynamic compression */
/* of some CGI output ( most notable Zope ) will start to fail. */
/* This DEBUG_CGI switch should NEVER be on for production runs. */
/*
#define DEBUG_CGI
*/

#ifdef DEBUG_CGI
#ifdef OS2
    FILE *dbg = fopen("con", "w");
#else
    #ifdef WIN32
    FILE *dbg = fopen("c:\\script.dbg", "a" );
    #else
    FILE *dbg = fopen("/dev/tty", "w");
    #endif
#endif
    int i;
#endif
    char **env;
    RAISE_SIGSTOP(CGI_CHILD);
#ifdef DEBUG_CGI
    fprintf(dbg, "Attempting to exec %s as %sCGI child (argv0 = %s)\n",
	    r->filename, cld->nph ? "NPH " : "", argv0);
#endif
    ap_add_cgi_vars(r);
    env = ap_create_environment(r->pool, r->subprocess_env);
#ifdef DEBUG_CGI
    fprintf(dbg, "Environment: \n");
    for (i = 0; env[i]; ++i)
	fprintf(dbg, "'%s'\n", env[i]);
#endif
#ifndef WIN32
    #ifdef DEBUG_CGI
    fprintf(dbg, "Call ap_chdir_file(r->filename=[%s]\n",r->filename);
    #endif
    ap_chdir_file(r->filename);
    #ifdef DEBUG_CGI
    fprintf(dbg, "Back ap_chdir_file(r->filename=[%s]\n",r->filename);
    #endif
#endif
    if (!cld->debug)
	ap_error_log2stderr(r->server);
#ifdef TPF
    #ifdef DEBUG_CGI
    #ifdef WIN32
    fprintf(dbg, "TPF defined... return( 0 ) now...\n");
    if ( dbg ) { fclose(dbg); dbg=0; }
    #endif
    #endif
    return (0);
#else
    #ifdef DEBUG_CGI
    fprintf(dbg, "Call ap_cleanup_for_exec()...\n");
    #endif
    ap_cleanup_for_exec();
    #ifdef DEBUG_CGI
    fprintf(dbg, "Back ap_cleanup_for_exec()...\n");
    fprintf(dbg, "Call ap_call_exec()...\n");
    #endif
    child_pid = ap_call_exec(r, pinfo, argv0, env, 0);
    #ifdef DEBUG_CGI
    fprintf(dbg, "Back ap_call_exec()...\n");
    #endif
#if defined(WIN32) || defined(OS2)
    #ifdef DEBUG_CGI
    #ifdef WIN32
    fprintf(dbg, "WIN32 or OS2 defined... return( child_pid ) now...\n");
    if ( dbg ) { fclose(dbg); dbg=0; }
    #endif
    #endif
    return (child_pid);
#else
    ap_log_error(APLOG_MARK, APLOG_ERR, NULL, "exec of %s failed", r->filename);
    exit(0);
    #ifdef DEBUG_CGI
    #ifdef WIN32
    if ( dbg ) { fclose(dbg); dbg=0; }
    #endif
    #endif
    return (0);
#endif
#endif  
}
#define MOD_GZIP_SET_BYTES_SENT(r) \
  do { if (r->sent_bodyct) \
          ap_bgetopt (r->connection->client, BO_BYTECT, &r->bytes_sent); \
  } while (0)
long mod_gzip_ap_send_fb( BUFF *fb, request_rec *r, int *final_return_code )
{
 long lrc;
 int  return_code=DECLINED;
 lrc = (long ) mod_gzip_ap_send_fb_length( fb, r, -1, &return_code );
 *final_return_code = return_code;
 return lrc;
}
#ifdef USE_TPF_SELECT
#define mod_gzip_ap_select(_a, _b, _c, _d, _e)   \
	tpf_select(_a, _b, _c, _d, _e)
#elif defined(SELECT_NEEDS_CAST)
#define mod_gzip_ap_select(_a, _b, _c, _d, _e)   \
    select((_a), (int *)(_b), (int *)(_c), (int *)(_d), (_e))
#else
#define mod_gzip_ap_select(_a, _b, _c, _d, _e)   \
	select(_a, _b, _c, _d, _e)
#endif
long mod_gzip_ap_send_fb_length(
BUFF *fb,
request_rec *r,
long length,
int *final_return_code
)
{
    char cn[]="mod_gzip_ab_send_fb_length()";
    char buf[IOBUFSIZE];
    long total_bytes_sent = 0;
    register int n;
    register int len;
    register int fd;
    fd_set   fds;
    int      rc;
    #ifndef  USE_GATHER
    register int w;
    register int o;
    #endif
    #ifdef USE_GATHER
    int   gather_on           = 0;
    int   gather_todisk       = 0;
    int   gather_origin       = 0;
    char *gather_bufstart     = 0;
    char *gather_source       = 0;
    char *gather_buf          = 0;
    int   gather_bufmaxlen    = 60000; 
    int   gather_byteswritten = 0;
    int   gather_length       = 0;
    int   gather_maxlen       = 0;
    long  gather_totlen       = 0; 
    FILE *gather_fh1          = 0;
    char  gather_filename[ MOD_GZIP_MAX_PATH_LEN + 2 ];
    #endif 
    void *modconf = r->server->module_config;
    mod_gzip_conf *conf;
    *final_return_code = DECLINED; 
    conf = (mod_gzip_conf *) ap_get_module_config( modconf, &gzip_module );
    if ( length == 0 )
      {
       return 0;
      }
    ap_bsetflag( fb, B_RD, 0 );
    #ifndef TPF
    ap_bnonblock( fb, B_RD );
    #endif 
    fd = ap_bfileno( fb, B_RD );
    #ifdef CHECK_FD_SETSIZE
    if ( fd >= FD_SETSIZE )
      {
       ap_log_error(APLOG_MARK, APLOG_NOERRNO|APLOG_WARNING, NULL,
       "send body: filedescriptor (%u) larger than FD_SETSIZE (%u) "
       "found, you probably need to rebuild Apache with a "
       "larger FD_SETSIZE", fd, FD_SETSIZE);
       return 0;
      }
    else 
      {
      }
    #else 
    #endif 
    ap_soft_timeout("send body", r);
    FD_ZERO( &fds );
    #ifdef USE_GATHER
    gather_on = 0; 
    if ( (long) conf->maximum_inmem_size < (long) gather_bufmaxlen )
      {
       gather_maxlen = (int) conf->maximum_inmem_size;
      }
    else
      {
       gather_maxlen = (int) gather_bufmaxlen;
      }
    gather_bufstart = malloc( (int)(gather_maxlen + 2) );
    if ( gather_bufstart )
      {
       gather_on     = 1; 
       gather_buf    = gather_bufstart;  
       gather_source = gather_bufstart;  
       gather_origin = 0;                
      }
    else 
      {
      }
    #endif 
    while( !r->connection->aborted )
      {
       #ifdef NDELAY_PIPE_RETURNS_ZERO
       int afterselect = 0;
       #endif
       if ( (length > 0) && (total_bytes_sent + IOBUFSIZE) > length )
         {
          len = length - total_bytes_sent;
         }
       else
         {
          len = IOBUFSIZE;
         }
       do {
           n = ap_bread( fb, buf, len );
           #ifdef NDELAY_PIPE_RETURNS_ZERO
           if ((n > 0) || (n == 0 && afterselect))
             {
              break;
             }
           #else 
           if (n >= 0)
             {
              break;
             }
           #endif 
           if ( r->connection->aborted )
             {
              break;
             }
           if ( n < 0 && errno != EAGAIN )
             {
              break;
             }
           if ( ap_bflush( r->connection->client ) < 0 )
             {
              ap_log_rerror(APLOG_MARK, APLOG_INFO, r,
              "client stopped connection before send body completed");
              ap_bsetflag( r->connection->client, B_EOUT, 1 );
              r->connection->aborted = 1; 
              break; 
             }
           #ifdef WIN32
           FD_SET( (unsigned) fd, &fds );
           #else
           FD_SET( fd, &fds );
           #endif
           #ifdef FUTURE_USE
           mod_gzip_ap_select(fd + 1, &fds, NULL, NULL, NULL);
           #endif
           #ifdef NDELAY_PIPE_RETURNS_ZERO
           afterselect = 1;
           #endif
          } while ( !r->connection->aborted );
       if ( n < 1 || r->connection->aborted )
         {
          break; 
         }
       #ifdef USE_GATHER
       if ( gather_on )
         {
          if ( ( gather_length + n ) >= gather_maxlen )
            {
             if ( !gather_fh1 ) 
               {
                mod_gzip_create_unique_filename(
                (mod_gzip_conf *) conf,
                (char *) gather_filename,
                sizeof(  gather_filename )
                );
                gather_fh1 = fopen( gather_filename, "wb" );
                if ( gather_fh1 )
                  {
                   gather_source = gather_filename; 
                   gather_origin = 1;               
                  }
                else
                  {
                   gather_on = 0; 
                  }
               }
             if ( ( gather_fh1 ) && ( gather_length > 0 ) )
               {
                gather_byteswritten =
                fwrite( gather_bufstart, 1, gather_length, gather_fh1 );
                if ( gather_byteswritten != gather_length )
                  {
                   gather_on = 0; 
                  }
               }
             if ( ( gather_fh1 ) && ( n > 0 ) )
               {
                gather_byteswritten =
                fwrite( buf, 1, n, gather_fh1 );
                if ( gather_byteswritten != n )
                  {
                   gather_on = 0; 
                  }
               }
             gather_buf    = gather_bufstart;
             gather_length = 0;
            }
          else 
            {
             if ( gather_on )
               {
                memcpy( gather_buf, buf, n );
                gather_buf    += n; 
                gather_length += n; 
               }
            }
          gather_totlen += n; 
         }
       #endif 
       #ifdef FUTURE_USE
       o = 0;
       while ( n && !r->connection->aborted )
         {
          #ifdef RUSSIAN_APACHE
          unsigned char *newbuf,*p;
          int newlen=0;
          if ( ra_charset_active(r) )
            {
             if ( ra_flag( r, RA_WIDE_CHARS_SC ) )
               {
                ra_data_server2client(r,&buf[o],n,&newbuf,&newlen);
                p=newbuf;
                while( newlen > 0 )
                  {
                   w = ap_bwrite( r->connection->client, p, newlen );
                   if(w<=0) goto RECODE_DONE;
                   newlen-=w;
                   p+=w;
                  }
                w=n;
               }
             else 
               {
                unsigned char *t   = r->ra_codep->cp_otabl_p;
                unsigned char *b   = (unsigned char *)&buf[o];
                unsigned char *end = b+n;
                while( b < end )
                  {
                   *b = t[*b];
                   b++;
                  }
                w = ap_bwrite( r->connection->client, &buf[o], n );
               }
            }
          else 
            {
             w = ap_bwrite( r->connection->client, &buf[o], n );
            }
          RECODE_DONE:; 
          #else 
          w = ap_bwrite( r->connection->client, &buf[o], n );
          #endif 
          if ( w > 0 ) 
            {
             ap_reset_timeout(r);
             total_bytes_sent += w;
             n -= w;
             o += w;
            }
          else if ( w < 0 ) 
            {
             if ( !r->connection->aborted )
               {
                ap_log_rerror(APLOG_MARK, APLOG_INFO, r,
                "client stopped connection before send body completed");
                ap_bsetflag(r->connection->client, B_EOUT, 1);
                r->connection->aborted = 1;
               }
             break;
            }
         }
       #endif 
      }
    ap_kill_timeout(r);
    MOD_GZIP_SET_BYTES_SENT(r); 
    #ifdef USE_GATHER
    if ( gather_fh1 ) 
      {
       if ( gather_length > 0 )
         {
          gather_byteswritten =
          fwrite( gather_bufstart, 1, gather_length, gather_fh1 );
          if ( gather_byteswritten != gather_length )
            {
             gather_on = 0; 
            }
         }
       fclose( gather_fh1 );
               gather_fh1 = 0;
      }
    if ( gather_totlen > 0 )
      {
       rc =
       mod_gzip_encode_and_transmit(
       (request_rec *) r,               
       (char        *) gather_source,   
       (int          ) gather_origin,   
       (long         ) gather_totlen,
       (int          ) 1 
       );
       *final_return_code = rc;
      }
    if ( gather_bufstart ) 
      {
       free( gather_bufstart );
             gather_bufstart = 0;
      }
    gather_on = 0; 
    #endif 
    return total_bytes_sent;
}

/*--------------------------------------------------------------------------*/
/* COMPRESSION SUPPORT ROUTINES                                             */
/*--------------------------------------------------------------------------*/

#define BIG_MEM

typedef unsigned       uns;
typedef unsigned int   uni;
typedef unsigned char  uch;
typedef unsigned short ush;
typedef unsigned long  ulg;
typedef int            gz1_file_t;

#ifdef __STDC__
   typedef void *voidp;
#else
   typedef char *voidp;
#endif

#if defined(__MSDOS__) && !defined(MSDOS)
#  define MSDOS
#endif

#if defined(__OS2__) && !defined(OS2)
#  define OS2
#endif

#if defined(OS2) && defined(MSDOS)
#  undef MSDOS
#endif

#ifdef MSDOS
#  ifdef __GNUC__
#    define near
#  else
#    define MAXSEG_64K
#    ifdef __TURBOC__
#      define NO_OFF_T
#      ifdef __BORLANDC__
#        define DIRENT
#      else
#        define NO_UTIME
#      endif
#    else
#      define HAVE_SYS_UTIME_H
#      define NO_UTIME_H
#    endif
#  endif
#  define PATH_SEP2 '\\'
#  define PATH_SEP3 ':'
#  define MAX_PATH_LEN  128
#  define NO_MULTIPLE_DOTS
#  define MAX_EXT_CHARS 3
#  define Z_SUFFIX "z"
#  define NO_CHOWN
#  define PROTO
#  define STDC_HEADERS
#  define NO_SIZE_CHECK
#  define casemap(c) tolow(c)
#  include <io.h>
#  undef  OS_CODE
#  define OS_CODE  0x00
#  define SET_BINARY_MODE(fd) setmode(fd, O_BINARY)
#  if !defined(NO_ASM) && !defined(ASMV)
#    define ASMV
#  endif
#else
#  define near
#endif

#ifdef OS2
#  define PATH_SEP2 '\\'
#  define PATH_SEP3 ':'
#  define MAX_PATH_LEN  260
#  ifdef OS2FAT
#    define NO_MULTIPLE_DOTS
#    define MAX_EXT_CHARS 3
#    define Z_SUFFIX "z"
#    define casemap(c) tolow(c)
#  endif
#  define NO_CHOWN
#  define PROTO
#  define STDC_HEADERS
#  include <io.h>
#  undef  OS_CODE
#  define OS_CODE  0x06
#  define SET_BINARY_MODE(fd) setmode(fd, O_BINARY)
#  ifdef _MSC_VER
#    define HAVE_SYS_UTIME_H
#    define NO_UTIME_H
#    define MAXSEG_64K
#    undef near
#    define near _near
#  endif
#  ifdef __EMX__
#    define HAVE_SYS_UTIME_H
#    define NO_UTIME_H
#    define DIRENT
#    define EXPAND(argc,argv) \
       {_response(&argc, &argv); _wildcard(&argc, &argv);}
#  endif
#  ifdef __BORLANDC__
#    define DIRENT
#  endif
#  ifdef __ZTC__
#    define NO_DIR
#    define NO_UTIME_H
#    include <dos.h>
#    define EXPAND(argc,argv) \
       {response_expand(&argc, &argv);}
#  endif
#endif

#ifdef WIN32
#  define HAVE_SYS_UTIME_H
#  define NO_UTIME_H
#  define PATH_SEP2 '\\'
#  define PATH_SEP3 ':'
#  undef  MAX_PATH_LEN
#  define MAX_PATH_LEN  260
#  define NO_CHOWN
#  define PROTO
#  define STDC_HEADERS
#  define SET_BINARY_MODE(fd) setmode(fd, O_BINARY)
#  include <io.h>
#  ifdef NTFAT
#    define NO_MULTIPLE_DOTS
#    define MAX_EXT_CHARS 3
#    define Z_SUFFIX "z"
#    define casemap(c) tolow(c)
#  endif
#  undef  OS_CODE

#  define OS_CODE  0x00

#endif

#ifdef MSDOS
#  ifdef __TURBOC__
#    include <alloc.h>
#    define DYN_ALLOC
     void * fcalloc (unsigned items, unsigned size);
     void fcfree (void *ptr);
#  else
#    define fcalloc(nitems,itemsize) halloc((long)(nitems),(itemsize))
#    define fcfree(ptr) hfree(ptr)
#  endif
#else
#  ifdef MAXSEG_64K
#    define fcalloc(items,size) calloc((items),(size))
#  else
#    define fcalloc(items,size) malloc((size_t)(items)*(size_t)(size))
#  endif
#  define fcfree(ptr) free(ptr)
#endif

#if defined(VAXC) || defined(VMS)
#  define PATH_SEP ']'
#  define PATH_SEP2 ':'
#  define SUFFIX_SEP ';'
#  define NO_MULTIPLE_DOTS
#  define Z_SUFFIX "-gz"
#  define RECORD_IO 1
#  define casemap(c) tolow(c)
#  undef  OS_CODE
#  define OS_CODE  0x02
#  define OPTIONS_VAR "GZIP_OPT"
#  define STDC_HEADERS
#  define NO_UTIME
#  define EXPAND(argc,argv) vms_expand_args(&argc,&argv);
#  include <file.h>
#  define unlink delete
#  ifdef VAXC
#    define NO_FCNTL_H
#    include <unixio.h>
#  endif
#endif

#ifdef AMIGA
#  define PATH_SEP2 ':'
#  define STDC_HEADERS
#  undef  OS_CODE
#  define OS_CODE  0x01
#  define ASMV
#  ifdef __GNUC__
#    define DIRENT
#    define HAVE_UNISTD_H
#  else
#    define NO_STDIN_FSTAT
#    define SYSDIR
#    define NO_SYMLINK
#    define NO_CHOWN
#    define NO_FCNTL_H
#    include <fcntl.h>
#    define direct dirent
     extern void _expand_args(int *argc, char ***argv);
#    define EXPAND(argc,argv) _expand_args(&argc,&argv);
#    undef  O_BINARY
#  endif
#endif

#if defined(ATARI) || defined(atarist)
#  ifndef STDC_HEADERS
#    define STDC_HEADERS
#    define HAVE_UNISTD_H
#    define DIRENT
#  endif
#  define ASMV
#  undef  OS_CODE
#  define OS_CODE  0x05
#  ifdef TOSFS
#    define PATH_SEP2 '\\'
#    define PATH_SEP3 ':'
#    define MAX_PATH_LEN  128
#    define NO_MULTIPLE_DOTS
#    define MAX_EXT_CHARS 3
#    define Z_SUFFIX "z"
#    define NO_CHOWN
#    define casemap(c) tolow(c)
#    define NO_SYMLINK
#  endif
#endif

#ifdef MACOS
#  define PATH_SEP ':'
#  define DYN_ALLOC
#  define PROTO
#  define NO_STDIN_FSTAT
#  define NO_CHOWN
#  define NO_UTIME
#  define chmod(file, mode) (0)
#  define OPEN(name, flags, mode) open(name, flags)
#  undef  OS_CODE
#  define OS_CODE  0x07
#  ifdef MPW
#    define isatty(fd) ((fd) <= 2)
#  endif
#endif

#ifdef __50SERIES
#  define PATH_SEP '>'
#  define STDC_HEADERS
#  define NO_MEMORY_H
#  define NO_UTIME_H
#  define NO_UTIME
#  define NO_CHOWN 
#  define NO_STDIN_FSTAT 
#  define NO_SIZE_CHECK 
#  define NO_SYMLINK
#  define RECORD_IO  1
#  define casemap(c)  tolow(c)
#  define put_char(c) put_byte((c) & 0x7F)
#  define get_char(c) ascii2pascii(get_byte())
#  undef  OS_CODE
#  define OS_CODE  0x0F
#  ifdef SIGTERM
#    undef SIGTERM
#  endif
#endif

#if defined(pyr) && !defined(NOMEMCPY)
#  define NOMEMCPY
#endif

#ifdef TOPS20
#  undef  OS_CODE
#  define OS_CODE  0x0a
#endif

#ifndef unix
#  define NO_ST_INO
#endif

#ifndef OS_CODE
#  undef  OS_CODE
#  define OS_CODE  0x03
#endif

#ifndef PATH_SEP
#  define PATH_SEP '/'
#endif

#ifndef casemap
#  define casemap(c) (c)
#endif

#ifndef OPTIONS_VAR
#  define OPTIONS_VAR "GZIP"
#endif

#ifndef Z_SUFFIX
#  define Z_SUFFIX ".gz"
#endif

#ifdef MAX_EXT_CHARS
#  define MAX_SUFFIX  MAX_EXT_CHARS
#else
#  define MAX_SUFFIX  30
#endif

#ifndef MIN_PART
#  define MIN_PART 3
#endif

#ifndef EXPAND
#  define EXPAND(argc,argv)
#endif

#ifndef RECORD_IO
#  define RECORD_IO 0
#endif

#ifndef SET_BINARY_MODE
#  define SET_BINARY_MODE(fd)
#endif

#ifndef OPEN
#  define OPEN(name, flags, mode) open(name, flags, mode)
#endif

#ifndef get_char
#  define get_char() get_byte()
#endif

#ifndef put_char
#  define put_char(c) put_byte(c)
#endif

#ifndef O_BINARY
#define O_BINARY 0
#endif

#define OK          0
#define LZ1_ERROR   1
#define WARNING     2
#define STORED      0
#define COMPRESSED  1
#define PACKED      2
#define LZHED       3
#define DEFLATED    8
#define MAX_METHODS 9

#ifndef O_CREAT
#include <sys/file.h>
#ifndef O_CREAT
#define O_CREAT FCREAT
#endif
#ifndef O_EXCL
#define O_EXCL FEXCL
#endif
#endif

#ifndef S_IRUSR
#define S_IRUSR 0400
#endif
#ifndef S_IWUSR
#define S_IWUSR 0200
#endif
#define RW_USER (S_IRUSR | S_IWUSR)

#ifndef MAX_PATH_LEN
#define MAX_PATH_LEN 256
#endif

#ifndef SEEK_END
#define SEEK_END 2
#endif

#define PACK_MAGIC     "\037\036"
#define GZIP_MAGIC     "\037\213"
#define OLD_GZIP_MAGIC "\037\236"
#define LZH_MAGIC      "\037\240"
#define PKZIP_MAGIC    "\120\113\003\004"
#define ASCII_FLAG   0x01 
#define CONTINUATION 0x02 
#define EXTRA_FIELD  0x04 
#define ORIG_NAME    0x08 
#define COMMENT      0x10 
#define ENCRYPTED    0x20 
#define RESERVED     0xC0 
#define UNKNOWN 0xffff
#define BINARY  0
#define ASCII   1

#ifndef WSIZE
#define WSIZE 0x8000
#endif

#ifndef INBUFSIZ
#ifdef  SMALL_MEM
#define INBUFSIZ  0x2000
#else
#define INBUFSIZ  0x8000
#endif
#endif
#define INBUF_EXTRA 64

#ifndef	OUTBUFSIZ
#ifdef SMALL_MEM
#define OUTBUFSIZ   8192
#else
#define OUTBUFSIZ  0x4000
#endif
#endif
#define OUTBUF_EXTRA 2048

#ifndef DIST_BUFSIZE
#ifdef  SMALL_MEM
#define DIST_BUFSIZE 0x2000
#else
#define DIST_BUFSIZE 0x8000
#endif
#endif

#ifndef BITS
#define BITS 16
#endif

#define LZW_MAGIC  "\037\235"

#define MIN_MATCH  3
#define MAX_MATCH  258

#define MIN_LOOKAHEAD (MAX_MATCH+MIN_MATCH+1)
#define MAX_DIST  (WSIZE-MIN_LOOKAHEAD)

#ifdef  SMALL_MEM
#define HASH_BITS  13
#endif
#ifdef  MEDIUM_MEM
#define HASH_BITS  14
#endif
#ifndef HASH_BITS
#define HASH_BITS  15
#endif

#define HASH_SIZE (unsigned)(1<<HASH_BITS)
#define HASH_MASK (HASH_SIZE-1)
#define WMASK     (WSIZE-1)
#define H_SHIFT   ((HASH_BITS+MIN_MATCH-1)/MIN_MATCH)

#ifndef TOO_FAR
#define TOO_FAR 4096
#endif

#define NIL          0
#define FAST         4
#define SLOW         2
#define REP_3_6      16
#define REPZ_3_10    17
#define REPZ_11_138  18
#define MAX_BITS     15
#define MAX_BL_BITS  7
#define D_CODES      30
#define BL_CODES     19
#define SMALLEST     1
#define LENGTH_CODES 29
#define LITERALS     256
#define END_BLOCK    256
#define L_CODES (LITERALS+1+LENGTH_CODES)

#ifndef LIT_BUFSIZE
#ifdef  SMALL_MEM
#define LIT_BUFSIZE  0x2000
#else
#ifdef  MEDIUM_MEM
#define LIT_BUFSIZE  0x4000
#else
#define LIT_BUFSIZE  0x8000
#endif
#endif
#endif

#define HEAP_SIZE (2*L_CODES+1)
#define STORED_BLOCK 0
#define STATIC_TREES 1
#define DYN_TREES    2
#define NO_FILE  (-1) 

#define BMAX 16         
#define N_MAX 288       

#define LOCSIG 0x04034b50L      
#define LOCFLG 6                
#define CRPFLG 1                
#define EXTFLG 8                
#define LOCHOW 8                
#define LOCTIM 10               
#define LOCCRC 14               
#define LOCSIZ 18               
#define LOCLEN 22               
#define LOCFIL 26               
#define LOCEXT 28               
#define LOCHDR 30               
#define EXTHDR 16               
#define RAND_HEAD_LEN  12
#define BUFSIZE (8 * 2*sizeof(char))

#define translate_eol 0  

#define FLUSH_BLOCK(eof) \
   flush_block(gz1,gz1->block_start >= 0L ? (char*)&gz1->window[(unsigned)gz1->block_start] : \
         (char*)NULL, (long)gz1->strstart - gz1->block_start, (eof))

#ifdef DYN_ALLOC
#  define ALLOC(type, array, size) { \
      array = (type*)fcalloc((size_t)(((size)+1L)/2), 2*sizeof(type)); \
      if (array == NULL) error("insufficient memory"); \
   }
#  define FREE(array) {if (array != NULL) fcfree(array), array=NULL;}
#else
#  define ALLOC(type, array, size)
#  define FREE(array)
#endif

#define GZ1_MAX(a,b) (a >= b ? a : b)

#define tolow(c)  (isupper(c) ? (c)-'A'+'a' : (c))    

#define smaller(tree, n, m) \
   (tree[n].fc.freq < tree[m].fc.freq || \
   (tree[n].fc.freq == tree[m].fc.freq && gz1->depth[n] <= gz1->depth[m]))

#define send_code(c, tree) send_bits(gz1,tree[c].fc.code, tree[c].dl.len)

#define put_byte(c) {gz1->outbuf[gz1->outcnt++]=(uch)(c); if (gz1->outcnt==OUTBUFSIZ)\
                     flush_outbuf(gz1);}

#define put_short(w) \
{ if (gz1->outcnt < OUTBUFSIZ-2) { \
    gz1->outbuf[gz1->outcnt++] = (uch) ((w) & 0xff); \
    gz1->outbuf[gz1->outcnt++] = (uch) ((ush)(w) >> 8); \
  } else { \
    put_byte((uch)((w) & 0xff)); \
    put_byte((uch)((ush)(w) >> 8)); \
  } \
}

#define put_long(n) { \
    put_short((n) & 0xffff); \
    put_short(((ulg)(n)) >> 16); \
}

#ifdef CRYPT

#  define NEXTBYTE() \
     (decrypt ? (cc = get_byte(), zdecode(cc), cc) : get_byte())
#else
#  define NEXTBYTE() (uch)get_byte()
#endif

#define NEEDBITS(n) {while(k<(n)){b|=((ulg)NEXTBYTE())<<k;k+=8;}}
#define DUMPBITS(n) {b>>=(n);k-=(n);}

#define SH(p) ((ush)(uch)((p)[0]) | ((ush)(uch)((p)[1]) << 8))
#define LG(p) ((ulg)(SH(p)) | ((ulg)(SH((p)+2)) << 16))

#define put_ubyte(c) {gz1->window[gz1->outcnt++]=(uch)(c); if (gz1->outcnt==WSIZE)\
   flush_window(gz1);}

#define WARN(msg) { if (gz1->exit_code == OK) gz1->exit_code = WARNING; }

#define get_byte()  (gz1->inptr < gz1->insize ? gz1->inbuf[gz1->inptr++] : fill_inbuf(gz1,0))
#define try_byte()  (gz1->inptr < gz1->insize ? gz1->inbuf[gz1->inptr++] : fill_inbuf(gz1,1))

#define d_code(dist) \
   ((dist) < 256 ? gz1->dist_code[dist] : gz1->dist_code[256+((dist)>>7)])

typedef struct config {
   ush good_length; 
   ush max_lazy;    
   ush nice_length; 
   ush max_chain;
} config;

config configuration_table[10] = {

 {0,    0,  0,    0},  
 {4,    4,  8,    4},  
 {4,    5, 16,    8},
 {4,    6, 32,   32},
 {4,    4, 16,   16},  
 {8,   16, 32,   32},
 {8,   16, 128, 128},
 {8,   32, 128, 256},
 {32, 128, 258, 1024},
 {32, 258, 258, 4096}}; 

typedef struct ct_data {

    union {
        ush  freq; 
        ush  code; 
    } fc;
    union {
        ush  dad;  
        ush  len;  
    } dl;

} ct_data;

typedef struct tree_desc {
    ct_data *dyn_tree;    
    ct_data *static_tree; 
    int     *extra_bits;  
    int     extra_base;   
    int     elems;        
    int     max_length;   
    int     max_code;     
} tree_desc;

struct huft {
  uch e;                
  uch b;                
  union {
    ush n;              
    struct huft *t;     
  } v;
};

uch bl_order[BL_CODES]
   = {16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15};

int extra_lbits[LENGTH_CODES]
   = {0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0};

int extra_dbits[D_CODES]
   = {0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13};

int extra_blbits[BL_CODES]
   = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,3,7};

ulg crc_32_tab[] = {
  0x00000000L, 0x77073096L, 0xee0e612cL, 0x990951baL, 0x076dc419L,
  0x706af48fL, 0xe963a535L, 0x9e6495a3L, 0x0edb8832L, 0x79dcb8a4L,
  0xe0d5e91eL, 0x97d2d988L, 0x09b64c2bL, 0x7eb17cbdL, 0xe7b82d07L,
  0x90bf1d91L, 0x1db71064L, 0x6ab020f2L, 0xf3b97148L, 0x84be41deL,
  0x1adad47dL, 0x6ddde4ebL, 0xf4d4b551L, 0x83d385c7L, 0x136c9856L,
  0x646ba8c0L, 0xfd62f97aL, 0x8a65c9ecL, 0x14015c4fL, 0x63066cd9L,
  0xfa0f3d63L, 0x8d080df5L, 0x3b6e20c8L, 0x4c69105eL, 0xd56041e4L,
  0xa2677172L, 0x3c03e4d1L, 0x4b04d447L, 0xd20d85fdL, 0xa50ab56bL,
  0x35b5a8faL, 0x42b2986cL, 0xdbbbc9d6L, 0xacbcf940L, 0x32d86ce3L,
  0x45df5c75L, 0xdcd60dcfL, 0xabd13d59L, 0x26d930acL, 0x51de003aL,
  0xc8d75180L, 0xbfd06116L, 0x21b4f4b5L, 0x56b3c423L, 0xcfba9599L,
  0xb8bda50fL, 0x2802b89eL, 0x5f058808L, 0xc60cd9b2L, 0xb10be924L,
  0x2f6f7c87L, 0x58684c11L, 0xc1611dabL, 0xb6662d3dL, 0x76dc4190L,
  0x01db7106L, 0x98d220bcL, 0xefd5102aL, 0x71b18589L, 0x06b6b51fL,
  0x9fbfe4a5L, 0xe8b8d433L, 0x7807c9a2L, 0x0f00f934L, 0x9609a88eL,
  0xe10e9818L, 0x7f6a0dbbL, 0x086d3d2dL, 0x91646c97L, 0xe6635c01L,
  0x6b6b51f4L, 0x1c6c6162L, 0x856530d8L, 0xf262004eL, 0x6c0695edL,
  0x1b01a57bL, 0x8208f4c1L, 0xf50fc457L, 0x65b0d9c6L, 0x12b7e950L,
  0x8bbeb8eaL, 0xfcb9887cL, 0x62dd1ddfL, 0x15da2d49L, 0x8cd37cf3L,
  0xfbd44c65L, 0x4db26158L, 0x3ab551ceL, 0xa3bc0074L, 0xd4bb30e2L,
  0x4adfa541L, 0x3dd895d7L, 0xa4d1c46dL, 0xd3d6f4fbL, 0x4369e96aL,
  0x346ed9fcL, 0xad678846L, 0xda60b8d0L, 0x44042d73L, 0x33031de5L,
  0xaa0a4c5fL, 0xdd0d7cc9L, 0x5005713cL, 0x270241aaL, 0xbe0b1010L,
  0xc90c2086L, 0x5768b525L, 0x206f85b3L, 0xb966d409L, 0xce61e49fL,
  0x5edef90eL, 0x29d9c998L, 0xb0d09822L, 0xc7d7a8b4L, 0x59b33d17L,
  0x2eb40d81L, 0xb7bd5c3bL, 0xc0ba6cadL, 0xedb88320L, 0x9abfb3b6L,
  0x03b6e20cL, 0x74b1d29aL, 0xead54739L, 0x9dd277afL, 0x04db2615L,
  0x73dc1683L, 0xe3630b12L, 0x94643b84L, 0x0d6d6a3eL, 0x7a6a5aa8L,
  0xe40ecf0bL, 0x9309ff9dL, 0x0a00ae27L, 0x7d079eb1L, 0xf00f9344L,
  0x8708a3d2L, 0x1e01f268L, 0x6906c2feL, 0xf762575dL, 0x806567cbL,
  0x196c3671L, 0x6e6b06e7L, 0xfed41b76L, 0x89d32be0L, 0x10da7a5aL,
  0x67dd4accL, 0xf9b9df6fL, 0x8ebeeff9L, 0x17b7be43L, 0x60b08ed5L,
  0xd6d6a3e8L, 0xa1d1937eL, 0x38d8c2c4L, 0x4fdff252L, 0xd1bb67f1L,
  0xa6bc5767L, 0x3fb506ddL, 0x48b2364bL, 0xd80d2bdaL, 0xaf0a1b4cL,
  0x36034af6L, 0x41047a60L, 0xdf60efc3L, 0xa867df55L, 0x316e8eefL,
  0x4669be79L, 0xcb61b38cL, 0xbc66831aL, 0x256fd2a0L, 0x5268e236L,
  0xcc0c7795L, 0xbb0b4703L, 0x220216b9L, 0x5505262fL, 0xc5ba3bbeL,
  0xb2bd0b28L, 0x2bb45a92L, 0x5cb36a04L, 0xc2d7ffa7L, 0xb5d0cf31L,
  0x2cd99e8bL, 0x5bdeae1dL, 0x9b64c2b0L, 0xec63f226L, 0x756aa39cL,
  0x026d930aL, 0x9c0906a9L, 0xeb0e363fL, 0x72076785L, 0x05005713L,
  0x95bf4a82L, 0xe2b87a14L, 0x7bb12baeL, 0x0cb61b38L, 0x92d28e9bL,
  0xe5d5be0dL, 0x7cdcefb7L, 0x0bdbdf21L, 0x86d3d2d4L, 0xf1d4e242L,
  0x68ddb3f8L, 0x1fda836eL, 0x81be16cdL, 0xf6b9265bL, 0x6fb077e1L,
  0x18b74777L, 0x88085ae6L, 0xff0f6a70L, 0x66063bcaL, 0x11010b5cL,
  0x8f659effL, 0xf862ae69L, 0x616bffd3L, 0x166ccf45L, 0xa00ae278L,
  0xd70dd2eeL, 0x4e048354L, 0x3903b3c2L, 0xa7672661L, 0xd06016f7L,
  0x4969474dL, 0x3e6e77dbL, 0xaed16a4aL, 0xd9d65adcL, 0x40df0b66L,
  0x37d83bf0L, 0xa9bcae53L, 0xdebb9ec5L, 0x47b2cf7fL, 0x30b5ffe9L,
  0xbdbdf21cL, 0xcabac28aL, 0x53b39330L, 0x24b4a3a6L, 0xbad03605L,
  0xcdd70693L, 0x54de5729L, 0x23d967bfL, 0xb3667a2eL, 0xc4614ab8L,
  0x5d681b02L, 0x2a6f2b94L, 0xb40bbe37L, 0xc30c8ea1L, 0x5a05df1bL,
  0x2d02ef8dL
};

typedef struct _GZ1 {
 long     compression_format;

 long     versionid1;
 int      state;
 int      done;
 int      deflate1_initialized;     
 unsigned deflate1_hash_head;       
 unsigned deflate1_prev_match;      
 int      deflate1_flush;           
 int      deflate1_match_available; 
 unsigned deflate1_match_length;    

 char ifname[MAX_PATH_LEN]; 
 char ofname[MAX_PATH_LEN]; 

 struct stat istat;     
 gz1_file_t  zfile;
 
 int   input_ismem;     
 char *input_ptr;       
 long  input_bytesleft; 
 
 int   output_ismem;    
 char *output_ptr;      
 uns   output_maxlen;   

 int  compr_level;      
 long time_stamp;       
 long ifile_size;       
 int  ifd;              
 int  ofd;              
 int  part_nb;          
 int  last_member;      
 int  save_orig_name;   
 long header_bytes;     
 long bytes_in;         
 long bytes_out;        
 uns  insize;           
 uns  inptr;            
 uns  outcnt;           
 uns  ins_h;            
 long block_start;      
 uns  good_match;       
 uni  max_lazy_match;   
 uni  prev_length;      
 uns  max_chain_length; 
 uns  strstart;         
 uns  match_start;      
 int  eofile;           
 uns  lookahead;        
 ush *file_type;        
 int *file_method;      
 ulg  opt_len;          
 ulg  static_len;       
 ulg  compressed_len;   
 ulg  input_len;        
 uns  last_flags;       
 uch  flags;            
 uns  last_lit;         
 uns  last_dist;        
 uch  flag_bit;         
 int  heap_len;         
 int  heap_max;         
 ulg  bb;               
 uns  bk;               
 ush  bi_buf;           
 int  bi_valid;         
 uns  hufts;            
 int  decrypt;          
 int  ascii;            
 int  msg_done;         
 int  abortflag;        
 int  decompress;       
 int  do_lzw;           
 int  to_stdout;        
 int  force;            
 int  verbose;
 int  quiet;
 int  list;             
 int  test;             
 int  ext_header;       
 int  pkzip;            
 int  method;           
 int  level;            
 int  no_time;          
 int  no_name;          
 int  exit_code;        
 int  lbits;            
 int  dbits;            
 ulg  window_size;      
 ulg  crc;              
 ulg  adler;

 uch  dist_code[512];
 uch  length_code[MAX_MATCH-MIN_MATCH+1];
 int  heap[2*L_CODES+1];
 uch  depth[2*L_CODES+1];
 int  base_length[LENGTH_CODES];
 int  base_dist[D_CODES];
 ush  bl_count[MAX_BITS+1];
 uch  flag_buf[(LIT_BUFSIZE/8)];

 #ifdef DYN_ALLOC
 uch *inbuf;
 uch *outbuf;
 ush *d_buf;
 uch *window;
 #else
 uch inbuf [INBUFSIZ +INBUF_EXTRA];
 uch outbuf[OUTBUFSIZ+OUTBUF_EXTRA];
 ush d_buf [DIST_BUFSIZE];
 uch window[2L*WSIZE];
 #endif

 #ifdef FULL_SEARCH
 #define nice_match MAX_MATCH
 #else
 int nice_match;
 #endif

 #ifdef CRYPT
 uch cc;
 #endif

 ct_data static_ltree[L_CODES+2];
 ct_data static_dtree[D_CODES];
 ct_data dyn_dtree[(2*D_CODES+1)];
 ct_data dyn_ltree[HEAP_SIZE];
 ct_data bl_tree[2*BL_CODES+1];

 tree_desc l_desc;
 tree_desc d_desc;
 tree_desc bl_desc;

 #ifndef MAXSEG_64K

 ush prev2[1L<<BITS];

 #define prev gz1->prev2
 #define head (gz1->prev2+WSIZE)

 #else

 ush * prev2;
 ush * tab_prefix1;

 #define prev gz1->prev2
 #define head gz1->tab_prefix1

 #endif

} GZ1;
typedef GZ1 *PGZ1;
int gz1_size = sizeof( GZ1 );

/* Declare some local function protypes... */

/* Any routine name that can/might conflict with */
/* other modules or object code should simply have */
/* the standard prefix 'gz1_' added to the front. */
/* This will only usually be any kind of problem at all */
/* if the code is being compiled directly into the parent */
/* instead of being built as a standalone DLL or DSO library. */

PGZ1  gz1_init        ( void     );
int   gz1_cleanup     ( PGZ1 gz1 );
ulg   gz1_deflate     ( PGZ1 gz1 );
ulg   gz1_deflate_fast( PGZ1 gz1 );

/* The rest of the routines should not need the 'gz1_' prefix. */
/* No conflicts reported at this time. */

int   inflate        ( PGZ1 gz1 );
int   inflate_dynamic( PGZ1 gz1 );
int   inflate_stored ( PGZ1 gz1 );
int   inflate_fixed  ( PGZ1 gz1 );
void  fill_window    ( PGZ1 gz1 );
void  flush_outbuf   ( PGZ1 gz1 );
void  flush_window   ( PGZ1 gz1 );
void  bi_windup      ( PGZ1 gz1 );
void  set_file_type  ( PGZ1 gz1 );
void  init_block     ( PGZ1 gz1 );
int   build_bl_tree  ( PGZ1 gz1 );
void  read_error     ( PGZ1 gz1 );
void  write_error    ( PGZ1 gz1 );
int   get_header     ( PGZ1 gz1, int in );
int   inflate_block  ( PGZ1 gz1, int *e );
int   fill_inbuf     ( PGZ1 gz1, int eof_ok );
char *gz1_basename   ( PGZ1 gz1, char *fname );
int   longest_match  ( PGZ1 gz1, unsigned cur_match );
void  bi_init        ( PGZ1 gz1, gz1_file_t zipfile );
int   file_read      ( PGZ1 gz1, char *buf, unsigned size );
void  write_buf      ( PGZ1 gz1, int fd, voidp buf, unsigned cnt );

void  error( char *msg   );

/* XXX - Precomputed zlib header. If you change the window size or
 * compression level from the defaults, this will break badly. The
 * algorithm to build this is fairly complex; you can find it in
 * the file deflate.c from the zlib distribution.
 */
#define ZLIB_HEADER "\170œ"

ulg adler32(ulg adler, uch *buf, unsigned len);

int zip(
PGZ1 gz1, 
int  in,  
int  out  
);

ulg flush_block(
PGZ1  gz1,        
char *buf,        
ulg   stored_len, 
int   eof         
);

void copy_block(
PGZ1      gz1,    
char     *buf,    
unsigned  len,    
int       header  
);

int ct_tally(
PGZ1 gz1,  
int  dist, 
int  lc    
);

void send_bits(
PGZ1 gz1,   
int  value, 
int  length 
);

void send_tree(
PGZ1      gz1,     
ct_data  *tree,    
int       max_code 
);

void send_all_trees(
PGZ1 gz1,    
int  lcodes, 
int  dcodes, 
int  blcodes 
);

void ct_init(
PGZ1  gz1,    
ush  *attr,   
int  *methodp 
);

void lm_init(
PGZ1 gz1,        
int  pack_level, 
ush *flags       
);

void build_tree(
PGZ1        gz1, 
tree_desc  *desc 
);

void compress_block(
PGZ1      gz1,   
ct_data  *ltree, 
ct_data  *dtree  
);

void gen_bitlen(
PGZ1        gz1, 
tree_desc  *desc 
);

void pqdownheap(
PGZ1      gz1,  
ct_data  *tree, 
int       k     
);

int huft_build(
PGZ1          gz1, 
unsigned     *b,   
unsigned      n,   
unsigned      s,   
ush          *d,   
ush          *e,   
struct huft **t,   
int          *m    
);

ulg updcrc(
PGZ1      gz1, 
uch      *s,   
unsigned  n    
);

int inflate_codes(
PGZ1         gz1,  
struct huft *tl,   
struct huft *td,   
int          bl,   
int          bd    
);

void gen_codes(
PGZ1      gz1,     
ct_data  *tree,    
int       max_code 
);

void scan_tree(
PGZ1     gz1,     
ct_data *tree,    
int      max_code 
);

unsigned bi_reverse(
PGZ1     gz1,  
unsigned code, 
int      len   
);

int huft_free(
PGZ1         gz1, 
struct huft *t    
);

PGZ1 gz1_init()
{
 PGZ1 gz1=0; 

 gz1 = (PGZ1) malloc( gz1_size );

 if ( !gz1 ) 
   {
    return 0; 
   }

 memset( gz1, 0, gz1_size );

 ALLOC(uch, gz1->inbuf,  INBUFSIZ +INBUF_EXTRA);

 if ( !gz1->inbuf ) 
   {
    free( gz1 ); 
    return 0;    
   }

 ALLOC(uch, gz1->outbuf, OUTBUFSIZ+OUTBUF_EXTRA);
 
 if ( !gz1->outbuf ) 
   {
    FREE( gz1->inbuf  ); 
    free( gz1         ); 
    return 0;            
   }

 ALLOC(ush, gz1->d_buf,  DIST_BUFSIZE);

 if ( !gz1->d_buf ) 
   {
    FREE( gz1->outbuf ); 
    FREE( gz1->inbuf  ); 
    free( gz1         ); 
    return 0;            
   }

 ALLOC(uch, gz1->window, 2L*WSIZE);

 if ( !gz1->window ) 
   {
    FREE( gz1->d_buf  ); 
    FREE( gz1->outbuf ); 
    FREE( gz1->inbuf  ); 
    free( gz1         ); 
    return 0;            
   }

 #ifndef MAXSEG_64K
 
 #else 
 
 ALLOC(ush, gz1->prev2, 1L<<(BITS-1) );

 if ( !gz1->prev2 ) 
   {
    FREE( gz1->window ); 
    FREE( gz1->d_buf  ); 
    FREE( gz1->outbuf ); 
    FREE( gz1->inbuf  ); 
    free( gz1         ); 
    return 0;            
   }

 ALLOC(ush, gz1->tab_prefix1, 1L<<(BITS-1) );

 if ( !gz1->tab_prefix1 ) 
   {
    FREE( gz1->prev2  ); 
    FREE( gz1->window ); 
    FREE( gz1->d_buf  ); 
    FREE( gz1->outbuf ); 
    FREE( gz1->inbuf  ); 
    free( gz1         ); 
    return 0;            
   }

 #endif 

 gz1->method      = DEFLATED;     
 gz1->level       = 6;            
 gz1->no_time     = -1;           
 gz1->no_name     = -1;           
 gz1->exit_code   = OK;           
 gz1->lbits       = 9;            
 gz1->dbits       = 6;            

 gz1->window_size = (ulg)2*WSIZE;     
 gz1->crc         = (ulg)0xffffffffL; 

 gz1->d_desc.dyn_tree     = (ct_data *) gz1->dyn_dtree;
 gz1->d_desc.static_tree  = (ct_data *) gz1->static_dtree;
 gz1->d_desc.extra_bits   = (int     *) extra_dbits; 
 gz1->d_desc.extra_base   = (int      ) 0;
 gz1->d_desc.elems        = (int      ) D_CODES;
 gz1->d_desc.max_length   = (int      ) MAX_BITS;
 gz1->d_desc.max_code     = (int      ) 0;

 gz1->l_desc.dyn_tree     = (ct_data *) gz1->dyn_ltree;
 gz1->l_desc.static_tree  = (ct_data *) gz1->static_ltree;
 gz1->l_desc.extra_bits   = (int     *) extra_lbits; 
 gz1->l_desc.extra_base   = (int      ) LITERALS+1;
 gz1->l_desc.elems        = (int      ) L_CODES;
 gz1->l_desc.max_length   = (int      ) MAX_BITS;
 gz1->l_desc.max_code     = (int      ) 0;

 gz1->bl_desc.dyn_tree    = (ct_data *) gz1->bl_tree;
 gz1->bl_desc.static_tree = (ct_data *) 0;
 gz1->bl_desc.extra_bits  = (int     *) extra_blbits; 
 gz1->bl_desc.extra_base  = (int      ) 0;
 gz1->bl_desc.elems       = (int      ) BL_CODES;
 gz1->bl_desc.max_length  = (int      ) MAX_BL_BITS;
 gz1->bl_desc.max_code    = (int      ) 0;

 return (PGZ1) gz1;

}

int gz1_cleanup( PGZ1 gz1 )
{
 
 #ifndef MAXSEG_64K
 
 #else
 
 FREE( gz1->tab_prefix1 );
 FREE( gz1->prev2       );
 #endif 

 FREE( gz1->window ); 
 FREE( gz1->d_buf  ); 
 FREE( gz1->outbuf ); 
 FREE( gz1->inbuf  ); 

 free( gz1 ); 
 gz1 = 0;     

 return 0;
}

int (*read_buf)(PGZ1 gz1, char *buf, unsigned size);

void error( char *msg )
{
 msg = msg;
}

int (*work)( PGZ1 gz1, int infile, int outfile ) = 0; 

#ifdef __BORLANDC__
#pragma argsused
#endif

int get_header( PGZ1 gz1, int in )
{
 uch       flags;    
 char      magic[2]; 
 ulg       stamp;    
 unsigned  len;      
 unsigned  part;     

 if ( gz1->force && gz1->to_stdout )
   {
    magic[0] = (char)try_byte();
    magic[1] = (char)try_byte();
   }
 else
   {
    magic[0] = (char)get_byte();
    magic[1] = (char)get_byte();
   }

 gz1->method       = -1;        
 gz1->header_bytes = 0;         
 gz1->last_member  = RECORD_IO; 
 gz1->part_nb++;                

 if ( memcmp(magic, GZIP_MAGIC,     2 ) == 0 ||
      memcmp(magic, OLD_GZIP_MAGIC, 2 ) == 0 )
   {
    gz1->method = (int)get_byte();

    if ( gz1->method != DEFLATED )
      {
       gz1->exit_code = LZ1_ERROR;

       return -1;
      }

    return -1;

    if ((flags & ENCRYPTED) != 0)
      {
       gz1->exit_code = LZ1_ERROR;
       return -1;
      }

    if ((flags & CONTINUATION) != 0)
      {
       gz1->exit_code = LZ1_ERROR;
       if ( gz1->force <= 1) return -1;
      }

    if ((flags & RESERVED) != 0)
      {
       gz1->exit_code = LZ1_ERROR;
       if ( gz1->force <= 1)
          return -1;
      }

    stamp  = (ulg)get_byte();
	stamp |= ((ulg)get_byte()) << 8;
	stamp |= ((ulg)get_byte()) << 16;
	stamp |= ((ulg)get_byte()) << 24;

    if ( stamp != 0 && !gz1->no_time )
      {
       gz1->time_stamp = stamp;
      }

    (void)get_byte(); 
    (void)get_byte(); 

    if ((flags & CONTINUATION) != 0)
      {
       part  = (unsigned)  get_byte();
       part |= ((unsigned) get_byte())<<8;
      }

    if ((flags & EXTRA_FIELD) != 0)
      {
        len  = (unsigned)  get_byte();
        len |= ((unsigned) get_byte())<<8;

        while (len--) (void)get_byte();
      }

    if ((flags & COMMENT) != 0)
      {
       while (get_char() != 0)  ;
      }

    if ( gz1->part_nb == 1 )
      {
       gz1->header_bytes = gz1->inptr + 2*sizeof(long);
      }
   }

 return gz1->method;
}

int fill_inbuf( PGZ1 gz1, int eof_ok )
{
 int len;
 int bytes_to_copy;

 gz1->insize = 0;
 errno       = 0;

 do {
     if ( gz1->input_ismem )
       {
        if ( gz1->input_bytesleft > 0 )
          {
           bytes_to_copy = INBUFSIZ - gz1->insize;

           if ( bytes_to_copy > gz1->input_bytesleft )
             {
              bytes_to_copy = gz1->input_bytesleft;
             }

           memcpy(
           (char*)gz1->inbuf+gz1->insize,
           gz1->input_ptr,
           bytes_to_copy
           );

           gz1->input_ptr       += bytes_to_copy;
           gz1->input_bytesleft -= bytes_to_copy;

           len = bytes_to_copy;
          }
        else 
          {
           len = 0; 
          }
       }
     else 
       {
        len =
        read(
        gz1->ifd,
        (char*)gz1->inbuf+gz1->insize,
        INBUFSIZ-gz1->insize
        );
       }

     if (len == 0 || len == EOF) break;

     gz1->insize += len;

    } while( gz1->insize < INBUFSIZ );

 if ( gz1->insize == 0 )
   {
    if( eof_ok ) return EOF;
    read_error( gz1 );
   }

 gz1->bytes_in += (ulg) gz1->insize;
 gz1->inptr     = 1;

 return gz1->inbuf[0];
}

ulg updcrc(
PGZ1      gz1, 
uch      *s,   
unsigned  n    
)
{
 register ulg c; 

 if ( s == NULL )
   {
    c = 0xffffffffL;
   }
 else
   {
    c = gz1->crc;

    if ( n )
      {
       do{
          c = crc_32_tab[((int)c ^ (*s++)) & 0xff] ^ (c >> 8);

         } while( --n );
      }
   }

 gz1->crc = c;

 return( c ^ 0xffffffffL ); 
}

void read_error( PGZ1 gz1 )
{
 gz1->abortflag = 1;
}

void mod_gzip_strlwr( char *s )
{
 char *p1=s;

 if ( s == 0 ) return;

 while ( *p1 != 0 )
   {
    if ( *p1 > 96 ) *p1 = *p1 - 32;
    p1++;
   }
}

#ifdef __BORLANDC__
#pragma argsused
#endif

char *gz1_basename( PGZ1 gz1, char *fname )
{
 char *p;

 if ((p = strrchr(fname, PATH_SEP))  != NULL) fname = p+1;

 #ifdef PATH_SEP2
 if ((p = strrchr(fname, PATH_SEP2)) != NULL) fname = p+1;
 #endif

 #ifdef PATH_SEP3
 if ((p = strrchr(fname, PATH_SEP3)) != NULL) fname = p+1;
 #endif

 #ifdef SUFFIX_SEP
 if ((p = strrchr(fname, SUFFIX_SEP)) != NULL) *p = '\0';
 #endif

 if (casemap('A') == 'a') mod_gzip_strlwr(fname);

 return fname;
}

void write_buf( PGZ1 gz1, int fd, voidp buf, unsigned cnt )
{
 unsigned n;

 if ( gz1->output_ismem )
   {
    if ( ( gz1->bytes_out + cnt ) < gz1->output_maxlen )
      {
       memcpy( gz1->output_ptr, buf, cnt );
       gz1->output_ptr += cnt;
      }
    else
      {
       write_error( gz1 );
      }
   }
 else
   {
    while ((n = write(fd, buf, cnt)) != cnt)
      {
       if (n == (unsigned)(-1))
         {
          write_error( gz1 );
         }
       cnt -= n;
       buf = (voidp)((char*)buf+n);
      }
   }
}

void write_error( PGZ1 gz1 )
{
 gz1->abortflag = 1;
}

#ifdef __TURBOC__
#ifndef BC55

static ush ptr_offset = 0;

void * fcalloc(
unsigned items, 
unsigned size   
)
{
 void * buf = farmalloc((ulg)items*size + 16L);

 if (buf == NULL) return NULL;

 if (ptr_offset == 0)
   {
    ptr_offset = (ush)((uch*)buf-0);
   }
 else if (ptr_offset != (ush)((uch*)buf-0))
   {
    error("inconsistent ptr_offset");
   }

 *((ush*)&buf+1) += (ptr_offset + 15) >> 4;
 *(ush*)&buf = 0;

 return buf;
}

void fcfree( void *ptr )
{
 *((ush*)&ptr+1) -= (ptr_offset + 15) >> 4;
 *(ush*)&ptr = ptr_offset;

 farfree(ptr);
}

#endif 
#endif 

int zip(
PGZ1 gz1, 
int in,   
int out   
)
{
 uch  flags = 0;         
 ush  attr  = 0;         
 ush  deflate_flags = 0; 

 gz1->ifd    = in;
 gz1->ofd    = out;
 gz1->outcnt = 0;

 gz1->method = DEFLATED;

 put_byte(GZIP_MAGIC[0]); 
 put_byte(GZIP_MAGIC[1]);
 put_byte(DEFLATED);      

 if ( gz1->save_orig_name )
   {
	flags |= ORIG_NAME;
   }

 put_byte(flags);           
 put_long(gz1->time_stamp); 

 gz1->crc = -1; 

 updcrc( gz1, NULL, 0 ); 

 bi_init( gz1, out );
 ct_init( gz1, &attr, &gz1->method );
 lm_init( gz1, gz1->level, &deflate_flags );

 put_byte((uch)deflate_flags); 

 put_byte(OS_CODE); 

 if ( gz1->save_orig_name )
   {
    char *p = gz1_basename( gz1, gz1->ifname );

    do {
	    put_char(*p);

       } while (*p++);
   }

 gz1->header_bytes = (long)gz1->outcnt;

 (void) gz1_deflate( gz1 );

 put_long( gz1->crc      );
 put_long( gz1->bytes_in );

 gz1->header_bytes += 2*sizeof(long);

 flush_outbuf( gz1 );

 return OK;
}

ulg gz1_deflate( PGZ1 gz1 )
{
    unsigned hash_head;      
    unsigned prev_match;     
    int flush;               
    int match_available = 0; 
    register unsigned match_length = MIN_MATCH-1; 
#ifdef DEBUG
    long isize;        
#endif

    if (gz1->compr_level <= 3)
      {
       return gz1_deflate_fast(gz1);
      }

    while (gz1->lookahead != 0)
      {
       gz1->ins_h =
       (((gz1->ins_h)<<H_SHIFT) ^ (gz1->window[gz1->strstart+MIN_MATCH-1])) & HASH_MASK;

       prev[ gz1->strstart & WMASK ] = hash_head = head[ gz1->ins_h ];

       head[ gz1->ins_h ] = gz1->strstart;

        gz1->prev_length = match_length, prev_match = gz1->match_start;
        match_length = MIN_MATCH-1;

        if (hash_head != NIL && gz1->prev_length < gz1->max_lazy_match &&
            gz1->strstart - hash_head <= MAX_DIST) {
            
            match_length = longest_match( gz1, hash_head );
            
            if (match_length > gz1->lookahead) match_length = gz1->lookahead;

            if (match_length == MIN_MATCH && gz1->strstart-gz1->match_start > TOO_FAR){
                
                match_length--;
            }
        }
        
        if (gz1->prev_length >= MIN_MATCH && match_length <= gz1->prev_length) {

            flush = ct_tally(gz1,gz1->strstart-1-prev_match, gz1->prev_length - MIN_MATCH);

            gz1->lookahead        -= ( gz1->prev_length - 1 );
            gz1->prev_length -= 2;

            do {
                gz1->strstart++;

                gz1->ins_h =
                (((gz1->ins_h)<<H_SHIFT) ^ (gz1->window[ gz1->strstart + MIN_MATCH-1])) & HASH_MASK;

                prev[ gz1->strstart & WMASK ] = hash_head = head[gz1->ins_h];

                head[ gz1->ins_h ] = gz1->strstart;

            } while (--gz1->prev_length != 0);
            match_available = 0;
            match_length = MIN_MATCH-1;
            gz1->strstart++;
            if (flush) FLUSH_BLOCK(0), gz1->block_start = gz1->strstart;

        } else if (match_available) {
            
            if (ct_tally( gz1, 0, gz1->window[gz1->strstart-1] )) {
                FLUSH_BLOCK(0), gz1->block_start = gz1->strstart;
            }
            gz1->strstart++;
            gz1->lookahead--;
        } else {
            
            match_available = 1;
            gz1->strstart++;
            gz1->lookahead--;
        }
        
        while (gz1->lookahead < MIN_LOOKAHEAD && !gz1->eofile) fill_window(gz1);
    }
    if (match_available) ct_tally( gz1, 0, gz1->window[gz1->strstart-1] );

    return FLUSH_BLOCK(1); 

 return 0;
}

void flush_outbuf( PGZ1 gz1 )
{
 if ( gz1->outcnt == 0 )
   {
    return;
   }

 write_buf( gz1, gz1->ofd, (char *)gz1->outbuf, gz1->outcnt );

 gz1->bytes_out += (ulg)gz1->outcnt;
 gz1->outcnt = 0;
}

void lm_init(
PGZ1 gz1,        
int  pack_level, 
ush *flags       
)
{
 register unsigned j;

 if ( pack_level < 1 || pack_level > 9 )
   {
    error("bad pack level");
   }

 gz1->compr_level = pack_level;

 #if defined(MAXSEG_64K) && HASH_BITS == 15
 for (j = 0;  j < HASH_SIZE; j++) head[j] = NIL;
 #else
 memset( (char*)head, 0, (HASH_SIZE*sizeof(*head)) );
 #endif

 gz1->max_lazy_match   = configuration_table[pack_level].max_lazy;
 gz1->good_match       = configuration_table[pack_level].good_length;
 #ifndef FULL_SEARCH
 gz1->nice_match       = configuration_table[pack_level].nice_length;
 #endif
 gz1->max_chain_length = configuration_table[pack_level].max_chain;

 if ( pack_level == 1 )
   {
    *flags |= FAST;
   }
 else if ( pack_level == 9 )
   {
    *flags |= SLOW;
   }

 gz1->strstart    = 0;
 gz1->block_start = 0L;
 #ifdef ASMV
 match_init(); 
 #endif

 gz1->lookahead = read_buf(gz1,(char*)gz1->window,
                  sizeof(int) <= 2 ? (unsigned)WSIZE : 2*WSIZE);

 if (gz1->lookahead == 0 || gz1->lookahead == (unsigned)EOF)
   {
    gz1->eofile = 1, gz1->lookahead = 0;
    return;
   }

 gz1->eofile = 0;

 while (gz1->lookahead < MIN_LOOKAHEAD && !gz1->eofile)
   {
    fill_window(gz1);
   }

 gz1->ins_h = 0;

 for ( j=0; j<MIN_MATCH-1; j++ )
    {
     gz1->ins_h =
     (((gz1->ins_h)<<H_SHIFT) ^ (gz1->window[j])) & HASH_MASK;
    }
}

void fill_window( PGZ1 gz1 )
{
 register unsigned n, m;

 unsigned more =
 (unsigned)( gz1->window_size - (ulg)gz1->lookahead - (ulg)gz1->strstart );

 if ( more == (unsigned)EOF)
   {
    more--;
   }
 else if ( gz1->strstart >= WSIZE+MAX_DIST )
   {
    memcpy((char*)gz1->window, (char*)gz1->window+WSIZE, (unsigned)WSIZE);

    gz1->match_start -= WSIZE;
    gz1->strstart    -= WSIZE; 

    gz1->block_start -= (long) WSIZE;

    for ( n = 0; n < HASH_SIZE; n++ )
       {
        m = head[n];
        head[n] = (ush)(m >= WSIZE ? m-WSIZE : NIL);
       }

    for ( n = 0; n < WSIZE; n++ )
       {
        m = prev[n];

        prev[n] = (ush)(m >= WSIZE ? m-WSIZE : NIL);
       }

    more += WSIZE;
   }

 if ( !gz1->eofile )
   {
    n = read_buf(gz1,(char*)gz1->window+gz1->strstart+gz1->lookahead, more);

    if ( n == 0 || n == (unsigned)EOF )
      {
       gz1->eofile = 1;
      }
    else
      {
       gz1->lookahead += n;
      }
   }
}

ulg gz1_deflate_fast( PGZ1 gz1 )
{
    unsigned hash_head; 
    int flush;      
    unsigned match_length = 0;  

    gz1->prev_length = MIN_MATCH-1;

    while (gz1->lookahead != 0)
      {
       gz1->ins_h =
       (((gz1->ins_h)<<H_SHIFT) ^ (gz1->window[ gz1->strstart + MIN_MATCH-1])) & HASH_MASK;
       
       prev[ gz1->strstart & WMASK ] = hash_head = head[ gz1->ins_h ];

       head[ gz1->ins_h ] = gz1->strstart;

        if (hash_head != NIL && gz1->strstart - hash_head <= MAX_DIST) {
            
            match_length = longest_match( gz1, hash_head );
            
            if (match_length > gz1->lookahead) match_length = gz1->lookahead;
        }
        if (match_length >= MIN_MATCH) {

            flush = ct_tally(gz1,gz1->strstart-gz1->match_start, match_length - MIN_MATCH);

            gz1->lookahead -= match_length;

            if (match_length <= gz1->max_lazy_match )
              {
                match_length--; 

                do {
                    gz1->strstart++;

                    gz1->ins_h =
                    (((gz1->ins_h)<<H_SHIFT) ^ (gz1->window[ gz1->strstart + MIN_MATCH-1])) & HASH_MASK;
                    
                    prev[ gz1->strstart & WMASK ] = hash_head = head[ gz1->ins_h ];

                    head[ gz1->ins_h ] = gz1->strstart;

                } while (--match_length != 0);
            gz1->strstart++;
            } else {
            gz1->strstart += match_length;
	        match_length = 0;
            gz1->ins_h = gz1->window[gz1->strstart];

            gz1->ins_h =
            (((gz1->ins_h)<<H_SHIFT) ^ (gz1->window[gz1->strstart+1])) & HASH_MASK;
            
#if MIN_MATCH != 3
                Call UPDATE_HASH() MIN_MATCH-3 more times
#endif
            }
        } else {
            
            flush = ct_tally(gz1, 0, gz1->window[gz1->strstart]);
            gz1->lookahead--;
        gz1->strstart++;
        }
        if (flush) FLUSH_BLOCK(0), gz1->block_start = gz1->strstart;

        while (gz1->lookahead < MIN_LOOKAHEAD && !gz1->eofile) fill_window(gz1);
    }

 return FLUSH_BLOCK(1);
}

void ct_init(
PGZ1  gz1,    
ush  *attr,   
int  *methodp 
)
{
 #ifdef DD1
 int i,ii;
 #endif

 int n;      
 int bits;   
 int length; 
 int code;   
 int dist;   

 gz1->file_type      = attr;
 gz1->file_method    = methodp;
 gz1->compressed_len = gz1->input_len = 0L;
        
 if ( gz1->static_dtree[0].dl.len != 0 )
   {
    return;
   }

 length = 0;

 for ( code = 0; code < LENGTH_CODES-1; code++ )
    {
     gz1->base_length[code] = length;

     for ( n = 0; n < (1<<extra_lbits[code]); n++ )
        {
         gz1->length_code[length++] = (uch)code;
        }
    }

 gz1->length_code[length-1] = (uch)code;

 dist = 0;

 for ( code = 0 ; code < 16; code++ )
    {
     gz1->base_dist[code] = dist;

     for ( n = 0; n < (1<<extra_dbits[code]); n++ )
        {
         gz1->dist_code[dist++] = (uch)code;
        }
    }

 dist >>= 7; 

 for ( ; code < D_CODES; code++ )
    {
     gz1->base_dist[code] = dist << 7;

     for ( n = 0; n < (1<<(extra_dbits[code]-7)); n++ )
        {
         gz1->dist_code[256 + dist++] = (uch)code;
        }
    }

 for ( bits = 0; bits <= MAX_BITS; bits++ )
    {
     gz1->bl_count[bits] = 0;
    }

 n = 0;

 while (n <= 143) gz1->static_ltree[n++].dl.len = 8, gz1->bl_count[8]++;
 while (n <= 255) gz1->static_ltree[n++].dl.len = 9, gz1->bl_count[9]++;
 while (n <= 279) gz1->static_ltree[n++].dl.len = 7, gz1->bl_count[7]++;
 while (n <= 287) gz1->static_ltree[n++].dl.len = 8, gz1->bl_count[8]++;

 gen_codes(gz1,(ct_data *)gz1->static_ltree, L_CODES+1);

 for ( n = 0; n < D_CODES; n++ )
    {
     gz1->static_dtree[n].dl.len  = 5;
     gz1->static_dtree[n].fc.code = bi_reverse( gz1, n, 5 );
    }

 init_block( gz1 );
}

ulg flush_block(
PGZ1  gz1,        
char *buf,        
ulg   stored_len, 
int   eof         
)
{
 ulg opt_lenb;     
 ulg static_lenb;  
 int max_blindex;  

 gz1->flag_buf[gz1->last_flags] = gz1->flags;

 if (*gz1->file_type == (ush)UNKNOWN) set_file_type(gz1);

 build_tree( gz1, (tree_desc *)(&gz1->l_desc) );
 build_tree( gz1, (tree_desc *)(&gz1->d_desc) );

 max_blindex = build_bl_tree( gz1 );

 opt_lenb         = (gz1->opt_len+3+7)>>3;
 static_lenb      = (gz1->static_len+3+7)>>3;
 gz1->input_len  += stored_len; 

 if (static_lenb <= opt_lenb) opt_lenb = static_lenb;

#ifdef FORCE_METHOD
 
 if ( level == 1 && eof && gz1->compressed_len == 0L )
#else
 if (stored_len <= opt_lenb && eof && gz1->compressed_len == 0L && 0 )
#endif
   {
    if (buf == (char*)0) error ("block vanished");

    copy_block( gz1, buf, (unsigned)stored_len, 0 ); 

    gz1->compressed_len = stored_len << 3;
    *gz1->file_method   = STORED;

#ifdef FORCE_METHOD
 } else if (level == 2 && buf != (char*)0) { 
#else
 } else if (stored_len+4 <= opt_lenb && buf != (char*)0) {
                    
#endif
     
     send_bits(gz1,(STORED_BLOCK<<1)+eof, 3);  
     gz1->compressed_len = (gz1->compressed_len + 3 + 7) & ~7L;
     gz1->compressed_len += (stored_len + 4) << 3;

     copy_block(gz1, buf, (unsigned)stored_len, 1); 

#ifdef FORCE_METHOD
 } else if (level == 3) { 
#else
 } else if (static_lenb == opt_lenb) {
#endif
     send_bits(gz1,(STATIC_TREES<<1)+eof, 3);

     compress_block(
     gz1,
     (ct_data *)gz1->static_ltree,
     (ct_data *)gz1->static_dtree
     );

     gz1->compressed_len += 3 + gz1->static_len;
    }
  else
    {
     send_bits(gz1,(DYN_TREES<<1)+eof, 3);

     send_all_trees(
     gz1,
     gz1->l_desc.max_code+1,
     gz1->d_desc.max_code+1,
     max_blindex+1
     );

     compress_block(
     gz1,
     (ct_data *)gz1->dyn_ltree,
     (ct_data *)gz1->dyn_dtree
     );

     gz1->compressed_len += 3 + gz1->opt_len;
    }

 init_block( gz1 );

 if ( eof )
   {
    bi_windup( gz1 );

    gz1->compressed_len += 7;  
   }

 return gz1->compressed_len >> 3;
}

#ifdef __BORLANDC__
#pragma argsused
#endif

unsigned bi_reverse(
PGZ1     gz1,  
unsigned code, 
int      len   
)
{
 register unsigned res = 0;

 do {
     res |= code & 1;
     code >>= 1, res <<= 1;

    } while (--len > 0);

 return res >> 1;
}

void set_file_type( PGZ1 gz1 )
{
 int n = 0;
 unsigned ascii_freq = 0;
 unsigned bin_freq = 0;

 while (n < 7)        bin_freq += gz1->dyn_ltree[n++].fc.freq;
 while (n < 128)    ascii_freq += gz1->dyn_ltree[n++].fc.freq;
 while (n < LITERALS) bin_freq += gz1->dyn_ltree[n++].fc.freq;

 *gz1->file_type = bin_freq > (ascii_freq >> 2) ? BINARY : ASCII;
}

void init_block( PGZ1 gz1 )
{
 int n; 

 for (n = 0; n < L_CODES;  n++) gz1->dyn_ltree[n].fc.freq = 0;
 for (n = 0; n < D_CODES;  n++) gz1->dyn_dtree[n].fc.freq = 0;
 for (n = 0; n < BL_CODES; n++) gz1->bl_tree[n].fc.freq   = 0;

 gz1->dyn_ltree[END_BLOCK].fc.freq = 1;

 gz1->opt_len    = 0L;
 gz1->static_len = 0L;
 gz1->last_lit   = 0;
 gz1->last_dist  = 0;
 gz1->last_flags = 0;
 gz1->flags      = 0;
 gz1->flag_bit   = 1;
}

void bi_init( PGZ1 gz1, gz1_file_t zipfile )
{
 gz1->zfile    = zipfile;
 gz1->bi_buf   = 0;
 gz1->bi_valid = 0;

 if ( gz1->zfile != NO_FILE )
   {
    read_buf = file_read;
   }
}

int ct_tally(
PGZ1 gz1,  
int  dist, 
int  lc    
)
{
 int dcode;

 gz1->inbuf[gz1->last_lit++] = (uch)lc;

 if ( dist == 0 )
   {
    gz1->dyn_ltree[lc].fc.freq++; 
   }
 else
   {
    dist--; 

    gz1->dyn_ltree[gz1->length_code[lc]+LITERALS+1].fc.freq++;
    gz1->dyn_dtree[d_code(dist)].fc.freq++;
    gz1->d_buf[gz1->last_dist++] = (ush)dist;
    gz1->flags |= gz1->flag_bit;
   }

 gz1->flag_bit <<= 1;

 if ( (gz1->last_lit & 7) == 0 )
   {
    gz1->flag_buf[gz1->last_flags++] = gz1->flags;
    gz1->flags = 0, gz1->flag_bit = 1;
   }

 if ( gz1->level > 2 && (gz1->last_lit & 0xfff) == 0)
   {
    ulg out_length = (ulg) ( gz1->last_lit * 8L );
    ulg in_length  = (ulg) ( gz1->strstart - gz1->block_start );

    for ( dcode = 0; dcode < D_CODES; dcode++ )
       {
        out_length += (ulg) ((gz1->dyn_dtree[dcode].fc.freq)*(5L+extra_dbits[dcode]));
       }

    out_length >>= 3;

    if ( gz1->last_dist < gz1->last_lit/2 && out_length < in_length/2 )
      {
       return 1;
      }
   }

 return( gz1->last_lit == LIT_BUFSIZE-1 || gz1->last_dist == DIST_BUFSIZE );
}

void compress_block(
PGZ1     gz1,   
ct_data *ltree, 
ct_data *dtree  
)
{
 unsigned dist;   
 int lc;          
 unsigned lx = 0; 
 unsigned dx = 0; 
 unsigned fx = 0; 
 uch flag = 0;    
 unsigned code;   
 int extra;       

 if (gz1->last_lit != 0) do {
     if ((lx & 7) == 0) flag = gz1->flag_buf[fx++];
     lc = gz1->inbuf[lx++];
     if ((flag & 1) == 0) {
         send_code(lc, ltree); 
     } else {
         
         code = gz1->length_code[lc];
         send_code(code+LITERALS+1, ltree); 
         extra = extra_lbits[code];
         if (extra != 0) {
             lc -= gz1->base_length[code];
             send_bits(gz1,lc, extra); 
         }
         dist = gz1->d_buf[dx++];
         
         code = d_code(dist);

         send_code(code, dtree);       
         extra = extra_dbits[code];
         if (extra != 0) {
             dist -= gz1->base_dist[code];
             send_bits(gz1,dist, extra); 
         }
     } 
     flag >>= 1;
 } while (lx < gz1->last_lit);

 send_code(END_BLOCK, ltree);
}

#ifndef ASMV

int longest_match( PGZ1 gz1, unsigned cur_match )
{
 unsigned chain_length = gz1->max_chain_length;   
 register uch *scan = gz1->window + gz1->strstart;     
 register uch *match;                        
 register int len;                           
 int best_len = gz1->prev_length;                 
 unsigned limit = gz1->strstart > (unsigned)MAX_DIST ? gz1->strstart - (unsigned)MAX_DIST : NIL;
 
#if HASH_BITS < 8 || MAX_MATCH != 258
   error: Code too clever
#endif

#ifdef UNALIGNED_OK
    
    register uch *strend    = gz1->window + gz1->strstart + MAX_MATCH - 1;
    register ush scan_start = *(ush*)scan;
    register ush scan_end   = *(ush*)(scan+best_len-1);
#else
    register uch *strend    = gz1->window + gz1->strstart + MAX_MATCH;
    register uch scan_end1  = scan[best_len-1];
    register uch scan_end   = scan[best_len];
#endif

    if (gz1->prev_length >= gz1->good_match) {
        chain_length >>= 2;
    }

    do {
        match = gz1->window + cur_match;

#if (defined(UNALIGNED_OK) && MAX_MATCH == 258)
        
        if (*(ush*)(match+best_len-1) != scan_end ||
            *(ush*)match != scan_start) continue;

        scan++, match++;
        do {
        } while (*(ush*)(scan+=2) == *(ush*)(match+=2) &&
                 *(ush*)(scan+=2) == *(ush*)(match+=2) &&
                 *(ush*)(scan+=2) == *(ush*)(match+=2) &&
                 *(ush*)(scan+=2) == *(ush*)(match+=2) &&
                 scan < strend);
        
        if (*scan == *match) scan++;

        len = (MAX_MATCH - 1) - (int)(strend-scan);
        scan = strend - (MAX_MATCH-1);
#else 
        if (match[best_len]   != scan_end  ||
            match[best_len-1] != scan_end1 ||
            *match            != *scan     ||
            *++match          != scan[1])      continue;

        scan += 2, match++;

        do {
        } while (*++scan == *++match && *++scan == *++match &&
                 *++scan == *++match && *++scan == *++match &&
                 *++scan == *++match && *++scan == *++match &&
                 *++scan == *++match && *++scan == *++match &&
                 scan < strend);

        len = MAX_MATCH - (int)(strend - scan);
        scan = strend - MAX_MATCH;
#endif 
        if (len > best_len) {
            gz1->match_start = cur_match;
            best_len = len;
            if (len >= gz1->nice_match) break;
#ifdef UNALIGNED_OK
            scan_end = *(ush*)(scan+best_len-1);
#else
            scan_end1  = scan[best_len-1];
            scan_end   = scan[best_len];
#endif
        }
    } while ((cur_match = prev[cur_match & WMASK]) > limit
	     && --chain_length != 0);

    return best_len;
}
#endif 

void send_bits(
PGZ1 gz1,   
int  value, 
int  length 
)
{
 if ( gz1->bi_valid > (int) BUFSIZE - length )
   {
    gz1->bi_buf |= (value << gz1->bi_valid);

    put_short(gz1->bi_buf);

    gz1->bi_buf = (ush)value >> (BUFSIZE - gz1->bi_valid);
    gz1->bi_valid += length - BUFSIZE;
   }
 else
   {
    gz1->bi_buf |= value << gz1->bi_valid;
    gz1->bi_valid += length;
   }
}

void build_tree(
PGZ1       gz1, 
tree_desc *desc 
)
{
 int elems      = desc->elems;
 ct_data *tree  = desc->dyn_tree;
 ct_data *stree = desc->static_tree;

 int n;             
 int m;             
 int max_code = -1; 
 int node = elems;  
 int new1;          

    gz1->heap_len = 0, gz1->heap_max = HEAP_SIZE;

    for (n = 0; n < elems; n++) {
        if (tree[n].fc.freq != 0) {
            gz1->heap[++gz1->heap_len] = max_code = n;
            gz1->depth[n] = 0;
        } else {
            tree[n].dl.len = 0;
        }
    }

    while (gz1->heap_len < 2) {
        new1 = gz1->heap[++gz1->heap_len] = (max_code < 2 ? ++max_code : 0);
        tree[new1].fc.freq = 1;
        gz1->depth[new1] = 0;
        gz1->opt_len--; if (stree) gz1->static_len -= stree[new1].dl.len;
    }
    desc->max_code = max_code;

    for (n = gz1->heap_len/2; n >= 1; n--) pqdownheap(gz1, tree, n);

    do {
        n = gz1->heap[SMALLEST];
        gz1->heap[SMALLEST] = gz1->heap[gz1->heap_len--];
        pqdownheap(gz1, tree, SMALLEST);
        m = gz1->heap[SMALLEST];
        gz1->heap[--gz1->heap_max] = n;
        gz1->heap[--gz1->heap_max] = m;
        tree[node].fc.freq = tree[n].fc.freq + tree[m].fc.freq;
        gz1->depth[node] = (uch) (GZ1_MAX(gz1->depth[n], gz1->depth[m]) + 1);
        tree[n].dl.dad = tree[m].dl.dad = (ush)node;
        gz1->heap[SMALLEST] = node++;
        pqdownheap(gz1, tree, SMALLEST);

    } while (gz1->heap_len >= 2);

    gz1->heap[--gz1->heap_max] = gz1->heap[SMALLEST];

    gen_bitlen(gz1,(tree_desc *)desc);

    gen_codes(gz1,(ct_data *)tree, max_code);
}

int build_bl_tree( PGZ1 gz1 )
{
 int max_blindex; 

 scan_tree( gz1, (ct_data *)gz1->dyn_ltree, gz1->l_desc.max_code );
 scan_tree( gz1, (ct_data *)gz1->dyn_dtree, gz1->d_desc.max_code );

 build_tree( gz1, (tree_desc *)(&gz1->bl_desc) );

 for ( max_blindex = BL_CODES-1; max_blindex >= 3; max_blindex-- )
    {
     if (gz1->bl_tree[bl_order[max_blindex]].dl.len != 0) break;
    }

 gz1->opt_len += 3*(max_blindex+1) + 5+5+4;

 return max_blindex;
}

void gen_codes(
PGZ1     gz1,     
ct_data *tree,    
int      max_code 
)
{
 ush next_code[MAX_BITS+1]; 
 ush code = 0;              
 int bits;                  
 int n;                     

 for ( bits = 1; bits <= MAX_BITS; bits++ )
    {
     next_code[bits] = code = (code + gz1->bl_count[bits-1]) << 1;
    }

 for ( n = 0;  n <= max_code; n++ )
    {
     int len = tree[n].dl.len;
     if (len == 0) continue;

     tree[n].fc.code = bi_reverse( gz1, next_code[len]++, len );
    }

 return;
}

void gen_bitlen(
PGZ1       gz1, 
tree_desc *desc 
)
{
 ct_data *tree   = desc->dyn_tree;
 int *extra      = desc->extra_bits;
 int base             = desc->extra_base;
 int max_code         = desc->max_code;
 int max_length       = desc->max_length;
 ct_data *stree  = desc->static_tree;
 int h;              
 int n, m;           
 int bits;           
 int xbits;          
 ush f;              
 int overflow = 0;   

 for (bits = 0; bits <= MAX_BITS; bits++) gz1->bl_count[bits] = 0;

 tree[gz1->heap[gz1->heap_max]].dl.len = 0;

 for (h = gz1->heap_max+1; h < HEAP_SIZE; h++) {
     n = gz1->heap[h];
     bits = tree[tree[n].dl.dad].dl.len + 1;
     if (bits > max_length) bits = max_length, overflow++;
     tree[n].dl.len = (ush)bits;
     
     if (n > max_code) continue; 

     gz1->bl_count[bits]++;
     xbits = 0;
     if (n >= base) xbits = extra[n-base];
     f = tree[n].fc.freq;
     gz1->opt_len += (ulg)f * (bits + xbits);
     if (stree) gz1->static_len += (ulg)f * (stree[n].dl.len + xbits);
 }
 if (overflow == 0) return;

 do {
     bits = max_length-1;
     while (gz1->bl_count[bits] == 0) bits--;
     gz1->bl_count[bits]--;      
     gz1->bl_count[bits+1] += 2; 
     gz1->bl_count[max_length]--;
     
     overflow -= 2;
 } while (overflow > 0);

 for (bits = max_length; bits != 0; bits--) {
     n = gz1->bl_count[bits];
     while (n != 0) {
         m = gz1->heap[--h];
         if (m > max_code) continue;
         if (tree[m].dl.len != (unsigned) bits) {
             gz1->opt_len += ((long)bits-(long)tree[m].dl.len)*(long)tree[m].fc.freq;
             tree[m].dl.len = (ush)bits;
         }
         n--;
     }
  }
}

void copy_block(
PGZ1      gz1,    
char     *buf,    
unsigned  len,    
int       header  
)
{
 #ifdef CRYPT
 int t;
 #endif

 bi_windup( gz1 ); 

 if ( header )
   {
    put_short((ush)len);
    put_short((ush)~len);
   }

 while( len-- )
   {
    #ifdef CRYPT
	if (key) zencode(*buf, t);
    #endif

    put_byte(*buf++);
   }
}

int file_read( PGZ1 gz1, char *buf, unsigned size )
{
 unsigned len = 0;
 unsigned bytes_to_copy = 0;

 if ( gz1->input_ismem )
   {
    if ( gz1->input_bytesleft > 0 )
      {
       bytes_to_copy = size;

       if ( bytes_to_copy > (unsigned) gz1->input_bytesleft )
         {
          bytes_to_copy = (unsigned) gz1->input_bytesleft;
         }

       memcpy( buf, gz1->input_ptr, bytes_to_copy );

       gz1->input_ptr       += bytes_to_copy;
       gz1->input_bytesleft -= bytes_to_copy;

       len = bytes_to_copy;
      }
    else
      {
       len = 0;
      }
   }
 else
   {
    len = read( gz1->ifd, buf, size );
   }

 if ( len == (unsigned)(-1) || len == 0 )
   {
	gz1->crc = gz1->crc ^ 0xffffffffL;
    /* XXX - Do we need to do something with Adler CRC's here?
	 * I don't think so--they don't seem to need postprocessing. */
    return (int)len;
   }

 if (gz1->compression_format != DEFLATE_FORMAT)
   {
    updcrc( gz1, (uch*)buf, len );
   }
 else
   {
	gz1->adler = adler32(gz1->adler, (uch*)buf, len);
   }

 gz1->bytes_in += (ulg)len;

 return (int)len;
}

void bi_windup( PGZ1 gz1 )
{
 if ( gz1->bi_valid > 8 )
   {
    put_short(gz1->bi_buf);
   }
 else if ( gz1->bi_valid > 0 )
   {
    put_byte(gz1->bi_buf);
   }

 gz1->bi_buf   = 0;
 gz1->bi_valid = 0;
}

void send_all_trees(
PGZ1 gz1,    
int  lcodes, 
int  dcodes, 
int  blcodes 
)
{
 int rank; 

 send_bits(gz1,lcodes-257, 5); 
 send_bits(gz1,dcodes-1,   5);
 send_bits(gz1,blcodes-4,  4); 

 for ( rank = 0; rank < blcodes; rank++ )
    {
     send_bits(gz1,gz1->bl_tree[bl_order[rank]].dl.len, 3 );
    }

 send_tree(gz1,(ct_data *)gz1->dyn_ltree, lcodes-1); 
 send_tree(gz1,(ct_data *)gz1->dyn_dtree, dcodes-1); 
}

void send_tree(
PGZ1     gz1,     
ct_data *tree,    
int      max_code 
)
{
 int n;                        
 int prevlen = -1;             
 int curlen;                   
 int nextlen = tree[0].dl.len; 
 int count = 0;                
 int max_count = 7;            
 int min_count = 4;            

 if (nextlen == 0) max_count = 138, min_count = 3;

 for ( n = 0; n <= max_code; n++ )
    {
     curlen  = nextlen;
     nextlen = tree[n+1].dl.len;

     if (++count < max_count && curlen == nextlen)
       {
        continue;
       }
     else if (count < min_count)
       {
        do { send_code(curlen, gz1->bl_tree); } while (--count != 0);
       }
     else if (curlen != 0)
       {
        if ( curlen != prevlen )
          {
           send_code(curlen, gz1->bl_tree); count--;
          }

        send_code( REP_3_6, gz1->bl_tree ); send_bits(gz1,count-3, 2);
       }
     else if (count <= 10)
       {
        send_code(REPZ_3_10, gz1->bl_tree); send_bits(gz1,count-3, 3);
       }
     else
       {
        send_code(REPZ_11_138, gz1->bl_tree); send_bits(gz1,count-11, 7);
       }

     count   = 0;
     prevlen = curlen;

     if (nextlen == 0)
       {
        max_count = 138, min_count = 3;
       }
     else if (curlen == nextlen)
       {
        max_count = 6, min_count = 3;
       }
     else
       {
        max_count = 7, min_count = 4;
       }
    }
}

void scan_tree(
PGZ1     gz1,     
ct_data *tree,    
int      max_code 
)
{
 int n;                        
 int prevlen = -1;             
 int curlen;                   
 int nextlen = tree[0].dl.len; 
 int count = 0;                
 int max_count = 7;            
 int min_count = 4;            

 if (nextlen == 0) max_count = 138, min_count = 3;

 tree[max_code+1].dl.len = (ush)0xffff; 

 for ( n = 0; n <= max_code; n++ )
    {
     curlen  = nextlen;
     nextlen = tree[n+1].dl.len;

     if ( ++count < max_count && curlen == nextlen )
       {
        continue;
       }
     else if ( count < min_count )
       {
        gz1->bl_tree[curlen].fc.freq += count;
       }
     else if ( curlen != 0 )
       {
        if ( curlen != prevlen ) gz1->bl_tree[curlen].fc.freq++;
        gz1->bl_tree[REP_3_6].fc.freq++;
       }
     else if ( count <= 10 )
       {
        gz1->bl_tree[REPZ_3_10].fc.freq++;
       }
     else
       {
        gz1->bl_tree[REPZ_11_138].fc.freq++;
       }

     count   = 0;
     prevlen = curlen;

     if ( nextlen == 0 )
       {
        max_count = 138;
        min_count = 3;
       }
     else if (curlen == nextlen)
       {
        max_count = 6;
        min_count = 3;
       }
     else
       {
        max_count = 7;
        min_count = 4;
       }
    }
}

void pqdownheap(
PGZ1     gz1,  
ct_data *tree, 
int      k     
)
{
 int v = gz1->heap[k];
 int j = k << 1;  

 while( j <= gz1->heap_len )
   {
    if (j < gz1->heap_len && smaller(tree, gz1->heap[j+1], gz1->heap[j])) j++;

    if (smaller(tree, v, gz1->heap[j])) break;

    gz1->heap[k] = gz1->heap[j];  k = j;

    j <<= 1;
   }

 gz1->heap[k] = v;
}


#define GZS_ZIP1      1
#define GZS_ZIP2      2
#define GZS_DEFLATE1  3
#define GZS_DEFLATE2  4

int gzs_fsp     ( PGZ1 gz1 ); 
int gzs_zip1    ( PGZ1 gz1 ); 
int gzs_zip2    ( PGZ1 gz1 ); 
int gzs_deflate1( PGZ1 gz1 ); 
int gzs_deflate2( PGZ1 gz1 ); 

int gzp_main( GZP_CONTROL *gzp )
{
 PGZ1 gz1 = 0; 
 int  rc  = 0;
 int  final_exit_code = 0;
 int  ofile_flags = O_RDWR | O_CREAT | O_TRUNC | O_BINARY;

 gzp->result_code = 0; 
 gzp->bytes_out   = 0; 

 gz1 = (PGZ1) gz1_init();

 if ( gz1 == 0 )
   {
    return 0;
   }

 gz1->decompress      = gzp->decompress;
 gz1->compression_format = gzp->compression_format;

 strcpy( gz1->ifname, gzp->input_filename  );
 strcpy( gz1->ofname, gzp->output_filename );

 gz1->input_ismem     = gzp->input_ismem;
 gz1->input_ptr       = gzp->input_ismem_ibuf;
 gz1->input_bytesleft = gzp->input_ismem_ibuflen;

 gz1->output_ismem    = gzp->output_ismem;
 gz1->output_ptr      = gzp->output_ismem_obuf;
 gz1->output_maxlen   = gzp->output_ismem_obuflen;

 if ( gz1->no_time < 0 ) gz1->no_time = gz1->decompress;
 if ( gz1->no_name < 0 ) gz1->no_name = gz1->decompress;

 work = zip; 

 if ( !gz1->input_ismem ) 
   {
    errno = 0;

    rc = stat( gz1->ifname, &gz1->istat );

    if ( rc != 0 ) 
      {
       gz1_cleanup( gz1 ); 

       return 0; 
      }

    gz1->ifile_size = gz1->istat.st_size;

    gz1->ifd =
    OPEN(
    gz1->ifname,
    gz1->ascii && !gz1->decompress ? O_RDONLY : O_RDONLY | O_BINARY,
    RW_USER
    );

    if ( gz1->ifd == -1 )
      {
       gz1_cleanup( gz1 ); 

       return 0; 
      }
   }

 if ( !gz1->output_ismem ) 
   {
    if ( gz1->ascii && gz1->decompress )
      {
       ofile_flags &= ~O_BINARY; 
      }

    gz1->ofd = OPEN( gz1->ofname, ofile_flags, RW_USER );

    if ( gz1->ofd == -1 )
      {
       if ( gz1->ifd ) 
         {
          close( gz1->ifd ); 
          gz1->ifd = 0;      
         }

       gz1_cleanup( gz1 ); 

       return 0; 
      }
   }

 gz1->outcnt    = 0;
 gz1->insize    = 0;
 gz1->inptr     = 0;
 gz1->bytes_in  = 0L;
 gz1->bytes_out = 0L; 
 gz1->part_nb   = 0;

 if ( gz1->decompress )
   {
    gz1->method = get_header( gz1, gz1->ifd );

    if ( gz1->method < 0 )
      {
       if ( gz1->ifd ) 
         {
          close( gz1->ifd ); 
          gz1->ifd = 0;      
         }

       if ( gz1->ofd ) 
         {
          close( gz1->ofd ); 
          gz1->ofd = 0;      
         }

       return 0; 
      }
   }

 gz1->save_orig_name = 0;

 gz1->state = GZS_ZIP1;

 for (;;) 
    {
     gzs_fsp( gz1 ); 

     if ( gz1->done == 1 ) break; 
    }

 if ( gz1->ifd ) 
   {
    close( gz1->ifd ); 
    gz1->ifd = 0;      
   }

 if ( gz1->ofd ) 
   {
    close( gz1->ofd ); 
    gz1->ofd = 0;      
   }

 gzp->result_code = gz1->exit_code;
 gzp->bytes_out   = gz1->bytes_out;

 final_exit_code = (int) gz1->exit_code;

 gz1_cleanup( gz1 );  

 return final_exit_code; 
}

int gzs_fsp( PGZ1 gz1 )
{
 int rc=0; 

 switch( gz1->state )
   {
    case GZS_ZIP1:

         rc = gzs_zip1( gz1 );

         break;

    case GZS_ZIP2:

         rc = gzs_zip2( gz1 );

         break;

    case GZS_DEFLATE1:

         rc = gzs_deflate1( gz1 );

         break;

    case GZS_DEFLATE2:

         rc = gzs_deflate2( gz1 );

         break;

    default: 

         gz1->done = 1;

         break;
   }

 return( rc );
}


int gzs_zip1( PGZ1 gz1 )
{
 uch  flags = 0;         
 ush  attr  = 0;         
 ush  deflate_flags = 0; 

 gz1->outcnt = 0;

 gz1->method = DEFLATED;

 if (gz1->compression_format != DEFLATE_FORMAT)
   {
    put_byte(GZIP_MAGIC[0]); 
    put_byte(GZIP_MAGIC[1]);
    put_byte(DEFLATED);      
   }
 else
   {
	/* Yes, I know RFC 1951 doesn't mention any header at the start of
	 * a deflated document, but zlib absolutely requires one. And since nearly
     * all "deflate" implementations use zlib, we need to play along with this
     * brain damage. */
    put_byte(ZLIB_HEADER[0]); 
    put_byte(ZLIB_HEADER[1]);
   }

 if ( gz1->save_orig_name )
   {
	flags |= ORIG_NAME;
   }

 if (gz1->compression_format != DEFLATE_FORMAT)
   {
    put_byte(flags);           
    put_long(gz1->time_stamp); 

	gz1->crc = -1; 
    updcrc( gz1, NULL, 0 ); 
   }
 else
   {
	/* Deflate compression uses an Adler32 CRC, not a CRC32. */
	gz1->adler = 1L;
   }

 gz1->state = GZS_ZIP2;

 return 0;
}

int gzs_zip2( PGZ1 gz1 )
{
 uch  flags = 0;         
 ush  attr  = 0;         
 ush  deflate_flags = 0; 

 bi_init( gz1, gz1->ofd );
 ct_init( gz1, &attr, &gz1->method );
 lm_init( gz1, gz1->level, &deflate_flags );

 if (gz1->compression_format != DEFLATE_FORMAT)
   {
    put_byte((uch)deflate_flags); 
    put_byte(OS_CODE); 
    if ( gz1->save_orig_name )
      {
       char *p = gz1_basename( gz1, gz1->ifname );

       do {
	       put_char(*p);

          } while (*p++);
      }
   }

 gz1->header_bytes = (long)gz1->outcnt;

 gz1->state = GZS_DEFLATE1;

 return 0;
}

int gzs_deflate1( PGZ1 gz1 )
{
 if ( !gz1->deflate1_initialized )
   {
    gz1->deflate1_match_available = 0;           
    gz1->deflate1_match_length    = MIN_MATCH-1; 
    gz1->deflate1_initialized     = 1;
   }

 if ( gz1->compr_level <= 3 )
   {
    gz1->done = 1; 

    return 0;
   }

 if ( gz1->lookahead == 0 )
   {
    if ( gz1->deflate1_match_available )
      {
       ct_tally( gz1, 0, gz1->window[gz1->strstart-1] );
      }

    gz1->state = GZS_DEFLATE2;

    return (int) FLUSH_BLOCK(1); 
   }

 #ifdef STAY_HERE_FOR_A_CERTAIN_AMOUNT_OF_ITERATIONS
 
 while( iterations < max_iterations_per_yield )
   {
 #endif

    gz1->ins_h =
    (((gz1->ins_h)<<H_SHIFT) ^ (gz1->window[gz1->strstart+MIN_MATCH-1])) & HASH_MASK;

    prev[ gz1->strstart & WMASK ] = gz1->deflate1_hash_head = head[ gz1->ins_h ];

    head[ gz1->ins_h ] = gz1->strstart;

    gz1->prev_length = gz1->deflate1_match_length, gz1->deflate1_prev_match = gz1->match_start;
    gz1->deflate1_match_length = MIN_MATCH-1;

    if ( gz1->deflate1_hash_head != NIL && gz1->prev_length < gz1->max_lazy_match &&
         gz1->strstart - gz1->deflate1_hash_head <= MAX_DIST)
      {
       gz1->deflate1_match_length = longest_match( gz1, gz1->deflate1_hash_head );

       if ( gz1->deflate1_match_length > gz1->lookahead )
         {
          gz1->deflate1_match_length = gz1->lookahead;
         }

       if (gz1->deflate1_match_length == MIN_MATCH && gz1->strstart-gz1->match_start > TOO_FAR)
         {
          gz1->deflate1_match_length--;
         }
      }

    if ( gz1->prev_length >= MIN_MATCH && gz1->deflate1_match_length <= gz1->prev_length )
      {
       gz1->deflate1_flush =
       ct_tally(gz1,gz1->strstart-1-gz1->deflate1_prev_match, gz1->prev_length - MIN_MATCH);

       gz1->lookahead   -= ( gz1->prev_length - 1 );
       gz1->prev_length -= 2;

       do {
           gz1->strstart++;

           gz1->ins_h =
           (((gz1->ins_h)<<H_SHIFT) ^ (gz1->window[ gz1->strstart + MIN_MATCH-1])) & HASH_MASK;

           prev[ gz1->strstart & WMASK ] = gz1->deflate1_hash_head = head[gz1->ins_h];

           head[ gz1->ins_h ] = gz1->strstart;

          } while (--gz1->prev_length != 0);

       gz1->deflate1_match_available = 0;
       gz1->deflate1_match_length    = MIN_MATCH-1;

       gz1->strstart++;

       if (gz1->deflate1_flush) FLUSH_BLOCK(0), gz1->block_start = gz1->strstart;
      }

    else
      {
       if ( gz1->deflate1_match_available )
         {
          if ( ct_tally( gz1, 0, gz1->window[gz1->strstart-1] ) )
            {
             FLUSH_BLOCK(0), gz1->block_start = gz1->strstart;
            }

          gz1->strstart++;
          gz1->lookahead--;
         }
       else 
         {
          gz1->deflate1_match_available = 1;
          gz1->strstart++;
          gz1->lookahead--;
         }

       while (gz1->lookahead < MIN_LOOKAHEAD && !gz1->eofile )
         {
          fill_window(gz1);
         }
      }

 return 0;
}

int gzs_deflate2( PGZ1 gz1 )
{
 #if !defined(NO_SIZE_CHECK) && !defined(RECORD_IO)
 if (gz1->ifile_size != -1L && gz1->isize != (ulg)gz1->ifile_size)
   {
   }
 #endif

 if (gz1->compression_format != DEFLATE_FORMAT)
   {
    put_long( gz1->crc );
    put_long( gz1->bytes_in );
    gz1->header_bytes += 2*sizeof(long);
   }
 else
   {
	/* Append an Adler32 CRC to our deflated data.
	 * Yes, I know RFC 1951 doesn't mention any CRC at the end of a
	 * deflated document, but zlib absolutely requires one. And since nearly
	 * all "deflate" implementations use zlib, we need to play along with this
	 * brain damage. */
	put_byte( (gz1->adler >> 24)        );
	put_byte( (gz1->adler >> 16) & 0xFF );
	put_byte( (gz1->adler >>  8) & 0xFF );
	put_byte( (gz1->adler      ) & 0xFF );
    gz1->header_bytes += 4*sizeof(uch);
   }

 flush_outbuf( gz1 );

 gz1->done = 1; 

 return OK;
}


/* =========================================================================
   adler32 -- compute the Adler-32 checksum of a data stream
   Copyright (C) 1995-1998 Mark Adler

   This software is provided 'as-is', without any express or implied
   warranty.  In no event will the authors be held liable for any damages
   arising from the use of this software.

   Permission is granted to anyone to use this software for any purpose,
   including commercial applications, and to alter it and redistribute it
   freely, subject to the following restrictions:

   1. The origin of this software must not be misrepresented; you must not
      claim that you wrote the original software. If you use this software
      in a product, an acknowledgment in the product documentation would be
      appreciated but is not required.
   2. Altered source versions must be plainly marked as such, and must not be
      misrepresented as being the original software.
   3. This notice may not be removed or altered from any source distribution.

   Modified by Eric Kidd <eric.kidd@pobox.com> to play nicely with mod_gzip.
 */

#define BASE 65521L /* largest prime smaller than 65536 */
#define NMAX 5552
/* NMAX is the largest n such that 255n(n+1)/2 + (n+1)(BASE-1) <= 2^32-1 */

#define DO1(buf,i)  {s1 += buf[i]; s2 += s1;}
#define DO2(buf,i)  DO1(buf,i); DO1(buf,i+1);
#define DO4(buf,i)  DO2(buf,i); DO2(buf,i+2);
#define DO8(buf,i)  DO4(buf,i); DO4(buf,i+4);
#define DO16(buf)   DO8(buf,0); DO8(buf,8);

ulg adler32(ulg adler, uch *buf, unsigned len)
{
    unsigned long s1 = adler & 0xffff;
    unsigned long s2 = (adler >> 16) & 0xffff;
    int k;

    if (buf == NULL) return 1L;

    while (len > 0) {
        k = len < NMAX ? len : NMAX;
        len -= k;
        while (k >= 16) {
            DO16(buf);
	    buf += 16;
            k -= 16;
        }
        if (k != 0) do {
            s1 += *buf++;
	    s2 += s1;
        } while (--k);
        s1 %= BASE;
        s2 %= BASE;
    }
    return (s2 << 16) | s1;
}


/* END OF FILE */


