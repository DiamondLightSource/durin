Durin
=====

XDS plugin for reading HDF5 files following the NeXuS format or those written by Dectris Eiger detectors.

See:
* https://www.nexusformat.org
* https://www.dectris.com/features/features-eiger-x/hdf5-and-nexus
* https://strucbio.biologie.uni-konstanz.de/xdswiki


## Usage
In your XDS.INP add:
```
LIB=[path to durin-plugin.so]
NAME_TEMPLATE_OF_DATA_FRAMES=[data_path]/data_images_??????.h5
```
XDS will instruct the plugin to load `[data_path]/data_images_master.h5` and this must be the
Eiger master file or the NeXus file for the data collection.

It is generally assumed that the files `[data_path]/data_images_data_xxxxxx.h5` contain the actual
datasets and the master file contains HDF5 external links to these, but that is not required, so long as
the master file contains an `NXdata` or `NXdetector` group with either a dataset named `data` or a
series of datasets named `data_000001`, `data_000002`, etc.


## Requirements
* HDF5 Library (https://www.hdfgroup.org/downloads)


## Building
First ensure that the HDF5 libraries are compiled with:

```
export CFLAGS=-fPIC
./configure --enable-threadsafe=yes --enable-unsupported
```

former is neeed to ensure that XDS in parallel mode will work, latter
to ensure that the chunk read (which for opaque reasons is defined to
be _high level_) works. Then make sure that wherever this was
installed, the h5cc is in PATH before:

```
cd durin
make
```

Plugin file is `build/durin-plugin.so` - this should be added to the
XDS.INP file i.e. as:

```
DETECTOR=PILATUS MINIMUM_VALID_PIXEL_VALUE=0 OVERLOAD=4096
LIB=/opt/durin/build/durin-plugin.so
SENSOR_THICKNESS= 0.450
!SENSOR_MATERIAL / THICKNESS Si 0.450
!SILICON= 3.953379
DIRECTION_OF_DETECTOR_X-AXIS= 1.00000 0.00000 0.00000
DIRECTION_OF_DETECTOR_Y-AXIS= 0.00000 1.00000 0.00000
NX=2070 NY=2164 QX=0.0750 QY=0.0750
DETECTOR_DISTANCE= 194.633000
ORGX= 1041.30 ORGY= 1160.90
ROTATION_AXIS= 0.00000 -1.00000 -0.00000
STARTING_ANGLE= -30.000
OSCILLATION_RANGE= 0.100
X-RAY_WAVELENGTH= 0.97891
INCIDENT_BEAM_DIRECTION= -0.000 -0.000 1.022
FRACTION_OF_POLARIZATION= 0.999
POLARIZATION_PLANE_NORMAL= 0.000 1.000 0.000
NAME_TEMPLATE_OF_DATA_FRAMES= ../image_9264_??????.h5
TRUSTED_REGION= 0.0 1.41
DATA_RANGE= 1 600
JOB=XYCORR INIT COLSPOT IDXREF DEFPIX INTEGRATE CORRECT
```

N.B. the master file is needed, not the .nxs one which follows the
standard. 
