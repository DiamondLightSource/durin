/*
 * Copyright (c) 2018 Diamond Light Source Ltd.
 * Author: Charles Mita
 */


#include <hdf5.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "file.h"
#include "err.h"

hid_t h5_int_type_from_width(const int width) {
	if (width == sizeof(char)) {
		return H5T_NATIVE_SCHAR;
	} else if (width == sizeof(short)) {
		return H5T_NATIVE_SHORT;
	} else if (width == sizeof(int)) {
		return H5T_NATIVE_INT;
	} else if (width == sizeof(long)) {
		return H5T_NATIVE_LONG;
	} else if (width == sizeof(long long)) {
		return H5T_NATIVE_LLONG;
	} else {
		/* TODO: error */
		return -1;
	}
}

void clear_det_visit_objects(struct det_visit_objects_t *objects) {
	if (objects->nxdata) {
		H5Oclose(objects->nxdata);
		objects->nxdata = 0;
	}
	if (objects->nxdetector) {
		H5Oclose(objects->nxdetector);
		objects->nxdetector = 0;
	}
}


void free_nxs_data_description(struct data_description_t *desc) {
	if (desc->extra) free(desc->extra); /* should just be NULL */
	desc->extra = NULL;
}


void free_eiger_data_description(struct data_description_t *desc) {
	if (!desc->extra) return;
	struct eiger_data_description_t *extra = desc->extra;
	if (extra->block_sizes) free(extra->block_sizes);
	free(extra);
	desc->extra = NULL;
}


double scale_from_units(const char* unit_string) {
	if (strcasecmp("m", unit_string) == 0 ||
			strcasecmp("metres", unit_string) == 0 ||
			strcasecmp("meters", unit_string) == 0) {
		return 1.;
	} else if (strcasecmp("cm", unit_string) == 0 ||
			strcasecmp("centimetres", unit_string) == 0 ||
			strcasecmp("centimeters", unit_string) == 0) {
		return 0.01;
	} else if (strcasecmp("mm", unit_string) == 0 ||
			strcasecmp("millimetres", unit_string) == 0 ||
			strcasecmp("millimeters", unit_string) == 0) {
		return 0.001;
	} else if (strcasecmp("um", unit_string) == 0 ||
			strcasecmp("microns", unit_string) == 0 ||
			strcasecmp("micrometres", unit_string) == 0 ||
			strcasecmp("micrometers", unit_string) == 0) {
		return 0.000001;
	} else {
		fprintf(stderr, "Unrecognised unit string %s", unit_string);
		return 1;
	}
}

int get_nxs_dataset_dims(const struct data_description_t *desc, struct dataset_properties_t *properties) {
	hid_t g_id, ds_id, s_id, t_id;
	int retval = 0;
	int ndims = 0;
	int width = 0;
	hsize_t dims[3] = {0};
	g_id = desc->data_group_id;;

	ds_id = H5Dopen2(g_id, "data", H5P_DEFAULT);
	if (ds_id <= 0) {
		ERROR_JUMP(-1, done, "Unable to open 'data' dataset");
	}

	t_id = H5Dget_type(ds_id);
	if (t_id <= 0) {
		ERROR_JUMP(-1, close_dataset, "Error getting datatype");
	}

	width = H5Tget_size(t_id);
	if (width <= 0) {
		ERROR_JUMP(-1, close_type, "Error getting type size");
	}

	s_id = H5Dget_space(ds_id);
	if (s_id <= 0) {
		ERROR_JUMP(-1, close_dataset, "Error getting dataspace");
	}

	ndims = H5Sget_simple_extent_ndims(s_id);
	if (ndims != 3) {
		char message[64];
		sprintf(message, "Dataset rank is %d, expected %d", ndims, 3);
		ERROR_JUMP(-1, close_space, message);
	}

	if (H5Sget_simple_extent_dims(s_id, dims, NULL) < 0) {
		ERROR_JUMP(-1, close_space, "Error getting dataset dimensions");
	}

	memcpy(properties->dims, dims, 3 * sizeof(*dims));
	properties->data_width = width;

close_space:
	H5Sclose(s_id);
close_type:
	H5Tclose(t_id);
close_dataset:
	H5Dclose(ds_id);
done:
	return retval;
}

