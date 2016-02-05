# Loading SRTM tiles to SciDB

`srtm2scidb.py` is a python script that does nothing else than calling gdal_translate in a for-loop.


## Arguments

All arguments except the input directory and target array name are optional. Please read the command line usage of the script for further details.

Argument   | Description 
--------   | ------------
`-d`       | Directory of SRTM GeoTiff files
`-a`       | Name of the target SciDB array
`-xmin`    | Minimum x coordinate of the target array (longitude in WGS84) which should match downloaded tiles
`-ymin`    | Minimum y coordinate of the target array (latitude in WGS84) which should match downloaded tiles
`-xmax`    | Maximum x coordinate of the target array (longitude in WGS84) which should match downloaded tiles
`-ymax`    | Maximum x coordinate of the target array (latitude in WGS84) which should match downloaded tiles
`-dbhost`     | Hostname of the coordinater SciDB instance running shim
`-dbport`     | Port to interface shim
`-dbuser`     | Database username (optional)
`-dbpassword` | Database password (if needed)
`-v` | Prints verbose output if set
`-srs` | Spatial reference system (EPSG:4326 for SRTM)  


## Example
`python3 srtm2scidb.py -d /home/xyz/srtm -a srtm_array -ymin 30.0 -xmin -12.0 -ymax 61.0 -xmax 31.0 -dbhost https://localhost -dbport 8083 -dbuser scidb -dbpassword scidb -srs EPSG:4326 -v`




## Instructions

1. Download a set of SRTM tiles (e.g. from ftp://srtm.csi.cgiar.org/SRTM_V41/SRTM_Data_GeoTIFF/).
2. Unzip all GeoTiffs to a single directory.
3. Run the `srtm2scidb.py` script as in the example above.
