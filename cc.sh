cc=`cat cluecon.tmpl | sed 's/\\\\/\\\\\\\\/g' | awk '{printf "%s\\\\n", $0}' `
cc_s=`cat cluecon_small.tmpl | sed 's/\\\\/\\\\\\\\/g' | awk '{printf "%s\\\\n", $0}' `

cat <<EOF > src/include/cc.h

const char *cc = "$cc";
const char *cc_s = "$cc_s";

EOF

