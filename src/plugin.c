/*
 * Copyright (c) 2018 Diamond Light Source Ltd.
 * Author: Charles Mita
 */

#include <hdf5.h>
#include <stdlib.h>

#include "file.h"
#include "filters.h"
#include "plugin.h"

/* XDS does not provide an error callback facility, so just write to stderr
   for now - generally regarded as poor practice */
#define ERROR_OUTPUT stderr

/* mask bits loosely based on what Neggia does and what NeXus says should be
   done basically - anything in the low byte (& 0xFF) means "ignore this"
   Neggia uses the value -2 if bit 1, 2 or 3 are set */
/* CV-GPhL-20210408: we want more control over the value non-masked
   pixels should be set to. */
#define COPY_AND_MASK(in, value, setValue, out, size, mask)                    \
  {                                                                            \
    int i;                                                                     \
    if (value!=0) {                                                            \
      if (mask) {                                                              \
        for (i = 0; i < size; ++i) {                                           \
          out[i] = (in[i] == value) ? setValue : in[i];                        \
          if (mask[i] & 0xFF)                                                  \
            out[i] = -1;                                                       \
          if (mask[i] & 30)                                                    \
            out[i] = -2;                                                       \
        }                                                                      \
      } else {                                                                 \
        for (i = 0; i < size; i++) {                                           \
          out[i] = (in[i] == value) ? setValue : in[i];                        \
        }                                                                      \
      }                                                                        \
    } else {                                                                   \
      if (mask) {                                                              \
        for (i = 0; i < size; ++i) {                                           \
          out[i] = in[i];                                                      \
          if (mask[i] & 0xFF)                                                  \
            out[i] = -1;                                                       \
          if (mask[i] & 30)                                                    \
            out[i] = -2;                                                       \
        }                                                                      \
      } else {                                                                 \
        for (i = 0; i < size; i++) {                                           \
          out[i] = in[i];                                                      \
        }                                                                      \
      }                                                                        \
    }                                                                          \
  }
#define APPLY_MASK(buffer, mask, size)                                         \
  {                                                                            \
    int i;                                                                     \
    if (mask) {                                                                \
      for (i = 0; i < size; ++i) {                                             \
        if (mask[i] & 0xFF)                                                    \
          buffer[i] = -1;                                                      \
        if (mask[i] & 30)                                                      \
          buffer[i] = -2;                                                      \
      }                                                                        \
    }                                                                          \
  }

static hid_t file_id = 0;
static struct ds_desc_t *data_desc = NULL;
static int *mask_buffer = NULL;

void fill_info_array(int info[1024]) {
  info[0] = DLS_CUSTOMER_ID;
  info[1] = VERSION_MAJOR;
  info[2] = VERSION_MINOR;
  info[3] = VERSION_PATCH;
  info[4] = VERSION_TIMESTAMP;
  info[5] = 0; // image number offset
  info[6] = -1; // marked pixels not already in pixel_mask: reset to this value

  char *cenv;
  cenv = getenv("DURIN_IMAGE_NUMBER_OFFSET");
  if (cenv!=NULL) {
    info[5] = atoi(cenv);
  }
  cenv = getenv("DURIN_RESET_UNMASKED_PIXEL");
  if (cenv!=NULL) {
    info[6] = atoi(cenv);
  }

}

int convert_to_int_and_mask(void *in_buffer, int width, int setValue, int *out_buffer,
                            int length, int *mask) {
  /* transfer data to output buffer, performing data conversion as required */
  int retval = 0;
  /* TODO: decide how conversion of data should work */
  /* Should we sign extend? Neggia doesn't (casts from uint*), but may be more
   * intuitive */

  int d_width = abs(width);

  // CV-20210407
  //   Dealing with a signed data array: no extra check for marker
  //   value needed (since data can already take advantage of the
  //   negative data range). It is unclear though why/when data would
  //   come in as a signed array in the first place ...
  if (width<0) {
    if (d_width == sizeof(signed char)) {
      // 8-bit
      signed char *in = in_buffer;
      COPY_AND_MASK(in, 0, setValue, out_buffer, length, mask);
    } else if (d_width == sizeof(short)) {
      // 16-bit
      short *in = in_buffer;
      COPY_AND_MASK(in, 0, setValue, out_buffer, length, mask);
    } else if (d_width == sizeof(int)) {
      // 16-bit
      int *in = in_buffer;
      COPY_AND_MASK(in, 0, setValue, out_buffer, length, mask);
    } else if (d_width == sizeof(long int)) {
      // 32-bit
      long int *in = in_buffer;
      COPY_AND_MASK(in, 0, setValue, out_buffer, length, mask);
    } else if (d_width == sizeof(long long int)) {
      // 64-bit
      long long int *in = in_buffer;
      COPY_AND_MASK(in, 0, setValue, out_buffer, length, mask);
    } else {
      char message[128];
      sprintf(message, "Unsupported conversion of data width %d to %ld (int)",
              d_width, sizeof(int));
      ERROR_JUMP(-1, done, message);
    }
  }
  // CV-20210407
  //   Dealing with an unsigned data array: extra check for marker
  //   value required (to handle overloaded pixels correctly if wanted
  //   - see also DURIN_RESET_UNMASKED_PIXEL environment variable).
  else {
    if (d_width == sizeof(unsigned char)) {
      // 8-bit
      unsigned char *in = in_buffer;
      COPY_AND_MASK(in, UINT8_MAX, setValue, out_buffer, length, mask);
    } else if (d_width == sizeof(unsigned short)) {
      // 16-bit
      unsigned short *in = in_buffer;
      COPY_AND_MASK(in, UINT16_MAX, setValue, out_buffer, length, mask);
    } else if (d_width == sizeof(unsigned int)) {
      // 16-bit
      unsigned int *in = in_buffer;
      COPY_AND_MASK(in, UINT16_MAX, setValue, out_buffer, length, mask);
    } else if (d_width == sizeof(unsigned long)) {
      // 32-bit
      unsigned long *in = in_buffer;
      COPY_AND_MASK(in, UINT32_MAX, setValue, out_buffer, length, mask);
    } else if (d_width == sizeof(unsigned long long)) {
      // 64-bit
      unsigned long long *in = in_buffer;
      COPY_AND_MASK(in, UINT32_MAX, setValue, out_buffer, length, mask);
    } else {
      char message[128];
      sprintf(message, "Unsupported conversion of data width %d to %ld (int)",
              d_width, sizeof(int));
      ERROR_JUMP(-1, done, message);
    }
  }

done:
  return retval;
}