int get_frame(hid_t g_id, const char* name, hsize_t *frame_idx, hsize_t *frame_size, int data_width, void *buffer) {
	int retval = 0;
	herr_t err = 0;
	hid_t ds_id, s_id, ms_id, t_id;
	ds_id = H5Dopen2(g_id, name, H5P_DEFAULT);
	if (ds_id <= 0) {
		char message[64];
		sprintf(message, "Unable to open dataset %.32s", name);
		ERROR_JUMP(-1, done, message);
	}
	s_id = H5Dget_space(ds_id);
	if (s_id <= 0) {
		ERROR_JUMP(-1, close_dataset, "Error getting dataspace");
	}
	err = H5Sselect_hyperslab(s_id, H5S_SELECT_SET, frame_idx, NULL, frame_size, NULL);
	if (err < 0) {
		ERROR_JUMP(-1, close_space, "Error seleting hyperslab");
	}
	ms_id = H5Screate_simple(3, frame_size, frame_size);
	if (ms_id < 0) {
		ERROR_JUMP(-1, close_space, "Could not create dataspace");
	}

	t_id = h5_int_type_from_width(data_width);
	if (t_id < 0) {
		char message[64];
		sprintf(message, "Could not infer signed integer from width %d", data_width);
		ERROR_JUMP(-1, close_mspace, message);
	}
	err = H5Dread(ds_id, t_id, ms_id, s_id, H5P_DEFAULT, buffer);
	if (err < 0) {
		ERROR_JUMP(-1, close_mspace, "Error reading dataset");
	}

close_mspace:
	H5Sclose(ms_id);
close_space:
	H5Sclose(s_id);
close_dataset:
	H5Dclose(ds_id);
done:
	return retval;
}

int get_nxs_frame(
		const struct data_description_t *desc,
		const struct dataset_properties_t *ds_prop,
		int n,
		int data_width,
		void *buffer) {
	/* detector data are the two inner most indices */
	/* n is indexed from one - hdf5 slices start at zero */
	/* TODO: handle ndims > 3 and select appropriately */
	int retval = 0;
	hsize_t frame_idx[3] = {n - 1, 0, 0};
	hsize_t frame_size[3] = {1, ds_prop->dims[1], ds_prop->dims[2]};
	retval = get_frame(desc->data_group_id, "data", frame_idx, frame_size, data_width, buffer);
	if (retval < 0) {
		ERROR_JUMP(retval, done, "");
	}
done:
	return retval;
}


int get_dectris_eiger_frame(
		const struct data_description_t *desc,
		const struct dataset_properties_t *ds_prop,
		int n,
		int data_width,
		void *buffer) {

	int retval = 0;
	int block, frame_count, idx;
	struct eiger_data_description_t *eiger_desc = desc->extra;
	char data_name[16] = {0};
	hsize_t frame_idx[3] = {0, 0, 0};
	hsize_t frame_size[3] = {1, ds_prop->dims[1], ds_prop->dims[2]};

	/* determine the relevant data block */
	frame_count = 0;
	block = 0;
	while ((frame_count += eiger_desc->block_sizes[block]) < (n-1)) block++;
	idx = n - (frame_count - eiger_desc->block_sizes[block]) - 1; /* index in current block */
	printf("n: %d -> Block: %d, idx: %d\n", n, block, idx);
	frame_idx[0] = idx;
	sprintf(data_name, "data_%06d", block + 1);
	retval = get_frame(desc->data_group_id, data_name, frame_idx, frame_size, data_width, buffer);
	if (retval < 0) {
		ERROR_JUMP(retval, done, "");
	}
done:
	return retval;
}


