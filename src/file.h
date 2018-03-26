/*
 * Copyright (c) 2018 Diamond Light Source Ltd.
 * Author: Charles Mita
 */


#ifndef NXS_XDS_FILE_H
#define NXS_XDS_FILE_H

#include <hdf5.h>

struct dataset_properties_t {
	int ndims;
	int data_width;
	hsize_t *dims;
};

void free_dataset_properties(struct dataset_properties_t *p);

struct data_description_t {
	hid_t det_group_id;
	hid_t data_group_id;
	int (*get_pixel_properties)(const struct data_description_t*, double*, double*);
	int (*get_pixel_mask)(const struct data_description_t*, int*);
	int (*get_data_properties)(const struct data_description_t*, struct dataset_properties_t*);
	int (*get_data_frame)(const struct data_description_t*, const struct dataset_properties_t*, int, int, void*);
	void *extra;
	void (*free_extra)(struct data_description_t*);
};

void free_nxs_data_description(struct data_description_t *desc);

struct eiger_data_description_t {
	int n_data_blocks;
	int *block_sizes;
};

void free_eiger_data_description(struct data_description_t *desc);

struct det_visit_objects_t {
	hid_t nxdata;
	hid_t nxdetector;
};

void clear_det_visit_objects(struct det_visit_objects_t *objects);

int get_nxs_dataset_dims(const struct data_description_t *desc, struct dataset_properties_t *properties);

int fill_data_descriptor(struct data_description_t *data_desc, struct det_visit_objects_t *visit_result);

int extract_detector_info(const hid_t fid, struct data_description_t *data_desc, struct dataset_properties_t *ds_prop);

#endif /* NXS_XDS_FILE_H */
