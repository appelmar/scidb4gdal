/*
Copyright (c) 2015 Marius Appel <marius.appel@uni-muenster.de> and
Florian Lahn <florian.lahn@uni.muenster.de>

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
#include <map>
#include "shim_client_structs.h"
#include "scidb_structs.h"

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
     * @brief Basic Shim client class
     * 
     * SciDB allows for two major interfaces to interact with the database. One is the commandline interface 'iquery', a specific interface to execute 
     * AFL or AQL queries, and the other one is a web client implemented in Python, called SHIM. This class offers functions to interact with the latter 
     * interface. For querying the SHIM web client the external library cURL is used. The web client offers different endpoints to either do user 
     * authentication actions, to execute queries, to upload files and to read data in plain text or as bytes.
     * For all the endpoints this class offers different functions to call the endpoints and especially at the execute-query-endpoint this class offers lots 
     * of function to either create, manipulate or delete SciDB arrays.
     * To perform all those interaction the SHIM client representation needs authentication credentials and other task dependent information, that are captured
     * in the scidb4gdal::ConnectionParameters, scidb4gdal::QueryParameters and scidb4gdal::CreationParameters, which will be created by the scidb4gdal::ParameterParser
     * that reads and evaluates the parameters from the command.
     * 
     * Information about arrays:
     * ShimClient::getArrayDesc -- array structure
     * ShimClient::getArrayMD -- array meta data
     * ShimClient::getAttributeDesc -- attribute information
     * ShimClient::getAttributeStats -- statistics on the attribute
     * ShimClient::arrayExists -- existence of arrays
     * 
     * Array creation:
     * ShimClient::createTempArray -- creation of temporary arrays
     * ShimClient::persistArray -- persiting temporary arrays
     * ShimClient::insertData -- upload of data into the specified chunks
     * 
     * Array manipulation:
     * ShimClient::setArrayMD -- setting metadata for an array
     * ShimClient::setAttributeMD -- setting metadata for an attribute / band
     * ShimClient::insertInto -- inserting an array into another one based on their spatio-temporal coordinates
     * ShimClient::updateSRS -- annotating the array with a spatial reference
     * ShimClient::updateTRS -- annotating the array with a temporal reference
     * 
     * Array deletion:
     * ShimClient::removeArray -- deleting an array from the database
     * 
     * Data retrieveal:
     * ShimClient::getData -- retreiving data based on a bounding box of image coordinates
     * 
     * @author Marius Appel, IfGI Muenster
     * @author Florian Lahn, IfGI Muenster
     */
    class ShimClient
    {

    public:

        /**
         * @brief Basic constructor
	 * 
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
        ShimClient ( string host, uint16_t port, string user, string passwd, bool ssl);
	
	/**
	 * @brief Constructor with scidb4gdal::ConnectionParameters
	 * 
	 * Creates a new SHIM client by passing the authentication parameters for the SHIM client.
	 * 
	 * @param scidb4gdal::ConnectionParameters
	 */
	ShimClient (ConnectionParameters *con);
	
        /**
         * Default destructor f Shim clients.
         */
        ~ShimClient();


        /**
	 * @brief Retreives the basic image information from SciDB and stores it in a appropriate representation.
	 * 
         * Requests metadata for a given array from the SHIM web client. The metadata includes information about dimensions, 
	 * attributes, and spatial reference information. All data will be queried from the SHIM client and it will be stored
	 * in the local SciDB array representation.
	 * 
         * @param inArrayName name of a SciDB array
         * @param out pointer to the metadata of an array, depending on the type of the array the correct SciDBArray instance has to be created in this function, spatial reference information can be missing if not found
         * @return scidb4gdal::StatusCode
         */
        StatusCode getArrayDesc ( const string &inArrayName, SciDBSpatialArray *&out );

        /**
        * @brief Retreives single attribute data from shim for a given bounding box
	* 
	* This functions performs a HTTP query with an AFL request to prepare a subset of an array for the stated band (as index). In the second the processed
	* image will be downloaded chunkwise as binary data from the SHIM web client and will be stored in GDALs bands and datasets, that can then be stored
	* on the local drive in the required image format.
	* 
        * @param array metadata of an existing array
        * @param nband index of the requested attribute (starting with 0).
        * @param outchunk pointer to a chunk of memory that gets result data, must be allocated before(!) calling this function, which is usually done by GDAL
        * @param xmin left boundary, we assume x to be "easting" which is different from GDAL!
        * @param ymin lower boundary, we assume y to be "northing" which is different from GDAL!
        * @param xmax right boundary, we assume x to be "easting" which is different from GDAL!
        * @param ymax upper boundary, we assume y to be "northing" which is different from GDAL!
	* @param use_subarray whether or not subarrays are used.
	* @param emptycheck a boolean to state whether or not to check for empty cells
        * @return scidb4gdal::StatusCode
        */
        StatusCode getData ( SciDBSpatialArray &array, uint8_t nband, void *outchunk, int32_t x_min, int32_t y_min, int32_t x_max, int32_t y_max, bool use_subarray = true, bool emptycheck = true);


        /**
         * @brief Fetches the band statistics of the data from the SciDB database
	 * 
	 * Gets simple band statistics using in-database aggregation functions
	 * 
         * @param array metadata of an existing array
         * @param nband band index, 0 based
         * @param out result statistics, i.e. min, max, mean, sd
         * @return scidb4gdal::StatusCode
         */
        StatusCode getAttributeStats ( SciDBSpatialArray &array, uint8_t nband, SciDBAttributeStats &out );

        /**
         * @brief intializes the cURL easy interface
	 * 
	 * Initializes cURL's easy interface, should be performaed before each web service request
	 * 
	 * @return void
         */
        void curlBegin();

        /**
         * @brief Cancels the cURL easy interface and cleans up
	 * 
	 * Cleans up cURL's easy interface, should be performaed after each web service request
	 * 
	 * @return void
         */
        void curlEnd();


        /**
         * @brief Executes a HTTP request
	 * 
	 * Wrapper function around curl_easy_perform that retries requests and includes some error handling
	 * 
	 * @return CURLcode
         */
        CURLcode curlPerform();


        /**
         * @brief Checks the cURL connection
	 * 
	 * Tests a shim connection by requesting version information
	 * 
	 * @return scidb4gdal::StatusCode
         */
        StatusCode testConnection();


        /**
         * @brief Creates a new (temporary) SciDB array
	 * 
	 * This function uses the stated SciDBSpatialArray with its original name and makes a HTTP request to
	 * the SHIM web client to create a new temporal array under the original array name with the attached suffix
	 * scidb4gdal::SCIDB4GDAL_ARRAYSUFFIX_TEMP ("_temp").
	 * 
         * @param array metadata representation of a spatial array
         * @return scidb4gdal::StatusCode
         */
        StatusCode createTempArray ( SciDBSpatialArray &array );

        /**
	* @brief Makes a temporary array persistent in SciDB
	* 
	* This function applies the 'store' command to a temporary array in SciDB, which means that the array in memory is stored
	* on the disk and will be available even if the server restarts. The temporary array will then be deleted.
	* 
        * @param srcArr array name of the source array
        * @param tarArr array name of the target array
        * @return scidb4gdal::StatusCode
        */
        StatusCode persistArray ( string srcArr, string tarArr );

        /**
         * @brief Inserts a chunk of data to an existing array
	 * 
	 * The data file will be uploaded to the server using the "upload file" endpoint of the Shim web client. After that
	 * this function will create another temporary array, accessable under the arrays name with the attached suffix 
	 * scidb4gdal::SCIDB4GDAL_ARRAYSUFFIX_TEMPLOAD ("_tempload"), opens the submitted file and stores its attibute data at the
	 * respective dimension values of the "tempload array". After this chunk is stored in the tempload array it will be stored
	 * in the prior create temporary array by using a insert/redimension command in AFL language. The temporary array has to be
	 * created prior to this function using scidb4gdal::ShimClient::createTempArray.
	 * 
         * @param array metadata representation of an existing SciDBSpatialArray
         * @param inchunk pointer to a chunk of memory that holds data in scidb binary format
         * @param xmin left boundary, we assume x to be "easting" which is different from GDAL!
         * @param ymin lower boundary, we assume y to be "northing" which is different from GDAL!
         * @param xmax right boundary, we assume x to be "easting" which is different from GDAL!
         * @param ymax upper boundary, we assume y to be "northing" which is different from GDAL!
         * @return scidb4gdal::StatusCode
         */
        StatusCode insertData ( SciDBSpatialArray &array, void *inChunk, int32_t x_min, int32_t y_min, int32_t x_max, int32_t y_max );
	
	/**
	 * @brief Inserts an array in SciDB into another one if they are compatible
	 * 
	 * Tries to inset an array in SciDB into another array. It is important that the two arrays are compatible, meaning that the
	 * target array needs to be large enough to capture the array that is to be inserted. Therefore the target arrays dimensions must be large 
	 * enough that the source array does not run out of bounds.
	 * 
	 * @param tmpArr The temporary array that was loaded into SciDB and has a spatial (and temporal) reference.
	 * @param collArr The collection array or target array in which the temporary array is inserted into.
	 * @return scidb4gdal::StatusCode
	 */
	StatusCode insertInto (SciDBArray &tmpArr, SciDBArray &collArr);

       /**
        * @brief Annotates an array in SciDB with a Spatial Reference
	* 
	* Performs a HTTP request to set the spatial reference system of an array in SciDB.
	* 
        * @param array metadata representation of an SciDBSpatialArray including its srs to be updated
        * @return scidb4gdal::StatusCode
        */
        StatusCode updateSRS ( SciDBSpatialArray &array );
	


        /**
        * @brief Removes an existing array in SciDB
	* 
	* Performs HTTP requests to delete an array in SciDB given the array name.
	* 
        * @param inArrayName the name of an existing array
        * @return scidb4gdal::StatusCode
        */
        StatusCode removeArray ( const string &inArrayName );


        /**
        * @brief Checks whether an array exists.
	* 
	* This function queries the SHIM client for the existence of an array name by counting the rows of
	* the result.
	* 
        * @param inArrayName input array name string
        * @param out output, true if array already exists in SciDB
	* @return scidb4gdal::StatusCode
        */
        StatusCode arrayExists ( const string &inArrayName, bool &out );


	/**
	 * @brief Adds metadata on an array in SciDB
	 * 
	 * This function feeds an SciDB array with some meta data. During the process it will
	 * perform several HTTP requests to store all metadata that is stored in the map.
	 * 
	 * @param arrayname The name of the array for which the metadata is stored
	 * @param kv A map structure with key-value pairs as strings.
	 * @param domain The domain under which the metadata is stored.
	 * @result scidb4gdal::StatusCode for the operation.
	 */
        StatusCode setArrayMD ( string arrayname, map<string, string> kv, string domain = "" );

	/**
	 * @brief Fetches metadata of an array from the SciDB database
	 * 
	 * This function retrieves metadata of an SciDB array.
	 * 
	 * @param kv A reference to a map structure for key-value pairs as strings.
	 * @param arrayname The name of the array from which the metadata is loaded.
	 * @param domain The domain from which the metadata is loaded.
	 * @result scidb4gdal::StatusCode for the operation.
	 */
        StatusCode getArrayMD ( map<string, string> &kv, string arrayname, string domain = "" );

	/**
	 * @brief Adds metadata on an attribute of an array in SciDB
	 * 
	 * Transfers and stores metadata of an attribute (e.g. NA values) for a SciDB array.
	 * 
	 * @param arrayname The name of the array for which the metadata is stored
	 * @param attribute The name of the attribute for which the metadata is stored
	 * @param kv A map structure with key-value pairs as strings.
	 * @param domain The domain under which the metadata is stored.
	 * @result scidb4gdal::StatusCode for the operation.
	 */
        StatusCode setAttributeMD ( string arrayname, string attribute, map<string, string> kv, string domain = "" );

	/**
	 * @brief Fetches the metadata stored for an attribute of an SciDB array.
	 * 
	 * This function retreives metadata for an attribute of an array in SciDB.
	 * 
	 * @param kv A reference to a map structure in which the result is stored as string key-value pairs. 
	 * @param arrayname The name of the array from which the metadata is loaded
	 * @param attribute The name of the attribute from which the metadata is loaded
	 * @param domain The domain under which the metadata is stored.
	 * @result scidb4gdal::StatusCode for the operation. 
	 */
        StatusCode getAttributeMD ( map<string, string> &kv, string arrayname, string attribute,  string domain = "" );
	
	/**
	 * @brief Setter for create options.
	 * 
	 * This function sets the create parameter obtained from the create options or the properties string to SHIM client.
	 * 
	 * @param par The create parameters
	 * @return Void.
	 */
	void setCreateParameters(CreationParameters &par);
	
	/**
	 * @brief Sets the connection parameters on the SHIM client.
	 * 
	 * This function sets the connection parameter obtained from the create or opening options or the connection file string. The connection
	 * parameter will be used to authenticate at the specified SHIM web client.
	 * 
	 * @param par the ConnectionParameters
	 * @return Void.
	 */
	void setConnectionParameters(ConnectionParameters &par);
	
	/**
	 * @brief Sets the query parameters on the SHIM client.
	 * 
	 * Sets the query parameter obtained from the opening options or the filename string.
	 * 
	 * @param par The @see QueryParameters
	 * @return Void.
	 */
	void setQueryParameters(QueryParameters &par);
	
	/**
	 * @brief Attaches the temporal reference to the array in SciDB.
	 * 
	 * The function writes the temporal reference to an array in SciDB. The temporal information is extracted from the parameter 
	 * that is a class that inherits from scidb4gdal::SciDBTemporalArray. Curl is used to transfer the correct AFL query to the
	 * SHIM web client. Whether or not the operation was successful is expressed by returning the scidb4gdal::StatusCode.
	 * 
	 * @param array A reference to a class inheriting from scidb4gdal::SciDBTemporalArray
	 * @return scidb4gdal::StatusCode of the operation.
	 */
        StatusCode updateTRS(SciDBTemporalArray& array);
	
	/**
	 * @brief Queries the SHIM client for the type of an array and creates the appropriate SciDB structure for GDAL.
	 * 
	 * This function calls the SHIM web client for the type of an array. During the process an appropriate SciDB structure 
	 * (e.g. scidb4gdal::SciDBSpatialArray or scidb4gdal::SciDBSpatioTemporalArray) will be created. Therefore the function
	 * needs a pointer for a parameter.
	 * 
	 * @param name The name of the array.
	 * @param array A pointer to SciDB structure that will be modified during the process
	 * @return scidb4gdal::StatusCode of the operation.
	 */
	StatusCode getType(const string &name, SciDBSpatialArray *&array);

    protected:

        /**
        * @brief Fetches all attribute metadata of an array in SciDB
	* 
	* Performs HTTP queries to query and retrieve all metadata of the attributes of an array
	* in SciDB.
	* 
        * @param inArrayName name of existing array
        * @param out list of SciDBAtribute descriptors (out)
        * @return scidb4gdal::StausCode
        * @see SciDBAttribute
        */
        StatusCode getAttributeDesc ( const string &inArrayName, vector<SciDBAttribute> &out );

        /**
        * @brief Fetches all dimension metadata of an array in SciDB
	* 
	* Performs HTTP queries to query and retrieve all the metadata for dimensions of an array
	* in SciDB.
	* 
        * @param inArrayName name of existing array
        * @param out list of SciDBDimension descriptors
        * @return scidb4gdal::StausCode
        * @see SciDBDimension
        */
        StatusCode getDimensionDesc ( const string &inArrayName, vector<SciDBDimension> &out );

        /**
        * @brief Fetches the spatial reference metadata of an array in SciDB
	* 
	* Performs HTTP queries to query and retrieve metadata of an array's spatial reference if available. 
	* Otherwise, result contains default values representing no reference.
	* 
        * @param inArrayName name of existing array
        * @param out Spatial reference description (output)
        * @return scidb4gdal::StatusCode
        * @see scidb4gdal::SciDBSpatialReference
        */
        StatusCode getSRSDesc ( const string &inArrayName, SciDBSpatialReference &out );
	
	/**
        * @brief Fetches the temporal reference metadata of an array in SciDB
	* 
	* Performs HTTP queries to query and retrieve metadata of an array's temporal reference if available. 
	* Otherwise, result contains default values representing no reference.
	* 
        * @param inArrayName name of existing array
        * @param out temporal reference description (output)
        * @return scidb4gdal::StausCode
        * @see scidb4gdal::SciDBSTemporalReference
        */
	StatusCode getTRSDesc (const string &inArrayName, SciDBTemporalReference &out);

        /**
         * @brief Creates a new shim session and returns its ID
	 * 
	 * Performs a HTTP request to the SHIM session enpoint and creates a new session ID.
	 * 
         * @return integer session ID
         */
        int newSession();

        /**
	 * @brief Releases an existing shim session
	 * 
	 * Performs a query to the SHIM session endpoint with the current session ID to release the session.
	 * 
         * @param sessionID integer session ID
	 * @return void.
         */
        void releaseSession ( int sessionID );

	/**
	 * @brief Login to the SHIM web client
	 * 
	 * A function used to login to the SHIM web client based on the connection parameters that were set.
	 * 
	 * @return void.
	 */
        void login();

	
	/**
	 * @brief function to logout from a SHIM client
	 * 
	 * This function is used to logout from the SHIM client by calling the logout endpoint of the SHIM client with
	 * the authentication number.
	 * 
	 * @return void
	 */
	void logout();

    private:
	/** host url */
        string      _host;
	/** port number */
        uint16_t    _port;
	/** user name */
        string      _user;
	/** password */
        string      _passwd;
	/** ssl use */
        bool        _ssl;
	/** The cURL class to handle the HTTP calls */
        CURL       *_curl_handle;
	/** initialize flag */
        bool _curl_initialized;
	/** authentication string after login */
        string _auth;
	/** pointer to the connection parameters */
	ConnectionParameters *_conp;
	/** pointer to the creation parameters */
	CreationParameters *_cp;
	/** pointer to the query parameter */
	QueryParameters *_qp;
    };
}

#endif
