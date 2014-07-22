/*
 * Copyright (c) 2014 Travis Cross <tc@traviscross.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "trie.h"

struct bit_trie_node* bit_trie_create() {
  struct bit_trie_node *node = malloc(sizeof(struct bit_trie_node));
  if (!node) abort();
  memset(node, 0, sizeof(struct bit_trie_node));
  return node;
}

uint32_t bit_trie_get(struct bit_trie_node **node_out,
                      struct bit_trie_node *node,
                      unsigned char *key,
                      uint32_t key_len) {
  unsigned char *keyp = key, *keyp0 = key;
  struct bit_trie_node *node0 = node;
  while(keyp < key + key_len) {
    uint8_t offset = 0;
    while(offset < 8) {
      uint8_t bit = (*keyp >> offset) & 0x01;
      struct bit_trie_node *next = node->next[bit];
      if (next) {
        node = next;
      } else {
        *node_out = node0;
        return keyp0 - key;
      }
      offset++;
    }
    keyp++;
    if (node->value) {
      node0 = node;
      keyp0 = keyp;
    }
  }
  *node_out = node0;
  return keyp0 - key;
}

struct bit_trie_node* bit_trie_set(struct bit_trie_node *node,
                                   unsigned char *key,
                                   uint32_t key_len,
                                   void *value,
                                   uint32_t value_len) {
  unsigned char *keyp = key;
  while(keyp < key + key_len) {
    uint8_t offset = 0;
    while(offset < 8) {
      uint8_t bit = (*keyp >> offset) & 0x01;
      struct bit_trie_node *next = node->next[bit];
      if (next) {
        node = next;
      } else {
        next = bit_trie_create();
        node->next[bit] = next;
        node = next;
      }
      offset++;
    }
    keyp++;
  }
  node->value = value;
  node->value_len = value_len;
  return node;
}

static uint32_t bit_trie_free_r(struct bit_trie_node *node) {
  uint32_t count = 0;
  if (!node) return count;
  count += bit_trie_free_r(node->next[0]);
  count += bit_trie_free_r(node->next[1]);
  free(node->value);
  free(node);
  count++;
  return count;
}

uint32_t bit_trie_free(struct bit_trie_node *node) {
  return bit_trie_free_r(node);
}

uint32_t bit_trie_byte_size(struct bit_trie_node *node) {
  uint32_t size = 0;
  if (!node) return size;
  size += bit_trie_byte_size(node->next[0]);
  size += bit_trie_byte_size(node->next[1]);
  size += sizeof(struct bit_trie_node) + node->value_len;
  return size;
}
