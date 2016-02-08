#!/bin/bash
# This file sets up the platform for automated builds with travis ci.
# It installs dependencies, downloads GDAL sources, and adds the SciDB driver. 
# To build GDAL including the SciDB driver, run ./configure, make, make install in the gdal-2.0.1 subfolder.
# The script must be executed from the repository root directory!

# 1. install dependencies
sudo apt-get update -qq
sudo apt-get install -y --no-install-recommends -y libboost-dev  libcurl4-openssl-dev

# 2. download GDAL source code
wget -N http://download.osgeo.org/gdal/2.0.1/gdal-2.0.1.tar.gz
tar -xf gdal-2.0.1.tar.gz
mv gdal-2.0.1 gdaldev

# 3. add scidb driver to GDAL source; given line numbers are very likely to change with GDAL versions!
sed -i "37i void CPL_DLL GDALRegister_SciDB(void);" gdaldev/gcore/gdal_frmts.h
sed -i "62i #ifdef FRMT_scidb" gdaldev/frmts/gdalallregister.cpp
sed -i "63i GDALRegister_SciDB();" gdaldev/frmts/gdalallregister.cpp
sed -i "64i #endif" gdaldev/frmts/gdalallregister.cpp
sed -i "498i scidb \\\\" gdaldev/GDALmake.opt.in

# 4. copy scidb driver sources to gdal sources
cp -R src gdaldev/frmts/scidb


# ./configure && make && make install