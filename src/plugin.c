/*
 * Copyright (c) 2018 Diamond Light Source Ltd.
 * Author: Charles Mita
 */


#include <hdf5.h>
#include <stdlib.h>
#include "file.h"
#include "plugin.h"


static hid_t file_id = 0;
static struct data_description_t data_desc = {0};
static struct dataset_properties_t ds_prop = {0};
static int *mask_buffer = NULL;


void apply_mask(int *data, int *mask, int size) {
	int *dptr, *mptr;
	dptr = data;
	mptr = mask;
	while (dptr < data + size && mptr < mask + size) {
		/* mask bits loosely based on what Neggia does and what NeXus says should be done */
		/* basically - anything in the low byte (& 0xFF) means "ignore this" */
		if (*mptr & 0x01) *dptr = -1;
		if (*mptr & 0xFE) *dptr = -2;
		dptr++;
		mptr++;
	}
}


#ifdef __cplusplus
extern "C" {
#endif

void plugin_open(
		const char *filename,
		int info[1024],
		int *error_flag) {
	int err = 0;
	info[0] = DLS_CUSTOMER_ID;
	info[1] = VERSION_MAJOR;
	info[2] = VERSION_MINOR;
	info[3] = VERSION_PATCH;
	info[4] = VERSION_TIMESTAMP;
	file_id = H5Fopen(filename, H5F_ACC_RDONLY, H5P_DEFAULT);
	if (file_id < 0) {
		/* TODO: backtrace */
		*error_flag = -4;
		return;
	}

	err = extract_detector_info(file_id, &data_desc, &ds_prop);
	if (err < 0) {
		*error_flag = -4;
		return;
	}

	mask_buffer = malloc(ds_prop.dims[1] * ds_prop.dims[2] * sizeof(int));
	if (mask_buffer) {
		err = data_desc.get_pixel_mask(&data_desc, mask_buffer);
		if (err < 0) {
			fprintf(stderr, "WARNING: Could not read pixel mask - no masking will be applied\n");
			free(mask_buffer);
			mask_buffer = NULL;
		}
	}

	*error_flag = 0;
}


void plugin_get_header(
		int *nx, int *ny,
		int *nbytes,
		float *qx, float *qy,
		int *number_of_frames,
		int info[1024],
		int *error_flag) {
	int err = 0;
	double x_pixel_size, y_pixel_size;

	err = data_desc.get_pixel_properties(&data_desc, &x_pixel_size, &y_pixel_size);
	if (err < 0) {
		*error_flag = -4;
		return;
	}

	*nx = ds_prop.dims[2];
	*ny = ds_prop.dims[1];
	*nbytes = ds_prop.dims[1] * ds_prop.dims[2] * ds_prop.data_width;
	*number_of_frames = ds_prop.dims[0];
	*qx = (float) x_pixel_size;
	*qy = (float) y_pixel_size;
	*error_flag = 0;
}


void plugin_get_data(
		int *frame_number,
		int *nx, int *ny,
		int *data_array,
		int info[1024],
		int *error_flag) {
	int err = 0;
	err = data_desc.get_data_frame(&data_desc, &ds_prop, *frame_number, sizeof(int), data_array);
	if (err < 0) {
		*error_flag = -2;
		return;
	}
	if (mask_buffer) {
		apply_mask(data_array, mask_buffer, ds_prop.dims[1] * ds_prop.dims[2]);
	}
	*error_flag = 0;
	return;
}


void plugin_close(int *error_flag) {
	if (file_id) {
		herr_t err = H5Fclose(file_id);
		if (err) {
			/* TODO: backtrace */
			*error_flag = -1;
		}
	}
	file_id = 0;

	if (mask_buffer) free(mask_buffer);
	if (data_desc.free_extra) data_desc.free_extra(&data_desc);
	free_dataset_properties(&ds_prop);
}

#ifdef __cplusplus
} /* extern "C" */
#endif
