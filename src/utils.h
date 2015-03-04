/*
Copyright (c) 2015 Marius Appel <marius.appel@uni-muenster.de>

This file is part of scidb4gdal. scidb4gdal is licensed under the MIT license.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
-----------------------------------------------------------------------------*/

#ifndef UTILS_H
#define UTILS_H

#define SCIDB4GDAL_DEFAULT_XDIMNAME "x"
#define SCIDB4GDAL_DEFAULT_YDIMNAME "y"
#define SCIDB4GDAL_DEFAULT_ZDIMNAME "z" // not used by gdal
#define SCIDB4GDAL_DEFAULT_TDIMNAME "t" // not used by gdal

#define SCIDB4GDAL_MAINMEM_HARD_LIMIT_MB 1024 // TODO: Calculate expected array size based on dimensions and attributes and stop if larger than this value

#include <string>
#include <iostream>
#include <vector>
#include <string>
#include <exception>


// TODO: Make this "gdal.h" when merging into gdal frmts/scidb
#include "gdal.h"
#include "gdal_pam.h"
#include "gdal_priv.h"
/////////////////////////////7


namespace scidb4gdal
{
    using namespace std;

    namespace Utils
    {
	/**
	 * Maps SciDB string type identifiers to GDAL data type enumeration items.
	 * @param typeId SciDB type identifier string e.g. "int32"
	 * @return A GDALDataType item
	 */
        GDALDataType scidbTypeIdToGDALType ( const string &typeId );


	/**
	 * Gets the size in bytes of given a SciDB type.
	 * @param typeId SciDB type identifier string e.g. "int32"
	 * @return Size in bytes
	 */
        size_t scidbTypeIdBytes ( const string &typeId );


        /**
	 * Error handling function, should call CPLError() in the future
	 */
        void error ( const string &msg );
	
	/**
	 * Error handling function, should call CPLError() in the future
	 */
        void warn ( const string &msg ) ;
	
	/**
	 * Error handling function, should call CPLDebug() in the future
	 */
        void debug ( const string &msg ) ;




    }



}

#endif
