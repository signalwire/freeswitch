/*
 * This file is part of the Sofia-SIP package
 *
 * Copyright (C) 2005 Nokia Corporation.
 *
 * Contact: Pekka Pessi <pekka.pessi@nokia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#include <openssl/rand.h>

int main(int argc, char **argv)
{
  int nb;

  if (argc != 2) {
    printf("Usage: tport_rand <output seed file>\n");
    exit(0);
  }

  printf("Please move mouse for until the application stops\n");

  RAND_load_file("/dev/random", 1024);
  RAND_write_file(argv[1]);

  nb = RAND_load_file(argv[1], -1);

  printf("Wrote %d bytes to the seed file\n", nb);

  return 0;
}
