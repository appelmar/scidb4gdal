[![Build Status](https://travis-ci.org/mappl/scidb4gdal.svg?branch=master)](https://travis-ci.org/mappl/scidb4gdal)
# scidb4gdal
A GDAL driver for SciDB arrays

## Description
This is a preliminary version of a [GDAL](http://www.gdal.org) driver for SciDB. Spatial reference of arrays is maintained if the SciDB database uses the [scidb4geo plugin](https://github.com/mappl/scidb4geo).
Otherwise, the GDAL driver might be still useful e.g. for converting two-dimensional arrays to a variety of image formats supported by GDAL. 

The driver offers support for reading and writing SciDB arrays. Update access to existing arrays is currently not implemented but planned for future releases.

## News
- (2015-09-01)
    - Driver now compiles under Windows
- (2015-08-27)
	- Support for empty SciDB cells (will be filled with NoData value) added
	- Fixed dimension mismatch between GDAL and SciDB
	- General GDAL metadata (e.g. color interpretation, no data, offset, and scaling values for bands) can be stored in SciDB's system catalog
- (2015-06-23)
	- Automated builds added, build/prepare_platform.sh might also help you to automatically build GDAL from source including this scidb driver
- (2015-04-02)
    - Support for HTTPS connections to Shim
    - Improved performance for both read and write access

## Getting Started
Similar to other database drivers for GDAL, we need to access a database in order to perform queries. Therefore two strategies can be utilized to do so. The first strategy was introduced by the early database drivers. They used a connection string to access the database. Typically the connection string was passed as the file name.
The second strategy was introduced as opening options parameter for the GDAL functions with GDAL version 2.0.

Examples for connection strings:
- file name based: `"SCIDB:array=<arrayname> [host=<host> port=<port> user=<user> password=<password>]"`
- 
- opening option based: `-oo "host=<host>" -oo "port=<port>" -oo "user=<user>" -oo "password=<password> -oo "ssl=true"`

Notice that the file name for SciDB must start with `SCIDB:` in order to let GDAL identify the dataset as a SciDB array. Default values for parameters, if additional information is provided, are the following:

    <host>     = https://localhost
    <port>     = 8083
    <user>     = scidb
    <password> = scidb

Since an array based data base like SciDB allows not only a two dimensional storage, but a multi dimensional storage. Our SciDB plugin extends the image storage for spatial data and also for spatio-temporal data. Internally SciDB uses integer indizes to reference the cells to coordinates. The spatial query can be formed as intended by GDAL functions. 
Since GDAL was developed primarily in a spatial context, the temporal query proved difficult to realize in the current GDAL version.
As a solution we allow three different methods:

1. suffix of the array name: `"SCIDB:array=array_name[t,1]"` or `"SCIDB:array=array_name[t,2015-03-03T10:00:00]"`
2. as part of a property string within the connection string: `"SCIDB:array=<arrayname> [host=<host> port=<port> user=<user> password=<password>] [properties=t=<temporal_index>]"`
3. as part of the opening options: `-oo "t=<temporal_index>"`

To address the temporal component the identifier "t" is used in most cases. "t" requires either an integer value (temporal index) or a valid timestamp string according to ISO 8601. The latter is currently only supported in the "suffix" strategy.

When using "gdal_translate" to access spatio-temporally references imagery in SciDB note that exactly one image will be returned. With this in mind the temporal request is limited to search with one time component, meaning that no interval query is currently supported. The temporal query will return the temporally nearest image that is found in the data base.

The following examples demonstrate how you can use gdal_translate to load / read imagery to / from SciDB: 

- Array metadata: `gdalinfo "SCIDB:array=sample_array_gdal"`

- Exporting SciDB arrays: `gdal_translate [-oo ...] -of GTiff "SCIDB:array=sample_array[t,1] [...]" sample.tif`

- Writing GDAL datasets to SciDB: `gdal_translate -of SciDB sample.tif "SCIDB:array=sample_array_gdal"`

In terms of loading spatial imagery into SciDB, we currently support simple uploads of images. The temporal differentiation of data is currently under development. We assume that the images imported into a single SciDB array using the same spatial reference system and they are assumed as being in the same temporal resolution. Other than that you can import multiple images into one array if the afore mentioned requirements are met.

## Dependencies
- At the moment the driver requires [Shim](https://github.com/Paradigm4/shim) to run on SciDB databases you want to connect to. In the future, this may or may not be changed to connecting directly to SciDB sockets using Google's protocol buffers
- [ST plugin] (https://github.com/mappl/scidb4geo) for SciDB
- We use [cURL](http://curl.haxx.se/) to interface with SciDB's web service shim
- Some [Boost](http://www.boost.org) header-only libraries (no external libraries required for linking) for string functions


## Build Instructions

The following instructions show you how to compile GDAL with added SciDB driver on Unix environments.

1. Download GDAL source
2. Clone this repository `git clone https://github.com/mappl/scidb4gdal` 
3. Copy the source to `GDAL_SRC_DIR/frmts/scidb` by `cp scidb4gdal/src GDAL_SRC_DIR/frmts/scidb`
4. Add driver to GDAL source tree (see http://www.gdal.org/gdal_drivertut.html#gdal_drivertut_addingdriver):
    1. Add `GDALRegister_SciDB()`to `GDAL_SRC_DIR/gcore/gdal_frmts.h`
    2. Add call to `GDALRegister_SciDB()` in `GDAL_SRC_DIR/frmts/gdalallregister.cpp` within `#ifdef FRMT_scidb`
    3. Add "scidb" to `GDAL_FORMATS` in `GDAL_SRC_DIR/GDALmake.opt.in`
5. Build GDAL `./configure && make && sudo make install`. 
6. Eventually, you might need to run `sudo ldconfig` to make GDAL's shared library available.

If you get some missing include file errors, you need to install Boost manually. Either use your distribution's package manager e.g. `sudo apt-get install libboost-dev` or simply copy Boost header files to a standard include directory like `/usr/include`.


### Build on Windows

The following instructions demonstrate how to compile GDAL with added SciDB driver on Windows using Visual Studio 2013.  
Detailed information for tweaking windows builds can be found at http://trac.osgeo.org/gdal/wiki/BuildingOnWindows.
We recommend the [OSGeo4W](http://trac.osgeo.org/osgeo4w/) network installer for managing and installing external GIS libraries on Windows. 
In particular this allows you to easily install curl and boost development libraries that are needed to compile this driver.
 
1. Download GDAL source
2. Clone this repository
3. Copy the `src` directory of your clone to `GDAL_SRC_DIR/frmts` and rename it `scidb`
4. Add driver to GDAL source tree (see http://www.gdal.org/gdal_drivertut.html#gdal_drivertut_addingdriver):
    1. Add `GDALRegister_SciDB()`to `GDAL_SRC_DIR/gcore/gdal_frmts.h`
    2. Add call to `GDALRegister_SciDB()` in `GDAL_SRC_DIR/frmts/gdalallregister.cpp` within `#ifdef FRMT_scidb`
    3. Open `GDAL_SRC_DIR/frmts/makefile.vc` and add `-DFRMT_scidb`  within the `!IFDEF CURL_LIB` block
5. Setup external include and library paths
    1. Uncomment lines to set `CURL_INC` and `CURL_LIB` in `GDAL_SRC_DIR/nmake.opt`
    2. Edit link to Boost header directory in `GDAL_SRC_DIR/frmts/scidb/makefile.vc`
6. Start a command line 
    1. Change directory to the GDAL source `cd GDAL_SRC_DIR`
	2. Load Visual studio command line tools e.g. by running `"C:\Program Files (x86)\Microsoft Visual Studio 12.0\VC\bin\x86_amd64\vcvarsx86_amd64.bat"
` for x64 builds using Visual Studio 2013
    3. Run nmake e.g. `nmake /f makefile.vc MSVC_VER=1800 WIN64=YES`
 
