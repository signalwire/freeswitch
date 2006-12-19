# This file generates C code for a character set alias lookup table
# from the ccs/charset.aliases and ces/charset.aliases files.

# Valid alias definitions must have at least two fields
NF > 1 {
    target = unescape(tolower($1));
    for (n = 2; n <= NF; ++n)
        aliases[unescape(tolower($n))] = target;
}

# Ignore all other lines
{ next }

# Now generate the sorted alias list and lookup code
END {
    # We'll have to sort the alias list, so that we can later use
    # bsearch to find an alias.
    alias_count = 0;
    for (name in aliases)
        alias_names[alias_count++] = name;
    sort(alias_names, alias_count);

    # Right, now generate the alias array and lookup code
    print "/* GENERATED CODE -- DO NOT EDIT                     -*- C -*-";
    print " * Use the following command to regenerate this file:";
    print " *     awk -f ../build/gen_aliases.awk \\";
    print " *            ../ccs/charset.aliases \\";
    print " *            ../ces/charset.aliases > charset_alias.h";
    print " */";
    print "#ifndef API_HAVE_CHARSET_ALIAS_TABLE";
    print "#define API_HAVE_CHARSET_ALIAS_TABLE";
    print "";
    print "#include <stdlib.h>";
    print "#include <string.h>";
    print "";
    print "/* This is a sorted table of alias -> true name mappings. */";
    print "static struct charset_alias {";
    print "    const char *name;";
    print "    const char *target;";
    print "} const charset_alias_list[] = {";

    for (i = 0; i < alias_count; ++i)
        print "    {\"" alias_names[i] "\", \"" aliases[alias_names[i]] "\"},";

    print "    {NULL, NULL} };";
    print "";
    print "static const size_t charset_alias_count =";
    print "    sizeof(charset_alias_list)/sizeof(charset_alias_list[0]) - 1;"
    print "";

    print "/* Compare two aliases. */";
    print "static int charset_alias_compare (const void *u, const void *v)";
    print "{";
    print "    const struct charset_alias *const a = u;";
    print "    const struct charset_alias *const b = v;";
    print "    return strcmp(a->name, b->name);";
    print "}";
    print "";

    print "/* Look up an alias in the sorted table and return its name,";
    print "   or NULL if it's not in the table. */";
    print "static const char *charset_alias_find (const char *name)";
    print "{";
    print "    struct charset_alias key;";
    print "    struct charset_alias *alias;";
    print "#if 'A' == '\\xC1' /* if EBCDIC host */";
    print "    /* The table is sorted in ASCII collation order, not in EBCDIC order.";
    print "     * At the first access, we sort it automatically";
    print "     * Criterion for the 1st time initialization is the fact that the";
    print "     * 1st name in the list starts with a digit (in ASCII, numbers";
    print "     * have a lower ordinal value than alphabetic characters; while";
    print "     * in EBCDIC, their ordinal value is higher)";
    print "     */";
    print "    if (isdigit(charset_alias_list[0].name[0]))  {";
    print "        qsort((void *)charset_alias_list, charset_alias_count,";
    print "              sizeof(charset_alias_list[0]),";
    print "              charset_alias_compare);";
    print "    }";
#endif
    print "    key.name = name;";
    print "    alias = bsearch(&key, charset_alias_list, charset_alias_count,";
    print "                    sizeof(charset_alias_list[0]),";
    print "                    charset_alias_compare);";
    print "    if (alias)";
    print "        return alias->target;";
    print "    else";
    print "        return NULL;"
    print "}";
    print "";

    print "#endif /* API_HAVE_CHARSET_ALIAS_TABLE */";
}

# Remove shell escapes from charset names
function unescape(name) {
    gsub(/\\\(/, "(", name);
    gsub(/\\\)/, ")", name);
    return name;
}

# Yes, bubblesort. So what?
function sort(list, len) {
    for (i = len; i > 1; --i) {
        swapped = 0;
        for (j = 1; j < i; ++j) {
            if (list[j-1] > list[j]) {
                temp = list[j];
                list[j] = list[j-1];
                list[j-1] = temp;
                swapped = 1;
            }
        }
        if (!swapped)
            break;
    }
}
