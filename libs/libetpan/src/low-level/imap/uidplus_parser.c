#include "uidplus_parser.h"

#include <stdio.h>
#include <stdlib.h>

#include "mailimap_parser.h"
#include "mailimap_keywords.h"
#include "mailimap_extension.h"
#include "uidplus_types.h"
#include "uidplus.h"

static int mailimap_uid_range_parse(mailstream * fd, MMAPString * buffer,
    size_t * index, struct mailimap_set_item ** result);

static int mailimap_uidplus_resp_code_parse(mailstream * fd, MMAPString * buffer,
    size_t * index,
    struct mailimap_extension_data ** result);

int mailimap_uidplus_parse(int calling_parser, mailstream * fd,
    MMAPString * buffer, size_t * index,
    struct mailimap_extension_data ** result,
    size_t progr_rate,
    progress_function * progr_fun)
{
  if (calling_parser != MAILIMAP_EXTENDED_PARSER_RESP_TEXT_CODE)
    return MAILIMAP_ERROR_PARSE;
  
  return mailimap_uidplus_resp_code_parse(fd, buffer, index, result);
}


/*
   uid-set         = (uniqueid / uid-range) *("," uid-set)
*/

static int uid_set_item_parse(mailstream * fd, MMAPString * buffer,
    size_t * index, struct mailimap_set_item ** result,
    size_t progr_rate,
    progress_function * progr_fun)
{
  int r;
  struct mailimap_set_item * set_item;
  uint32_t uniqueid;
  size_t cur_token;
  
  cur_token = * index;
  
  r = mailimap_uid_range_parse(fd, buffer, &cur_token, &set_item);
  if (r == MAILIMAP_NO_ERROR) {
    * result = set_item;
    * index = cur_token;
    return r;
  }
  
  if (r != MAILIMAP_ERROR_PARSE)
    return r;
  
  r = mailimap_uniqueid_parse(fd, buffer, &cur_token, &uniqueid);
  if (r == MAILIMAP_NO_ERROR) {
    set_item = mailimap_set_item_new(uniqueid, uniqueid);
    if (set_item == NULL)
      return MAILIMAP_ERROR_MEMORY;
    
    * result = set_item;
    * index = cur_token;
    
    return MAILIMAP_NO_ERROR;
  }
  
  return r;
}

static void uid_set_item_destructor(struct mailimap_set_item * set_item)
{
  mailimap_set_item_free(set_item);
}

static int mailimap_uid_set_parse(mailstream * fd, MMAPString *buffer,
    size_t * index,
    struct mailimap_set ** result)
{
  int r;
  clist * list;
  struct mailimap_set * set;
  size_t cur_token;
  
  cur_token = * index;
  
  r = mailimap_struct_list_parse(fd, buffer, &cur_token, &list,
      ',',
      (mailimap_struct_parser *) uid_set_item_parse,
      (mailimap_struct_destructor *) uid_set_item_destructor,
      0, NULL);
  if (r != MAILIMAP_NO_ERROR)
    return r;
  
  set = mailimap_set_new(list);
  if (set == NULL) {
    clistiter * cur;
    
    for(cur = clist_begin(list) ; cur != NULL ; cur = clist_next(cur)) {
      struct mailimap_set_item * item;
      
      item = clist_content(cur);
      free(item);
    }
    clist_free(list);
    return MAILIMAP_ERROR_MEMORY;
  }
  
  * result = set;
  * index = cur_token;
  
  return MAILIMAP_NO_ERROR;
}

/*
   uid-range       = (uniqueid ":" uniqueid)
                     ; two uniqueid values and all values
                     ; between these two regards of order.
                     ; Example: 2:4 and 4:2 are equivalent.
*/

static int mailimap_uid_range_parse(mailstream * fd, MMAPString * buffer,
    size_t * index, struct mailimap_set_item ** result)
{
  uint32_t first;
  uint32_t last;
  int r;
  struct mailimap_set_item * item;
  size_t cur_token;
  
  cur_token = * index;
  
  r = mailimap_uniqueid_parse(fd, buffer, &cur_token, &first);
  if (r != MAILIMAP_NO_ERROR)
    return r;
  
  r = mailimap_colon_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR)
    return r;
  
  r = mailimap_uniqueid_parse(fd, buffer, &cur_token, &last);
  if (r != MAILIMAP_NO_ERROR)
    return r;
  
  item = mailimap_set_item_new(first, last);
  if (item == NULL)
    return MAILIMAP_ERROR_MEMORY;
  
  * index = cur_token;
  * result = item;
  
  return MAILIMAP_NO_ERROR;
}

/*
   append-uid      = uniqueid

   append-uid      =/ uid-set
                     ; only permitted if client uses [MULTIAPPEND]
                     ; to append multiple messages.
*/

static int mailimap_append_uid_parse(mailstream * fd, MMAPString * buffer,
    size_t * index,
    struct mailimap_set ** result)
{
  return mailimap_uid_set_parse(fd, buffer, index, result);
}

/*
   resp-code-apnd  = "APPENDUID" SP nz-number SP append-uid
*/

static int mailimap_resp_code_apnd_parse(mailstream * fd, MMAPString * buffer,
    size_t * index,
    struct mailimap_uidplus_resp_code_apnd ** result)
{
  int r;
  size_t cur_token;
  uint32_t uidvalidity;
  struct mailimap_set * set;
  struct mailimap_uidplus_resp_code_apnd * resp_code_apnd;
  
  cur_token = * index;
  
  r = mailimap_token_case_insensitive_parse(fd, buffer, &cur_token,
      "APPENDUID");
  if (r != MAILIMAP_NO_ERROR)
    return r;

