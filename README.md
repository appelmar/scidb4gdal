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
Similar to other database drivers for GDAL, we use a connection string as a descriptor for array data sources and as an alternative we utilize GDALs opening option handling: 

`"SCIDB:array=<arrayname> [host=<host> port=<port> user=<user> password=<password>] [properties=t=<temporal_index>]"` 
or
`"gdal_translate -oo "host=<host>" -oo "port=<port>" -oo "user=<user>" -oo "password=<password>" ... "SCIDB:array=img[t,2015-02-01T00:00:00]" img.tif"`


Notice that the string must start with `SCIDB:` in order to let GDAL identify the dataset as a SciDB array. Default values for parameters are

    <host>     = https://localhost
    <port>     = 8083
    <user>     = scidb
    <password> = scidb

Additional query parameter like the temporal index are passed to the driver either via the connection string as part of the `"properties"` statement or by using a key value pair in GDALs opening options, i.e. `"-oo "t=2""`.

The following examples demonstrate how you can use gdal_translate to load / read imagery to / from SciDB: 

- Writing GDAL datasets to SciDB: `gdal_translate -of SciDB sample.tif "SCIDB:array=sample_array_gdal"`

- Exporting SciDB arrays: `gdal_translate -of GTiff "SCIDB:array=sample_array_gdal" sample.tif`

- Array metadata: `gdalinfo "SCIDB:array=sample_array_gdal`

## Dependencies
- At the moment the driver requires [Shim](https://github.com/Paradigm4/shim) to run on SciDB databases you want to connect to. In the future, this may or may not be changed to connecting directly to SciDB sockets using Google's protocol buffers
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
5. Build GDAL `./configure && make && make install`. 
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
 