int get_dectris_eiger_dataset_dims(const struct data_description_t *desc, struct dataset_properties_t *properties) {
	int retval = 0;
	int n_datas = 0;
	int n = 0;
	int data_width = 0;
	int ndims = 3;
	char ds_name[16] = {0}; /* 12 chars in "data_xxxxxx\0" */
	int *frame_counts = NULL;
	hsize_t dims[3] = {0};

	/* datasets are "data_%06d % n" - need to determine how many of these there are and what the ranges are */

	sprintf(ds_name, "data_%06d", n_datas + 1);
	while (H5Lexists(desc->data_group_id, ds_name, H5P_DEFAULT) > 0) {
		sprintf(ds_name, "data_%06d", ++n_datas + 1);
	}

	frame_counts = malloc(n_datas * sizeof(*frame_counts));

	for (n = 0; n < n_datas; n++) {
		hid_t ds_id, t_id, s_id;
		hsize_t block_dims[3] = {0};
		sprintf(ds_name, "data_%06d", n + 1);
		ds_id = H5Dopen2(desc->data_group_id, ds_name, H5P_DEFAULT);
		if (ds_id < 0) {
			char message[64];
			sprintf("Unable to open dataset %.16s", ds_name);
			ERROR_JUMP(-1, loop_end, message);
		}
		t_id = H5Dget_type(ds_id);
		if (t_id < 0) {
			ERROR_JUMP(-1, close_dataset, "Unable to get datatype")
		}
		s_id = H5Dget_space(ds_id);
		if (s_id < 0) {
			ERROR_JUMP(-1, close_type, "Unable to get dataspace");
		}

		data_width = H5Tget_size(t_id);
		if (data_width <= 0) {
			ERROR_JUMP(-1, close_space, "Unable to get type size");
		}

		ndims = H5Sget_simple_extent_ndims(s_id);
		if (ndims != 3) {
			char message[64];
			sprintf(message, "Dataset %.16s has rank %d, expected %d", ds_name, ndims, 3);
			ERROR_JUMP(-1, close_space, message);
		}
		if (H5Sget_simple_extent_dims(s_id, block_dims, NULL) < 0) {
			ERROR_JUMP(-1, close_space, "Unable to read dataset dimensions");
		}

		dims[1] = block_dims[1];
		dims[2] = block_dims[2];

		dims[0] += block_dims[0];
		frame_counts[n] = block_dims[0];

close_space:
		H5Sclose(s_id);
close_type:
		H5Tclose(t_id);
close_dataset:
		H5Dclose(ds_id);
loop_end:
		if (retval < 0) break;
	}

	if (retval < 0) {
		free(frame_counts);
	} else {
		memcpy(properties->dims, dims, 3 * sizeof(*dims));
		properties->data_width = data_width;
		((struct eiger_data_description_t *) desc->extra)->n_data_blocks = n_datas;
		((struct eiger_data_description_t *) desc->extra)->block_sizes = frame_counts;
	}
	return retval;
}


int read_pixel_info(hid_t g_id, const char *path, double *size) {
	/*
	 * NXdetector allows pixel size to be an array (for varied pixel size),
	 * however XDS only allows for a single value.
	 * TODO: handle array case (return first value maybe?)
	 */

	/* read the scalar dataset value and scale according to the unit in the attribute */
	/* returned value is in metres */
	int retval = 0;
	herr_t err = 0;
	hid_t ds_id;
	double value = 0;
	ds_id = H5Dopen2(g_id, path, H5P_DEFAULT);
	if (ds_id < 0) {
		char message[64];
		sprintf("Error opening dataset %.32s", path);
		ERROR_JUMP(-1, done, message);
	}

	err = H5Dread(ds_id, H5T_NATIVE_DOUBLE, H5S_ALL, H5S_ALL, H5P_DEFAULT, &value);
	if (err < 0) {
		char message[64];
		sprintf("Error reading dataset %.32s", path);
		ERROR_JUMP(-1, close_dataset, message);
	}

	if (H5Aexists(ds_id, "units") > 0) {
		/* string may be variable length */
		hid_t a_id, t_id, mt_id;
		void *str_buffer = NULL;
		int str_size = -1;
		double scale = 1;
		a_id = H5Aopen(ds_id, "units", H5P_DEFAULT);
		if (a_id < 0) {
			char message[64];
			sprintf("Error openeing units attribute for %.32s after existence check", path);
			ERROR_JUMP(-1, close_dataset, message);
		}

		t_id = H5Aget_type(a_id);
		if (t_id < 0) {
			ERROR_JUMP(-1, close_attribute, "Error getting datatype");
		}
		/* TODO: handle multiple strings in attribute (just detect and error) */
		if (H5Tis_variable_str(t_id) > 0) {
			str_size = -1;
			str_buffer = malloc(sizeof(char*));
		} else {
			str_size = H5Tget_size(t_id);
			/* do not assume room has been left for null-byte in fixed length string */
			str_buffer = malloc(str_size + 1);
		}
		if (str_buffer == NULL) {
			ERROR_JUMP(-1, close_datatype, "Unable to allocate space for variable length string");
		}
		mt_id = H5Tcopy(H5T_C_S1);
		if (mt_id < 0) {
			ERROR_JUMP(-1, free_string, "Error creating HDF5 String datatype");
		}
		err = H5Tset_size(mt_id, str_size == -1 ? H5T_VARIABLE : str_size);
		if (err < 0) {
			char message[64];
			sprintf(message, "Error setting datatype size to %d", str_size);
			ERROR_JUMP(-1, close_mem_datatype, message);
		}

		err = H5Aread(a_id, mt_id, str_buffer);
		if (err < 0) {
			ERROR_JUMP(-1, close_mem_datatype, "Error reading units attribute");
		}
		/* ensure last byte is null */
		if (str_size > 0) ((char*) str_buffer)[str_size] = '\0';

		scale = scale_from_units(str_size == -1 ? *(char**)str_buffer : (char*)str_buffer);
		value *= scale;

		if (str_size == -1) {
			/* we have to create this dataspace just to indicate we want
			 * to clear the entire vlen dataset (all 1 element - good job HDF5)
			 */
			hsize_t dims[1] = {1};
			hid_t s_id = H5Screate_simple(1, dims, NULL);
			H5Sselect_all(s_id);
			H5Dvlen_reclaim(mt_id, s_id, H5P_DEFAULT, str_buffer);
			H5Sclose(s_id);
		}
close_mem_datatype:
		H5Tclose(mt_id);
free_string:
		free(str_buffer);
close_datatype:
		H5Tclose(t_id);
close_attribute:
		H5Aclose(a_id);
	} /* if H5Aexists(...) */


close_dataset:
	H5Dclose(ds_id);
done:
	if (retval == 0) *size = value;
	return retval;
}


