/* Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* fspr_password_get.c: abstraction to provide for obtaining a password from the
 * command line in whatever way the OS supports.  In the best case, it's a
 * wrapper for the system library's getpass() routine; otherwise, we
 * use one we define ourselves.
 */
#include "fspr_private.h"
#include "fspr_strings.h"
#include "fspr_lib.h"
#include "fspr_errno.h"
#if APR_HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#if APR_HAVE_ERRNO_H
#include <errno.h>
#endif

#if APR_HAVE_UNISTD_H
#include <unistd.h>
#endif
#if APR_HAVE_CONIO_H
#pragma warning(disable: 4032)
#include <conio.h>
#pragma warning(default: 4032)
#endif
#if APR_HAVE_STDLIB_H
#include <stdlib.h>
#endif
#if APR_HAVE_STRING_H
#include <string.h>
#endif
#if APR_HAVE_STRINGS_H
#include <strings.h>
#endif

#if defined(HAVE_TERMIOS_H) && !defined(HAVE_GETPASS)
#include <termios.h>
#endif

#if !APR_CHARSET_EBCDIC
#define LF 10
#define CR 13
#else /* APR_CHARSET_EBCDIC */
#define LF '\n'
#define CR '\r'
#endif /* APR_CHARSET_EBCDIC */

#define MAX_STRING_LEN 256

#define ERR_OVERFLOW 5

#ifndef HAVE_GETPASS

/* MPE, Win32, NetWare and BeOS all lack a native getpass() */

#if !defined(HAVE_TERMIOS_H) && !defined(WIN32) && !defined(NETWARE)
/*
 * MPE lacks getpass() and a way to suppress stdin echo.  So for now, just
 * issue the prompt and read the results with echo.  (Ugh).
 */

static char *getpass(const char *prompt)
{
    static char password[MAX_STRING_LEN];

    fputs(prompt, stderr);
    fgets((char *) &password, sizeof(password), stdin);

    return (char *) &password;
}

#elif defined (HAVE_TERMIOS_H)
#include <stdio.h>

static char *getpass(const char *prompt)
{
    struct termios attr;
    static char password[MAX_STRING_LEN];
    int n=0;
    fputs(prompt, stderr);
    fflush(stderr);
	
    if (tcgetattr(STDIN_FILENO, &attr) != 0)
        return NULL;
    attr.c_lflag &= ~(ECHO);

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &attr) != 0)
	    return NULL;
    while ((password[n] = getchar()) != '\n') {
        if (n < sizeof(password) - 1 && password[n] >= ' ' && password[n] <= '~') {
            n++;
        } else {
            fprintf(stderr,"\n");
            fputs(prompt, stderr);
            fflush(stderr);
            n = 0;
        }
    }
 
    password[n] = '\0';
    printf("\n");
    if (n > (MAX_STRING_LEN - 1)) {
        password[MAX_STRING_LEN - 1] = '\0';
    }

    attr.c_lflag |= ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &attr);
    return (char*) &password;
}

#else

/*
 * Windows lacks getpass().  So we'll re-implement it here.
 */

static char *getpass(const char *prompt)
{
/* WCE lacks console. So the getpass is unsuported
 * The only way is to use the GUI so the getpass should be implemented
 * on per-application basis.
 */ 
#ifdef _WIN32_WCE
    return NULL;
#else
    static char password[128];
    int n = 0;
    int ch;

    fputs(prompt, stderr);
    
    while ((ch = _getch()) != '\r') {
        if (ch == EOF) /* EOF */ {
            fputs("[EOF]\n", stderr);
            return NULL;
        }
        else if (ch == 0 || ch == 0xE0) {
            /* FN Keys (0 or E0) are a sentinal for a FN code */ 
            ch = (ch << 4) | _getch();
            /* Catch {DELETE}, {<--}, Num{DEL} and Num{<--} */
            if ((ch == 0xE53 || ch == 0xE4B || ch == 0x053 || ch == 0x04b) && n) {
                password[--n] = '\0';
                fputs("\b \b", stderr);
            }
            else {
                fputc('\a', stderr);
            }
        }
        else if ((ch == '\b' || ch == 127) && n) /* BS/DEL */ {
            password[--n] = '\0';
            fputs("\b \b", stderr);
        }
        else if (ch == 3) /* CTRL+C */ {
            /* _getch() bypasses Ctrl+C but not Ctrl+Break detection! */
            fputs("^C\n", stderr);
            exit(-1);
        }
        else if (ch == 26) /* CTRL+Z */ {
            fputs("^Z\n", stderr);
            return NULL;
        }
	else if (ch == 27) /* ESC */ {
            fputc('\n', stderr);
            fputs(prompt, stderr);
            n = 0;
        }
        else if ((n < sizeof(password) - 1) && !fspr_iscntrl(ch)) {
            password[n++] = ch;
            fputc('*', stderr);
        }
	else {
            fputc('\a', stderr);
        }
    }
 
    fputc('\n', stderr);
    password[n] = '\0';
    return password;
#endif
}

#endif /* no getchar or _getch */

#endif /* no getpass */

/*
 * Use the OS getpass() routine (or our own) to obtain a password from
 * the input stream.
 *
 * Exit values:
 *  0: Success
 *  5: Partial success; entered text truncated to the size of the
 *     destination buffer
 *
 * Restrictions: Truncation also occurs according to the host system's
 * getpass() semantics, or at position 255 if our own version is used,
 * but the caller is *not* made aware of it unless their own buffer is
 * smaller than our own.
 */

APR_DECLARE(fspr_status_t) fspr_password_get(const char *prompt, char *pwbuf, fspr_size_t *bufsiz)
{
#ifdef HAVE_GETPASSPHRASE
    char *pw_got = getpassphrase(prompt);
#else
    char *pw_got = getpass(prompt);
#endif
    fspr_status_t rv = APR_SUCCESS;

    if (!pw_got)
        return APR_EINVAL;
    if (strlen(pw_got) >= *bufsiz) {
        rv = APR_ENAMETOOLONG;
    }
    fspr_cpystrn(pwbuf, pw_got, *bufsiz);
    memset(pw_got, 0, strlen(pw_got));
    return rv;
}
