#ifndef UIDPLUS_PARSER_H

#define UIDPLUS_PARSER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>
#include "mailstream_types.h"
#include "mmapstring.h"
#include "mailimap_types.h"
#include "mailimap_extension_types.h"

int mailimap_uidplus_parse(int calling_parser, mailstream * fd,
    MMAPString * buffer, size_t * index,
    struct mailimap_extension_data ** result,
    size_t progr_rate,
    progress_function * progr_fun);

#ifdef __cplusplus
}
#endif

#endif
