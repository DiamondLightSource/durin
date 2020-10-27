/*
 * Copyright (c) 2018 Diamond Light Source Ltd.
 * Author: Charles Mita
 */

#ifndef NXS_XDS_FILTER_H
#define NXS_XDS_FILTER_H

#define BS_H5_N_PARAMS 5
#define BS_H5_FILTER_ID 32008
#define BS_H5_PARAM_LZ4_COMPRESS 2

int bslz4_decompress(const unsigned int *bs_params, size_t in_size,
                     void *in_buffer, size_t out_size, void *out_buffer);

#endif /* NXS_XDS_FILTER_H */