#ifdef __cplusplus
extern "C" {
#endif

void plugin_open(const char *filename, int info[1024], int *error_flag) {
  int retval = 0;
  *error_flag = 0;

  init_error_handling();

  if (H5dont_atexit() < 0) {
    ERROR_JUMP(-2, done, "Failed configuring HDF5 library behaviour");
  }

  if (init_h5_error_handling() < 0) {
    ERROR_JUMP(-2, done, "Failed to configure HDF5 error handling");
  }

  fill_info_array(info);
  file_id = H5Fopen(filename, H5F_ACC_RDONLY, H5P_DEFAULT);
  if (file_id < 0) {
    char message[128] = {0};
    sprintf(message, "Could not open %.100s", filename);
    ERROR_JUMP(-4, done, message);
  }

  reset_error_stack();
  retval = get_detector_info(file_id, &data_desc);
  if (retval < 0) {
    ERROR_JUMP(-4, done, "");
  }

  data_desc->image_number_offset = info[5];

  mask_buffer = malloc(data_desc->dims[1] * data_desc->dims[2] * sizeof(int));
  if (mask_buffer) {
    retval = data_desc->get_pixel_mask(data_desc, mask_buffer);
    if (retval < 0) {
      fprintf(
          ERROR_OUTPUT,
          "WARNING: Could not read pixel mask - no masking will be applied\n");
      dump_error_stack(ERROR_OUTPUT);
      free(mask_buffer);
      mask_buffer = NULL;
    }
  }
  retval = 0;

done:
  *error_flag = retval;
  if (retval < 0) {
    if ((data_desc) && (data_desc->free_desc)) {
      data_desc->free_desc(data_desc);
      data_desc = NULL;
    }
    dump_error_stack(ERROR_OUTPUT);
  }
}

void plugin_get_header(int *nx, int *ny, int *nbytes, float *qx, float *qy,
                       int *number_of_frames, int info[1024], int *error_flag) {
  int err = 0;
  int retval = 0;
  double x_pixel_size, y_pixel_size;
  reset_error_stack();
  fill_info_array(info);

  err =
      data_desc->get_pixel_properties(data_desc, &x_pixel_size, &y_pixel_size);
  if (err < 0) {
    ERROR_JUMP(err, done, "Failed to retrieve pixel information");
  }

  *nx = data_desc->dims[2];
  *ny = data_desc->dims[1];
  *nbytes = abs(data_desc->data_width);
  *number_of_frames = data_desc->dims[0];
  *qx = (float)x_pixel_size;
  *qy = (float)y_pixel_size;

done:
  *error_flag = retval;
  if (retval < 0) {
    dump_error_stack(ERROR_OUTPUT);
  }
}

void plugin_get_data(int *frame_number, int *nx, int *ny, int *data_array,
                     int info[1024], int *error_flag) {

  int retval = 0;
  int frame_size_px = data_desc->dims[1] * data_desc->dims[2];
  reset_error_stack();
  fill_info_array(info);

  void *buffer = NULL;
  if (sizeof(*data_array) == abs(data_desc->data_width)) {
    buffer = data_array;
  } else {
    buffer = malloc(abs(data_desc->data_width) * frame_size_px);
    if (!buffer) {
      ERROR_JUMP(-1, done, "Unable to allocate data buffer");
    }
  }

  if (data_desc->get_data_frame(data_desc, (*frame_number) - 1, buffer) < 0) {
    char message[64] = {0};
    sprintf(message, "Failed to retrieve data for frame %d", *frame_number);
    ERROR_JUMP(-2, done, message);
  }

  if (buffer != data_array) {
    if (convert_to_int_and_mask(buffer, data_desc->data_width, info[6], data_array,
                                frame_size_px, mask_buffer) < 0) {
      char message[64];
      sprintf(message, "Error converting data for frame %d", *frame_number);
      ERROR_JUMP(-2, done, message);
    }
  } else {
    APPLY_MASK(data_array, mask_buffer, frame_size_px);
  }

done:
  *error_flag = retval;
  if (retval < 0) {
    dump_error_stack(ERROR_OUTPUT);
  }
  if (buffer && (buffer != data_array))
    free(buffer);
}

void plugin_close(int *error_flag) {
  if (file_id) {
    if (H5Fclose(file_id) < 0) {
      /* TODO: backtrace */
      *error_flag = -1;
    }
  }
  file_id = 0;

  if (mask_buffer) {
    free(mask_buffer);
    mask_buffer = NULL;
  }
  if (data_desc->free_desc) {
    data_desc->free_desc(data_desc);
    data_desc = NULL;
  }
  if (H5close() < 0) {
    *error_flag = -1;
  }
}

#ifdef __cplusplus
} /* extern "C" */
#endif
