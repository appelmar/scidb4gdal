#!/bin/bash
# This file builds GDAL including SciDB driver with minimal other drivers to reduce TravisCI build time.
# The script must be executed from the repository root directory!
# Currently, option --without-ogr breaks the build

cd gdaldev

./configure --with-threads \
            --with-curl \
            --without-geos \
            --without-libtool \
            --without-gif \
            --without-pg \
            --without-grass \
            --without-libgrass \
            --without-cfitsio \
            --without-pcraster \
            --without-netcdf \
            --without-ogdi \
            --without-fme \
            --without-hdf4 \
            --without-hdf5 \
            --without-jasper \
            --without-ecw \
            --without-kakadu \
            --without-mrsid \
            --without-jp2mrsid \
            --without-bsb \
            --without-grib \
            --without-mysql \
            --without-ingres \
            --without-xerces \
            --without-expat \
            --without-odbc \
            --without-sqlite3 \
            --without-dwgdirect \
            --without-panorama \
            --without-idb \
            --without-sde \
            --without-perl \
            --without-php \
            --without-python \
            --without-xml2 \
            --with-hide-internal-symbols
			
			
make

