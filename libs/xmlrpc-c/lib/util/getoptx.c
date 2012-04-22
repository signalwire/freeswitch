/* This version of `getopt' appears to the caller like standard Unix getopt()
   but it behaves differently for the user, since it allows the user
   to intersperse the options with the other arguments.

   As getopt() works, it permutes the elements of `argv' so that,
   when it is done, all the options precede everything else.  Thus
   all application programs are extended to handle flexible argument order.

   Setting the environment variable _POSIX_OPTION_ORDER disables permutation.
   Then the behavior is completely standard.

   GNU application programs can use a third alternative mode in which
   they can distinguish the relative order of options and other arguments.  
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "getoptx.h"

/* Note that on some systems, the header files above declare variables
   for use with their native getopt facilities, and those variables have
   the same names as we'd like to use.  So we use things like optargx
   instead of optarg to avoid the collision.
*/

/* For communication from `getopt' to the caller.
   When `getopt' finds an option that takes an argument,
   the argument value is returned here.
*/
static char *optargx = 0;

/* Index in ARGV of the next element to be scanned.
   This is used for communication to and from the caller
   and for communication between successive calls to getoptx().

   On entry to getoptx(), zero means this is the first call; initialize.

   When getoptx() returns EOF, this is the index of the first of the
   non-option elements that the caller should itself scan.

   Otherwise, `optindx' communicates from one call to the next
   how much of ARGV has been scanned so far.  
*/

static int optindx = 0;

/* The next char to be scanned in the option-element
   in which the last option character we returned was found.
   This allows us to pick up the scan where we left off.

   If this is zero, or a null string, it means resume the scan
   by advancing to the next ARGV-element.  */

static char *nextchar;

/* Callers store zero here to inhibit the error message
   for unrecognized options.  
*/

static int opterrx;

/* Index in _GETOPT_LONG_OPTIONS of the long-named option actually found.
   Only valid when a long-named option was found. */

static int option_index;

struct optionx * _getopt_long_options;

/* Handle permutation of arguments.  */

/* Describe the part of ARGV that contains non-options that have
   been skipped.  `first_nonopt' is the index in ARGV of the first of them;
   `last_nonopt' is the index after the last of them.  */

static int first_nonopt;
static int last_nonopt;

/* Exchange two adjacent subsequences of ARGV.
   One subsequence is elements [first_nonopt,last_nonopt)
    which contains all the non-options that have been skipped so far.
   The other is elements [last_nonopt,optindx), which contains all
    the options processed since those non-options were skipped.

   `first_nonopt' and `last_nonopt' are relocated so that they describe
    the new indices of the non-options in ARGV after they are moved.  */

static void
exchange(char ** const argv) {
    unsigned int const nonopts_size = 
        (last_nonopt - first_nonopt) * sizeof (char *);
    char **temp = (char **) malloc (nonopts_size);

    if (temp == NULL)
        abort();

    /* Interchange the two blocks of data in argv.  */

    memcpy (temp, &argv[first_nonopt], nonopts_size);
    memcpy (&argv[first_nonopt], &argv[last_nonopt], 
            (optindx - last_nonopt) * sizeof (char *));
    memcpy (&argv[first_nonopt + optindx - last_nonopt], temp, 
            nonopts_size);

    /* Update records for the slots the non-options now occupy.  */

    first_nonopt += (optindx - last_nonopt);
    last_nonopt = optindx;

    free(temp);
}

