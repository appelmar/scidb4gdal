# scidb4gdal
A GDAL driver for SciDB arrays

## Description
This is a preliminary version of a [GDAL](http://www.gdal.org) driver for SciDB. Spatial reference of arrays is maintained if the SciDB database uses the [scidb4geo plugin](https://github.com/mappl/scidb4geo).
Otherwise, the GDAL driver might be still useful e.g. for converting two-dimensional arrays to a variety of image formats supported by GDAL. 

The main purpose of the plugin is to use SciDB for complex geospatial analytics and your favorite GIS tool to explore results. 

Currently, the driver is read only.

## Getting Started
Similar to other database drivers for GDAL, we use a connection string as a descriptor for array data sources. 

`"SCIDB:array=<arrayname> [host=<host> port=<port> user=<user> password=<password>]"`

Notice that the string must start with `SCIDB:` in order to let GDAL identify the dataset as a SciDB array.



## Dependencies
- [Shim](https://github.com/Paradigm4/shim) must be running on SciDB databases you want to connect to
- [cURL](http://curl.haxx.se/) We use cURL to interface with SciDB's web service shim
- Some [Boost](http://www.boost.org) header-only libraries (no external libraries required for linking) 




## Build Instructions
The following instructions show you how to compile GDAL with added SciDB driver on Unix environments.

1. Download GDAL source
2. Clone this repository `git clone https://github.com/mappl/scidb4gdal` 
3. Move the content to GDAL_SRC_DIR/frmts/scidb `mv scidb4gdal GDAL_SRC_DIR/frmts/scidb`
4. Add driver to GDAL source tree (see http://www.gdal.org/gdal_drivertut.html):
    1. Add `GDALRegister_SciDB()`to GDAL_SRC_DIR/gcore/gdal_frmts.h
    2. Add call to `GDALRegister_SciDB()? in GDAL_SRC_DIR/frmts/gdalallregister.cpp
    3. Add `SciDB` to GDAL_FORMATS in GDAL_SRC_DIR/GDALmake.opt.in
    4. Add a corresponding entry to EXTRAFLAGS in GDAL_SRC_DIR/frmts/makefile.vc
5. Build GDAL `./configure && make && make install`

Windows build instructions will follow.

