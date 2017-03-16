[![Build Status](https://travis-ci.org/appelmar/scidb4gdal.svg?branch=master)](https://travis-ci.org/appelmar/scidb4gdal)
# scidb4gdal
A GDAL driver for SciDB arrays

## Description
This [GDAL](http://www.gdal.org) driver implements read and write access to SciDB arrays. SciDB is an open-source data management and analytics system developed by [Paradigm4](http://www.paradigm4.com). 

To interface SciDB with most earth-observation products the driver supports representing multi-tiled and multi-temporal imagery as single multidimensional arrays. Single images can be added to an existing array as tile or temporal slice where array indexes are automatically computed. For advanced functionality like this, SciDB instances need to run the extension for geographic reference  [scidb4geo](https://github.com/appelmar/scidb4geo). 


A reproducible example where this GDAL driver is used from R to ingest and download MODIS and SRTM data can be found in a recent blog-post [Scalable Earth Observation analytics with R and SciDB](http://r-spatial.org/r/2016/05/11/scalable-earth-observation-analytics.html).



## News
- (2017-03-16)
    - Support for SciDB 16.9
- (2016-06-02)
    - Support for SciDB 15.12
    - Improved performance of array upload
    - Automatic build now uses GDAL 2.1.0
- (2016-02-26)
    - Chunk sizes can be provided as create option


## Getting Started
Similar to other database drivers for GDAL, we define a connection string to reference SciDB arrays. The connection string must contain an array name and may have further arguments like connection details of the database. With release of GDAL version 2.0, we added opening and create options to specify database connection details. Examples below demonstrate how a SciDB array can be referenced using gdalinfo.

- identifier based: `gdalinfo "SCIDB:array=<arrayname> [host=<host> port=<port> user=<user> password=<password>]"`

- opening option based: `gdalinfo -oo "host=<host>" -oo "port=<port>" -oo "user=<user>" -oo "password=<password> -oo "ssl=true" "SCIDB:array=<arrayname>"`

Connection details may alternatively be set as environment variables `SCIDB4GDAL_HOST`, `SCIDB4GDAL_PORT`, `SCIDB4GDAL_USER`, `SCIDB4GDAL_PASSWD`, or as a text file in `$HOME/.scidbpass`.


### Simple array download
The following examples demonstrate how to download a simple two-dimensional arrays using  [gdal_translate](http://www.gdal.org/gdal_translate.html). We assume that database connection details are set es environment variables.

1. Download the whole array
`gdal_translate "SCIDB:array=hello_scidb" "hello_scidb.tif"`

2. Download a subset based on array coordinates
`gdal_translate -srcwin 0 0 100 100 "SCIDB:array=hello_scidb" "hello_scidb_subset.tif"`

3. Download a spatial subset based on spatial coordinates (assuming WGS84) and only the first array attribute (band 1)
`gdal_translate -proj_win 7.1 52.2 7.6 51.9 -b 1 "SCIDB:array=hello_scidb" "hello_scidb_subset.tif"`


### Simple two-dimensional array upload
The following examples demonstrate how to upload single images to simple two-dimensional arrays using the [gdal_translate](http://www.gdal.org/gdal_translate.html) utility. 

1. Upload the whole array
`gdal_translate -of SciDB "hello_scidb.tif" "SCIDB:array=hello_scidb"`

## Dependencies
- The driver requires [Shim](https://github.com/Paradigm4/shim) to run on SciDB databases you want to connect to. 
- [cURL](http://curl.haxx.se/) to communicate with SciDB's HTTP web service shim
- Some [Boost](http://www.boost.org) header-only libraries (no external libraries required for linking) for string functions


## Build Instructions

The following instructions show you how to compile GDAL with added SciDB driver on Unix environments. These steps are automated in a script in `build/prepare_platform.sh`.

1. Download GDAL source
2. Clone this repository `git clone https://github.com/appelmar/scidb4gdal` 
3. Copy the source to `GDAL_SRC_DIR/frmts/scidb` by `cp scidb4gdal/src GDAL_SRC_DIR/frmts/scidb`
4. Add driver to GDAL source tree (see http://www.gdal.org/gdal_drivertut.html#gdal_drivertut_addingdriver):
    1. Add `GDALRegister_SciDB()`to `GDAL_SRC_DIR/gcore/gdal_frmts.h`
    2. Add call to `GDALRegister_SciDB()` in `GDAL_SRC_DIR/frmts/gdalallregister.cpp` within `#ifdef FRMT_scidb`
    3. Add "scidb" to `GDAL_FORMATS` in `GDAL_SRC_DIR/GDALmake.opt.in`
5. Build GDAL `./configure && make && sudo make install`. 
6. Eventually, you might need to run `sudo ldconfig` to make GDAL's shared library available.

If you get some missing include file errors, you need to install Boost manually. Either use your distribution's package manager e.g. `sudo apt-get install libboost-dev` or simply copy Boost header files to a standard include directory like `/usr/include`.


### Build on Windows (not tested)

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
 
