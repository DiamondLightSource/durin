/*
 * Copyright (c) 2018 Diamond Light Source Ltd.
 * Author: Charles Mita
 */

/*
 * External library interface for XDS.
 * Ref: https://strucbio.biologie.uni-konstanz.de/xdswiki/index.php/LIB
 */

#ifndef NXS_XDS_PLUGIN_H
#define NXS_XDS_PLUGIN_H

#ifdef __cplusplus
extern "C" {
#endif

#define DLS_CUSTOMER_ID                                                        \
  0x01 /* pretend we're Dectris, otherwise XDS doesn't work */
#define VERSION_MAJOR 0
#define VERSION_MINOR 0
#define VERSION_PATCH 0
#define VERSION_TIMESTAMP -1 /* good enough for Dectris apparantely */

void plugin_open(const char *filename, int info[1024], int *error_flag);

void plugin_get_header(int *nx, int *ny, int *nbytes, float *qx, float *qy,
                       int *number_of_frames, int info[1024], int *error_flag);

void plugin_get_data(int *frame_number, int *nx, int *ny, int *data_array,
                     int info[1024], int *error_flag);

void plugin_close(int *error_flag);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* NXS_XDS_PLUGIN_H */
