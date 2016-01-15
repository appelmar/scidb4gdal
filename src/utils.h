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

#include <cmath>
#include <cfloat>

#define SCIDB4GDAL_DEFAULT_XDIMNAME "x"
#define SCIDB4GDAL_DEFAULT_YDIMNAME "y"
#define SCIDB4GDAL_DEFAULT_ZDIMNAME "z" // not used by gdal
#define SCIDB4GDAL_DEFAULT_TDIMNAME "t" // not used by gdal

//#define SCIDB4GDAL_DEFAULT_BLOCKSIZE_X 512
//#define SCIDB4GDAL_DEFAULT_BLOCKSIZE_Y 512

#define SCIDB4GDAL_DEFAULT_BLOCKSIZE 512
#define SCIDB4GEO_DEFAULT_CHUNKSIZE_MB 32 // This is an upper limit, SciDB recommends smaller chunks ~ 10 MB, but for ingestion and download, larger chunks turned out to be faster.


#define SCIDB4GDAL_DEFAULT_UPLOAD_FILENAME "scidb4gdal_temp.bin"
#define SCIDB4GDAL_ARRAYSUFFIX_TEMP "_temp"
#define SCIDB4GDAL_ARRAYSUFFIX_TEMPLOAD "_tempload"
#define SCIDB4GDAL_ARRAYSUFFIX_COLLECTION_INTEGRATION "_integrate"

//#define SCIDB4GDAL_ARRAY_PREFIX "GDAL_" // Names of created arrays get a prefix, not yet implemented

#define SCIDB4GDAL_MAINMEM_HARD_LIMIT_MB 1024

#define SCIDB_MAX_DIM_INDEX 4611686018427387903


#define SCIDB4GDAL_DEFAULTNODATA_INT8      -pow(2,7)
#define SCIDB4GDAL_DEFAULTNODATA_INT16     -pow(2,15)
#define SCIDB4GDAL_DEFAULTNODATA_INT32     -pow(2,31)
#define SCIDB4GDAL_DEFAULTNODATA_INT64     -pow(2,63)
#define SCIDB4GDAL_DEFAULTNODATA_UINT8      pow(2,8)
#define SCIDB4GDAL_DEFAULTNODATA_UINT16     pow(2,16)
#define SCIDB4GDAL_DEFAULTNODATA_UINT32     pow(2,32)
#define SCIDB4GDAL_DEFAULTNODATA_UINT64     pow(2,64)
#define SCIDB4GDAL_DEFAULTNODATA_FLOAT      nan("")
#define SCIDB4GDAL_DEFAULTNODATA_DOUBLE     nanf("")


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
#include <windows.h> // TODO: Should we define WIN32_LEAN_AND_MEAN?
#define WIN32_LEAN_AND_MEAN
#else
#include <unistd.h>
#endif



namespace scidb4gdal
{
    using namespace std;

    enum StatusCode {
	/** Successfully executed a function */
        SUCCESS                             = 0,
	/** A function is currently pending */
	PENDING 			= 1,
	/** Error statement that input array is unknown */
        ERR_READ_ARRAYUNKNOWN               = 100 + 1,
	/** Error code if a index for a dimension is out of bounds */
        ERR_READ_WRONGDIMENSIONALITY        = 100 + 2,
	/** Error code if the user input of the bounding box is not valid */
	ERR_READ_BBOX			= 100 + 3,
	/** Error code if a bounding box was stated, but there was no spatial reference to interprete the coordinates */
	ERR_READ_BBOX_SRS_MISSING	= 100 + 4,
	/** Error code for a unspecified */
        ERR_READ_UNKNOWN                    = 100 + 99,

        ERR_CREATE_ARRAYEXISTS              = 200 + 1,
        ERR_CREATE_WRONGDIMENSIONALITY      = 200 + 2,
        ERR_CREATE_INVALIDARRAYNAME         = 200 + 3,
	ERR_CREATE_AUTOCLEANUPFAILED 		= 200 + 4,
	ERR_CREATE_AUTOCLEANUPSUCCESS		= 200 + 5,
	ERR_CREATE_TERMINATEDBYUSER		= 200 + 6,
	ERR_CREATE_TEMPARRAY			= 200 + 7,
	ERR_CREATE_NOARRAY			= 200+8,
	ERR_CREATE_ARRAY_NOT_INSERTABLE		= 200 + 9,
        ERR_CREATE_UNKNOWN                  = 200 + 99,