/* Scan elements of ARGV (whose length is ARGC) for option characters
   given in OPTSTRING.

   If an element of ARGV starts with '-', and is not exactly "-" or "--",
   then it is an option element.  The characters of this element
   (aside from the initial '-') are option characters.  If getoptx()
   is called repeatedly, it returns successively each of the option characters
   from each of the option elements.

   If getoptx() finds another option character, it returns that character,
   updating `optindx' and `nextchar' so that the next call to getoptx() can
   resume the scan with the following option character or ARGV-element.

   If there are no more option characters, getoptx() returns `EOF'.
   Then `optindx' is the index in ARGV of the first ARGV-element
   that is not an option.  (The ARGV-elements have been permuted
   so that those that are not options now come last.)

   OPTSTRING is a string containing the legitimate option characters.
   If an option character is seen that is not listed in OPTSTRING,
   return '?' after printing an error message.  If you set `opterrx' to
   zero, the error message is suppressed but we still return '?'.

   If a char in OPTSTRING is followed by a colon, that means it wants an arg,
   so the following text in the same ARGV-element, or the text of the following
   ARGV-element, is returned in `optargx'.  Two colons mean an option that
   wants an optional arg; if there is text in the current ARGV-element,
   it is returned in `optargx', otherwise `optargx' is set to zero.

   If OPTSTRING starts with `-', it requests a different method of handling the
   non-option ARGV-elements.  See the comments about RETURN_IN_ORDER, above.

   Long-named options begin with `+' instead of `-'.
   Their names may be abbreviated as long as the abbreviation is unique
   or is an exact match for some defined option.  If they have an
   argument, it follows the option name in the same ARGV-element, separated
   from the option name by a `=', or else the in next ARGV-element.
   getoptx() returns 0 when it finds a long-named option.  */

