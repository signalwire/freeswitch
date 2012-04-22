# Based on apr's make_export.awk, which is
# based on Ryan Bloom's make_export.pl

BEGIN {
    printf(" (APRLIB)\n")
}

# List of functions that we don't support, yet??
#/apr_##name##_set_inherit/{next}
#/apr_##name##_unset_inherit/{next}


function add_symbol (sym_name) {
	if (count) {
		found++
	}
    gsub (/ /, "", sym_name)
	line = line sym_name ",\n"

	if (count == 0) {
		printf(" %s", line)
		line = ""
	}
}

/^[ \t]*AP[RUI]?_DECLARE[^(]*[(][^)]*[)]([^ ]* )*[^(]+[(]/ {
    sub("[ \t]*AP[RUI]?_DECLARE[^(]*[(][^)]*[)][ \t]*", "")
    sub("[(].*", "")
    sub("([^ ]* (^([ \t]*[(])))+", "")

    add_symbol($0)
    next
}

/^[ \t]*AP_DECLARE_HOOK[^(]*[(][^)]*/ {
    split($0, args, ",")
    symbol = args[2]
    sub("^[ \t]+", "", symbol)
    sub("[ \t]+$", "", symbol)

    add_symbol("ap_hook_" symbol)
    add_symbol("ap_hook_get_" symbol)
    add_symbol("ap_run_" symbol)
    next
}

/^[ \t]*APR_POOL_DECLARE_ACCESSOR[^(]*[(][^)]*[)]/ {
    sub("[ \t]*APR_POOL_DECLARE_ACCESSOR[^(]*[(]", "", $0)
    sub("[)].*$", "", $0)
    add_symbol("apr_" $0 "_pool_get")
    next
}

/^[ \t]*APR_DECLARE_INHERIT_SET[^(]*[(][^)]*[)]/ {
    sub("[ \t]*APR_DECLARE_INHERIT_SET[^(]*[(]", "", $0)
    sub("[)].*$", "", $0)
    add_symbol("apr_" $0 "_inherit_set")
    next
}

/^[ \t]*APR_DECLARE_INHERIT_UNSET[^(]*[(][^)]*[)]/ {
    sub("[ \t]*APR_DECLARE_INHERIT_UNSET[^(]*[(]", "", $0)
    sub("[)].*$", "", $0)
    add_symbol("apr_" $0 "_inherit_unset")
    next
}

/^[ \t]*AP[RUI]?_DECLARE_DATA .*;$/ {
       varname = $NF;
       gsub( /[*;]/, "", varname);
       gsub( /\[.*\]/, "", varname);
       add_symbol(varname);
}


#END {
#	printf(" %s", line)
#}
