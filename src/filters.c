/*
 * Copyright (c) 2018 Diamond Light Source Ltd.
 * Author: Charles Mita
 */

#include <stdio.h>

#include "bitshuffle.h"
#include "err.h"
#include "filters.h"

/* Required prototypes from bitshuffle.c but not included in header */
uint64_t bshuf_read_uint64_BE(const void *buffer);
uint32_t bshuf_read_uint32_BE(const void *buffer);

/*
 * Derived from the h5 filter code from the bitshuffle project (not included
 * here)
 */
int bslz4_decompress(const unsigned int *bs_params, size_t in_size,
                     void *in_buffer, size_t out_size, void *out_buffer) {

  int retval = 0;
  size_t size, elem_size, block_size, u_bytes;

  elem_size = bs_params[2];
  u_bytes = bshuf_read_uint64_BE(in_buffer);

  if (u_bytes != out_size) {
    char message[64];
    sprintf(message, "Decompressed chunk is %lu bytes, expected %lu", u_bytes,
            out_size);
    ERROR_JUMP(-1, done, message);
  }

  block_size = bshuf_read_uint32_BE((const char *)in_buffer + 8) / elem_size;
  if (!block_size) {
    ERROR_JUMP(-1, done, "Read block bitshuffle lz4 block size as 0");
  }
  /* skip over header */
  in_buffer += 12;
  size = u_bytes / elem_size;

  if (bs_params[4] == BS_H5_PARAM_LZ4_COMPRESS) {
    if (bshuf_decompress_lz4(in_buffer, out_buffer, size, elem_size,
                             block_size) < 0) {
      ERROR_JUMP(-1, done, "Error performing bitshuffle_lz4 decompression");
    }
  } else {
    if (bshuf_bitunshuffle(in_buffer, out_buffer, size, elem_size, block_size) <
        0) {
      ERROR_JUMP(-1, done, "Error performing bit unshuffle");
    }
  }

done:
  return retval;
}
