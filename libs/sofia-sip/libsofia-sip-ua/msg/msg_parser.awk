#! /usr/bin/env awk
#
# This script recreates C files containing header-specific boilerplate stuff
# using the given list of headers (usually obtained from the master structure).
#
# It can also create a parser table.
#
# --------------------------------------------------------------------
#
# This file is part of the Sofia-SIP package
#
# Copyright (C) 2005 Nokia Corporation.
#
# Contact: Pekka Pessi <pekka.pessi@nokia.com>
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public License
# as published by the Free Software Foundation; either version 2.1 of
# the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
# 02110-1301 USA
#
# --------------------------------------------------------------------
#
# Contributor(s): Pekka.Pessi@nokia.com.
#
# Created: Fri Apr  6 12:59:59 2001 ppessi
#

BEGIN {
  "date '+%a %b %e %H:%M:%S %Y'" | getline date;

  ascii =			       \
     "                               " \
    " !\"#$%&'()*+,-./0123456789:;<=>?" \
    "@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_" \
    "`abcdefghijklmnopqrstuvwxyz{|}~";
  lower_case = "abcdefghijklmnopqrstuvwxyz";

  N=0;
  # Initialize these as arrays
  split("", symbols);
  split("", names);
  split("", comments);
  split("", hashes);
  split("", NAMES);
  split("", Comments);
  split("", COMMENTS);

  # indexed by the C name of the header
  split("", Since);		# Non-NUL if extra
  split("", Extra);		# Offset in extra headers

  total = 0;
  ordinary = 0;
  basic = 0;
  extra = 0;
  without_experimental = 0;

  template="";
  template1="";
  template2="";
  template3="";
  prefix="";
  tprefix="";
  failed=0;
  success=0;

  ERRNO="error";
}

function name_hash (name)
{
  hash = 0;

  len = length(name);

  for (i = 1; i <= len; i++) {
    c = tolower(substr(name, i, 1));
    hash = (38501 * (hash + index(ascii, c))) % 65536;
  }

  if (hash == 0) {
    print "*** msg_parser.awk: calculating hash failed\n";
    exit(5);
  }

  if (0) {
    # Test that hash algorithm above agrees with the C version
    pipe = ("../msg/msg_name_hash " name);
    pipe | getline;
    close(pipe);
    if (hash != $0) {
      print name ": " hash " != " $0 > "/dev/stderr";
    }
  }

  return hash "";
}

#
# Replace magic patterns in template p with header-specific values
#
function protos (name, comment, hash, since)
{
  NAME=toupper(name);
  sub(/.*[\/][*][*][<][ 	]*/, "", comment);
  sub(/[ 	]*[*][\/].*/, "", comment);
  sub(/[ 	]+/, " ", comment);

  short = match(comment, /[(][a-z][)]/);
  if (short) {
    short = substr(comment, short + 1, 1);
    sub(/ *[(][a-z][)]/, "", comment);
    shorts[index(lower_case, short)] = name;
  }

  do_hash = hash == 0;

  if (do_hash) {
    split(comment, parts, " ");
    name2 = tolower(parts[1]);
    gsub(/-/, "_", name2);
    if (name2 != name && name2 != tprefix "_" name) {
      print name " mismatch with " comment " (" real ")" > "/dev/stderr";
    }

    hash = name_hash(parts[1]);

    hashed[name] = hash;

    if (comment !~ /header/) {
      comment = comment " header";
    }
  }

  Comment = comment;
  if (!do_hash) {
    comment = tolower(comment);
  }
  COMMENT = toupper(comment);

  # Store the various forms into an array for the footer processing
  N++;
  hashes[N] = hash;
  names[N] = name;
  NAMES[N] = NAME;
  comments[N] = comment;
  Comments[N] = comment;
  COMMENTS[N] = COMMENT;

  symbols[name] = comment;
  if (since) {
    Since[name] = since;
  }

  expr = (without_experimental > 0 && do_hash);
  if (expr) {
    printf "%s is experimental\n", Comment;
  }

  experimental[N] = expr;

  if (PR) {
    if (expr) {
      print "#if SU_HAVE_EXPERIMENTAL" > PR;
    }
    replace(template, hash, name, NAME, comment, Comment, COMMENT, since);
    replace(template1, hash, name, NAME, comment, Comment, COMMENT, since);
    replace(template2, hash, name, NAME, comment, Comment, COMMENT, since);
    replace(template3, hash, name, NAME, comment, Comment, COMMENT, since);
    if (expr) {
      print "#endif /* SU_HAVE_EXPERIMENTAL */" > PR;
    }
  }
}

