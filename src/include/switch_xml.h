/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005/2006, Anthony Minessale II <anthmct@yahoo.com>
 *
 * Version: MPL 1.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 *
 * The Initial Developer of the Original Code is
 * Anthony Minessale II <anthmct@yahoo.com>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * Anthony Minessale II <anthmct@yahoo.com>
 *
 *
 * switch_xml.h -- XML PARSER
 *
 * Derived from EZXML http://ezxml.sourceforge.net
 * Original Copyright
 *
 * Copyright 2004, 2005 Aaron Voisine <aaron@voisine.org>
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef _SWITCH_XML_H
#define _SWITCH_XML_H
#include <switch_types.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <fcntl.h>

BEGIN_EXTERN_C

#define SWITCH_XML_BUFSIZE 1024 // size of internal memory buffers

typedef enum {
	SWITCH_XML_ROOT =	   (1 << 0), // root
	SWITCH_XML_NAMEM =  (1 << 1), // name is malloced
	SWITCH_XML_TXTM =   (1 << 2), // txt is malloced
	SWITCH_XML_DUP   =  (1 << 3) // attribute name and value are strduped
} switch_xml_flag_t;


struct switch_xml {
    char *name;      // tag name
    char **attr;     // tag attributes { name, value, name, value, ... NULL }
    char *txt;       // tag character content, empty string if none
    switch_size_t off;      // tag offset from start of parent tag character content
    switch_xml_t next;    // next tag with same name in this section at this depth
    switch_xml_t sibling; // next tag with different name in same section and depth
    switch_xml_t ordered; // next tag, same section and depth, in original order
    switch_xml_t child;   // head of sub tag list, NULL if none
    switch_xml_t parent;  // parent tag, NULL if current tag is root tag
    uint32_t flags;     // additional information
};

// Given a string of xml data and its length, parses it and creates an switch_xml
// structure. For efficiency, modifies the data by adding null terminators
// and decoding ampersand sequences. If you don't want this, copy the data and
// pass in the copy. Returns NULL on failure.
SWITCH_DECLARE(switch_xml_t) switch_xml_parse_str(char *s, switch_size_t len);

// A wrapper for switch_xml_parse_str() that accepts a file descriptor. First
// attempts to mem map the file. Failing that, reads the file into memory.
// Returns NULL on failure.
SWITCH_DECLARE(switch_xml_t) switch_xml_parse_fd(int fd);

// a wrapper for switch_xml_parse_fd() that accepts a file name
SWITCH_DECLARE(switch_xml_t) switch_xml_parse_file(const char *file);
    
// Wrapper for switch_xml_parse_str() that accepts a file stream. Reads the entire
// stream into memory and then parses it. For xml files, use switch_xml_parse_file()
// or switch_xml_parse_fd()
SWITCH_DECLARE(switch_xml_t) switch_xml_parse_fp(FILE *fp);

// returns the first child tag (one level deeper) with the given name or NULL
// if not found
SWITCH_DECLARE(switch_xml_t) switch_xml_child(switch_xml_t xml, const char *name);

SWITCH_DECLARE(switch_xml_t) switch_xml_find_child(switch_xml_t node, char *childname, char *attrname, char *value);

// returns the next tag of the same name in the same section and depth or NULL
// if not found
#define switch_xml_next(xml) ((xml) ? xml->next : NULL)

// Returns the Nth tag with the same name in the same section at the same depth
// or NULL if not found. An index of 0 returns the tag given.
switch_xml_t switch_xml_idx(switch_xml_t xml, int idx);

// returns the name of the given tag
#define switch_xml_name(xml) ((xml) ? xml->name : NULL)

// returns the given tag's character content or empty string if none
#define switch_xml_txt(xml) ((xml) ? xml->txt : "")

// returns the value of the requested tag attribute, or NULL if not found
SWITCH_DECLARE(const char *) switch_xml_attr(switch_xml_t xml, const char *attr);

