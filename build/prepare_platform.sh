#!/bin/bash
# This file sets up the platform for automated builds with travis ci.
# It installs dependencies, downloads GDAL sources, and adds the SciDB driver. 
# To build GDAL including the SciDB driver, run ./configure, make, make install in the gdaldev subfolder.
# The script must be executed from the repository root directory!

# 1. install dependencies
sudo apt-get update -qq
sudo apt-get install -y --no-install-recommends -y libboost-dev  libcurl4-openssl-dev

# 2. download GDAL source code
wget -N http://download.osgeo.org/gdal/2.1.0/gdal-2.1.0.tar.gz
tar -xf gdal-2.1.0.tar.gz
mv gdal-2.1.0 gdaldev

# 3. add scidb driver to GDAL source; given line numbers are very likely to change with GDAL versions!
sed -i "37i void CPL_DLL GDALRegister_SciDB(void);" gdaldev/gcore/gdal_frmts.h
sed -i "68i #ifdef FRMT_scidb" gdaldev/frmts/gdalallregister.cpp
sed -i "69i GDALRegister_SciDB();" gdaldev/frmts/gdalallregister.cpp
sed -i "70i #endif" gdaldev/frmts/gdalallregister.cpp
sed -i "566i GDAL_FORMATS += scidb" gdaldev/GDALmake.opt.in

# 4. copy scidb driver sources to gdal sources
cp -R src gdaldev/frmts/scidb


# cd gdaldev && ./configure && make && make install