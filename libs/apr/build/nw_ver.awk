BEGIN {

  # fetch APR version numbers from input file and writes them to STDOUT

  while ((getline < ARGV[1]) > 0) {
    if (match ($0, /^#define APR_MAJOR_VERSION/)) {
      ver_major = $3;
    }
    else if (match ($0, /^#define APR_MINOR_VERSION/)) {
      ver_minor = $3;
    }
    else if (match ($0, /^#define APR_PATCH_VERSION/)) {
      ver_str_patch = $3;
      if (match (ver_str_patch, /[0-9][0-9]*/)) {
         ver_patch = substr(ver_str_patch, RSTART, RLENGTH); 
      }
    }
  }
  ver = ver_major "," ver_minor "," ver_patch;
  ver_str = ver_major "." ver_minor "." ver_str_patch;

  print "VERSION = " ver "";
  print "VERSION_STR = " ver_str "";

}
