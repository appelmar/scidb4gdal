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

#ifndef SHIM_CLIENT_H
#define SHIM_CLIENT_H


#include <string>
#include <iostream>
#include <vector>
#include <curl/curl.h>
#include <inttypes.h>
#include <sstream>
#include <stack>

#include "affinetransform.h"
#include "utils.h"


#define SHIMENDPOINT_NEW_SESSION        "/new_session"
#define SHIMENDPOINT_EXECUTEQUERY       "/execute_query"
#define SHIMENDPOINT_READ_LINES         "/read_lines "
#define SHIMENDPOINT_READ_BYTES         "/read_bytes"
#define SHIMENDPOINT_RELEASE_SESSION    "/release_session"
#define SHIMENDPOINT_LOGIN          "/login"
#define SHIMENDPOINT_LOGOUT             "/logout"
#define SHIMENDPOINT_UPLOAD_FILE        "/upload_file"
#define SHIMENDPOINT_VERSION            "/version"

#define CURL_RETRIES 3
//#define CURL_VERBOSE  // Uncomment this line if you want to debug CURL requests and responses



namespace scidb4gdal
{

    using namespace std;

    /**
     * A structure for storing metadata of a SciDB array attribute
     */
    struct SciDBAttribute {
        string name;
        string typeId;
        bool nullable;
    };

    struct SciDBAttributeStats {
        double min, max, mean, stdev;
    };

    /**
    * A structure for storing metadata of a SciDB array dimension
    */
    struct SciDBDimension {
        string name;
        int64_t low;
        int64_t high;
        uint32_t chunksize;
        string typeId;

    };


    /**
    * A structure for storing general metadata of a SciDB array
    */
    struct SciDBArray {
        string name;
        vector<SciDBAttribute> attrs;
        vector<SciDBDimension> dims;

        string toString() {
            stringstream s;
            s << "'" << name << "'" << ":";
            for ( uint32_t i = 0; i < dims.size(); ++i ) s << "<'" << dims[i].name << "'," << dims[i].low << ":" << dims[i].high << "," << dims[i].typeId << ">";
            for ( uint32_t i = 0; i < attrs.size(); ++i ) s << "['" << attrs[i].name << "'," << attrs[i].typeId << "," << attrs[i].nullable << "]";
            s << "\n";
            return s.str();
        }

        string getFormatString() {
            stringstream s;
            s << "(";
            for ( uint32_t i = 0; i < attrs.size() - 1; ++i ) {
                s << attrs[i].typeId << ","; // TODO: Add nullable
            }
            s << attrs[attrs.size() - 1].typeId; // TODO: Add nullable
            s << ")";
            return s.str();
        }
    };

    /**
    * A structure for storing spatial reference of a SciDB array
    */
    struct SciDBSpatialReference {
        string srtext;
        string proj4text; // TODO: Fill while reading a dataset or already done?
        string xdim;
        string ydim;
        string auth_name; // TODO: Fill while reading a dataset
        uint32_t auth_srid; // TODO: Fill while reading a dataset

        AffineTransform affineTransform;

        string toString() {
            stringstream s;
            s << "SPATIAL REFERENCE (" << xdim << "," << ydim << ") :" << affineTransform.toString() << "-->" << proj4text;
            s << "\n";
            return s.str();
        }

        bool isSpatial() {
            return ( xdim != "" && ydim != "" && ( srtext != "" || proj4text != "" ) );
        }
    };



    /**
     * A structure for storing metadata of a spatially referenced SciDB array
     */
    struct SciDBSpatialArray : SciDBArray, SciDBSpatialReference {

        SciDBSpatialArray() : _x_idx ( -1 ), _y_idx ( -1 ) {}

        string toString() {
            stringstream s;
            s << SciDBArray::toString();
            s << SciDBSpatialReference::toString();
            s << "\n";
            return s.str();
        }

        SciDBDimension getYDim() {
            if ( _y_idx < 0 ) deriveDimensionIndexes();
            return dims[_y_idx];
        }
        SciDBDimension getXDim() {
            if ( _x_idx < 0 ) deriveDimensionIndexes();
            return dims[_x_idx];
        }

        int getXDimIdx() {
            if ( _x_idx < 0 ) deriveDimensionIndexes();
            return _x_idx;
        }


        int getYDimIdx() {
            if ( _y_idx < 0 ) deriveDimensionIndexes();
            return _y_idx;
        }

    private:
        int _x_idx;
        int _y_idx;


        void deriveDimensionIndexes() {
            _x_idx = 0;
            _y_idx = 1;
            if ( xdim != "" && ydim != "" ) {
                for ( int i = 0; i < 2; ++i ) { // Assuming 2 dimensions!!!
                    if ( dims[i].name == xdim ) _x_idx = i;
                    if ( dims[i].name == ydim ) _y_idx = i;
                }
                // TODO: Assert x_idx != y_idx
            }
            else { // Try default dimension names
                for ( int i = 0; i < 2; ++i ) { // Assuming 2 dimensions!!!
                    if ( dims[i].name == SCIDB4GDAL_DEFAULT_XDIMNAME ) _x_idx = i;
                    if ( dims[i].name == SCIDB4GDAL_DEFAULT_YDIMNAME ) _y_idx = i;
                }
                // TODO: Assert x_idx != y_idx
            }
        }




    };



