# scidb4gdal
A GDAL driver for SciDB arrays

## Description
This is a preliminary version of a [GDAL](http://www.gdal.org) driver for SciDB. Spatial reference of arrays is maintained if the SciDB database uses the [scidb4geo plugin](https://github.com/mappl/scidb4geo).
Otherwise, the GDAL driver might be still useful e.g. for converting two-dimensional arrays to a variety of image formats supported by GDAL. 

The main purpose of the plugin is to use SciDB for complex geospatial analytics and your favorite GIS tool to explore results. 

The driver offers support for reading and writing SciDB arrays. Update access to existing arrays is currently not implemented but planned for future releases.


## Getting Started
Similar to other database drivers for GDAL, we use a connection string as a descriptor for array data sources. 

`"SCIDB:array=<arrayname> [host=<host> port=<port> user=<user> password=<password>]"`

Notice that the string must start with `SCIDB:` in order to let GDAL identify the dataset as a SciDB array. The following examples demonstrate how you can use gdal_translate to load / read imagery to / from SciDB: 

- Writing GDAL datasets to SciDB: `gdal_translate -of SciDB sample.tif "SCIDB:array=sample_array_gdal host=http://localhost"`

- Exporting SciDB arrays: `gdal_translate -of GTiff "SCIDB:array=sample_array_gdal host=http://localhost" sample.tif`

- Array metadata: `gdalinfo "SCIDB:array=sample_array_gdal host=http://localhost"`

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

If you get some missing include file errors, you need to install Boost manually. Either use your distribution's package manager e.g. `sudo apt-get install libboost-dev` or simply copy Boost header files to a standard include directory like `/usr/include`.

Windows build instructions will follow.
<!--

The following instructions demonstrate how to compile GDAL with added SciDB driver on Windows using Visual Studio.  Detailed information for tweaking windows builds can be found at http://trac.osgeo.org/gdal/wiki/BuildingOnWindows.

We recommend the [OSGeo4W](http://trac.osgeo.org/osgeo4w/) network installer for managing and installing GIS libraries on Windows.
Before you 

1. Do steps 1 to 4 as above.
2. Add a corresponding entry to `EXTRAFLAGS` in `GDAL_SRC_DIR/frmts/makefile.vc`
3. Edit `GDAL_SRC_DIR/nmake.opt` to fit your needs and environment. In particular, you should uncomment references to curl.
4. Open a command line. Depending on the location you want to install GDAL to, do that as administrator
5. 
-->
