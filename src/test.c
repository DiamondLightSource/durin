#include <hdf5.h>
#include <stdio.h>
#include <stdlib.h>

#include "err.h"
#include "file.h"

#define COPY_AND_MASK(in, out, size, mask)                                     \
  {                                                                            \
    int i;                                                                     \
    if (mask) {                                                                \
      for (i = 0; i < size; ++i) {                                             \
        out[i] = in[i];                                                        \
        if (mask[i] & 0xFE)                                                    \
          out[i] = -2;                                                         \
        if (mask[i] & 0x01)                                                    \
          out[i] = -1;                                                         \
      }                                                                        \
    } else {                                                                   \
      for (i = 0; i < size; i++) {                                             \
        out[i] = in[i];                                                        \
      }                                                                        \
    }                                                                          \
  }

int parse_args(int argc, char **argv, char **file_name, int *frame_idx) {
  int retval = 0;
  if (argc == 2) {
    *frame_idx = 0;
    *file_name = argv[1];
  } else if (argc >= 2) {
    *file_name = argv[1];
    *frame_idx = atoi(argv[2]);
  } else {
    ERROR_JUMP(-1, done, "Require filename argument");
  }
done:
  return retval;
}

int main(int argc, char **argv) {
  int err = 0;
  int retval = 0;
  char *test_file = "";
  struct ds_desc_t *desc;
  int dims[3] = {0};
  hid_t fid = 0;
  int frame_idx = 0;
  int *mask = NULL;
  int *data = NULL;
  void *buffer = NULL;

  init_error_handling();
  if (init_h5_error_handling() < 0) {
    ERROR_JUMP(-1, done, "");
  }

  if (parse_args(argc, argv, &test_file, &frame_idx) < 0) {
    ERROR_JUMP(-1, done, "Failure parsing arguments");
  }

  fid = H5Fopen(test_file, H5F_ACC_RDONLY, H5P_DEFAULT);
  if (fid < 0)
    ERROR_JUMP(-1, done, "Error opening file");

  err = get_detector_info(fid, &desc);
  if (err < 0) {
    ERROR_JUMP(err, done, "");
  }
  dims[0] = desc->dims[0];
  dims[1] = desc->dims[1];
  dims[2] = desc->dims[2];

  printf("Dims: %d, %d, %d\n", dims[0], dims[1], dims[2]);

  mask = malloc(dims[1] * dims[2] * sizeof(*mask));
  if (!mask) {
    ERROR_JUMP(err, done, "Failed to allocate space for pixel mask");
  }
  err = desc->get_pixel_mask(desc, mask);
  if (err < 0) {
    ERROR_JUMP(err, done, "");
  }

  data = malloc(dims[1] * dims[2] * sizeof(*data));
  if (sizeof(*data) != desc->data_width) {
    buffer = malloc(dims[1] * dims[2] * desc->data_width);
  } else {
    buffer = data;
  }

  err = desc->get_data_frame(desc, frame_idx, buffer);
  if (err < 0) {
    ERROR_JUMP(err, done, "");
  }

  if (buffer != data) {
    if (desc->data_width == sizeof(signed char)) {
      signed char *in = buffer;
      COPY_AND_MASK(in, data, dims[1] * dims[2], mask);
    } else if (desc->data_width == sizeof(short)) {
      short *in = buffer;
      COPY_AND_MASK(in, data, dims[1] * dims[2], mask);
    } else if (desc->data_width == sizeof(int)) {
      int *in = buffer;
      COPY_AND_MASK(in, data, dims[1] * dims[2], mask);
    } else if (desc->data_width == sizeof(long int)) {
      long int *in = buffer;
      COPY_AND_MASK(in, data, dims[1] * dims[2], mask);
    } else if (desc->data_width == sizeof(long long int)) {
      long long int *in = buffer;
      COPY_AND_MASK(in, data, dims[1] * dims[2], mask);
    }
  }

  {
    int i, j;
    int max_i = 30;
    int max_j = 10;
    max_j = max_j < dims[1] ? max_j : dims[1];
    max_i = max_i < dims[2] ? max_i : dims[2];
    for (j = 0; j < max_j; j++) {
      for (i = 0; i < max_i; i++) {
        printf("%3d ", data[i + j * dims[2]]);
      }
      printf("\n");
    }
  }

done:
  if (fid > 0)
    H5Fclose(fid);
  if (data)
    free(data);
  if (buffer && (data != buffer))
    free(buffer);
  if (mask)
    free(mask);
  if (retval != 0)
    dump_error_stack(stderr);
  return retval;
}
