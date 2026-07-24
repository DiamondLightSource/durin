/*
 * Copyright (c) 2018 Diamond Light Source Ltd.
 * Author: Charles Mita
 */

#ifndef NXS_XDS_FILE_H
#define NXS_XDS_FILE_H

#include "err.h"
#include "filters.h"
#include <hdf5.h>

struct ds_desc_t {
  hid_t det_g_id;
  hid_t data_g_id;
  hsize_t dims[3];
  int data_width;
  int image_number_offset;
  int (*get_pixel_properties)(const struct ds_desc_t *, double *, double *);
  int (*get_pixel_mask)(const struct ds_desc_t *, int *);
  int (*get_data_frame)(const struct ds_desc_t *, const int, void *);
  void (*free_desc)(struct ds_desc_t *);
  int i2i[]; // array to hold a translation from the image number requested by XDS and the actual position in the HDF5 file
};

struct nxs_ds_desc_t {
  struct ds_desc_t base;
};

struct eiger_ds_desc_t {
  struct ds_desc_t base;
  int n_data_blocks;
  int *block_sizes;
  int (*frame_func)(const struct ds_desc_t *, const char *, const hsize_t *,
                    const hsize_t *, void *);
};

struct opt_eiger_ds_desc_t {
  struct eiger_ds_desc_t base;
  int bs_applied;
  unsigned int bs_params[BS_H5_N_PARAMS];
};

int get_detector_info(const hid_t fid, struct ds_desc_t **desc);

struct det_visit_objects_t {
  hid_t nxdata;
  hid_t nxdetector;
};

#endif /* NXS_XDS_FILE_H */