    /**
     * Basic Shim client class
     */
    class ShimClient
    {

    public:

        /**
         * Default constructor for the Shim client, initializes all members with default values.
         */
        ShimClient();


        /**
         * Custom constructor for the Shim client providing connection information as arguments.
         * Currently, only HTTP digest authentification is supported, HTTPS with PAM authentification should be added in the future.
         * @param host URL of shim e.g. http://localhost
         * @param port integer port to connect to shim
         * @param user username
         * @param passwd password
         */
        ShimClient ( string host, uint16_t port, string user, string passwd );

        /**
         * Default destructor f Shim clients.
         */
        ~ShimClient();


        /**
         * Requests metadata for a given array from shim.
         * Metadata include dimensions, attributes, and spatial reference information.
         * @param inArrayName name of a SciDB array
         * @return metadata of an array as SciDBSpatialArray instance, spatial reference information can be missing if not found
         */
        SciDBSpatialArray getArrayDesc ( const string &inArrayName );

        /**
         * Gets a list of all spatially referenced arrays, currently not needed!
         */
        //vector<SciDBSpatialArray> getArrayList ( const string *outList );



        //void getData ( SciDBSpatialArray &array, uint8_t nband, void *outchunk );

        /**
        * Retreives single attribute data from shim for a given bounding box
         * @param array metadata of an existing array
         * @param nband index of the requested attribute (starting with 0).
         * @param outchunk pointer to a chunk of memory that gets result data, must be allocated before(!) calling this function, which is usually done by GDAL
         * @param xmin left boundary, we assume x to be "easting" which is different from GDAL!
         * @param ymin lower boundary, we assume y to be "northing" which is different from GDAL!
         * @param xmax right boundary, we assume x to be "easting" which is different from GDAL!
         * @param ymax upper boundary, we assume y to be "northing" which is different from GDAL!
         */
        void getData ( SciDBSpatialArray &array, uint8_t nband, void *outchunk, int32_t x_min, int32_t y_min, int32_t x_max, int32_t y_max );


        SciDBAttributeStats getAttributeStats ( SciDBSpatialArray &array, uint8_t nband );

        /**
         * Initializes cURL's easy interface, should be performaed before each web service request
         */
        void curlBegin();

        /**
         * Cleans up cURL's easy interface, should be performaed after each web service request
         */
        void curlEnd();


        /**
         * Wrapper function around curl_easy_perform that retries requests and includes some error handling
         */
        CURLcode curlPerform();


        /**
         * Tests a shim connection by requesting version information
         */
        void testConnection();


        /**
         * Creates a new SciDB array
         * @param array metadata of the new array
         */
        void createArray ( SciDBSpatialArray &array );

        /**
         * Inserts a chunk of data to an existing array
         * @param array metadata of an existing array
         * @param inchunk pointer to a chunk of memory that holds data in scidb binary format
         * @param xmin left boundary, we assume x to be "easting" which is different from GDAL!
         * @param ymin lower boundary, we assume y to be "northing" which is different from GDAL!
         * @param xmax right boundary, we assume x to be "easting" which is different from GDAL!
         * @param ymax upper boundary, we assume y to be "northing" which is different from GDAL!
         */
        void insertData ( SciDBSpatialArray &array, void *inChunk, int32_t x_min, int32_t y_min, int32_t x_max, int32_t y_max );


        /**
         * Updates the spatial reference system of an array
         * @param array metadata including its srs to be updated
         */
        void updateSRS ( SciDBSpatialArray &array );



    protected:

        /**
         * Gets metadata of an array's attributes
         * @param inArrayName name of existing array
         * @return list of SciDBAtribute descriptors
         * @see SciDBAttribute
         */
        vector<SciDBAttribute> getAttributeDesc ( const string &inArrayName );

        /**
         * Gets metadata of an array's dimensions
         * @param inArrayName name of existing array
         * @return list of SciDBDimension descriptors
         * @see SciDBDimension
         */
        vector<SciDBDimension> getDimensionDesc ( const string &inArrayName );

        /**
         * Gets metadata of an array's spatial reference if available. Otherwise, result contains default values representing no reference.
         * @param inArrayName name of existing array
         * @return Spatial reference description
         * @see SciDBSpatialReference
         */
        SciDBSpatialReference  getSRSDesc ( const string &inArrayName );

        /**
         * Creates a new shim session and returns its ID
         * @return integer session ID
         */
        int newSession();

        /**
         * Releases an existing shim session
         * @param sessionID integer session ID
         */
        void releaseSession ( int sessionID );







        // TODO: Add general executeQuery(), readBytes() functions to make code less redundant



    private:

        string      _host;
        uint16_t    _port;
        string      _user;
        string      _passwd;
        CURL       *_curl_handle;

        bool _curl_initialized;


    };
}

#endif
