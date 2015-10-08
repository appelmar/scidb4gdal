#!/bin/bash
# This file sets up the platform for automated builds with travis ci.
# Must be executed from the repository root directory!

# 1. install dependencies
sudo apt-get install  -y libboost-dev  libcurl4-openssl-dev

# 2. download GDAL source code
wget http://download.osgeo.org/gdal/2.0.1/gdal-2.0.1.tar.gz
tar -xf gdal-2.0.1.tar.gz

# 3. add scidb driver to GDAL source; given line numbers are very likely to change with GDAL versions!
sed -i "37i void CPL_DLL GDALRegister_SciDB(void);" gdal-2.0.1/gcore/gdal_frmts.h
sed -i "78i #ifdef FRMT_scidb" gdal-2.0.1/frmts/gdalallregister.cpp
sed -i "79i GDALRegister_SciDB();" gdal-2.0.1/frmts/gdalallregister.cpp
sed -i "80i #endif" gdal-2.0.1/frmts/gdalallregister.cpp
sed -i "484i scidb \\\\" gdal-2.0.1/GDALmake.opt.in

# 4. copy scidb driver sources to gdal sources
cp -R src gdal-2.0.1/frmts/scidb
