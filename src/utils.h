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

#define SCIDB4GDAL_DEFAULT_BLOCKSIZE_X 512
#define SCIDB4GDAL_DEFAULT_BLOCKSIZE_Y 512

#define SCIDB4GDAL_DEFAULT_UPLOAD_FILENAME "scidb4gdal_temp.bin"

//#define SCIDB4GDAL_ARRAY_PREFIX "GDAL_" // Names of created arrays get a prefix, not yet implemented

#define SCIDB4GDAL_MAINMEM_HARD_LIMIT_MB 1024 // TODO: Calculate expected array size based on dimensions and attributes and stop if larger than this value

#define SCIDB_MAX_DIM_INDEX 4611686018427387903

#include <string>
#include <iostream>
#include <vector>
#include <string>
#include <inttypes.h>
#include <exception>


#include "gdal.h"
#include "gdal_pam.h"
#include "gdal_priv.h"
#include "ogr_spatialref.h"

// Sleep function
#ifdef WIN32
#include <windows.h> // TODO: Should be define WIN32_LEAN_AND_MEAN?
#define WIN32_LEAN_AND_MEAN
#else
#include <unistd.h>
#endif



namespace scidb4gdal
{
    using namespace std;

    enum StatusCode
    {
        SUCCESS                   		= 0,

        ERR_READ_ARRAYUNKNOWN         		= 100 + 1,
        ERR_READ_WRONGDIMENSIONALITY      	= 100 + 2,
        ERR_READ_UNKNOWN             		= 100 + 99,

        ERR_CREATE_ARRAYEXISTS            	= 200 + 1,
        ERR_CREATE_WRONGDIMENSIONALITY        	= 200 + 2,
        ERR_CREATE_INVALIDARRAYNAME       	= 200 + 3,
        ERR_CREATE_UNKNOWN            		= 200 + 99,

        ERR_GLOBAL_CANNOTCONNECT          	= 300 + 1,
        ERR_GLOBAL_INVALIDARRAYNAME       	= 300 + 2,
        ERR_GLOBAL_DATATYPEMISMATCH       	= 300 + 3,
        ERR_GLOBAL_INVALIDCONNECTIONSTRING    	= 300 + 4,

        ERR_GLOBAL_UNKNOWN            		= 300 + 99,


        ERR_SRS_NOSPATIALREFFOUND   		= 400 + 1,
        ERR_SRS_NOSCIDB4GEO     		= 400 + 2
    };


    namespace Utils
    {
        /**
         * Maps SciDB string type identifiers to GDAL data type enumeration items.
         * @param typeId SciDB type identifier string e.g. "int32"
         * @return A GDALDataType item
         */
        GDALDataType scidbTypeIdToGDALType (const string& typeId);


        /**
         * Maps  GDAL data type to SciDB string type identifiers.
         * @param type value of GDALDataType enumeration
         * @return SciDB type identifier string e.g. "int32"
         */
        string gdalTypeToSciDBTypeId (GDALDataType type);


        /**
         * Gets the size in bytes of given a SciDB type.
         * @param typeId SciDB type identifier string e.g. "int32"
         * @return Size in bytes
         */
        size_t scidbTypeIdBytes (const string& typeId);


        /**
         * Gets the size in bytes of given a GDAL type.
         * @param typeId SciDB type identifier string e.g. "int32"
         * @return Size in bytes
         */
        size_t gdalTypeBytes (GDALDataType type);


        /**
        * Error handling function, calls CPLError()
         */
        void error (const string& msg, bool kill = false);

        /**
         * Error handling function, calls CPLError()
         */
        void warn (const string& msg) ;

        /**
         * Error handling function, calls CPLDebug()
         */
        void debug (const string& msg) ;

        /**
         * Utility function for sleeping after connection retries
         */
        void sleep (long ms);


        /**
         * Rounds up to the next power of two
         * @param x integer number
         */
        uint32_t nextPow2 (uint32_t x);

    }



}

#endif

