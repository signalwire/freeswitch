/* Interface to getopt_long_onlyx() */


enum argreq {no_argument, required_argument, optional_argument};

struct optionx {
    /* This describes an option.  If the field `flag' is nonzero, it
       points to a variable that is to be set to the value given in
       the field `val' when the option is found, but left unchanged if
       the option is not found.  
    */
    const char * name;
    enum argreq has_arg;
    int * flag;
    int val;
};

/* long_options[] is a list terminated by an element that contains
   a NULL 'name' member.
*/
void
getopt_long_onlyx(int              const argc, 
                  char **          const argv, 
                  const char *     const options, 
                  struct optionx * const long_options, 
                  unsigned int *   const opt_index, 
                  int              const opterrArg,
                  int *            const end_of_options,
                  const char **    const optarg_arg,
                  const char **    const unrecognized_option);

unsigned int
getopt_argstart(void);

/* 
   Copyright (C) 1989 Free Software Foundation, Inc.

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
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