int get_nxs_pixel_info(const struct data_description_t *desc, double *x_size, double *y_size) {
	int retval = 0;
	if (read_pixel_info(desc->det_group_id, "x_pixel_size", x_size) < 0) {
		ERROR_JUMP(-1, done, "");
	}
	if (read_pixel_info(desc->det_group_id, "y_pixel_size", y_size) < 0) {
		ERROR_JUMP(-1, done, "");
	}
done:
	return retval;
}


int get_dectris_eiger_pixel_info(const struct data_description_t *desc, double *x_size, double *y_size) {
	int retval = 0;
	if (read_pixel_info(desc->det_group_id, "detectorSpecific/x_pixel_size", x_size) < 0) {
		ERROR_JUMP(-1, done, "");
	}
	if (read_pixel_info(desc->det_group_id, "detectorSpecific/y_pixel_size", y_size) < 0) {
		ERROR_JUMP(-1, done, "");
	}
done:
	return retval;
}


int get_nxs_pixel_mask(const struct data_description_t *desc, int *buffer) {
	int retval = 0;
	hid_t ds_id;
	herr_t err = 0;

	ds_id = H5Dopen2(desc->det_group_id, "pixel_mask", H5P_DEFAULT);
	if (ds_id < 0) {
		ERROR_JUMP(-1, done, "Error opening pixel_mask dataset");
	}

	err = H5Dread(ds_id, H5T_NATIVE_INT, H5S_ALL, H5S_ALL, H5P_DEFAULT, buffer);
	if (err < 0) {
		ERROR_JUMP(-1, close_dataset, "Error reading pixel_mask dataset");
	}

close_dataset:
	H5Dclose(ds_id);
done:
	return retval;
}


int get_dectris_eiger_pixel_mask(const struct data_description_t *desc, int *buffer) {
	int retval = 0;
	hid_t ds_id;
	herr_t err = 0;

	ds_id = H5Dopen2(desc->det_group_id, "detectorSpecific/pixel_mask", H5P_DEFAULT);
	if (ds_id < 0) {
		ERROR_JUMP(-1, done, "Error opening detectorSpecific/pixel_mask");
	}

	err = H5Dread(ds_id, H5T_NATIVE_INT, H5S_ALL, H5S_ALL, H5P_DEFAULT, buffer);
	if (err < 0) {
		ERROR_JUMP(-1, close_dataset, "Error reading detectorSpecific/pixel_mask");
	}

close_dataset:
	H5Dclose(ds_id);
done:
	return retval;

}


