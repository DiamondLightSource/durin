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
```
cd durin
make
```
Plugin file is `build/durin-plugin.so`
