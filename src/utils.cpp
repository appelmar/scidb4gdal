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

namespace scidb4gdal
{
    using namespace std;

    namespace Utils
    {
        // see http://stackoverflow.com/questions/997946/how-to-get-current-time-and-date-in-c
        string getCurDatetime()
        {
            // Current date/time based on current system
            time_t now = time(0);

            // Convert now to tm struct for local timezone
            tm* localtm = localtime(&now);

            std::stringstream out;
            out << (localtm->tm_year + 1900) << "-"  << (localtm->tm_mon + 1) << "-" <<  localtm->tm_mday << " " << localtm->tm_hour << ":" << localtm->tm_min << ":" << localtm->tm_sec;
            return out.str();
        }


        void error(const string& msg)
        {
            //std::cout << "(" << getCurDatetime() << ") ERROR: " << msg << std::endl;
            CPLError(CE_Fatal, CPLE_AppDefined, msg.c_str());
            throw msg;
        }


        void warn(const string& msg)
        {
            //std::cout << "(" << getCurDatetime() << ") WARNING: " << msg << std::endl;
            CPLError(CE_Warning, CPLE_AppDefined, msg.c_str());
        }

        void debug(const string& msg)
        {
            //std::cout << "(" << getCurDatetime() << ") DEBUG: " << msg << std::endl;
            CPLDebug("scidb4gdal", msg.c_str());
        }

        GDALDataType scidbTypeIdToGDALType(const string& typeId)
        {
            // see src/query/TypeSystem.h of SciDB for definitions
            if (typeId == "int8")         return GDT_Byte;   // signed vs unsigned might lead to conflicts
            else if (typeId == "int16")   return GDT_Int16;
            else if (typeId == "int32")   return GDT_Int32;
            else if (typeId == "uint8")   return GDT_Byte;
            else if (typeId == "uint16")  return GDT_UInt16;
            else if (typeId == "uint32")  return GDT_UInt32;
            else if (typeId == "float")   return GDT_Float32;
            else if (typeId == "double")  return GDT_Float64;
            return GDT_Unknown; // No GDAL support for int64, uint64, string
        }

        size_t scidbTypeIdBytes(const string& typeId)
        {
            if (typeId == "int8")         return 1;   
            else if (typeId == "int16")   return 2;
            else if (typeId == "int32")   return 4;
            else if (typeId == "uint8")   return 1;
            else if (typeId == "uint16")  return 2;
            else if (typeId == "uint32")  return 4;
            else if (typeId == "float")   return 4;
            else if (typeId == "double")  return 8;
            return 0;
        }
    }
}
