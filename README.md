[![Build Status](https://travis-ci.org/mappl/scidb4gdal.svg?branch=master)](https://travis-ci.org/mappl/scidb4gdal)
# scidb4gdal
A GDAL driver for SciDB arrays

## Description
This is a preliminary version of a [GDAL](http://www.gdal.org) driver for the array database system SciDB. Spatial reference of arrays is maintained if the SciDB database uses the [scidb4geo plugin](https://github.com/mappl/scidb4geo).
Otherwise, the GDAL driver might be still useful e.g. for converting two-dimensional arrays to a variety of image formats supported by GDAL. 

The driver offers support for reading and writing SciDB arrays. A single SciDB array may or may not be constructed from multiple (tiled) files. To build three-dimensional spacetime arrays, imagery can be automatically added to existing arrays based on its temporal snapshot (see details below).  

## News
- (2016-04-22)
    - Python tool for batch uploading images into a spatio-temporal series
- (2016-03-22)
    - Annotating date-time statement when downloading a spatio-temporal array from SciDB
    - Reading of time and temporal resolution from metadata of input file
- (2016-02-26)
    - Chunksizes can be manually configured
- (2016-02-05)
    - Major update to automatically add imagery to existing spacetime arrays 
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
Similar to other database drivers for GDAL, we define a connection string to address a SciDB array as a GDAL dataset. The connection string must contain an array name and may have further arguments like connection details of the database. With release of GDAL version 2.0, we added opening and create options to specify database connection details. Examples below demonstrate how a SciDB array can be accessed using gdalinfo.


Examples how to address a SciDB array:

- identifier based: `gdalinfo "SCIDB:array=<arrayname> [host=<host> port=<port> user=<user> password=<password>]"`

- opening option based: `gdalinfo -oo "host=<host>" -oo "port=<port>" -oo "user=<user>" -oo "password=<password> -oo "ssl=true" "SCIDB:array=<arrayname>"`

Notice that the dataset identifier must start with `SCIDB:` in order to let GDAL identify the dataset as a SciDB array. Default values for connection arguments are the following:

    <host>     = https://localhost
    <port>     = 8083
    <user>     = scidb
    <password> = scidb

Alternatively, these parameters may be set as environment variables `SCIDB4GDAL_HOST`, `SCIDB4GDAL_PORT`, `SCIDB4GDAL_USER`, `SCIDB4GDAL_PASSWD`.


### Simple array download
The following examples demonstrate how to download simple two-dimensional arrays using the [gdal_translate](http://www.gdal.org/gdal_translate.html) utility. 

1. Download the whole array
`gdal_translate "SCIDB:array=hello_scidb" "hello_scidb.tif"`

2. Download a subset based on array coordinates
`gdal_translate -srcwin 0 0 100 100 "SCIDB:array=hello_scidb" "hello_scidb_subset.tif"`

3. Download a spatial subset based on spatial coordinates (assuming WGS84) and only the first array attribute (band 1)
`gdal_translate -proj_win 7.1 52.2 7.6 51.9 -b 1 "SCIDB:array=hello_scidb" "hello_scidb_subset.tif"`


### Simple array upload
The following examples demonstrate how to upload single images to simple two-dimensional arrays using the [gdal_translate](http://www.gdal.org/gdal_translate.html) utility. 

1. Upload the whole array
`gdal_translate -of SciDB "hello_scidb.tif" "SCIDB:array=hello_scidb"`

Note:
There are limitation on [naming the SciDB Array](http://paradigm4.com/HTMLmanual/13.3/scidb_ug/ch04s01.html). Allowed are alphanumerical values and the Underscore sign ("_"). 


## Details

One of the benefits of SciDB is that it allows to store not only two dimensional data, but it supports multi-dimensional storage. The [scidb4geo](https://github.com/mappl/scidb4geo) plugin, extends SciDB to store images with spatial and temporal reference that is annotated to the SciDB arrays as metadata. With this annotations image coordinates (or dimension indices of arrays) can be transformed into real world coordinates and dates. GDAL can already deal with spatial operations like subsetting and image reprojections and  standard parameters for those operations can be also used with SciDB arrays. For example the gdal_translate parameter like `-projwin` or `-srcwin` will work for spatial subsetting and querying an array in SciDB.

We introduced several ways to state the temporal context, when querying a temporally referenced array.

There are three major approaches to do this:

1. suffix of the array name: `"SCIDB:array=array_name[i,1]"` or `"SCIDB:array=array_name[t,2015-03-03T10:00:00]"`
2. as part of a property string within the connection string: `"SCIDB:array=<arrayname> [host=<host> port=<port> user=<user> password=<password>] [properties=i=<temporal_index>]"`
3. as part of the opening options: `-oo "i=<temporal_index>"`

To address the temporal component the identifiers "t" and "i" are used. "t" refers to a string representation of a data or data/time that is a valid string representation according to ISO 8601. With the identifier "i" the temporal index of an image is addressed. The temporal index refers to the discrete dimension value of the assigned temporal dimension. This means i=0 would query for the image that was inserted at the starting date of the time series. A value higher then zero would mean that SciDB would try to access the i*dt image from the start (dt being the time resolution stated with the temporal reference).

When using "gdal_translate" to access spatio-temporally references imagery in SciDB note that exactly one image will be returned. With this in mind the temporal request is limited to search with one time component, meaning that no interval query is currently supported. The temporal query will return the temporally nearest image that is found in the data base.


### Image ingestion into SciDB
If you want to load an image into SciDB, we consider three different array representations. Those array representations are important to create the correct array structure in SciDB, meaning that if the image shall have a temporal component that an additional dimension needs to be assigned.
The preferred mechanism to pass user defined settings is to use GDALs Create Options (-co flag). Each setting of the create options has to be a key-value pair that is separated by "=". 

1. Spatial Array

   The spatial array is the default case of spatial image representation. This representation just assigns two dimensions for the spatial components and it attaches the spatial reference system that is stated in the metadata of the source file to the SciDB array. When creating this representation you should use the key-value pair `"type=S"`.

2. Spatio-temporal Array

   The spatio temporal array is a representation where the spatial image has also a time stamp. For this purpose the driver will assign one additional temporal dimension to the array and it assigns a user defined temporal reference system (TRS). The temporal reference system consists of the starting date and the temporal resolution. The starting date needs to be written as a date or date/time string according to ISO 8601 and the temporal resolution is a temporal period string. For example one possible valid TRS statement would look like `"t=2014-08-10T10:00"` and `"dt=P1D"` meaning that this reference starts at 2014-08-10 and has a temporal resolution of one day. To create this array type use `"type=ST"`. Please be advised that this type refers simply to exactly one point in time. There is no way to add later images into this array. The minimum and maximum of the temporal dimension for this array will be set to zero.

3. Spatio-temporal Series

   This array type is very similar to the Spatio-temporal Array, but it removes the restriction of the temporal dimension on carrying only one image. To be more concrete: This type only starts a time series. Additional images can then be inserted into this array by using the same array name and by using a Spatio-temporal Array. In order to start the spatio-temporal series, please use `"type=STS"`.

Now that we have covered the main types, there is another addition in creating SciDB arrays. The before mentioned types will restrict the spatial dimension to the images boundary. This means that if it not explicitly stated otherwise the spatial boundaries will be fixed to that extent. But to allow later insertion of images into in an existing array, a bounding box and its coordinates reference system need to be stated, when creating the image (the first upload). We use the parameter keys "bbox" and "srs" for this purpose. By setting an alternate bounding box we will refer to the data stored as a coverage, whereas before the data represented the original image. Here is also an example on setting a valid coverage statement: `"bbox=443000 4650000 455000 4629000"` and `"srs=EPSG:26716"`. Note that the spatial reference system is addressed to by stating the authority name and the systems id.

In the following we will give some explicit gdal_translate statements on how to create arrays:

**Create a spatial image from a file:**

   `gdal_translate -co "host=https://your.host.de" -co "port=31000" -co "user=user" -co "password=passwd" -co "type=S" -of SciDB input_image.tif "SCIDB:array=test_spatial"`

**Create a spatial coverage from multiple files:**

1. Start a coverage by stating a bounding box with SRS  
   `gdal_translate -co "host=https://your.host.de" -co "port=31000" -co "user=user" -co "password=passwd" -co "bbox=443000 4650000 455000 4629000" -co "srs=EPSG:26716" -co "type=S" -of SciDB part1.tif "SCIDB:array=test_spatial_coverage"`

2. Insert an image into the coverage  
   `gdal_translate -co "host=https://your.host.de" -co "port=31000" -co "user=user" -co "password=passwd" -co "type=S" -of SciDB part2.tif "SCIDB:array=test_spatial_coverage"`

**Create a Spatio-Temporal Array:**

   `gdal_translate -co "host=https://your.host.de" -co "port=31000" -co "user=user" -co "password=passwd" -co "dt=P1D" -co "t=2015-10-15" -co "type=ST" -of SciDB input_image.tif "SCIDB:array=test_spatio_temporal"`

**Create a Spatio-Temporal Series:**

1. Start series  
   `gdal_translate -co "host=https://your.host.de" -co "port=31000" -co "user=user" -co "password=passwd" -co "dt=P1D" -co "t=2015-10-15" -co "type=STS" -of SciDB input_image_15.tif "SCIDB:array=test_spatio_temporal_series"`

2. Insert an image to another time index after the starting date  
   `gdal_translate -co "host=https://your.host.de" -co "port=31000" -co "user=user" -co "password=passwd" -co "dt=P1D" -co "t=2015-10-16" -co "type=ST" -of SciDB input_image_16.tif "SCIDB:array=test_spatio_temporal_series"`

**Create a Spatio-Temporal Series with larger boundaries:**

1. Start series and stating an additional extent  
   `gdal_translate -co "host=https://your.host.de" -co "port=31000" -co "user=user" -co "password=passwd" -co "dt=P1D" -co "t=2015-10-15" -co "bbox=443000 4650000 455000 4629000" -co "srs=EPSG:26716" -co "type=STS" -of SciDB input_image_15.tif "SCIDB:array=test_spatio_temporal_series_coverage"`

2. Add image into the image of the 15th october  
   `gdal_translate -co "host=https://your.host.de" -co "port=31000" -co "user=user" -co "password=passwd" -co "dt=P1D" -co "t=2015-10-15" -co "type=ST" -of SciDB input_image_15_2.tif "SCIDB:array=test_spatio_temporal_series_coverage"`
   
3. Add image at another date  
   `gdal_translate -co "host=https://your.host.de" -co "port=31000" -co "user=user" -co "password=passwd" -co "dt=P1D" -co "t=2015-10-17" -co "type=ST" -of SciDB input_image_17.tif "SCIDB:array=test_spatio_temporal_series_coverage"`

If a coverage is used please make sure that the coordinates of the images refer to the same spatial reference system. It is also important that all images that are inserted into a coverage are within the initially stated boundary!

### Deleting arrays
In order to allow GDAL to delete arrays, we enabled this particular feature via gdalmanage. Since gdalmanage does not support opening options, the connection string approach must be used, e.g. `gdalmanage delete "SCIDB:array=test_spatial host=https://your.host.de port=31000 user=user password=passwd confirmDelete=Y"`. This command completely removes the array from the database. Please be sure that the array is gone once this command is executed. An additional parameter "confirmDelete" was introduced in order to prevent accidental deletion of an array. This is due to GDALs QuietDelete function that is called on each gdal_translate call. As values the following strings are allowed (case-insensitive): YES, Y, TRUE, T or 1.

### Adjusting chunksizes manually
SciDB manages its data in so called chunks. Pradigm4 suggests for those chunks to select sizes so that each chunk uses about 10 to 20 MB storage. Important to know is also that the chunks are stored for each attribute individually that are combined by a process called vertical partioning [link] (http://paradigm4.com/HTMLmanual/13.3/scidb_ug/ch04s05s02.htmlhttp://paradigm4.com/HTMLmanual/13.3/scidb_ug/ch04s05s02.html). In order to chose a suitable size this plugin will calculate chunksizes based on the spatial dimensions, the temporal dimension (if set) and the attributes data type sizes. Also we allow the setting of custom chunksizes with the create option keys "CHUNKSIZE_SP" and "CHUNKSIZE_T". The values that are required in this key-value pairs are the number of cells (integer value). If just one chunksize is stated the missing one will be calculated using the default total chunk size of 32MB and the biggest data type of the bands. In the case you don't specify a chunksize the default temporal chunksize of 1 is assumed and the spatial chunksize will be treated as missing and therefore calculated.

Examples:
```
gdal_translate (...) -co "CHUNKSIZE_SP=3000" -co "CHUNKSIZE_T=1" -of SciDB input.tif "SCIDB:array=targetArray"
gdal_translate (...) -co "CHUNKSIZE_SP=6000" -of SciDB input.tif "SCIDB:array=targetArray"
gdal_translate (...) -co -co "CHUNKSIZE_T=10" -of SciDB input.tif "SCIDB:array=targetArray"
```

### Using file metadata for time statements
If an image was uploaded into SciDB with a temporal reference, then the downloaded image file will also carry date-time information. The metadata tag *TIMESTAMP* has the temporal information in the ISO 8601 format. Similarly you can use the tags *TIMESTAMP* and *TINTERVAL* to state the date of the image and the intended temporal resolution. If this metadata attachment is set, it will be used instead of the same create options (*t* and *dt*).

Using gdal_translates assign metadata flag will help you to set the temporal metadata accordingly. Note that the naming of metadata tags is case-sensitive, which means that `TIMESTAMP` and `TINTERVAL` need to be written as stated.

```
#prepare input image
gdal_translate -mo "TIMESTAMP=2016-03-01" -mo "TINTERVAL=P1D" input.tif temporal.tif

#upload to scidb w/o date and resolution in create options
gdal_translate -co "host=https://your.host.de" -co "port=31000" -co "user=user" -co "password=passwd" -co "type=ST" -of SciDB temporal.tif "SCIDB:array=test_spatio_temporal"
```

The metadata for spatial-timeseries will look similar, but here the temporal extent and the temporal resolution will be attached instead of a simple date-time. *TS_START* marks the date of the first entry. *TS_END* marks the last date of the entry and *TINTERVAL* states the used temporal resolution. This is a special case, when `gdalinfo` was called on a SciDB array (type STS) simply make no restriction of the query in the temporal domain and then it will show the temporal extent.

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
 
