/* This function exists solely to prevent libwinmmap.la from being empty. Empty
 * libraries cause problems on some platforms, e.g. Mac OS X. */

int tpl_nonempty(int i) {
    return i+1;
}
