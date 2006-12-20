#include "uidplus_types.h"

#include <stdio.h>
#include <stdlib.h>
#include "mailimap_extension_types.h"

struct mailimap_uidplus_resp_code_apnd * mailimap_uidplus_resp_code_apnd_new(uint32_t uid_uidvalidity, struct mailimap_set * uid_set)
{
  struct mailimap_uidplus_resp_code_apnd * resp_code_apnd;
  
  resp_code_apnd = malloc(sizeof(* resp_code_apnd));
  if (resp_code_apnd == NULL)
    return NULL;
  
  resp_code_apnd->uid_uidvalidity = uid_uidvalidity;
  resp_code_apnd->uid_set = uid_set;
  
  return resp_code_apnd;
}

void mailimap_uidplus_resp_code_apnd_free(struct mailimap_uidplus_resp_code_apnd * resp_code_apnd)
{
  if (resp_code_apnd->uid_set != NULL)
    mailimap_set_free(resp_code_apnd->uid_set);
  free(resp_code_apnd);
}

struct mailimap_uidplus_resp_code_copy *
mailimap_uidplus_resp_code_copy_new(uint32_t uid_uidvalidity, struct mailimap_set * uid_source_set, struct mailimap_set * uid_dest_set)
{
  struct mailimap_uidplus_resp_code_copy * resp_code_copy;
  
  resp_code_copy = malloc(sizeof(* resp_code_copy));
  if (resp_code_copy == NULL)
    return NULL;
  
  resp_code_copy->uid_uidvalidity = uid_uidvalidity;
  resp_code_copy->uid_source_set = uid_source_set;
  resp_code_copy->uid_dest_set = uid_dest_set;
  
  return resp_code_copy;
}

void mailimap_uidplus_resp_code_copy_free(struct mailimap_uidplus_resp_code_copy * resp_code_copy)
{
  if (resp_code_copy->uid_dest_set != NULL)
    mailimap_set_free(resp_code_copy->uid_dest_set);
  if (resp_code_copy->uid_source_set != NULL)
    mailimap_set_free(resp_code_copy->uid_source_set);
  free(resp_code_copy);
}

void mailimap_uidplus_free(struct mailimap_extension_data * ext_data)
{
  switch (ext_data->ext_type) {
  case MAILIMAP_UIDPLUS_RESP_CODE_APND:
    mailimap_uidplus_resp_code_apnd_free(ext_data->ext_data);
    break;
  case MAILIMAP_UIDPLUS_RESP_CODE_COPY:
    mailimap_uidplus_resp_code_copy_free(ext_data->ext_data);
    break;
  case MAILIMAP_UIDPLUS_RESP_CODE_UIDNOTSTICKY:
    /* nothing to deallocate */
    break;
  }
  
  free(ext_data);
}