// Traverses the switch_xml sturcture to retrieve a specific subtag. Takes a
// variable length list of tag names and indexes. The argument list must be
// terminated by either an index of -1 or an empty string tag name. Example: 
// title = switch_xml_get(library, "shelf", 0, "book", 2, "title", -1);
// This retrieves the title of the 3rd book on the 1st shelf of library.
// Returns NULL if not found.
SWITCH_DECLARE(switch_xml_t) switch_xml_get(switch_xml_t xml, ...);

// Converts an switch_xml structure back to xml. Returns a string of xml data that
// must be freed.
SWITCH_DECLARE(char *) switch_xml_toxml(switch_xml_t xml);

// returns a NULL terminated array of processing instructions for the given
// target
SWITCH_DECLARE(const char **) switch_xml_pi(switch_xml_t xml, const char *target);

// frees the memory allocated for an switch_xml structure
SWITCH_DECLARE(void) switch_xml_free(switch_xml_t xml);
    
// returns parser error message or empty string if none
SWITCH_DECLARE(const char *) switch_xml_error(switch_xml_t xml);

// returns a new empty switch_xml structure with the given root tag name
SWITCH_DECLARE(switch_xml_t) switch_xml_new(const char *name);

// wrapper for switch_xml_new() that strdup()s name
#define switch_xml_new_d(name) switch_xml_set_flag(switch_xml_new(strdup(name)), SWITCH_XML_NAMEM)

// Adds a child tag. off is the offset of the child tag relative to the start
// of the parent tag's character content. Returns the child tag.
switch_xml_t switch_xml_add_child(switch_xml_t xml, const char *name, switch_size_t off);

// wrapper for switch_xml_add_child() that strdup()s name
#define switch_xml_add_child_d(xml, name, off) \
    switch_xml_set_flag(switch_xml_add_child(xml, strdup(name), off), SWITCH_XML_NAMEM)

// sets the character content for the given tag and returns the tag
switch_xml_t switch_xml_set_txt(switch_xml_t xml, const char *txt);

// wrapper for switch_xml_set_txt() that strdup()s txt
#define switch_xml_set_txt_d(xml, txt) \
    switch_xml_set_flag(switch_xml_set_txt(xml, strdup(txt)), SWITCH_XML_TXTM)

// Sets the given tag attribute or adds a new attribute if not found. A value
// of NULL will remove the specified attribute.
SWITCH_DECLARE(void) switch_xml_set_attr(switch_xml_t xml, const char *name, const char *value);

// Wrapper for switch_xml_set_attr() that strdup()s name/value. Value cannot be NULL
#define switch_xml_set_attr_d(xml, name, value) \
    switch_xml_set_attr(switch_xml_set_flag(xml, SWITCH_XML_DUP), strdup(name), strdup(value))

// sets a flag for the given tag and returns the tag
SWITCH_DECLARE(switch_xml_t) switch_xml_set_flag(switch_xml_t xml, switch_xml_flag_t flag);


// removes a tag along with all its subtags
SWITCH_DECLARE(void) switch_xml_remove(switch_xml_t xml);

SWITCH_DECLARE(switch_xml_t) switch_xml_open_root(uint8_t reload);
SWITCH_DECLARE(switch_status_t) switch_xml_init(switch_memory_pool_t *pool);
SWITCH_DECLARE(switch_status_t) switch_xml_destroy(void);
SWITCH_DECLARE(switch_xml_t) switch_xml_root(void);

SWITCH_DECLARE(switch_status_t) switch_xml_locate(char *section,
												  char *tag_name,
												  char *key_name,
												  char *key_value,
												  switch_xml_t *root,
												  switch_xml_t *node,
												  char *params);
												 
SWITCH_DECLARE(switch_xml_t) switch_xml_open_cfg(char *file_path, switch_xml_t *node, char *params);
SWITCH_DECLARE(switch_status_t) switch_xml_bind_search_function(switch_xml_search_function_t function);

END_EXTERN_C


#endif // _SWITCH_XML_H