herr_t det_visit_callback(hid_t root_id, const char *name, const H5O_info_t *info, void *op_data) {
	struct det_visit_objects_t *output_data = op_data;
	hid_t g_id;
	herr_t retval = 0;
	if (info->type != H5O_TYPE_GROUP) return 0;
	g_id = H5Oopen(root_id, name, H5P_DEFAULT);
	if (g_id < 0) {
		char message[256];
		sprintf(message, "H5OVisit callback: Unable to open group %.128s", name);
		ERROR_JUMP(-1, done, message);
	}

	/* check for an "NX_class" attribute */
	{
		int str_size = 0;
		void* buffer = NULL;
		hid_t a_id, t_id, mt_id;
		if (H5Aexists(g_id, "NX_class") <= 0) {
			/* not an error - just close group and allow continuation */
			retval = 0;
			goto close_group;
		}
		a_id = H5Aopen(g_id, "NX_class", H5P_DEFAULT);
		if (a_id <= 0) {
			char message[256];
			sprintf(message,
					"H5OVisit callback: Error opening NX_class attribute on %.128s after existence check",
					name);
			ERROR_JUMP(-1, close_group, message);
		}

		t_id = H5Aget_type(a_id);
		if (t_id < 0) {
			ERROR_JUMP(-1, close_attr, "Error getting datatype");
		}
		if (H5Tis_variable_str(t_id) > 0) {
			str_size = -1;
			buffer = malloc(sizeof(char*));
		} else {
			str_size = H5Tget_size(t_id);
			buffer = malloc(str_size + 1);
		}
		if (!buffer) {
			ERROR_JUMP(-1, close_type, "Error allocating string buffer");
		}

		mt_id = H5Tcopy(H5T_C_S1);
		if (mt_id < 0) {
			ERROR_JUMP(-1, free_buffer, "Error creating HDF5 String datatype");
		}
		if (H5Tset_size(mt_id, str_size == -1 ? H5T_VARIABLE : str_size) < 0) {
			char message[64];
			sprintf(message, "Error setting string datatype to size %d", str_size);
			ERROR_JUMP(-1, close_mtype, message);
		}

		if (H5Aread(a_id, mt_id, buffer) < 0) {
			char message[256];
			sprintf(message,
					"H5OVisit callback: Error reading NX_class attribute on group %.128s",
					name);
			ERROR_JUMP(-1, close_mtype, message);
		}

		/* at least one file has been seen where the NX_class attribute was not null terminated
		 * and extraneous bytes where being read by strcmp -  set the end byte to null
		*/
		if (str_size > 0) ((char*) buffer)[str_size] = '\0';
		/* test for NXdata or NXdetector */
		{
			char *nxclass = str_size > 0 ? (char*) buffer : *((char**) buffer);
			if (strcmp("NXdata", nxclass) == 0) {
				hid_t out_id = H5Gopen(root_id, name, H5P_DEFAULT);
				output_data->nxdata = out_id;
			} else if (strcmp("NXdetector", nxclass) == 0) {
				hid_t out_id = H5Gopen(root_id, name, H5P_DEFAULT);
				output_data->nxdetector = out_id;
			}
		}

		if (str_size == -1) {
			hsize_t dims[1] = {1};
			hid_t s_id = H5Screate_simple(1, dims, NULL);
			H5Sselect_all(s_id);
			H5Dvlen_reclaim(mt_id, s_id, H5P_DEFAULT, buffer);
			H5Sclose(s_id);
		}

close_mtype:
		H5Tclose(mt_id);
free_buffer:
		free(buffer);
close_type:
		H5Tclose(t_id);
close_attr:
		H5Aclose(a_id);
	}

close_group:
	if (H5Gclose(g_id) < 0) {
		/* TODO: error trace */
		retval = -1;
	}
done:
	return retval;
}