function replace (p, hash, name, NAME, comment, Comment, COMMENT, since)
{
  #
  # Replace various forms of header name in template, print it out
  #
  if (p) {
    gsub(/#hash#/, hash, p);
    gsub(/#xxxxxx#/, name, p);
    gsub(/#XXXXXX#/, NAME, p);
    gsub(/#xxxxxxx_xxxxxxx#/, comment, p);
    gsub(/#Xxxxxxx_Xxxxxxx#/, Comment, p);
    gsub(/#XXXXXXX_XXXXXXX#/, COMMENT, p);

    if (since) {
      gsub(/#version#/, since, p);
    }
    else {
      # Remove line with #version#
      gsub(/\n[^#\n]*#version#[^\n]*/, "", p);
    }

    print p > PR;
  }
}

#
# Repeat each line in the footer containing the magic replacement
# pattern with an instance of all headers
#
function process_footer (text)
{
  if (!match(tolower(text), /#(xxxxxx(x_xxxxxxx)?|hash)#/)) {
    n = length(text);
    while (substr(text, n) == "\n") {
      n = n - 1;
      text = substr(text, 1, n);
    }
    if (n > 0)
      print text > PR;
    return;
  }

  n = split(text, lines, RS);

  for (i = 1; i <= n; i++) {
    l = lines[i];
    if (match(tolower(l), /#(xxxxxx(x_xxxxxxx)?|hash)#/)) {
      expr = 0;

      for (j = 1; j <= N; j++) {
	l = lines[i];
	if (expr != experimental[j]) {
	  expr = experimental[j];
	  if (expr) {
	    print "#if SU_HAVE_EXPERIMENTAL" > PR;
	  }
	  else {
	    print "#endif /* SU_HAVE_EXPERIMENTAL */" > PR;
	  }
	}
	gsub(/#hash#/, hashes[j], l);
	gsub(/#xxxxxxx_xxxxxxx#/, comments[j], l);
	gsub(/#Xxxxxxx_Xxxxxxx#/, Comments[j], l);
	gsub(/#XXXXXXX_XXXXXXX#/, COMMENTS[j], l);
	gsub(/#xxxxxx#/, names[j], l);
	gsub(/#XXXXXX#/, NAMES[j], l);
	print l > PR;
      }

      if (expr) {
	print "#endif /* SU_HAVE_EXPERIMENTAL */" > PR;
      }
    } else {
      print l > PR;
    }
  }
}

#
# Read flags used with headers
#
function read_header_flags (flagfile,    line, tokens, name, value)
{
  while ((getline line < flagfile) > 0) {
    sub(/^[ \t]+/, "", line);
    sub(/[ \t]+$/, "", line);
    if (line ~ /^#/ || line ~ /^$/)
      continue;

    split(line, tokens,  /[ \t]*=[ \t]*/);
    name = tolower(tokens[1]);
    gsub(/-/, "_", name);
    gsub(/,/, " ", name);
    # print "FLAG: " name " = " tokens[2]

    if (header_flags[name]) {
      print flagfile ": already defined " tokens[1];
    }
    else if (!symbols[name]) {
      print flagfile ": unknown header \"" tokens[1] "\"";
    }
    else {
      header_flags[name] = tokens[2];
    }
  }
  close(flagfile);
}

#
# Read in templates
#
function templates ()
{
  if (!auto) {
    auto = FILENAME;

    if (!prefix) { prefix = module; }
    if (!tprefix) { tprefix = prefix; }

    sub(/.*\//, "", auto);
    auto = "This file is automatically generated from <" auto "> by msg_parser.awk.";

    if (PR) {
      if (TEMPLATE == "") { TEMPLATE = PR ".in"; }
      RS0=RS; RS="\f\n";
      if ((getline theader < TEMPLATE) < 0) {
	print ( TEMPLATE ": " ERRNO );
	failed=1;
        exit(1);
      }
      getline header < TEMPLATE;
      getline template < TEMPLATE;
      getline footer < TEMPLATE;

      if (TEMPLATE1) {
	if ((getline dummy < TEMPLATE1) < 0) {
	  print(TEMPLATE1 ": " ERRNO);
	  failed=1;
          exit(1);
        }
	getline dummy < TEMPLATE1;
	getline template1 < TEMPLATE1;
	getline dummy < TEMPLATE1;
      }

      if (TEMPLATE2) {
	if ((getline dummy < TEMPLATE2) < 0) {
	  print( TEMPLATE2 ": " ERRNO );
	  failed=1;
	  exit(1);
	}
	getline dummy < TEMPLATE2;
	getline template2 < TEMPLATE2;
	getline dummy < TEMPLATE2;
      }

      if (TEMPLATE3) {
	if ((getline dummy < TEMPLATE3) < 0) {
	  print( TEMPLATE3 ": " ERRNO );
	  failed=1;
	  exit(1);
	}
	getline dummy < TEMPLATE3;
	getline template3 < TEMPLATE3;
	getline dummy < TEMPLATE3;
      }

      sub(/.*[\/]/, "", TEMPLATE);
      gsub(/#AUTO#/, auto, header);
      gsub(/#DATE#/, "@date Generated: " date, header);
      if (PACKAGE_NAME) gsub(/#PACKAGE_NAME#/, PACKAGE_NAME, header);
      if (PACKAGE_VERSION) gsub(/#PACKAGE_VERSION#/, PACKAGE_VERSION, header);
      print header > PR;

      RS=RS0;
    }

    if (!NO_FIRST) {
      protos("request", "/**< Request line */", -1);
      protos("status", "/**< Status line */", -2);
    }
  }
}

/^#### EXTRA HEADER LIST STARTS HERE ####$/ { HLIST=1; templates(); }
HLIST && /^#### DEFAULT HEADER LIST ENDS HERE ####$/ { basic=total; }
HLIST && /^#### EXPERIMENTAL HEADER LIST STARTS HERE ####$/ {
  without_experimental = total; }

HLIST && /^[a-z]/ { protos($1, $0, 0, $2);
  headers[total++] = $1;
  Extra[$1] = extra++;
}
/^#### EXTRA HEADER LIST ENDS HERE ####$/ { HLIST=0;  }

/^ *\/\* === Headers start here \*\// { in_header_list=1;  templates(); }
/^ *\/\* === Headers end here \*\// { in_header_list=0; }

PT && /^ *\/\* === Hash headers end here \*\// { in_header_list=0;}

in_header_list && /^  (sip|rtsp|http|msg|mp)_[a-z_0-9]+_t/ {
  n=$0
  sub(/;.*$/, "", n);
  sub(/^ *(sip|rtsp|http|msg|mp)_[a-z0-9_]*_t[ 	]*/, "", n);
  sub(/^[*](sip|rtsp|http|msg|mp)_/, "", n);

  if ($0 !~ /[\/][*][*][<]/) {
    getline;
  }
  if ($0 !~ /[\/][*][*][<]/) {
    printf "msg_protos.awk: header %s is malformed\n", n;
    failed=1;
    exit 1;
  }

  if (!NO_MIDDLE)
    protos(n, $0, 0);

  headers[total] = n; total++; ordinary++;
}

function print_parser_table(struct, scope, name, N, N_EXPERIMENTAL)
{
  if (PT) {
    if (N > ordinary) {
      printf("/* Ordinary %u, extra %u, experimental %u */\n",
	     ordinary, N - ordinary, N_EXPERIMENTAL - N) > PT;
      printf("struct %s {\n", struct) > PT;
      printf("  %s_t base;\n", module) > PT;
      printf("  msg_header_t *extra[%u];\n", N - ordinary) > PT;
      if (N != N_EXPERIMENTAL) {
	print "#if SU_HAVE_EXPERIMENTAL" > PT;
	printf("  msg_header_t *extra[%u];\n", N_EXPERIMENTAL - N) > PT;
	print "#endif" > PT;
      }
      printf("};\n\n") > PT;
    }

    printf("%s\n", scope) > PT;
    printf("msg_mclass_t const %s[1] = \n{{\n", name) > PT;
    printf("# if defined (%s_HCLASS)\n", toupper(module)) > PT;
    printf("  %s_HCLASS,\n", toupper(module)) > PT;
    printf("#else\n") > PT;
    printf("  {{ 0 }},\n") > PT;
    printf("#endif\n") > PT;
    printf("  %s_VERSION_CURRENT,\n", toupper(module)) > PT;
    printf("  %s_PROTOCOL_TAG,\n", toupper(module)) > PT;
    printf("#if defined (%s_PARSER_FLAGS)\n", toupper(module)) > PT;
    printf("  %s_PARSER_FLAGS,\n", toupper(module)) > PT;
    printf("#else\n") > PT;
    printf("  0,\n") > PT;
    printf("#endif\n") > PT;
    if (N > ordinary) {
      printf("  sizeof (struct %s),\n", struct) > PT;
    }
    else {
      printf("  sizeof (%s_t),\n", module) > PT;
    }
    printf("  %s_extract_body,\n", module) > PT;

    len = split("request status separator payload unknown error", unnamed, " ");

    for (i = 1; i <= len; i++) {
      printf("  {{ %s_%s_class, msg_offsetof(%s_t, %s_%s) }},\n",
	     tprefix, unnamed[i], module, prefix, unnamed[i]) > PT;
    }
    if (multipart) {
      printf("  {{ %s_class, msg_offsetof(%s_t, %s_multipart) }},\n",
	     multipart, module, prefix) > PT;
    } else {
      printf("  {{ NULL, 0 }},\n") > PT;
    }
    if (MC_SHORT_SIZE) {
      printf("  %s_short_forms, \n", module) > PT;
    }
    else {
      printf("  NULL, \n") > PT;
    }
    printf("  %d, \n", MC_HASH_SIZE) > PT;
    if (N != N_EXPERIMENTAL) {
      print "#if SU_HAVE_EXPERIMENTAL" > PT;
      printf("  %d,\n", N_EXPERIMENTAL) > PT;
      print "#else" > PT;
    }
    printf("  %d,\n", N) > PT;
    if (N != N_EXPERIMENTAL) {
      print "#endif" > PT;
    }

    printf("  {\n") > PT;

    for (j = 0; j < MC_HASH_SIZE; j++) {
      c = (j + 1 == MC_HASH_SIZE) ? "" : ",";
      if (j in header_hash) {
	n = header_hash[j];
	i = index_hash[j];

        flags = header_flags[n]; if (flags) flags = ",\n      " flags;

	if (i >= N) {
	  print "#if SU_HAVE_EXPERIMENTAL" > PT;
	}

	if (i >= ordinary) {
	  printf("    { %s_%s_class,\n" \
		 "      msg_offsetof(struct %s, extra[%u])%s }%s\n",
		 tprefix, n, struct, Extra[n], flags, c) > PT;
	}
	else {
	  printf("    { %s_%s_class, msg_offsetof(%s_t, %s_%s)%s }%s\n",
		 tprefix, n, module, prefix, n, flags, c) > PT;
	}

	if (i >= N) {
	  printf("#else\n    { NULL, 0 }%s\n#endif\n", c) > PT;
	}
      }
      else {
	printf("    { NULL, 0 }%s\n", c) > PT;
      }
    }
    printf("  }\n}};\n\n") > PT;

    }
}

END {
  if (failed) { exit };

  if (!NO_LAST) {
    protos("unknown", "/**< Unknown headers */", -3);
    protos("error", "/**< Erroneous headers */", -4);
    protos("separator", "/**< Separator line between headers and body */", -5);
    protos("payload", "/**< Message payload */", -6);
    if (multipart)
      protos("multipart", "/**< Multipart payload */", -7);
  }

  if (PR) {
    process_footer(footer);
  }
  else if (PT) {
    if (FLAGFILE)
      read_header_flags(FLAGFILE);

    if (TEMPLATE == "") { TEMPLATE = PT ".in"; }
    RS0=RS; RS="\n";
    getline theader < TEMPLATE;
    getline header < TEMPLATE;
    getline template < TEMPLATE;
    getline footer < TEMPLATE;
    RS=RS0;

    sub(/.*[\/]/, "", TEMPLATE);
    gsub(/#AUTO#/, auto, header);
    gsub(/#DATE#/, "@date Generated: " date, header);
    print header > PT;

    print "" > PT;
    print "#define msg_offsetof(s, f) ((unsigned short)offsetof(s ,f))" > PT;
    print "" > PT;

    if (MC_SHORT_SIZE) {
      printf("static msg_href_t const " \
	     "%s_short_forms[MC_SHORT_SIZE] = \n{\n",
	     module) > PT;

      for (i = 1; i <= MC_SHORT_SIZE; i = i + 1) {
	c = (i == MC_SHORT_SIZE) ? "" : ",";
	if (i in shorts) {
	  n = shorts[i];
        flags = header_flags[n]; if (flags) flags = ",\n      " flags;

	printf("  { /* %s */ %s_%s_class, msg_offsetof(%s_t, %s_%s)%s }%s\n",
	       substr(lower_case, i, 1),
	       tprefix, n, module, prefix, n, flags, c)	\
	    > PT;
	}
	else {
	  printf("  { NULL }%s\n", c) \
	    > PT;
	}
      }
      printf("};\n\n") > PT;
    }

    # printf("extern msg_hclass_t msg_multipart_class[];\n\n") > PT;

    if (basic == 0) basic = total;
    if (without_experimental == 0) without_experimental = total;

    split("", header_hash);
    split("", index_hash);

    for (i = 0; i < basic; i++) {
      n = headers[i];
      h = hashed[n];

      if (h < 0)
	continue;

      j = h % MC_HASH_SIZE; if (j == -0) j = 0;

      for (; j in header_hash;) {
	if (++j == MC_HASH_SIZE) {
	  j = 0;
	}
      }

      header_hash[j] = n;
      index_hash[j] = i;
    }

    m = module "_mclass";
    s = "_d_" module "_t";

    # Add basic headers
    if (ordinary == basic) {
      print_parser_table(s, "", m, basic, basic);
    }
    else if (basic < without_experimental) {
      print_parser_table(s, "", m, basic, basic);
    }
    else {
      print_parser_table(s, "", m, without_experimental, basic);
   }

   if (0) {

   # Hash extra headers
   for (i = basic; i < total; i++) {
      n = headers[i];
      h = hashed[n];

      if (h < 0)
	continue;

      j = h % MC_HASH_SIZE; if (j == -0) j = 0;

      for (; j in header_hash;) {
	if (++j == MC_HASH_SIZE) {
	  j = 0;
	}
      }

      header_hash[j] = n;
      index_hash[j] = i;
    }

    if (basic < total) {
      m = module "_ext_mclass";
      s = "_e_" module "_s";
      print_parser_table(s, "static", m, without_experimental, total);
    }

    printf("msg_mclass_t const * %s_extended_mclass = %s;\n\n", module, m) > PT;

    }

    if (basic < total) {
      printf("msg_hclass_t * const %s_extensions[] = {\n", module) > PT;
      for (i = basic; i < total; i++) {
	if (i == without_experimental) {
	  print "#if SU_HAVE_EXPERIMENTAL" > PT;
        }
	printf("  %s_%s_class,\n", module, headers[i]) > PT;
      }
      if (total != without_experimental)
	print "#endif" > PT;
      print "  NULL\n};\n\n" > PT;
    }
  }

  exit success;
}