        ERR_GLOBAL_CANNOTCONNECT            = 300 + 1,
        ERR_GLOBAL_INVALIDARRAYNAME         = 300 + 2,
        ERR_GLOBAL_DATATYPEMISMATCH         = 300 + 3,
        ERR_GLOBAL_INVALIDCONNECTIONSTRING  = 300 + 4,
	ERR_GLOBAL_PARSE			= 300 +5,
	ERR_GLOBAL_NO_SCIDB4GEO			= 300 +6,

        ERR_GLOBAL_UNKNOWN                  = 300 + 99,


        ERR_SRS_NOSPATIALREFFOUND           = 400 + 1,
        ERR_SRS_NOSCIDB4GEO                 = 400 + 2,
	ERR_SRS_INVALID			    = 400 + 3,
	  
	ERR_TRS_INVALID		            = 500 + 1
	
        
    };


    namespace Utils
    {
        /**
         * @brief Maps SciDB string type identifiers to GDAL data type enumeration items.
         * @param typeId SciDB type identifier string e.g. "int32"
         * @return A GDALDataType item
         */
        GDALDataType scidbTypeIdToGDALType ( const string &typeId );


        /**
         * @brief Maps GDAL data type to SciDB string type identifiers.
         * @param type value of GDALDataType enumeration
         * @return SciDB type identifier string e.g. "int32"
         */
        string gdalTypeToSciDBTypeId ( GDALDataType type );


        /**
         * @brief Gets the size in bytes of given a SciDB type.
         * @param typeId SciDB type identifier string e.g. "int32"
         * @return Size in bytes
         */
        size_t scidbTypeIdBytes ( const string &typeId );


        /**
         * @brief Gets the size in bytes of given a GDAL type.
         * @param typeId SciDB type identifier string e.g. "int32"
         * @return Size in bytes
         */
        size_t gdalTypeBytes ( GDALDataType type );


	/**
	 * @brief Returns the default no data value for a GDAL data type.
	 * 
	 * @param type one of the type definitions of GDAL
	 * @return double
	 */
	double defaultNoDataGDAL ( GDALDataType type );
	
	/**
	 * @brief Returns the default no data value for a SciDB data type
	 * 
	 * @param typeId a string representation of a SciDB data type
	 * @return double
	 */
	double defaultNoDataSciDB ( const string &typeId );

	/**
	 * @brief makes an error output
	 * 
	 * Error handling function, calls CPLError() and aborts the running process
	 * 
	 * @param msg the message to print
	 * @return void
	 */
        void error ( const string &msg, bool kill = false );

	/**
	 * @brief makes an warn output
	 * 
	 * Error handling function, calls CPLWarn()
	 * 
	 * @param msg the message to print
	 * @return void
	 */
	void warn ( const string &msg ) ;

	/**
	 * @brief makes a debugging output to console
	 * 
	 * Error handling function, calls CPLDebug()
	 * 
	 * @param msg the message to print
	 * @return void
	 */
	void debug ( const string &msg ) ;

	/**
	 * @brief time out function
	 * 
	 * Utility function for sleeping after connection retries
	 * 
	 * @param ms the amount of milliseconds to wait
	 * @return void
	 */
	void sleep ( long ms );


	/**
	 * @brief Rounds up to the next power of two.
	 * 
	 * @param x integer number
	 * @return uint32_t the next higher power of two value
	 */
	uint32_t nextPow2 ( uint32_t x );
	
	/**
	 * @brief Function to validate the time string that is passed
	 * 
	 * @param in the date/time string that needs to be evaluated
	 * @return bool whether or not the passed string is valid
	 */
	bool validateTimestampString(string &in);
	

    }



}

#endif