static int
getoptx(int          const argc, 
        char **      const argv, 
        const char * const optstring) {

    optargx = 0;

    /* Initialize the internal data when the first call is made.
       Start processing options with ARGV-element 1 (since ARGV-element 0
       is the program name); the sequence of previously skipped
       non-option ARGV-elements is empty.  */

    if (optindx == 0)
    {
        first_nonopt = last_nonopt = optindx = 1;

        nextchar = 0;

    }

    if (nextchar == 0 || *nextchar == 0)
    {
        /* If we have just processed some options following some non-options,
               exchange them so that the options come first.  */

        if (first_nonopt != last_nonopt && last_nonopt != optindx)
            exchange (argv);
        else if (last_nonopt != optindx)
            first_nonopt = optindx;

            /* Now skip any additional non-options
               and extend the range of non-options previously skipped.  */

        while (optindx < argc
               && (argv[optindx][0] != '-'|| argv[optindx][1] == 0)
               && (argv[optindx][0] != '+'|| argv[optindx][1] == 0))
            optindx++;
        last_nonopt = optindx;

        /* Special ARGV-element `--' means premature end of options.
           Skip it like a null option,
           then exchange with previous non-options as if it were an option,
           then skip everything else like a non-option.  */

        if (optindx != argc && !strcmp (argv[optindx], "--"))
        {
            optindx++;

            if (first_nonopt != last_nonopt && last_nonopt != optindx)
                exchange (argv);
            else if (first_nonopt == last_nonopt)
                first_nonopt = optindx;
            last_nonopt = argc;

            optindx = argc;
        }

        /* If we have done all the ARGV-elements, stop the scan
           and back over any non-options that we skipped and permuted.  */

        if (optindx == argc)
        {
            /* Set the next-arg-index to point at the non-options
               that we previously skipped, so the caller will digest them.  */
            if (first_nonopt != last_nonopt)
                optindx = first_nonopt;
            return EOF;
        }
     
        /* If we have come to a non-option and did not permute it,
           either stop the scan or describe it to the caller and pass
           it by.  
        */

        if ((argv[optindx][0] != '-' || argv[optindx][1] == 0)
            && (argv[optindx][0] != '+' || argv[optindx][1] == 0))
        {
            optargx = argv[optindx++];
            return 1;
        }

        /* We have found another option-ARGV-element.
           Start decoding its characters.  */

        nextchar = argv[optindx] + 1;
    }

    if ((argv[optindx][0] == '+' || (argv[optindx][0] == '-'))
        )
    {
        struct optionx *p;
        char *s = nextchar;
        int exact = 0;
        int ambig = 0;
        struct optionx * pfound;
        int indfound;

        while (*s && *s != '=') s++;

        indfound = 0;  /* quite compiler warning */

        /* Test all options for either exact match or abbreviated matches.  */
        for (p = _getopt_long_options, option_index = 0, pfound = NULL;
             p->name; 
             p++, option_index++)
            if (!strncmp (p->name, nextchar, s - nextchar))
            {
                if ((unsigned int)(s - nextchar) == strlen (p->name))
                {
                    /* Exact match found.  */
                    pfound = p;
                    indfound = option_index;
                    exact = 1;
                    break;
                }
                else if (!pfound)
                {
                    /* First nonexact match found.  */
                    pfound = p;
                    indfound = option_index;
                }
                else
                    /* Second nonexact match found.  */
                    ambig = 1;
            }

        if (ambig && !exact)
        {
            fprintf (stderr, "%s: option `%s' is ambiguous\n",
                     argv[0], argv[optindx]);
            nextchar += strlen (nextchar);               
            return '?';
        }

        if (pfound)
        {
            option_index = indfound;
            optindx++;
            if (*s)
            {
                if (pfound->has_arg > 0)
                    optargx = s + 1;
                else
                {
                    fprintf (stderr,
                             "%s: option `%c%s' doesn't allow an argument\n",
                             argv[0], argv[optindx - 1][0], pfound->name);
                    nextchar += strlen (nextchar);               
                    return '?';
                }
            }
            else if (pfound->has_arg)
            {
                if (optindx < argc)
                    optargx = argv[optindx++];
                else if (pfound->has_arg != 2)
                {
                    fprintf (stderr, "%s: option `%s' requires an argument\n",
                             argv[0], argv[optindx - 1]);
                    nextchar += strlen (nextchar);           
                    return '?';
                }
            }
            nextchar += strlen (nextchar);
            if (pfound->flag)
                *(pfound->flag) = pfound->val;
            return 0;
        }
        if (argv[optindx][0] == '+' || strchr (optstring, *nextchar) == 0)
        {
            if (opterrx != 0)
                fprintf (stderr, "%s: unrecognized option `%c%s'\n",
                         argv[0], argv[optindx][0], nextchar);
            nextchar += strlen (nextchar);           
            return '?';
        }
    }
 
    /* Look at and handle the next option-character.  */

    {
        char c = *nextchar++;
        char *temp = strchr (optstring, c);

        /* Increment `optindx' when we start to process its last character.  */
        if (*nextchar == 0)
            optindx++;

        if (temp == 0 || c == ':')
        {
            if (opterrx != 0)
            {
                if (c < 040 || c >= 0177)
                    fprintf (stderr, "%s: unrecognized option, "
                             "character code 0%o\n",
                             argv[0], c);
                else
                    fprintf (stderr, "%s: unrecognized option `-%c'\n",
                             argv[0], c);
            }
            return '?';
        }
        if (temp[1] == ':')
        {
            if (temp[2] == ':')
            {
                /* This is an option that accepts an argument optionally.  */
                if (*nextchar != 0)
                {
                    optargx = nextchar;
                    optindx++;
                }
                else
                    optargx = 0;
                nextchar = 0;
            }
            else
            {
                /* This is an option that requires an argument.  */
                if (*nextchar != 0)
                {
                    optargx = nextchar;
                    /* If we end this ARGV-element by taking the rest
                       as an arg, we must advance to the next element
                       now.  
                    */
                    optindx++;
                }
                else if (optindx == argc)
                {
                    if (opterrx != 0)
                        fprintf (stderr,
                                 "%s: option `-%c' requires an argument\n",
                                 argv[0], c);
                    c = '?';
                }
                else
                    /* We already incremented `optindx' once;
                       increment it again when taking next ARGV-elt as
                       argument.
                    */
                    optargx = argv[optindx++];
                nextchar = 0;
            }
        }
        return c;
    }
}



void
getopt_long_onlyx(int              const argc, 
                  char **          const argv, 
                  const char *     const options, 
                  struct optionx * const long_options, 
                  unsigned int *   const opt_index, 
                  int              const opterrArg,
                  int *            const end_of_options,
                  const char **    const optarg_arg,
                  const char **    const unrecognized_option) {

    int rc;

    opterrx = opterrArg;
    _getopt_long_options = long_options;
    rc = getoptx(argc, argv, options);
    if (rc == 0)
        *opt_index = option_index;

    if (rc == '?')
        *unrecognized_option = argv[optindx];
    else
        *unrecognized_option = NULL;

    if (rc < 0)
        *end_of_options = 1;
    else
        *end_of_options = 0;

    *optarg_arg = optargx;
}
     

unsigned int
getopt_argstart(void) {
/*----------------------------------------------------------------------------
   This is a replacement for what traditional getopt does with global
   variables.

   You call this after getopt_long_onlyx() has returned "end of
   options"
-----------------------------------------------------------------------------*/
    return optindx;
}


/* Getopt for GNU.
   Copyright (C) 1987, 1989 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 1, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  
*/