  r = mailimap_space_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR)
    return r;
  
  r = mailimap_nz_number_parse(fd, buffer, &cur_token, &uidvalidity);
  if (r != MAILIMAP_NO_ERROR)
    return r;
  
  r = mailimap_space_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR)
    return r;
  
  r = mailimap_append_uid_parse(fd, buffer, &cur_token, &set);
  if (r != MAILIMAP_NO_ERROR)
    return r;

  resp_code_apnd = mailimap_uidplus_resp_code_apnd_new(uidvalidity, set);
  if (resp_code_apnd == NULL) {
    mailimap_set_free(set);
    return MAILIMAP_ERROR_MEMORY;
  }
  
  * index = cur_token;
  * result = resp_code_apnd;
  
  return MAILIMAP_NO_ERROR;
}

/*
   resp-code-copy  = "COPYUID" SP nz-number SP uid-set SP uid-set
*/

static int mailimap_resp_code_copy_parse(mailstream * fd, MMAPString * buffer,
    size_t * index,
    struct mailimap_uidplus_resp_code_copy ** result)
{
  int r;
  size_t cur_token;
  uint32_t uidvalidity;
  struct mailimap_set * source_set;
  struct mailimap_set * dest_set;
  struct mailimap_uidplus_resp_code_copy * resp_code_copy;
  int res;
  
  cur_token = * index;

  r = mailimap_token_case_insensitive_parse(fd, buffer, &cur_token,
      "COPYUID");
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto err;
  }
  
  r = mailimap_space_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto err;
  }
  
  r = mailimap_nz_number_parse(fd, buffer, &cur_token, &uidvalidity);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto err;
  }
  
  r = mailimap_space_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto err;
  }
  
  r = mailimap_uid_set_parse(fd, buffer, &cur_token, &source_set);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto err;
  }

  r = mailimap_space_parse(fd, buffer, &cur_token);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto free_source_set;
  }
  
  r = mailimap_uid_set_parse(fd, buffer, &cur_token, &dest_set);
  if (r != MAILIMAP_NO_ERROR) {
    res = r;
    goto free_source_set;
  }
  
  resp_code_copy = mailimap_uidplus_resp_code_copy_new(uidvalidity,
      source_set, dest_set);
  if (resp_code_copy == NULL) {
    res = MAILIMAP_ERROR_MEMORY;
    goto free_dest_set;
  }
  
  * index = cur_token;
  * result = resp_code_copy;
  
  return MAILIMAP_NO_ERROR;
  
 free_dest_set:
  mailimap_set_free(dest_set);
 free_source_set:
  mailimap_set_free(source_set);
 err:
  return res;
}

/*
  "UIDNOTSTICKY"
*/

static int mailimap_uidplus_uidnotsticky_parse(mailstream * fd, MMAPString * buffer,
    size_t * index)
{
  return mailimap_token_case_insensitive_parse(fd, buffer, index,
      "UIDNOTSTICKY");
}

/*
   resp-text-code  =/ resp-code-apnd / resp-code-copy / "UIDNOTSTICKY"
                     ; incorporated before the expansion rule of
                     ;  atom [SP 1*<any TEXT-CHAR except "]">]
                     ; that appears in [IMAP]
*/

static int mailimap_uidplus_resp_code_parse(mailstream * fd, MMAPString * buffer,
    size_t * index,
    struct mailimap_extension_data ** result)
{
  struct mailimap_uidplus_resp_code_apnd * resp_code_apnd;
  struct mailimap_uidplus_resp_code_copy * resp_code_copy;
  size_t cur_token;
  struct mailimap_extension_data * ext;
  int r;
  
  cur_token = * index;
  
  r = mailimap_resp_code_apnd_parse(fd, buffer, &cur_token, &resp_code_apnd);
  if (r == MAILIMAP_NO_ERROR) {
    ext = mailimap_extension_data_new(&mailimap_extension_uidplus,
        MAILIMAP_UIDPLUS_RESP_CODE_APND, resp_code_apnd);
    if (ext == NULL) {
      mailimap_uidplus_resp_code_apnd_free(resp_code_apnd);
      return MAILIMAP_ERROR_MEMORY;
    }
    
    * index = cur_token;
    * result = ext;
    
    return MAILIMAP_NO_ERROR;
  }
  
  r = mailimap_resp_code_copy_parse(fd, buffer, &cur_token, &resp_code_copy);
  if (r == MAILIMAP_NO_ERROR) {
    ext = mailimap_extension_data_new(&mailimap_extension_uidplus,
        MAILIMAP_UIDPLUS_RESP_CODE_COPY, resp_code_copy);
    if (ext == NULL) {
      mailimap_uidplus_resp_code_copy_free(resp_code_copy);
      return MAILIMAP_ERROR_MEMORY;
    }
    
    * index = cur_token;
    * result = ext;
    
    return MAILIMAP_NO_ERROR;
  }
  
  r = mailimap_uidplus_uidnotsticky_parse(fd, buffer, &cur_token);
  if (r == MAILIMAP_NO_ERROR) {
    ext = mailimap_extension_data_new(&mailimap_extension_uidplus,
        MAILIMAP_UIDPLUS_RESP_CODE_UIDNOTSTICKY, resp_code_copy);
    if (ext == NULL) {
      mailimap_uidplus_resp_code_copy_free(resp_code_copy);
      return MAILIMAP_ERROR_MEMORY;
    }
    
    * index = cur_token;
    * result = ext;
    
    return MAILIMAP_NO_ERROR;
  }
  
  return MAILIMAP_ERROR_PARSE;
}

