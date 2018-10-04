/*
 * Copyright (c) 2018 Diamond Light Source Ltd.
 * Author: Charles Mita
 */


#include <hdf5.h>
#include <stdlib.h>
#include "file.h"
#include "plugin.h"


/* XDS does not provide an error callback facility, so just write to stderr for now */
/* generally regarded as poor practice */
#define ERROR_OUTPUT stderr


static hid_t file_id = 0;
static struct data_description_t data_desc = {0};
static struct dataset_properties_t ds_prop = {0};
static int *mask_buffer = NULL;


void fill_info_array(int info[1024]) {
	info[0] = DLS_CUSTOMER_ID;
	info[1] = VERSION_MAJOR;
	info[2] = VERSION_MINOR;
	info[3] = VERSION_PATCH;
	info[4] = VERSION_TIMESTAMP;
}

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
	retval = extract_detector_info(file_id, &data_desc, &ds_prop);
	if (retval < 0) {
		ERROR_JUMP(-4, done, "");
	}

	mask_buffer = malloc(ds_prop.dims[1] * ds_prop.dims[2] * sizeof(int));
	if (mask_buffer) {
		retval = data_desc.get_pixel_mask(&data_desc, mask_buffer);
		if (retval < 0) {
			fprintf(ERROR_OUTPUT, "WARNING: Could not read pixel mask - no masking will be applied\n");
			dump_error_stack(ERROR_OUTPUT);
			free(mask_buffer);
			mask_buffer = NULL;
		}
	}
	retval = 0;

done:
	*error_flag = retval;
	if (retval < 0) {
		dump_error_stack(ERROR_OUTPUT);
	}
}


void plugin_get_header(
		int *nx, int *ny,
		int *nbytes,
		float *qx, float *qy,
		int *number_of_frames,
		int info[1024],
		int *error_flag) {
	int err = 0;
	int retval = 0;
	double x_pixel_size, y_pixel_size;
	reset_error_stack();
	fill_info_array(info);

	err = data_desc.get_pixel_properties(&data_desc, &x_pixel_size, &y_pixel_size);
	if (err < 0) {
		ERROR_JUMP(err, done, "Failed to retrieve pixel information");
	}

	*nx = ds_prop.dims[2];
	*ny = ds_prop.dims[1];
	*nbytes = ds_prop.data_width;
	*number_of_frames = ds_prop.dims[0];
	*qx = (float) x_pixel_size;
	*qy = (float) y_pixel_size;

done:
	*error_flag = retval;
	if (retval < 0) {
		dump_error_stack(ERROR_OUTPUT);
	}
}


void plugin_get_data(
		int *frame_number,
		int *nx, int *ny,
		int *data_array,
		int info[1024],
		int *error_flag) {
	int retval = 0;
	reset_error_stack();
	fill_info_array(info);
	if (data_desc.get_data_frame(&data_desc, &ds_prop, (*frame_number) - 1, sizeof(int), data_array) < 0) {
		char message[64] = {0};
		sprintf(message, "Failed to retrieve data for frame %d", *frame_number);
		ERROR_JUMP(-2, done, message);
	}

	// nasty hack 
	for (int ij = 0; ij < nx * ny; ij++) {
	  if (data_array[ij] == 0xffff) {
	    data_array[ij] = -2;
	  }
	}
	  
	
	if (mask_buffer) {
		apply_mask(data_array, mask_buffer, ds_prop.dims[1] * ds_prop.dims[2]);
	}

done:
	*error_flag = retval;
	if (retval < 0) {
		dump_error_stack(ERROR_OUTPUT);
	}
}


void plugin_close(int *error_flag) {
	if (file_id) {
		if (H5Fclose(file_id) < 0) {
			/* TODO: backtrace */
			*error_flag = -1;
		}
	}
	file_id = 0;

	if (mask_buffer) free(mask_buffer);
	if (data_desc.free_extra) data_desc.free_extra(&data_desc);
	if (H5close() < 0) {
		*error_flag = -1;
	}
}

#ifdef __cplusplus
} /* extern "C" */
#endif
