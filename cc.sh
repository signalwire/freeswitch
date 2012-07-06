cc=`cat cluecon.tmpl | sed 's/\\\\/\\\\\\\\/g' | awk '{printf "%s\\\\n", $0}' `

cat <<EOF > src/include/cc.h

const char *cc = "$cc";

EOF

