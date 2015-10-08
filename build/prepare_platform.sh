#!/bin/bash
# This file sets up the platform for automated builds with travis ci.
# Must be executed from the repository root directory!

# 1. install dependencies
sudo apt-get install  -y libboost-dev  libcurl4-openssl-dev

# 2. download GDAL source code
wget http://download.osgeo.org/gdal/1.11.2/gdal-1.11.2.tar.gz
tar -xf gdal-1.11.2.tar.gz

# 3. add scidb driver to GDAL source; given line numbers are very likely to change with GDAL versions!
sed -i "37i void CPL_DLL GDALRegister_SciDB(void);" gdal-1.11.2/gcore/gdal_frmts.h
sed -i "78i #ifdef FRMT_scidb" gdal-1.11.2/frmts/gdalallregister.cpp
sed -i "79i GDALRegister_SciDB();" gdal-1.11.2/frmts/gdalallregister.cpp
sed -i "80i #endif" gdal-1.11.2/frmts/gdalallregister.cpp
sed -i "484i scidb \\\\" gdal-1.11.2/GDALmake.opt.in

# 4. copy scidb driver sources to gdal sources
cp -R src gdal-1.11.2/frmts/scidb
