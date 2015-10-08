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

#include "utils.h"
#include <ctime>
#include <sstream>
#include <cctype>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>

namespace scidb4gdal
{
    using namespace std;

    namespace Utils
    {
        // see http://stackoverflow.com/questions/997946/how-to-get-current-time-and-date-in-c
        string getCurDatetime()
        {
            // Current date/time based on current system
            time_t now = time ( 0 );

            // Convert now to tm struct for local timezone
            tm *localtm = localtime ( &now );

            std::stringstream out;
            out << ( localtm->tm_year + 1900 ) << "-"  << ( localtm->tm_mon + 1 ) << "-" <<  localtm->tm_mday << " " << localtm->tm_hour << ":" << localtm->tm_min << ":" << localtm->tm_sec;
            return out.str();
        }


        void error ( const string &msg, bool kill )
        {
            //std::cout << "(" << getCurDatetime() << ") ERROR: " << msg << std::endl;
            if ( kill )
                CPLError ( CE_Fatal, CPLE_AppDefined, msg.c_str(), "" );
            else
                CPLError ( CE_Failure, CPLE_AppDefined, msg.c_str(), "" );
            //throw msg;
        }


        void warn ( const string &msg )
        {
            //std::cout << "(" << getCurDatetime() << ") WARNING: " << msg << std::endl;
            CPLError ( CE_Warning, CPLE_AppDefined, msg.c_str(), "" );
        }

        void debug ( const string &msg )
        {
            std::cout << "(" << getCurDatetime() << ") DEBUG: " << msg << std::endl;
//             CPLDebug ( "scidb4gdal", msg.c_str(), "" );
        }

        GDALDataType scidbTypeIdToGDALType ( const string &typeId )
        {
            // see src/query/TypeSystem.h of SciDB for definitions
            if ( typeId == "int8" )         return GDT_Byte; // signed vs unsigned might lead to conflicts
            else if ( typeId == "int16" )   return GDT_Int16;
            else if ( typeId == "int32" )   return GDT_Int32;
            else if ( typeId == "uint8" )   return GDT_Byte;
            else if ( typeId == "uint16" )  return GDT_UInt16;
            else if ( typeId == "uint32" )  return GDT_UInt32;
            else if ( typeId == "float" )   return GDT_Float32;
            else if ( typeId == "double" )  return GDT_Float64;
            return GDT_Unknown; // No GDAL support for int64, uint64, string
        }


        string gdalTypeToSciDBTypeId ( GDALDataType type )
        {
            if ( type == GDT_Byte )     return "uint8";
            else if ( type == GDT_UInt32 )  return "uint32";
            else if ( type == GDT_Int32 )   return "int32";
            else if ( type == GDT_Int16 )   return "int16";
            else if ( type == GDT_UInt16 )  return "uint16";
            else if ( type == GDT_Float32 ) return "float";
            else if ( type == GDT_Float64 ) return "double";
            return 0;
        }




        size_t scidbTypeIdBytes ( const string &typeId )
        {
            if ( typeId == "int8" )         return 1;
            else if ( typeId == "int16" )   return 2;
            else if ( typeId == "int32" )   return 4;
            else if ( typeId == "uint8" )   return 1;
            else if ( typeId == "uint16" )  return 2;
            else if ( typeId == "uint32" )  return 4;
            else if ( typeId == "float" )   return 4;
            else if ( typeId == "double" )  return 8;
            return 0;
        }


        size_t gdalTypeBytes ( GDALDataType type )
        {
            if ( type == GDT_Byte )     return 1;
            else if ( type == GDT_UInt32 )  return 4;
            else if ( type == GDT_Int32 )   return 4;
            else if ( type == GDT_Int16 )   return 2;
            else if ( type == GDT_UInt16 )  return 2;
            else if ( type == GDT_Float32 ) return 4;
            else if ( type == GDT_Float64 ) return 8;
            return 0;
        }



        double defaultNoDataGDAL ( GDALDataType type )
        {
            switch ( type ) {
            case ( GDT_UInt16 ) :
                return SCIDB4GDAL_DEFAULTNODATA_UINT16;
            case ( GDT_UInt32 ) :
                return SCIDB4GDAL_DEFAULTNODATA_UINT32;
            case ( GDT_Int16 ) :
                return SCIDB4GDAL_DEFAULTNODATA_INT16;
            case ( GDT_Int32 ) :
                return SCIDB4GDAL_DEFAULTNODATA_INT32;
            case ( GDT_Byte ) :
                return SCIDB4GDAL_DEFAULTNODATA_UINT8;
            case ( GDT_Float32 ) :
                return SCIDB4GDAL_DEFAULTNODATA_FLOAT;
            case ( GDT_Float64 ) :
                return SCIDB4GDAL_DEFAULTNODATA_DOUBLE;
            default:
                return 0;
            }
            return 0;
        }

        double defaultNoDataSciDB ( const string &typeId )
        {
            return defaultNoDataGDAL ( scidbTypeIdToGDALType ( typeId ) );
        }


        void sleep ( long int ms )
        {
#ifdef WIN32
            Sleep ( ms );
#else
            usleep ( ms * 1000 );
#endif
        }




        uint32_t nextPow2 ( uint32_t x )
        {
            if ( ! ( x & ( x - 1 ) ) ) {
                return ( x );
            }
            while ( x & ( x - 1 ) ) {
                x = x & ( x - 1 );
            }
            x = x << 1;
            return x;
        }

	bool validateTimestampString(string &in) {
	  /*
	   * comment: use REGEX instead! But gcc 4.84 shipped with Ubuntu 14.04 does not support reg expressions...
	   * gcc > 4.9 uses it...
	   */
	  bool isValid = true;
	  boost::algorithm::trim(in);
	  //check date String
	  //year
	  if (!(isdigit(in[0]) && isdigit(in[1]) && isdigit(in[2]) && isdigit(in[3]) )) {
	    Utils::debug("malformed year");
	    return false;
	  }
	  //month
	  if (!(isdigit(in[5]) && isdigit(in[6]))) {
	    Utils::debug("malformed month");
	    return false;
	  }
	  //days
	  if (!(isdigit(in[8]) && isdigit(in[9]))) {
	    Utils::debug("malformed day");
	    return false;
	  }
	  
	  if (!((in[4] == '-' && in[7] == '-') || ( in[4] == '/' && in[7] == '/'))) {
	    Utils::debug("malformed date divider");
	    return false;
	  }
	  
	  //check length if end then stop
	  if (in.size() == 10) return isValid;
	  
	  //if contains T at position 10 check the time string
	  if (in[10] != 'T') {
	    Utils::debug("malformed date divider");
	    return false;
	  }
	  
	  //hours
	  if (!(isdigit(in[11]) && isdigit(in[12]))) {
	    Utils::debug("malformed hour");
	    return false;
	  }
	  //minutes
	  if (!(isdigit(in[14]) && isdigit(in[15]))) {
	    Utils::debug("malformed minute");
	    return false;
	  }
	  //seconds
	  if (!(isdigit(in[17]) && isdigit(in[18]))) {
	    Utils::debug("malformed seconds");
	    return false;
	  }
	  
	  if (!boost::algorithm::contains(in.substr(13,1), ":") || !boost::algorithm::contains(in.substr(16,1), ":")) {
	    Utils::debug("malformed time divider");
	    return false;
	  }
	  
	  return isValid;
      }






    }
}
