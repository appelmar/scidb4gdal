# Batch Upload Script
This Python script was created with the intention to upload multiple images, intentionally of the same sensor, into a SciDB array using the SciDB GDAL driver.

## Description
The script offers functionalities to upload temporally and spatially referenced data into an SciDB array. Rather than performing multiple GDAL commands manually, this script will automate the procedure alot.

## Prerequisites

There are several prerequisites that have to be met before you can upload the data. The prerequisites are divided into software and data prerequisites.

### Software

For the script you need the SciDB-driver extension for GDAL and also on the server site, you need a SciDB database (>= version 14.12) with the SCIDB4GEO extension. For GDAL, please make sure that the PATH variables are set, meaning that you can call for example `gdalinfo` without further path suffix. Here, we provide some links on how to [install GDAL from source](https://trac.osgeo.org/gdal/wiki/BuildHints) and how to [install the SciDB driver](https://github.com/mappl/scidb4gdal/blob/master/README.md).
This script also needs Python v2.7 or newer.

### Data

First, the data needs to be stored in one folder. The script will scan for all GDAL readable sources in the assigned folder and it will try to upload the data into the designated target array.

Then it is assumed that the data will have spatial and temporal dimensions. Satellite imagery products are often available at different levels of quality, but in most cases the data will already be georeferenced. This means the image has a spatial Bounding Box and an assigend spatial reference system. However, the temporal information needs to be added manually into the file, in the way the SciDB driver for GDAL can recognize. This means that in the metadata of the image the key-value pairs for the time ('TIMESTAMP') and the temporal interval ('TINTERVAL') needs to be set otherwise the script cannot reproduce the temporal dimension and hence will fail to execute the upload procedure. 

Different attribute data needs to be stacked. To make this more clear imagine Landsat data products that usually come as multiple single band files (a file contains a certain band of the sensors spectral configuration). To clarify the fact that the bands are the attributes for the SciDB assignment rather than different images the bands need to be stacked as a single file. An example on how to stack bands, you can find in the Examples section.

Also it might be beneficial to provide the data referenced with the same spatial reference system. For example the UTM system is divided into various UTM zones to account for the Earths shape and to minimize distorition. However storing the imagery data in a data base requires the images to be put into discrete spatial space (usually cell indices). By having two different SRS in the data SciDB will have problems to transform the data by itself so the transformation needs to be done locally. This can be done either by using `gdalwarp` or by stating a target SRS as a parameter. The automated transformation by this script will usually work fine, but if you need a more elaborated transformation, then it is recommended to use a more customizable `gdalwarp` command.

## Using the Script
The script can be configured by using different parameters. The following table shows all possible command options and their intended meaning.

### Parameter
| Parameter | Abbreviation | Required | Meaning |
| --- | --- | --- | --- |
| --help= | | no | The help file containing similar information like this table. |
| --dir= | -d | yes | The input directory. |
| --array= | -a	| yes | The name of the target array. |
| --host= | -h | *1 | URL for the SHIM client. |
| --port= | -p | *1 | The SHIM client port on the host |
| --user= | -u | *1 | A registered user at the SHIM client. |
| --pwd= | -w | *1 | The users password. |
| --border= | | no | The size of the border in percent. Default 0.5% = 0.005 |
| --chunk_sp= | | no | The spatial chunksize for the target array. As a default the size will be calculated automatically by the driver based on the image properties. |
| --chunk_t= | | no | The temporal chunksize for the target array. As a default assumed to be 1 for optimal upload performance. |
| --t_srs= | | no | The target spatial reference system in case the images have different spatial reference systems |
| --add= | | no | Boolean value to mark, whether or not the images shall be uploaded into an existing SciDB array. Allowed case-insensitive variables (T,TRUE,1,Y,YES) otherwise it is marked as false. |
| --type=| | yes | The type of the array that needs to be created. Use 'S' to create a purely spatial array (no temporal dimension) or use 'ST' or 'STS' to create a spatio-temporal array. |
| --product= | | no *2 | LANDSAT / MODIS or CUSTOM. If set then the temporal information will be extracted from the well defined file name for those products.
| --regexp= | | no *2 | In case you want to use a custom regular expression to read the temporal information, then use regexp, replacement and t_interval. Please, use group naming in order to address groups for the replace statement |
| --replace= | | no *2 | Another regular expression that will be used to transform the file name with the regular expression into a valid ISO 8601 date-time string |
| --t_interval= | | no *2 | The temporal resolution that will be used passed as a valid ISO 8601 interval string. |

Note 1: The connection details (host, port, user and password) are required! However, it is not necessary to use the parameters to do so. The SciDB driver for GDAL also allows the use of environment variables. In case they are set, explicitly stated parameters will be treated as the preferred information source.

Note 2: If you use a custom regular expression, make sure to use all parameters: product=CUSTOM, regexp, replace and t_interval.

### Information on automated spatial transformation
The automated transformation is a simplified transformation and as that the following parameter and settings for the `gdalwarp` command are fixed:

- output format is GeoTiff
- intermediate warped file is named after the original with '_warped' appended to the file name
- the resampling method is bilinear
- the target reference system will be passed directly from the batch_upload.py parameter (`--t_srs=`) to gdalwarp
- warped files will not be deleted, but if they were transformed as a result of this script, then the original image is excluded in future batch_upload.py calls

### Additional Notes
The `--add=` parameter is quite essential, because this triggers also whether or not the array with the name you used in this call is deleted on the server.

The `--border=` parameter is a tolerance value of how much the bounding box is extended due to possible discretization errors.

If SCIDB4GDAL_HOST, SCIDB4GDAL_PORT, SCIDB4GDAL_USER or SCIDB4GDAL_PASSWD are set, then respective parameters are omissible. If they are still set, then they will be treated as preferred and superimpose the environment variables.

## Examples
The following examples shall be used as an orientation on how to perform certain queries, rather than ready-to-use commands.

### Stacking separated bands using GDAL (Landsat)
```
gdal_merge.py -separate -of GTiff -a_nodata 0 -o <foldername>.tif <foldername>/*_band1.tif <foldername>/*_band2.tif <foldername>/*_band3.tif <foldername>/*_band4.tif <foldername>/*_band5.tif <foldername>/*_band61.tif <foldername>/*_band62.tif <foldername>/*_band7.tif
```

Note: <foldername> needs to be replaced by the actual folder name and also the 'bandX' suffix may differ from your data. Further information on how to use `gdal_merge` you can find [here](http://www.gdal.org/gdal_merge.html).

### Annotating temporal information to an image

```
gdal_edit.py -mo TIMESTAMP=2016-04-20 -mo TINTERVAL=P1D your_file.tif
```

Note: The keys 'TIMESTAMP' and 'TINTERVAL' are essential and they need to be written in capital letters. The timestamp and the temporal interval need to be formulated as a valid ISO 8601 string. Also further information on how to use gdal_edit, you can find [here](http://www.gdal.org/gdal_edit.html).

### Manual Spatial Transformation

```
gdalwarp -of GTiff -t_srs EPSG:4326 -r bilinear input.tif output.tif
```

Note: More information on gdalwarp you can find [here](http://www.gdal.org/gdalwarp.html). In order to minimize distortions due to the spatial reference system, you should consider global reference systems (like EPSG:4326 or EPSG:3395) for long-range coverage and other suitable systems for medium (e.g. NAD27 or UTM) and local coverage. Please choose the reference system wisely according to your intended observation area.

### Automated Spatial Transformation

```
batch_upload.py -d /your/image/path -a target_array --t_srs EPSG:3395 --border=0.01
```

### Batch Upload Examples

Minimal with environment connection variables
```
batch_upload.py -d /your/image/path -a target_array --type=ST
```

Minimal w/o environment connection variables
```
batch_upload.py -h https://www.host.net/ -p 32000 -u user -w password -d /your/image/path -a target_array --type=ST
batch_upload.py --host=https://www.host.net/ --port=32000 --user=user --pwd=password --dir=/your/image/path --array=target_array --type=ST
```

Adding to existing array
```
batch_upload.py -d /your/image/path -a target_array --type=S --add=T
batch_upload.py -d /your/image/path -a target_array --type=S --add=TRUE
batch_upload.py -d /your/image/path -a target_array --type=S --add=YES
batch_upload.py -d /your/image/path -a target_array --type=S --add=Y
batch_upload.py -d /your/image/path -a target_array --type=S --add=1
```

Using file names as temporal information
    ```
    ./batch_upload.py -d /your/image/path -a target_array --type=ST --regexp="^.{9}(?P<y>[\d]{4})(?P<doy>[\d]{3}).*" --replace="\g<y>-\g<doy>" --t_interval=P1D --product=CUSTOM
    ```
    This command will use a custom version of the temporal information extraction by filename. It will use the regular expression of `regexp` to match each filename. By using named groups (?P<parameter_name>) the second regular expression provided by `replace` can call the named groups, so that a ISO 8601 conform date or date-time string can be created.
    
    You can also use predefined products like MODIS or LANDSAT, since the filenaming for those products follows a certain standard.
    ```
    ./batch_upload.py -d /your/image/path -a target_array --type=ST --product=MODIS
    ./batch_upload.py -d /your/image/path -a target_array --type=ST --product=LANDSAT
    ./batch_upload.py -d /your/image/path -a target_array --type=ST --product=LANDSAT --t_interval=P16D
    ```
    As a default the temporal resolution for those products is one day ("P1D"), but with `t_interval` you can change this according to your data set.
    
    Please make sure you use valid RegExpression, because those will not be validated in the code. To get started with regular expression you might want to use [this](https://en.wikipedia.org/wiki/Regular_expression) or [this](https://docs.python.org/2/library/re.html) as a starting point.