int fill_data_descriptor(struct data_description_t *data_desc, struct det_visit_objects_t *visit_result) {
	int retval = 0;
	data_desc->det_group_id = visit_result->nxdetector;

	/* determine the pixel information location */
	if (H5Lexists(data_desc->det_group_id, "x_pixel_size", H5P_DEFAULT) > 0 &&
			H5Lexists(data_desc->det_group_id, "y_pixel_size", H5P_DEFAULT)) {
		data_desc->get_pixel_properties = &get_nxs_pixel_info;
	} else if (H5Lexists(data_desc->det_group_id, "detectorSpecific", H5P_DEFAULT) > 0 &&
			H5Lexists(data_desc->det_group_id, "detectorSpecific/x_pixel_size", H5P_DEFAULT) > 0 &&
			H5Lexists(data_desc->det_group_id, "detectorSpecific/y_pixel_size", H5P_DEFAULT) > 0) {
		data_desc->get_pixel_properties = &get_dectris_eiger_pixel_info;
	} else {
		data_desc->get_pixel_properties = NULL;
		ERROR_JUMP(-1, done, "Could not locate x_pixel_size and y_pixel_size");
	}

	/* determine pixel mask location */
	if (H5Lexists(data_desc->det_group_id, "pixel_mask", H5P_DEFAULT) > 0) {
		data_desc->get_pixel_mask = &get_nxs_pixel_mask;
	} else if (H5Lexists(data_desc->det_group_id, "detectorSpecific", H5P_DEFAULT) > 0 &&
			H5Lexists(data_desc->det_group_id, "detectorSpecific/pixel_mask", H5P_DEFAULT) > 0) {
		data_desc->get_pixel_mask = &get_dectris_eiger_pixel_mask;
	} else {
		data_desc->get_pixel_mask = NULL;
		ERROR_JUMP(-1, done, "Could not locate pixel_mask");
	}

	/* determine where the data is stored and what strategy to use */
	/* we select the "dectris-eiger" strategy if both are valid due to
	 * potential confusion with the sizes of a virtual dataset (and possible
	 * failure opening it if the library version is not up to date)
	 */
	if (H5Lexists(visit_result->nxdetector, "data_000001", H5P_DEFAULT) > 0) {
		data_desc->data_group_id = visit_result->nxdetector;
		data_desc->get_data_properties = &get_dectris_eiger_dataset_dims;
		data_desc->get_data_frame = &get_dectris_eiger_frame;
	} else if (H5Lexists(visit_result->nxdetector, "data", H5P_DEFAULT) > 0) {
		data_desc->data_group_id = visit_result->nxdetector;
		data_desc->get_data_properties = &get_nxs_dataset_dims;
		data_desc->get_data_frame = &get_nxs_frame;
	} else if (H5Lexists(visit_result->nxdata, "data_000001", H5P_DEFAULT) > 0) {
		data_desc->data_group_id = visit_result->nxdata;
		data_desc->get_data_properties = &get_dectris_eiger_dataset_dims;
		data_desc->get_data_frame = &get_dectris_eiger_frame;
	} else if (H5Lexists(visit_result->nxdata, "data", H5P_DEFAULT) > 0) {
		data_desc->data_group_id = visit_result->nxdata;
		data_desc->get_data_properties = &get_nxs_dataset_dims;
		data_desc->get_data_frame = &get_nxs_frame;
	} else {
		data_desc->data_group_id = 0;
		data_desc->get_data_properties = NULL;
		data_desc->get_data_frame = NULL;
		ERROR_JUMP(-1, done, "Could not locate detector dataset");
	}

	if (data_desc->get_data_properties == &get_dectris_eiger_dataset_dims) {
		/* setup the "extra eiger info" struct */
		struct eiger_data_description_t *eiger_desc = malloc(sizeof(*eiger_desc));
		if (!eiger_desc) {
			ERROR_JUMP(-1, done, "Memory error creating data description for Eiger");
		}
		memset(eiger_desc, 0, sizeof(*eiger_desc));
		data_desc->extra = eiger_desc;
		data_desc->free_extra = free_eiger_data_description;
	} else {
		data_desc->free_extra = free_nxs_data_description;
	}

done:
	return retval;
}


int extract_detector_info(
		const hid_t fid,
		struct data_description_t *data_desc,
		struct dataset_properties_t *dataset_prop) {
	int retval = 0;
	herr_t err = 0;
	struct det_visit_objects_t objects = {0};
	err = H5Ovisit(fid, H5_INDEX_NAME, H5_ITER_INC, &det_visit_callback, &objects);
	if (err < 0) {
		clear_det_visit_objects(&objects);
		ERROR_JUMP(-1, done, "Error during H5Ovisit callback");
	}
	if (objects.nxdata == 0) {
		fprintf(stderr, "WARNING: Could not locate an NXdata entry\n");
	}
	if (objects.nxdetector == 0) {
		fprintf(stderr, "WARNING: Could not locate an NXdetector entry\n");
	}

	if ((retval = fill_data_descriptor(data_desc, &objects)) < 0) {
		ERROR_JUMP(retval, done, "");
	};
	if ((retval = data_desc->get_data_properties(data_desc, dataset_prop)) < 0) {
		ERROR_JUMP(retval, done, "");
	}
done:
	return retval;
}
