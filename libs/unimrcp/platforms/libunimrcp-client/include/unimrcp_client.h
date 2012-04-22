/*
 * Copyright 2008-2010 Arsen Chaloyan
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 * 
 * $Id: unimrcp_client.h 1474 2010-02-07 20:51:47Z achaloyan $
 */

#ifndef UNIMRCP_CLIENT_H
#define UNIMRCP_CLIENT_H

/**
 * @file unimrcp_client.h
 * @brief UniMRCP Client
 */ 

#include "mrcp_client.h"

APT_BEGIN_EXTERN_C

/**
 * Create UniMRCP client.
 * @param dir_layout the dir layout structure
 */
MRCP_DECLARE(mrcp_client_t*) unimrcp_client_create(apt_dir_layout_t *dir_layout);


APT_END_EXTERN_C

#endif /* UNIMRCP_CLIENT_H */
