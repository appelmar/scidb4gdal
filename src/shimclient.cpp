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

#include "shimclient.h"
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>
#include <cctype> // Microsoft Visual C++ compatibility
#include "TemporalReference.h"
#include "scidb_structs.h"


namespace scidb4gdal
{
    using namespace scidb4geo;
    
    ShimClient::ShimClient() : 
      _host ( "https://localhost" ), 
      _port ( 8083 ), _user ( "scidb" ), 
      _passwd ( "scidb" ), 
      _ssl ( true ), 
      _curl_handle ( 0 ), 
      _curl_initialized ( false ),
      _auth ( "" )
    {
        curl_global_init ( CURL_GLOBAL_ALL );
    }
    
    ShimClient::ShimClient ( string host, uint16_t port, string user, string passwd, bool ssl = false) : 
      _host ( host ), 
      _port ( port ), 
      _user ( user ), 
      _passwd ( passwd ), 
      _ssl ( ssl ),  
      _curl_handle ( 0 ), 
      _curl_initialized ( false ), 
      _auth ( "" )
    {
        curl_global_init ( CURL_GLOBAL_ALL );
    }
    
    ShimClient::ShimClient(ConnectionParameters* con)
    {
      _host = con->host; 
      _port = con->port; 
      _user = con->user; 
      _passwd = con->passwd; 
      _ssl = con->ssl;  
      _curl_handle = 0; 
      _curl_initialized = false; 
      _auth = "";
      curl_global_init ( CURL_GLOBAL_ALL );
    }

    
    ShimClient::~ShimClient()
    {
        if ( _ssl && !_auth.empty() ) logout();
        curl_global_cleanup();
        _curl_handle = 0;
    }



    static size_t responseSilentCallback ( void *ptr, size_t size, size_t count, void *stream )
    {
        return size * count;
    }

    void ShimClient::curlBegin()
    {
        if ( _curl_initialized ) curlEnd();

        _curl_handle = curl_easy_init();
        _curl_initialized = true;
        //curl_easy_setopt ( _curl_handle, CURLOPT_URL, _host.c_str() );
        curl_easy_setopt ( _curl_handle, CURLOPT_PORT, _port );
        curl_easy_setopt ( _curl_handle, CURLOPT_HTTPAUTH, CURLAUTH_DIGEST );
        curl_easy_setopt ( _curl_handle, CURLOPT_USERNAME, _user.c_str() );
        curl_easy_setopt ( _curl_handle, CURLOPT_PASSWORD, _passwd.c_str() );

        if ( _ssl ) {
            curl_easy_setopt ( _curl_handle, CURLOPT_SSL_VERIFYPEER, 0 );
            curl_easy_setopt ( _curl_handle, CURLOPT_SSL_VERIFYHOST, 0 );
        }

#ifdef CURL_VERBOSE
        curl_easy_setopt ( _curl_handle, CURLOPT_VERBOSE, 1L );
#else
        curl_easy_setopt ( _curl_handle, CURLOPT_WRITEFUNCTION, &responseSilentCallback ); // default silent, otherwise weird number output on stdout
#endif
    }




    void ShimClient::curlEnd()
    {
        if ( _curl_initialized ) {
            curl_easy_cleanup ( _curl_handle );
            _curl_initialized = false;
        }

    }


    CURLcode ShimClient::curlPerform()
    {

        CURLcode res = curl_easy_perform ( _curl_handle );
        for ( int i = 1; i < CURL_RETRIES && res == CURLE_COULDNT_CONNECT; ++i ) {
            stringstream s;
            s << "Connection error, retrying ... " << "(#" << i << ")";
            Utils::warn ( s.str() );
            Utils::sleep ( i * 100 );
            res = curl_easy_perform ( _curl_handle );
        }
        if ( res != CURLE_OK ) {
            Utils::error ( ( string ) ( "curl_easy_perform() failed: " ) + curl_easy_strerror ( res ) );
        }
        return res;
    }








    static size_t responseToStringCallback ( void *ptr, size_t size, size_t count, void *stream )
    {
        ( ( string * ) stream )->append ( ( char * ) ptr, 0, size * count );
        return size * count;
    }





    StatusCode ShimClient::testConnection()
    {

      
        curlBegin();

        stringstream ss;
        ss << _host << SHIMENDPOINT_VERSION;
        if ( _ssl && !_auth.empty() ) ss << "?auth=" << _auth; // Add auth parameter if using ssl
        curl_easy_setopt ( _curl_handle, CURLOPT_URL, ss.str().c_str() );

        // Test connection
        string response;
        curl_easy_setopt ( _curl_handle, CURLOPT_WRITEFUNCTION, &responseToStringCallback );
        curl_easy_setopt ( _curl_handle, CURLOPT_WRITEDATA, &response );

        curlPerform();
        curlEnd();
        Utils::debug ( "SHIM Version: " + response );


        return SUCCESS;
    }







    int ShimClient::newSession()
    {

        if ( _ssl && _auth.empty() ) login();


        curlBegin();

        stringstream ss;
        string response;

        // NEW SESSION ID ////////////////////////////
        ss << _host << SHIMENDPOINT_NEW_SESSION;
        if ( _ssl && !_auth.empty() ) ss << "?auth=" << _auth; // Add auth parameter if using ssl
        curl_easy_setopt ( _curl_handle, CURLOPT_URL, ss.str().c_str() );
        curl_easy_setopt ( _curl_handle, CURLOPT_HTTPGET, 1 );


        // Test connection

        curl_easy_setopt ( _curl_handle, CURLOPT_WRITEFUNCTION, &responseToStringCallback );
        curl_easy_setopt ( _curl_handle, CURLOPT_WRITEDATA, &response );

        curlPerform();

        curlEnd();

        //int sessionID = boost::lexical_cast<int>(response.data());
        int sessionID = atoi ( response.c_str() );
        if ( sessionID > 0 )
            return sessionID;


        Utils::error ( ( string ) ( "Invalid session ID" ) );
        return -1;

    }


    void ShimClient::releaseSession ( int sessionID )
    {
        curlBegin();
        stringstream ss;
        ss << _host << SHIMENDPOINT_RELEASE_SESSION;
        ss << "?" << "id=" << sessionID;
        if ( _ssl && !_auth.empty() ) ss << "&auth=" << _auth; // Add auth parameter if using ssl
        curl_easy_setopt ( _curl_handle, CURLOPT_URL, ss.str().c_str() );
        curl_easy_setopt ( _curl_handle, CURLOPT_HTTPGET, 1 );
        string response;
        curl_easy_setopt ( _curl_handle, CURLOPT_WRITEFUNCTION, &responseToStringCallback );
        curl_easy_setopt ( _curl_handle, CURLOPT_WRITEDATA, &response );
        curlPerform();
        curlEnd();

    }


    void ShimClient::login()
    {

        curlBegin();

        stringstream ss;
        string response;

        ss << _host << SHIMENDPOINT_LOGIN << "?username=" << _user << "&password=" << _passwd;
        curl_easy_setopt ( _curl_handle, CURLOPT_URL, ss.str().c_str() );
        curl_easy_setopt ( _curl_handle, CURLOPT_HTTPGET, 1 );

        curl_easy_setopt ( _curl_handle, CURLOPT_WRITEFUNCTION, &responseToStringCallback );
        curl_easy_setopt ( _curl_handle, CURLOPT_WRITEDATA, &response );
        curlPerform();
        curlEnd();

        //int sessionID = boost::lexical_cast<int>(response.data());
        if ( response.length() > 0 ) {
            _auth = response;
            Utils::debug ( ( string ) "Login to SciDB successsful, using auth key: " + _auth );
        }
        else {
            Utils::error ( ( string ) ( "Login to SciDB failed" ), true );
        }
    }


    void ShimClient::logout()
    {
        curlBegin();

        stringstream ss;


        // NEW SESSION ID ////////////////////////////
        ss << _host << SHIMENDPOINT_LOGOUT << "?auth=" << _auth;
        curl_easy_setopt ( _curl_handle, CURLOPT_URL, ss.str().c_str() );
        curl_easy_setopt ( _curl_handle, CURLOPT_HTTPGET, 1 );


        curlPerform();
        curlEnd();

    }






    StatusCode  ShimClient::getAttributeDesc ( const string &inArrayName, vector< SciDBAttribute > &out )
    {


        out.clear();


        int sessionID = newSession();

        string response;
        stringstream ss;
        // EXECUTE QUERY  /////

	/*
	 * Make a request to fetch the attributes of the data set
	 */
        {

            stringstream afl;
            ss.str ( "" );
            curlBegin();
            afl << "project(attributes(" << inArrayName << "),name,type_id,nullable)";
            Utils::debug ( "Performing AFL Query: " +  afl.str() );
            ss << _host << SHIMENDPOINT_EXECUTEQUERY;
            ss << "?" << "id=" << sessionID << "&query=" << afl.str() << "&save=" << "csv";
            if ( _ssl && !_auth.empty() ) ss << "&auth=" << _auth; // Add auth parameter if using ssl

            curl_easy_setopt ( _curl_handle, CURLOPT_URL, ss.str().c_str() );
            curl_easy_setopt ( _curl_handle, CURLOPT_HTTPGET, 1 );

            response = "";
            curl_easy_setopt ( _curl_handle, CURLOPT_WRITEFUNCTION, &responseToStringCallback );
            curl_easy_setopt ( _curl_handle, CURLOPT_WRITEDATA, &response );

            if ( curlPerform() != CURLE_OK ) {
                curlEnd();
                Utils::error ( "Cannot get attribute information for array '" + inArrayName + "'. Does it exist?" );
                return ERR_READ_UNKNOWN;
            }
            curlEnd();

        }
	
	/**
	 * Read data from the request and save it at the variable "response"
	 */
        {

            curlBegin();
            ss.str ( "" );
            // READ BYTES  ///////////////////////////
            ss << _host << SHIMENDPOINT_READ_BYTES << "?" << "id=" << sessionID << "&n=0";
            if ( _ssl && !_auth.empty() ) ss << "&auth=" << _auth; // Add auth parameter if using ssl
            curl_easy_setopt ( _curl_handle, CURLOPT_URL, ss.str().c_str() );
            curl_easy_setopt ( _curl_handle, CURLOPT_HTTPGET, 1 );

            response = "";
            curl_easy_setopt ( _curl_handle, CURLOPT_WRITEFUNCTION, &responseToStringCallback );
            curl_easy_setopt ( _curl_handle, CURLOPT_WRITEDATA, &response );
            if ( curlPerform() != CURLE_OK ) {
                curlEnd();
                Utils::error ( "Cannot get attribute information for array '" + inArrayName + "'. Does it exist?" );
                return ERR_READ_UNKNOWN;
            }
            curlEnd();
        }


        // Parse CSV
        ss.str ( "" );
        vector<string> rows;
        boost::split ( rows, response, boost::is_any_of ( "\n" ) );
        for ( vector<string>::iterator it = ++ ( rows.begin() ); it != rows.end(); ++it ) { // ignore first row

            vector<string> cols;
            boost::split ( cols, *it, boost::is_any_of ( "," ) );
            if ( cols.size() != 3 ) {
                continue;// TODO: Error handling, expected 3 cols
            }
            if ( cols[0].length() < 2 || cols[1].length() < 2 || cols[2].length() < 2 ) continue;

            SciDBAttribute attr;

            attr.name = cols[0].substr ( 1, cols[0].length() - 2 ); // Remove quotes
            attr.typeId = cols[1].substr ( 1, cols[1].length() - 2 ); // Remove quotes
            attr.nullable = ( cols[2].compare ( "TRUE" ) * cols[2].compare ( "true" ) == 0 );

            // Assert attr has datatype that is supported by GDAL
            if ( Utils::scidbTypeIdToGDALType ( attr.typeId ) == GDT_Unknown ) {
                ss.str ( "" );
                ss << "SciDB GDAL driver does not support data type " << attr.typeId << " of attribute " << attr.name;
                Utils::error ( ss.str() );
                return ERR_READ_UNKNOWN;
            }

            out.push_back ( attr );

        }


        releaseSession ( sessionID );

        return SUCCESS;

    }

    StatusCode ShimClient::getDimensionDesc ( const string &inArrayName, vector< SciDBDimension> &out )
    {
        out.clear();
	
        int sessionID = newSession();

        string response;

	/*
	 * Request
	 */
        {
            curlBegin();
            stringstream ss;
            stringstream afl;
            afl << "project(dimensions(" << inArrayName << "),name,low,high,type,chunk_interval,start,length)";  // project(dimensions(chicago2),name,low,high,type)
            Utils::debug ( "Performing AFL Query: " +  afl.str() );

            ss << _host << SHIMENDPOINT_EXECUTEQUERY << "?" << "id=" << sessionID << "&query=" << afl.str()  << "&save=" << "csv";
            if ( _ssl && !_auth.empty() ) ss << "&auth=" << _auth; // Add auth parameter if using ssl
            curl_easy_setopt ( _curl_handle, CURLOPT_URL, ss.str().c_str() );
            curl_easy_setopt ( _curl_handle, CURLOPT_HTTPGET, 1 );

            response = "";
            curl_easy_setopt ( _curl_handle, CURLOPT_WRITEFUNCTION, &responseToStringCallback );
            curl_easy_setopt ( _curl_handle, CURLOPT_WRITEDATA, &response );
            if ( curlPerform() != CURLE_OK ) {
                curlEnd();
                Utils::error ( "Cannot get dimension information for array '" + inArrayName + "'. Does it exist?" );
                return ERR_READ_UNKNOWN;
            }
            curlEnd();

        }
	
	/*
	 * Get response
	 */
        {
            curlBegin();
            stringstream ss;
            response = "";
            ss << _host << SHIMENDPOINT_READ_BYTES << "?" << "id=" << sessionID << "&n=0";
            if ( _ssl && !_auth.empty() ) ss << "&auth=" << _auth; // Add auth parameter if using ssl
            curl_easy_setopt ( _curl_handle, CURLOPT_URL, ss.str().c_str() );
            curl_easy_setopt ( _curl_handle, CURLOPT_HTTPGET, 1 );
            curl_easy_setopt ( _curl_handle, CURLOPT_WRITEFUNCTION, &responseToStringCallback );
            curl_easy_setopt ( _curl_handle, CURLOPT_WRITEDATA, &response );
            if ( curlPerform() != CURLE_OK ) {
                curlEnd();
                Utils::error ( "Cannot get dimension information for array '" + inArrayName + "'. Does it exist?" );
                return ERR_READ_UNKNOWN;
            }
            curlEnd();
        }

        // Parse CSV
        stringstream ss;

        vector<string> rows;
        boost::split ( rows, response, boost::is_any_of ( "\n" ) );
        for ( vector<string>::iterator it = ++ ( rows.begin() ); it != rows.end(); ++it ) { // ignore first row
            vector<string> cols;
            boost::split ( cols, *it, boost::is_any_of ( "," ) );
            if ( cols.size() != 7 ) {

                continue;// TODO: Error handling, expected 4 cols
            }
            if ( cols[0].length() < 2 || cols[1].length() < 1 || cols[2].length() < 1 || cols[3].length() < 1 || cols[4].length() < 1 || cols[5].length() < 1 || cols[6].length() < 1 ) {
                continue;
            }

            SciDBDimension dim;
            dim.name = cols[0].substr ( 1, cols[0].length() - 2 );
            dim.low = boost::lexical_cast<int64_t> ( cols[1].c_str() );
            dim.high = boost::lexical_cast<int64_t> ( cols[2].c_str() );
            dim.typeId = cols[3].substr ( 1, cols[3].length() - 2 ); // Remove quotes
            dim.chunksize = boost::lexical_cast<uint32_t> ( cols[4].c_str() );
            dim.start = boost::lexical_cast<int64_t> ( cols[5].c_str() );
            dim.length = boost::lexical_cast<int64_t> ( cols[6].c_str() );

            if ( dim.high == SCIDB_MAX_DIM_INDEX || dim.low == SCIDB_MAX_DIM_INDEX  || dim.high == -SCIDB_MAX_DIM_INDEX || dim.low == -SCIDB_MAX_DIM_INDEX ) { // yet unspecified e.g. for newly created arrays
                dim.low = boost::lexical_cast<int64_t> ( cols[5].c_str() );
                dim.high = dim.low + boost::lexical_cast<int64_t> ( cols[6].c_str() ) - 1;
            }


            // Assert  dim.typeId is integer
            if ( ! ( dim.typeId == "int32" || dim.typeId == "int64" || dim.typeId == "int16" || dim.typeId == "int8" ||
                     dim.typeId == "uint32" || dim.typeId == "uint64" || dim.typeId == "uint16" || dim.typeId == "uint8" ) ) {
                ss.str ( "" );
                ss << "SciDB GDAL driver works with integer dimensions only. Got dimension " << dim.name << ":" << dim.typeId;
                Utils::error ( ss.str() );
                return ERR_READ_UNKNOWN;
            }

            out.push_back ( dim );

        }

        releaseSession ( sessionID );

        return SUCCESS;

    }








    StatusCode ShimClient::getSRSDesc ( const string &inArrayName, SciDBSpatialReference &out )
    {
        int sessionID = newSession();
        string response;


        {
            curlBegin();
            stringstream ss;
            stringstream afl;
            afl << "project(st_getsrs(" << inArrayName << "),name,xdim,ydim,srtext,proj4text,A)";  // project(dimensions(chicago2),name,low,high,type)
            Utils::debug ( "Performing AFL Query: " +  afl.str() );
            ss << _host << SHIMENDPOINT_EXECUTEQUERY << "?" << "id=" << sessionID << "&query=" << afl.str() << "&save=" << "csv";
            if ( _ssl && !_auth.empty() ) ss << "&auth=" << _auth; // Add auth parameter if using ssl
            curl_easy_setopt ( _curl_handle, CURLOPT_URL, ss.str().c_str() );
            curl_easy_setopt ( _curl_handle, CURLOPT_HTTPGET, 1 );
            response = "";
            curl_easy_setopt ( _curl_handle, CURLOPT_WRITEFUNCTION, &responseToStringCallback );
            curl_easy_setopt ( _curl_handle, CURLOPT_WRITEDATA, &response );
            if ( curlPerform() != CURLE_OK ) {
                curlEnd();
                Utils::warn ( "Cannot find spatial reference information for array '" + inArrayName + "'" );
                return ERR_SRS_NOSPATIALREFFOUND;
            };
            curlEnd();
        }


        {
            curlBegin();
            stringstream ss;
            // READ BYTES  ////////////////////////////
            ss.str ( "" );
            ss << _host << SHIMENDPOINT_READ_BYTES << "?" << "id=" << sessionID << "&n=0";
            if ( _ssl && !_auth.empty() ) ss << "&auth=" << _auth; // Add auth parameter if using ssl

            curl_easy_setopt ( _curl_handle, CURLOPT_URL, ss.str().c_str() );
            curl_easy_setopt ( _curl_handle, CURLOPT_HTTPGET, 1 );

            response = "";
            curl_easy_setopt ( _curl_handle, CURLOPT_WRITEFUNCTION, &responseToStringCallback );
            curl_easy_setopt ( _curl_handle, CURLOPT_WRITEDATA, &response );
            if ( curlPerform() != CURLE_OK ) {
                curlEnd();
                Utils::warn ( "Cannot read spatial reference information for array '" + inArrayName + "'" );
                return ERR_SRS_NOSPATIALREFFOUND;
            };
            curlEnd();
        }


        // Parse CSV

        vector<string> rows;
        boost::split ( rows, response, boost::is_any_of ( "\n" ) );

        if ( rows.size() < 2 || rows.size() > 3 ) { // Header + 1 data row
            Utils::warn ( "Cannot extract SRS information from response: "  + response );
            return ERR_SRS_NOSPATIALREFFOUND;
        }

        string row = rows[1];
        vector <string> cols;
        int cur_start = -1;
        for ( uint32_t i = 0; i < row.length(); ++i ) { // Find items between single quotes 'XXX', only works if all SciDB attributes of query result are strings
            if ( row[i] == '\'' ) {
                if ( cur_start < 0 ) cur_start = i;
                else {
                    cols.push_back ( row.substr ( cur_start + 1, i - cur_start - 1 ) );
                    cur_start = -1;

                }
            }
        }


        if ( cols.size() != 6 ) {
            Utils::warn ( "Cannot extract SRS information from response: "  + response );
            return ERR_SRS_NOSPATIALREFFOUND;
//             out.affineTransform = * ( new AffineTransform() );
//             out.ydim  =  out.xdim  = out.proj4text  = out.srtext = "";

        }
        else {
            out.xdim = cols[1];
            out.ydim = cols[2];
            out.srtext = cols[3];
            out.proj4text = cols[4];
            out.affineTransform = * ( new AffineTransform ( cols[5] ) ); // Should be released
        }

        releaseSession ( sessionID );

        return SUCCESS;

    }
    
    StatusCode  ShimClient::getTRSDesc (const string &inArrayName, SciDBTemporalReference &out) {
      int sessionID = newSession();
        string response;

	//st_gettrs query preparation
        {
            curlBegin();
            stringstream ss;
            stringstream afl;
            afl << "project(st_gettrs(" << inArrayName << "),tdim,t0,dt)"; 
            Utils::debug ( "Performing AFL Query: " +  afl.str() );
            ss << _host << SHIMENDPOINT_EXECUTEQUERY << "?" << "id=" << sessionID << "&query=" << afl.str() << "&save=" << "csv";
            if ( _ssl && !_auth.empty() ) ss << "&auth=" << _auth; // Add auth parameter if using ssl
            curl_easy_setopt ( _curl_handle, CURLOPT_URL, ss.str().c_str() );
            curl_easy_setopt ( _curl_handle, CURLOPT_HTTPGET, 1 );
            response = "";
            curl_easy_setopt ( _curl_handle, CURLOPT_WRITEFUNCTION, &responseToStringCallback );
            curl_easy_setopt ( _curl_handle, CURLOPT_WRITEDATA, &response );
            if ( curlPerform() != CURLE_OK ) {
                curlEnd();
                Utils::warn ( "Cannot find spatial reference information for array '" + inArrayName + "'" );
                return ERR_SRS_NOSPATIALREFFOUND;
            };
            curlEnd();
        }

	//st_gettrs call
        {
            curlBegin();
            stringstream ss;
            // READ BYTES  ////////////////////////////
            ss.str ( "" );
            ss << _host << SHIMENDPOINT_READ_BYTES << "?" << "id=" << sessionID << "&n=0";
            if ( _ssl && !_auth.empty() ) ss << "&auth=" << _auth; // Add auth parameter if using ssl

            curl_easy_setopt ( _curl_handle, CURLOPT_URL, ss.str().c_str() );
            curl_easy_setopt ( _curl_handle, CURLOPT_HTTPGET, 1 );

            response = "";
            curl_easy_setopt ( _curl_handle, CURLOPT_WRITEFUNCTION, &responseToStringCallback );
            curl_easy_setopt ( _curl_handle, CURLOPT_WRITEDATA, &response );
            if ( curlPerform() != CURLE_OK ) {
                curlEnd();
                Utils::warn ( "Cannot read temporal reference information for array '" + inArrayName + "'" );
                return ERR_SRS_NOSPATIALREFFOUND;
            };
            curlEnd();
        }


        // Parse CSV

        vector<string> rows;
        boost::split ( rows, response, boost::is_any_of ( "\n" ) );
	
	if (rows.size() > 2) {
	  string row = rows[1]; //TODO ensure that rows has more than 1 row (more than header)
	  vector <string> cols;
	  int cur_start = -1;
	  for ( uint32_t i = 0; i < row.length(); ++i ) { // Find items between single quotes 'XXX', only works if all SciDB attributes of query result are strings
	      if ( row[i] == '\'' ) {
		  if ( cur_start < 0 ) cur_start = i;
		  else {
		      cols.push_back ( row.substr ( cur_start + 1, i - cur_start - 1 ) );
		      cur_start = -1;

		  }
	      }
	  }
	  {
	      out.tdim = cols[0];
	      TPoint *p = new TPoint(cols[1]);
	      TInterval *i = new TInterval(cols[2]);	      
	      out.setTPoint(p);
	      out.setTInterval(i);

	  }
	}
        releaseSession ( sessionID );

        return SUCCESS;

    }

    StatusCode  ShimClient::getArrayDesc ( const string &inArrayName, SciDBSpatialArray *&out )
    {
        bool exists;
        arrayExists ( inArrayName, exists );
        if ( !exists ) {
            Utils::error ( "Array '" + inArrayName + "' does not exist in SciDB database" );
            return ERR_READ_ARRAYUNKNOWN;
        }
        
	getType(inArrayName, out);
	if (!out) {
	  Utils::debug("No array was created");
	}
        out->name = inArrayName;
        // Get dimensions: project(dimensions(inArrayName),name,low,high,type)
        StatusCode res;
        res = getDimensionDesc ( inArrayName, out->dims );
        if ( res != SUCCESS ) {
            Utils::error ( "Cannot extract array dimension metadata" );
            return res;
        }


        // Get attributes: project(attributes(inArrayName),name,type_id,nullable);
        res = getAttributeDesc ( inArrayName, out->attrs );
        if ( res != SUCCESS ) {
            Utils::error ( "Cannot extract array attribute metadata" );
            return res;
        }

	/*
	 * Make calls for metadata. First try to get the spatial reference system, then try to get the temporal rs
	 */
        getSRSDesc ( inArrayName, *out );
	
	SciDBSpatioTemporalArray* starr_ptr = dynamic_cast<SciDBSpatioTemporalArray*>(out);
	if (starr_ptr) {
	  getTRSDesc ( inArrayName, *starr_ptr );
	}
	
	//set the metadata
        MD m;
        getArrayMD ( m, inArrayName, "" );
        out->md.insert ( pair < string, MD> ( "", m ) ); // TODO: Add domain

        for ( int i = 0; i < out->attrs.size(); ++i ) {
            MD ma;
            getAttributeMD ( ma, inArrayName, out->attrs[i].name, "" ); // TODO: Add domain
            out->attrs[i].md.insert ( pair < string, MD> ( "", ma ) ); // TODO: Add domain
        }


        return SUCCESS;

    }


    StatusCode ShimClient::getType(const string& name, SciDBSpatialArray *&array)
    {
	int sessionID = newSession();
        string response;


        {
            curlBegin();
            stringstream ss;
            stringstream afl;
            afl << "eo_arrays()";
            Utils::debug ( "Performing AFL Query: " +  afl.str() );
            ss << _host << SHIMENDPOINT_EXECUTEQUERY << "?" << "id=" << sessionID << "&query=" << afl.str() << "&save=" << "csv";
            if ( _ssl && !_auth.empty() ) ss << "&auth=" << _auth; // Add auth parameter if using ssl
            curl_easy_setopt ( _curl_handle, CURLOPT_URL, ss.str().c_str() );
            curl_easy_setopt ( _curl_handle, CURLOPT_HTTPGET, 1 );
            response = "";
            curl_easy_setopt ( _curl_handle, CURLOPT_WRITEFUNCTION, &responseToStringCallback );
            curl_easy_setopt ( _curl_handle, CURLOPT_WRITEDATA, &response );
            if ( curlPerform() != CURLE_OK ) {
                curlEnd();
                Utils::warn ( "Cannot query for spatial or temporal annotated query. SCIDB4GEO module activated in SCIDB?" );
                return ERR_GLOBAL_NO_SCIDB4GEO;
            };
            curlEnd();
        }


        {
            curlBegin();
            stringstream ss;
            // READ BYTES  ////////////////////////////
            ss.str ( "" );
            ss << _host << SHIMENDPOINT_READ_BYTES << "?" << "id=" << sessionID << "&n=0";
            if ( _ssl && !_auth.empty() ) ss << "&auth=" << _auth; // Add auth parameter if using ssl

            curl_easy_setopt ( _curl_handle, CURLOPT_URL, ss.str().c_str() );
            curl_easy_setopt ( _curl_handle, CURLOPT_HTTPGET, 1 );

            response = "";
            curl_easy_setopt ( _curl_handle, CURLOPT_WRITEFUNCTION, &responseToStringCallback );
            curl_easy_setopt ( _curl_handle, CURLOPT_WRITEDATA, &response );
            if ( curlPerform() != CURLE_OK ) {
                curlEnd();
                return ERR_GLOBAL_UNKNOWN;
            };
            curlEnd();
        }


        // Parse CSV into string vector
        vector<string> rows;
        boost::split ( rows, response, boost::is_any_of ( "\n" ) );
	
	if (rows.size() > 2) {
	  for ( vector<string>::iterator it = ++ ( rows.begin() ); it != rows.end(); ++it ) { // Find items between single quotes 'XXX', only works if all SciDB attributes of query result are strings
	      vector<string> cols;
	      boost::split ( cols, *it, boost::is_any_of ( "," ) );
	      
	      string n = cols[0].substr ( 1, cols[0].length() - 2 );
	      if(n == name) {
		string type = cols[1].substr ( 1, cols[1].length() - 2 );
		if (type == "s") {
		  array = new SciDBSpatialArray();
		  break;
		} else if (type == "st") {
		  array = new SciDBSpatioTemporalArray();
		  break;
		} else {
		  Utils::debug("Can't evaluate "+type);
		  return ERR_GLOBAL_UNKNOWN;
		}
	      } else {
		continue;
	      }
	  }
	}

        releaseSession ( sessionID );
        return SUCCESS;
    }






    /**
     * Callback function for receiving scidb binary data
     */
    static size_t responseBinaryCallback ( void *ptr, size_t size, size_t count, void *stream )
    {
        size_t realsize = size * count;
        struct SingleAttributeChunk *mem = ( struct SingleAttributeChunk * ) stream;
        memcpy ( & ( mem->memory[mem->size] ), ptr, realsize );
        mem->size += realsize;
        return realsize;
    }


    StatusCode ShimClient::getData ( SciDBSpatialArray &array, uint8_t nband, void *outchunk, int32_t x_min, int32_t y_min, int32_t x_max, int32_t y_max, bool use_subarray, bool emptycheck)

    {	
// 	std::stringstream sstm;
// 	sstm << "Fire getData in SHIM client with the following image coordinates " << x_min << " " << y_min << " " << x_max << " " << y_max;
// 	Utils::debug(sstm.str());
      
	int t_index;
        if ( x_min < array.getXDim()->low || x_min > array.getXDim()->high ||
                x_max < array.getXDim()->low || x_max > array.getXDim()->high ||
                y_min < array.getYDim()->low || y_min > array.getYDim()->high ||
                y_max < array.getYDim()->low || y_max > array.getYDim()->high ) {
            Utils::error ( "Requested array subset is outside array boundaries" );
        }

        if ( nband >= array.attrs.size() )
            Utils::error ( "Requested array band does not exist" );

        int8_t x_idx = array.getXDimIdx();
        int8_t y_idx = array.getYDimIdx();


        stringstream ss;
        string response;


        int sessionID = newSession();



        /* Depending on dimension ordering, array must be transposed. */


        curlBegin();
        // EXECUTE QUERY  ////////////////////////////
        ss.str();
        ss << _host << SHIMENDPOINT_EXECUTEQUERY << "?" << "id=" << sessionID;
	
	stringstream tslice;
	if (SciDBSpatioTemporalArray* starray = dynamic_cast<SciDBSpatioTemporalArray*>(&array)) {
	  // if we have a temporal index, we need to slice the data set
	  if (_qp) {
	    t_index = _qp->temp_index;
	  } else if (_cp) {
	    TPoint temp_point = TPoint(_cp->timestamp);
	    t_index = starray->indexAtDatetime(temp_point);
	  } else {
	   //TODO throw error 
	    t_index = 0;
	    Utils::debug("Neither query nor creation parameter were found.");
	  }
	   
	  tslice << "slice(" << array.name << ","+starray->tdim+"," << t_index << ")";
	} else {
	  //Utils::debug("Cast failed. Skipping the slicing.");
	  tslice << array.name;
	}
	
	string arr = tslice.str();
	
        stringstream afl;
        if ( x_idx > y_idx ) { // TODO: need to check performance of differend ordering

            if ( use_subarray ) {
                if ( emptycheck ) {
                    afl << "(merge(";
                    afl << "project(subarray(" << arr << "," << y_min << "," << x_min << "," << y_max << "," << x_max << ")," << array.attrs[nband].name << ")";
                    afl << ",build(<" << array.attrs[nband].name << ":" << array.attrs[nband].typeId << "> ["
                        << array.getYDim()->name << "=" << 0  << ":" << y_max - y_min  << "," << array.getYDim()->chunksize << "," << 0 << ","
                        << array.getXDim()->name << "=" << 0  << ":" << x_max - x_min  << "," << array.getXDim()->chunksize << "," << 0 << "],"
                        << Utils::defaultNoDataSciDB ( array.attrs[nband].typeId ) << ")))";
                }
                else {
                    afl << "(project(subarray(" << arr << "," << y_min << "," << x_min << "," << y_max << "," << x_max << ")," << array.attrs[nband].name << "))";
                }
            }
            else { // Between
                // TODO: Test which way is the fastest
                if ( emptycheck ) {

                    afl << "(merge(";
                    afl << "project(between(" << arr << "," << y_min << "," << x_min << "," << y_max << "," << x_max << ")," << array.attrs[nband].name << ")";
                    afl << ",between(build(<" << array.attrs[nband].name << ":" << array.attrs[nband].typeId << "> ["
                        << array.getYDim()->name << "=" << array.getYDim()->start  << ":" << array.getYDim()->start + array.getYDim()->length - 1 << "," << array.getYDim()->chunksize << "," << 0 << ","
                        << array.getXDim()->name << "=" << array.getXDim()->start  << ":" << array.getXDim()->start + array.getXDim()->length - 1 << "," << array.getXDim()->chunksize << "," << 0 << "],"
                        << 0 << ")," << y_min << "," << x_min << "," << y_max << "," << x_max << ")))";
                }

                else {
                    afl << "(project(between(" << arr << "," << y_min << "," << x_min << "," << y_max << "," << x_max << ")," << array.attrs[nband].name << "))";
                }


            }
        }


        else {

            if ( use_subarray ) {

                if ( emptycheck ) {
                    afl << "transpose(merge(";
                    afl << "project(subarray(" << arr << "," << x_min << "," << y_min << "," << x_max << "," << y_max << ")," << array.attrs[nband].name << ")";
                    afl << ",build(<" << array.attrs[nband].name << ":" << array.attrs[nband].typeId << "> ["
                        << array.getXDim()->name << "=" << 0  << ":" << x_max - x_min  << "," << array.getXDim()->chunksize << "," << 0 << ","
                        << array.getYDim()->name << "=" << 0  << ":" << y_max - y_min  << "," << array.getYDim()->chunksize << "," << 0 << "],"
                        << Utils::defaultNoDataSciDB ( array.attrs[nband].typeId ) << ")))";

                }
                else {
                    afl << "transpose(project(subarray(" << arr << "," << x_min << "," << y_min << "," << x_max << "," << y_max << ")," << array.attrs[nband].name << "))";
                }
            }
            else {
                if ( emptycheck ) {

                    afl << "transpose(merge(";
                    afl << "project(between(" << arr << "," << x_min << "," << y_min << "," << x_max << "," << y_max << ")," << array.attrs[nband].name << ")";
                    afl << ",between(build(<" << array.attrs[nband].name << ":" << array.attrs[nband].typeId << "> ["
                        << array.getXDim()->name << "=" << array.getXDim()->start  << ":" << array.getXDim()->start + array.getXDim()->length - 1 << "," << array.getXDim()->chunksize << "," << 0 << ","
                        << array.getYDim()->name << "=" << array.getYDim()->start  << ":" << array.getYDim()->start + array.getYDim()->length - 1 << "," << array.getYDim()->chunksize << "," << 0 << "],"
                        << 0 << ")," << x_min << "," << y_min << "," << x_max << "," << y_max << ")))";
                }
                else {
                    afl << "transpose(project(between(" << arr << "," << x_min << "," << y_min << "," << x_max << "," << y_max << ")," << array.attrs[nband].name << "))";

                }

            }

        }



        Utils::debug ( "Performing AFL Query: " +  afl.str() );

        ss << "&query=" << curl_easy_escape ( _curl_handle, afl.str().c_str(), 0 ) << "&save=" << "(" << array.attrs[nband].typeId << ")"; //  //if (array.attrs[nband].nullable) ss << " null";  TODO: Null value handling
        if ( _ssl && !_auth.empty() ) ss << "&auth=" << _auth; // Add auth parameter if using ssl

        curl_easy_setopt ( _curl_handle, CURLOPT_URL, ss.str().c_str() );
        curl_easy_setopt ( _curl_handle, CURLOPT_HTTPGET, 1 );
        response = "";
        curl_easy_setopt ( _curl_handle, CURLOPT_WRITEFUNCTION, &responseToStringCallback );
        curl_easy_setopt ( _curl_handle, CURLOPT_WRITEDATA, &response );
        curlPerform();
        curlEnd();

        curlBegin();
        // READ BYTES  ////////////////////////////
        ss.str ( "" );
        ss << _host << SHIMENDPOINT_READ_BYTES << "?" << "id=" << sessionID << "&n=0";
        if ( _ssl && !_auth.empty() ) ss << "&auth=" << _auth; // Add auth parameter if using ssl
        curl_easy_setopt ( _curl_handle, CURLOPT_URL, ss.str().c_str() );
        curl_easy_setopt ( _curl_handle, CURLOPT_HTTPGET, 1 );

        response = "";
        struct SingleAttributeChunk data;
        data.memory = ( char * ) outchunk;
        data.size = 0;

        curl_easy_setopt ( _curl_handle, CURLOPT_WRITEFUNCTION, responseBinaryCallback );
        curl_easy_setopt ( _curl_handle, CURLOPT_WRITEDATA, ( void * ) &data );
        curlPerform();
        curlEnd();

        releaseSession ( sessionID );

        outchunk = ( void * ) data.memory;

        return SUCCESS;
    }



    StatusCode ShimClient::createTempArray ( SciDBSpatialArray &array )
    {
// 	if (array  == NULL) {
// 	    return ERR_CREATE_NOARRAY;
// 	}

	
        if ( array.name == "" ) {
            Utils::error ( "Cannot create unnamed arrays" );
            return ERR_CREATE_INVALIDARRAYNAME;

        }
        bool exists;
        arrayExists ( array.name, exists );
        if ( exists ) {
	  if (!(_cp->type == ST_SERIES || _cp->type == ST_ARRAY || _cp->type == S_ARRAY)) {
            Utils::error ( "Array '" + array.name + "' already exists in SciDB database" );
            return ERR_CREATE_ARRAYEXISTS;
	  }
        }

        if ( array.attrs.size() == 0 ) {
            Utils::error ( "No array attributes specified" );
            return ERR_CREATE_UNKNOWN;
        }

        stringstream ss;

        int sessionID = newSession();

        // Build afl query, e.g. CREATE ARRAY A <x: double, err: double> [i=0:99,10,0, j=0:99,10,0];
        stringstream afl;
        afl << "CREATE TEMP ARRAY " << array.name << SCIDB4GDAL_ARRAYSUFFIX_TEMP;
	//Append attribute specification
        afl << " <";
        for ( uint32_t i = 0; i < array.attrs.size(); ++i ) {
            afl << array.attrs[i].name << ": " << array.attrs[i].typeId;
	    if (i != array.attrs.size() - 1) {
	      afl << ",";
	    } else {
	      afl << ">";
	    }
        }
        
        //Append dimension spec
        afl << " [";
        for ( uint32_t i = 0; i < array.dims.size(); ++i ) {
	    string dimHigh = "";
	    if (array.dims[i].high == INT64_MAX) {
		dimHigh = "*";
	    } else {
	      dimHigh = boost::lexical_cast<string>(array.dims[i].high);
	    }
	    
            afl << array.dims[i].name << "=" << array.dims[i].low << ":" << dimHigh << "," << array.dims[i].chunksize << "," << 0 ; // TODO: Overlap
            if (i != array.dims.size()-1) {
	      afl << ", ";
	    } else {
	      afl << "]";
	    }
        }

        //afl << ";";
        string aflquery = afl.str();
        Utils::debug ( "Performing AFL Query: " +  afl.str() );

        //Utils::debug("AFL QUERY: " + afl.str());

        curlBegin();
        // EXECUTE QUERY  ////////////////////////////
        ss.str ( "" );
        ss << _host << SHIMENDPOINT_EXECUTEQUERY  << "?" << "id=" << sessionID << "&query=" << curl_easy_escape ( _curl_handle, aflquery.c_str(), 0 );
        if ( _ssl && !_auth.empty() ) ss << "&auth=" << _auth; // Add auth parameter if using ssl
        
        //Set the HTTP options URL and the operations
        curl_easy_setopt ( _curl_handle, CURLOPT_URL, ss.str().c_str() );
        curl_easy_setopt ( _curl_handle, CURLOPT_HTTPGET, 1 );
        if ( curlPerform() != CURLE_OK ) {
            curlEnd();
            return ERR_CREATE_UNKNOWN;
        }
        curlEnd();


        releaseSession ( sessionID );


        return SUCCESS;

    }



    StatusCode ShimClient::persistTempArray ( string src, string dest )
    {
        if ( dest == "" ) {
            Utils::error ( "Cannot create unnamed arrays" );
            return ERR_CREATE_INVALIDARRAYNAME;

        }
        bool exists;
	
	arrayExists ( src, exists );
        if ( !exists ) {
            Utils::error ( "Source array '" + src + "' does not exist in SciDB database" );
            return ERR_CREATE_ARRAYEXISTS;
        }
	
        arrayExists ( dest, exists );
        if ( exists ) {
	    bool integrateable = arrayIntegrateable(src,dest);
	    
	    
	    //TODO check if array can be integrated into an array collection
	    // therefore check dimensionality
	    // check boundaries
	    // calculate position of array in the other (first in temporal axis)
	    // problem: at this point there is no temporal reference
	  
	    
	    if (integrateable) {
	      return TEMP_ARRAY_READY_FOR_INTEGRATION;
	    } else {
	      Utils::error ( "Target array '" + dest + "' already exists in SciDB database and cannot be extended." );
	      return ERR_CREATE_ARRAYEXISTS;
	    }
            
        } else {
	    //array does not exist, no problem in storing the data
	    return persistArray(src, dest);
	}

        
    }
    
    StatusCode ShimClient::persistArray(string srcArr, string tarArr)
    {
	  //create new array
	  int sessionID = newSession();


	  stringstream afl;
	  afl << "store(" << srcArr << ", " << tarArr << ")";
	  Utils::debug ( "Performing AFL Query: " +  afl.str() );


	  curlBegin();
	  // EXECUTE QUERY  ////////////////////////////
	  stringstream ss;
	  ss << _host << SHIMENDPOINT_EXECUTEQUERY  << "?" << "id=" << sessionID << "&query=" << curl_easy_escape ( _curl_handle, afl.str().c_str(), 0 );
	  if ( _ssl && !_auth.empty() ) ss << "&auth=" << _auth; // Add auth parameter if using ssl
	  curl_easy_setopt ( _curl_handle, CURLOPT_URL, ss.str().c_str() );
	  curl_easy_setopt ( _curl_handle, CURLOPT_HTTPGET, 1 );
	  if ( curlPerform() != CURLE_OK ) {
	      curlEnd();
	      return ERR_GLOBAL_UNKNOWN;
	  }
	  curlEnd();

	  releaseSession ( sessionID );

	  return SUCCESS;
    }
    
    StatusCode ShimClient::insertInto(SciDBArray &srcArray, SciDBArray &destArray)
    {
      	  //create new array
	  int sessionID = newSession();
	  string collArr = destArray.name; //array name in which we want to store the data
	  string tmpArr = srcArray.name; //array name with the source data
	  
	  stringstream castSchema;
	  castSchema << "<";
	  
	  //attributes
	  for ( uint32_t i = 0; i < srcArray.attrs.size(); i++ ) {
	      string nullable = (srcArray.attrs[i].nullable) ? " NULL" : "";
	      castSchema << srcArray.attrs[i].name << ":" << srcArray.attrs[i].typeId << "" << nullable << ",";
	  }
	  
	  // special handling for the over attributes (over_x, over_y, over_t) -> rename them based on the stored names for dimensions in 
	  // the destination array
	  if ( SciDBSpatioTemporalArray* starray = dynamic_cast<SciDBSpatioTemporalArray*>(&destArray)) {
	    castSchema << starray->getXDim()->name <<":int64 NULL, "<< starray->getYDim()->name <<":int64 NULL, "<< starray->getTDim()->name <<":int64 NULL>["; //target attibute names from (over_x, over_y, over_t)
	  } else if (SciDBSpatialArray* sarray = dynamic_cast<SciDBSpatialArray*>(&destArray)) {
	    //eo_over creates attributes over_x,over_y AND over_t every time
	    castSchema << sarray->getXDim()->name <<":int64 NULL, "<< sarray->getYDim()->name <<":int64 NULL, t:int64 NULL>["; //target attibute names from (over_x, over_y, over_t)
	  } else {
	     Utils::debug("Cannot cast array to spatial or spatiotemporal. Using standard axis definitions.");
	     castSchema << "x:int64 NULL, y:int64 NULL, t:int64 NULL>[";
	  }
	  
	  // add dimension statement and rename it with src_{x|y|t}
	  // [src_x=0:1000,512,0, ...] name=min:max,chunksize,overlap
	  for ( uint32_t i = 0; i < srcArray.dims.size(); i++ ) {
	      string dim_name = srcArray.dims[i].name;
	      string dimHigh;
	      if (srcArray.dims[i].high == INT64_MAX) {
		  dimHigh = "*";
	      } else {
		  dimHigh = boost::lexical_cast<string>(srcArray.dims[i].high);
	      }
	      castSchema << "src_" << dim_name << "=" << srcArray.dims[i].low << ":" << dimHigh  << "," << srcArray.dims[i].chunksize << ",0" ; //TODO change statement for overlap
	      
	      if (i < srcArray.dims.size() - 1) {
		castSchema << ", ";
	      }
	  }
	  
	  castSchema << "]";
	  //the dimensions of the src array will be discarded later
	  
	  stringstream afl;
	  //afl << "store(" << srcArr << ", " << tarArr << ")";
	  //insert(redimension(cast(join(tmpArr, eo_over(tmpArr,collArr)),<>[]),collArr),collArr) *<>[] being the template
	  string eo_over = "eo_over(" + tmpArr + "," + collArr + ")";
	  string join = "join("+ tmpArr + ", "+ eo_over + ")";
	  string cast = "cast("+ join + ", "+ castSchema.str() + ")";
	  string redimension = "redimension("+ cast +","+ collArr +")";
	  
	  afl << "insert(" << redimension << ", " << collArr <<")";
	  Utils::debug ( "Performing AFL Query: " +  afl.str() );


	  curlBegin();
	  // EXECUTE QUERY  ////////////////////////////
	  stringstream ss;
	  ss << _host << SHIMENDPOINT_EXECUTEQUERY  << "?" << "id=" << sessionID << "&query=" << curl_easy_escape ( _curl_handle, afl.str().c_str(), 0 );
	  if ( _ssl && !_auth.empty() ) ss << "&auth=" << _auth; // Add auth parameter if using ssl
	  curl_easy_setopt ( _curl_handle, CURLOPT_URL, ss.str().c_str() );
	  curl_easy_setopt ( _curl_handle, CURLOPT_HTTPGET, 1 );
	  if ( curlPerform() != CURLE_OK ) {
	      curlEnd();
	      return ERR_GLOBAL_UNKNOWN;
	  }
	  curlEnd();

	  releaseSession ( sessionID );

	  return SUCCESS;
    }


  //TODO implement
    bool ShimClient::arrayIntegrateable(string srcArr, string tarArr)
    {
	return true;
    }




    // This is probably incomplete but should be enough for shim generated filenames on the server
    bool isIllegalFilenameCharacter ( char c )
    {
        return ! ( std::isalnum ( c ) || c == '/' || c == '_' || c == '-' || c == '.' );
    }

    StatusCode ShimClient::insertData ( SciDBSpatialArray &array, void *inChunk, int32_t x_min, int32_t y_min, int32_t x_max, int32_t y_max )
    {
        // TODO: Do some checks

        string tempArray = array.name + SCIDB4GDAL_ARRAYSUFFIX_TEMPLOAD; // SciDB 14.12 marks nested load operations as deprecated, so we create a temporary array first
        // If temporary load array exists, it will be automatically removed
        bool exists;
        arrayExists ( tempArray, exists );
        if ( exists ) {
            removeArray ( tempArray ); // TODO: Error checks
        }


        // Shim create session
        int sessionID = newSession();


        // Shim upload file from binary stream
        string format = array.getFormatString();

        // Get total size in bytes of one pixel, i.e. sum of attribute sizes
        size_t pixelSize = 0;
        uint32_t nx = ( 1 + x_max - x_min );
        uint32_t ny = ( 1 + y_max - y_min );
        for ( uint32_t i = 0; i < array.attrs.size(); ++i ) pixelSize += Utils::scidbTypeIdBytes ( array.attrs[i].typeId );
        size_t totalSize = pixelSize *  nx * ny;

        //Utils::debug ( "TOTAL SIZE TO UPLOAD: " + boost::lexical_cast<string> ( totalSize ) );




        // UPLOAD FILE ////////////////////////////
        stringstream ss;
        ss.str ( "" );
        ss << _host << ":" << _port <<  SHIMENDPOINT_UPLOAD_FILE; // neccessary to put port in URL due to  HTTP 100-continue
        ss << "?" << "id=" << sessionID;
        if ( _ssl && !_auth.empty() ) ss << "&auth=" << _auth; // Add auth parameter if using ssl

        struct curl_httppost *formpost = NULL;
        struct curl_httppost *lastptr = NULL;

        // Load file from buffer instead of file!
	// Form HTTP POST, first two pointers next the KVP for the form
        curl_formadd ( &formpost, &lastptr, 
		       CURLFORM_COPYNAME, "file", 
		       CURLFORM_BUFFER, SCIDB4GDAL_DEFAULT_UPLOAD_FILENAME, 
		       CURLFORM_BUFFERPTR, inChunk, 
		       CURLFORM_BUFFERLENGTH, totalSize,  
		       CURLFORM_CONTENTTYPE, "application/octet-stream", 
		       CURLFORM_END );

        curlBegin();
        string remoteFilename = "";
        //curl_easy_setopt(_curl_handle, CURLOPT_FOLLOWLOCATION, 1L);

        curl_easy_setopt ( _curl_handle, CURLOPT_URL, ss.str().c_str() );
        curl_easy_setopt ( _curl_handle, CURLOPT_HTTPPOST, formpost );
        curl_easy_setopt ( _curl_handle, CURLOPT_WRITEFUNCTION, &responseToStringCallback );
        curl_easy_setopt ( _curl_handle, CURLOPT_WRITEDATA, &remoteFilename );

        if ( curlPerform() != CURLE_OK ) {
            curlEnd();
            return ERR_CREATE_UNKNOWN;
        }
        curlEnd();
        curl_formfree ( formpost );


        // Remove special characters from remote filename
        remoteFilename.erase ( std::remove_if ( remoteFilename.begin(), remoteFilename.end(), isIllegalFilenameCharacter ), remoteFilename.end() );

        {
            /*
             * CREATE ARRAY tempArray < ... > [i=0:*,SCIDB4GDAL_DEFAULT_BLOCKSIZE_X*SCIDB4GDAL_DEFAULT_BLOCKSIZE_Y,0];
             */

            stringstream afl;
            afl << "CREATE TEMP ARRAY " << tempArray;
            afl << " <";
            for ( uint32_t i = 0; i < array.attrs.size() - 1; ++i ) {
                afl << array.attrs[i].name << ": " << array.attrs[i].typeId << ",";
            }
            afl << array.attrs[array.attrs.size() - 1].name << ": " << array.attrs[array.attrs.size() - 1].typeId << ">";
            //afl << " [" << "i=0:*," << SCIDB4GDAL_DEFAULT_BLOCKSIZE_X *SCIDB4GDAL_DEFAULT_BLOCKSIZE_Y << ",0]";
            afl << " [" << "i=0:*," << array.getXDim()->chunksize *array.getXDim()->chunksize << ",0]";
            Utils::debug ( "Performing AFL Query: " +  afl.str() );

            curlBegin();
            ss.str ( "" );
            ss << _host << SHIMENDPOINT_EXECUTEQUERY << "?" << "id=" << sessionID << "&query=" << curl_easy_escape ( _curl_handle, afl.str().c_str(), 0 );
            if ( _ssl && !_auth.empty() ) ss << "&auth=" << _auth; // Add auth parameter if using ssl
            curl_easy_setopt ( _curl_handle, CURLOPT_URL, ss.str().c_str() );
            curl_easy_setopt ( _curl_handle, CURLOPT_HTTPGET, 1 );
            if ( curlPerform() != CURLE_OK ) {
                curlEnd();
                if ( removeArray ( tempArray ) != SUCCESS ) {
                    Utils::error ( "Could not delete temporary load array '" + tempArray + "'. This may result in an inconsistent state, please check in SciDB." );
                }
                return ERR_CREATE_UNKNOWN;
            }
            curlEnd();
        }



        {
            /*
             * load(tempArray,'remoteFilename', -2, 'format');
             */
            stringstream afl;
            afl << "load(" << tempArray << ",'" << remoteFilename.c_str() << "', -2, '" << format << "')"; // TODO: Add error checking, max errors, and shadow array
            Utils::debug ( "Performing AFL Query: " +  afl.str() );
            curlBegin();
            ss.str ( "" );
            ss << _host << SHIMENDPOINT_EXECUTEQUERY << "?" << "id=" << sessionID << "&query=" << curl_easy_escape ( _curl_handle, afl.str().c_str(), 0 );
            if ( _ssl && !_auth.empty() ) ss << "&auth=" << _auth; // Add auth parameter if using ssl
            curl_easy_setopt ( _curl_handle, CURLOPT_URL, ss.str().c_str() );
            curl_easy_setopt ( _curl_handle, CURLOPT_HTTPGET, 1 );
            if ( curlPerform() != CURLE_OK ) {
                curlEnd();
                Utils::warn ( "Loading binary data to temporary SciDB array'" + tempArray + "' failed. Trying to recover initial state." );
                if ( removeArray ( tempArray ) != SUCCESS ) {
                    Utils::warn ( "Could not delete temporary load array '" + tempArray + "'. This may result in an inconsistent state, please check in SciDB." );
                }
                return ERR_CREATE_UNKNOWN;
            }
            curlEnd();
        }

	 /* insert
	  * insert(
	  *     redimension(
	  *         apply(TEMP_ARRAY, array.dims[xidx].name,  (int64)xmin +  ((int64)i % (int64)nx),          array.dims[yidx].name,  (int64)ymin +  (int64)((int64)i / (int64)ny))
	  *         ,array.name)
	  *     ,a)
	*/
        {
            stringstream afl;
            afl << "insert(redimension(apply(" << tempArray << ","; // TODO: Check dimension indices + ordering...

            afl << array.dims[array.getXDimIdx()].name << "," << "int64(" << x_min  << ") + int64(i) % int64(" << nx << ")";
            afl << ",";
            afl << array.dims[array.getYDimIdx()].name << "," << "int64(" << y_min  << ") + int64(int64(i) / int64(" << nx << "))";
            afl << "), " << array.name << SCIDB4GDAL_ARRAYSUFFIX_TEMP << "), " << array.name << SCIDB4GDAL_ARRAYSUFFIX_TEMP << ")";

            Utils::debug ( "Performing AFL Query: " +  afl.str() );


            curlBegin();
            ss.str ( "" );
            ss << _host << SHIMENDPOINT_EXECUTEQUERY << "?" << "id=" << sessionID << "&query=" << curl_easy_escape ( _curl_handle, afl.str().c_str(), 0 );
            if ( _ssl && !_auth.empty() ) ss << "&auth=" << _auth; // Add auth parameter if using ssl
            curl_easy_setopt ( _curl_handle, CURLOPT_URL, ss.str().c_str() );
            curl_easy_setopt ( _curl_handle, CURLOPT_HTTPGET, 1 );
            curl_easy_setopt ( _curl_handle, CURLOPT_HTTPGET, 1 );
            if ( curlPerform() != CURLE_OK ) {
                curlEnd();
                Utils::warn ( "Insertion or redimensioning of temporary array '" + tempArray + "' failed. Trying to recover initial state." );
                if ( removeArray ( tempArray ) != SUCCESS ) {
                    Utils::warn ( "Could not delete temporary load array '" + tempArray + "'. This may result in an inconsistent state, please check in SciDB." );
                }
                return ERR_CREATE_UNKNOWN;
            }
            curlEnd();
        }


	/*
	 * clean up
	* remove (tempArray);
	*/
	removeArray(tempArray);


        // Release session
        releaseSession ( sessionID );

        return SUCCESS;
    }



    StatusCode ShimClient::getAttributeStats ( SciDBSpatialArray &array, uint8_t nband, SciDBAttributeStats &out )
    {


        if ( nband >= array.attrs.size() ) {
            Utils::error ( "Invalid attribute index" );
        }

        int sessionID = newSession();

        stringstream ss, afl;
        string aname = array.attrs[nband ].name;
	
	//create an output table based on the name of array
	//min(), max(),avg(),stdev()
	// and save it as 4 double values
        afl << "aggregate(" << array.name << ",min(" << aname << "),max(" << aname << "),avg(" << aname << "),stdev(" << aname << "))";
        Utils::debug ( "Performing AFL Query: " +  afl.str() );

        ss.str();
        ss << _host << SHIMENDPOINT_EXECUTEQUERY << "?" << "id=" << sessionID << "&query=" << afl.str() << "&save=" << "(double,double,double,double)";
        if ( _ssl && !_auth.empty() ) ss << "&auth=" << _auth; // Add auth parameter if using ssl

        curlBegin();
        curl_easy_setopt ( _curl_handle, CURLOPT_URL, ss.str().c_str() );
        curl_easy_setopt ( _curl_handle, CURLOPT_HTTPGET, 1 );
        curlPerform();
        curlEnd();



        curlBegin();
        // READ BYTES  ////////////////////////////
        ss.str ( "" );
        ss << _host << SHIMENDPOINT_READ_BYTES << "?" << "id=" << sessionID << "&n=0";
        if ( _ssl && !_auth.empty() ) ss << "&auth=" << _auth; // Add auth parameter if using ssl
        curl_easy_setopt ( _curl_handle, CURLOPT_URL, ss.str().c_str() );
        curl_easy_setopt ( _curl_handle, CURLOPT_HTTPGET, 1 );

        struct SingleAttributeChunk data;
        data.memory = ( char * ) malloc ( sizeof ( double ) * 4 );
        data.size = 0;

        curl_easy_setopt ( _curl_handle, CURLOPT_WRITEFUNCTION, responseBinaryCallback );
        curl_easy_setopt ( _curl_handle, CURLOPT_WRITEDATA, ( void * ) &data );
        curlPerform();
        curlEnd();


        out.min = ( ( double * ) data.memory ) [0];
        out.max = ( ( double * ) data.memory ) [1];
        out.mean = ( ( double * ) data.memory ) [2];
        out.stdev = ( ( double * ) data.memory ) [3];

        free ( data.memory );


        releaseSession ( sessionID );

        return SUCCESS;


    }





    StatusCode ShimClient::arrayExists ( const string &inArrayName, bool &out )
    {
        stringstream ss, afl;


        int sessionID = newSession();

        // There might be less complex queries but this one always succeeds and does not give HTTP 500 SciDB errors
        afl << "aggregate(filter(list('arrays'),name='" << inArrayName << "'),count(name))";
        Utils::debug ( "Performing AFL Query: " +  afl.str() );
	
// 	createSHIMExecuteString(ss, sessionID, afl);
        ss.str();
        ss << _host << SHIMENDPOINT_EXECUTEQUERY << "?" << "id=" << sessionID << "&query=" << afl.str() << "&save=" << "(int64)";
        if ( _ssl && !_auth.empty() ) ss << "&auth=" << _auth; // Add auth parameter if using ssl

        curlBegin();
        curl_easy_setopt ( _curl_handle, CURLOPT_URL, ss.str().c_str() );
        curl_easy_setopt ( _curl_handle, CURLOPT_HTTPGET, 1 );
        curlPerform();
        curlEnd();



        curlBegin();
        // READ BYTES  ////////////////////////////
        ss.str ( "" );
        ss << _host << SHIMENDPOINT_READ_BYTES << "?" << "id=" << sessionID << "&n=0";
        if ( _ssl && !_auth.empty() ) ss << "&auth=" << _auth; // Add auth parameter if using ssl
        curl_easy_setopt ( _curl_handle, CURLOPT_URL, ss.str().c_str() );
        curl_easy_setopt ( _curl_handle, CURLOPT_HTTPGET, 1 );

        struct SingleAttributeChunk data;
        data.memory = ( char * ) malloc ( sizeof ( int64_t ) * 1 ); // Expect just one integer
        data.size = 0;

        curl_easy_setopt ( _curl_handle, CURLOPT_WRITEFUNCTION, responseBinaryCallback );
        curl_easy_setopt ( _curl_handle, CURLOPT_WRITEDATA, ( void * ) &data );
        curlPerform();
        curlEnd();


        uint64_t count = ( ( int64_t * ) data.memory ) [0];

        if ( count > 0 ) out = true;
        else out = false;


        free ( data.memory );


        releaseSession ( sessionID );

        return SUCCESS;

    }



    StatusCode ShimClient::updateSRS ( SciDBSpatialArray &array )
    {
        // Add spatial reference system information if available
        if ( array.srtext != "" ) {

            int sessionID = newSession();


            /* In the following, we assume the SRS to be already known by scidb4geo.
               This might only work for EPSG codes and in the future, this should be checked and
               unknown SRS should be registered via st_regnewsrs() automatically */
            curlBegin();
            // EXECUTE QUERY  ////////////////////////////
            stringstream afl;
            afl << "st_setsrs(" << array.name << ",'" << array.getXDim()->name << "','" << array.getYDim()->name << "','" << array.auth_name << "'," << array.auth_srid << ",'" <<  array.affineTransform.toString() << "')";
            Utils::debug ( "Performing AFL Query: " +  afl.str() );

            stringstream ss;
            ss << _host << SHIMENDPOINT_EXECUTEQUERY << "?" << "id=" << sessionID << "&query=" << curl_easy_escape ( _curl_handle, afl.str().c_str(), 0 );
            if ( _ssl && !_auth.empty() ) ss << "&auth=" << _auth; // Add auth parameter if using ssl
            curl_easy_setopt ( _curl_handle, CURLOPT_URL, ss.str().c_str() );
            curl_easy_setopt ( _curl_handle, CURLOPT_HTTPGET, 1 );
            curlPerform();
            curlEnd();



            releaseSession ( sessionID );
        }
        else {
            // TODO: How to remove SRS information? Is this neccessary at all?
	    Utils::debug("No spatial reference was set. Continuing without SR. Maybe no longer referenceable by GDAL");

        }

        return SUCCESS;
    }


    StatusCode ShimClient::removeArray ( const string &inArrayName )
    {
        int sessionID = newSession();


        curlBegin();
        stringstream ss,afl;
        afl << "remove(" << inArrayName << ")";
	
        Utils::debug ( "Performing AFL Query: " +  afl.str() );
        createSHIMExecuteString(ss, sessionID, afl);
// 	ss << _host << SHIMENDPOINT_EXECUTEQUERY << "?" << "id=" << sessionID << "&query=" << afl.str();
// 	if ( _ssl && !_auth.empty() ) ss << "&auth=" << _auth; // Add auth parameter if using ssl
// 	
        curl_easy_setopt ( _curl_handle, CURLOPT_URL, ss.str().c_str() );
        curl_easy_setopt ( _curl_handle, CURLOPT_HTTPGET, 1 );
        if ( curlPerform() != CURLE_OK ) {
            curlEnd();
            Utils::warn ( "Cannot remove array '" + inArrayName + "'" );
            return ERR_GLOBAL_UNKNOWN;
        }
        curlEnd();
        releaseSession ( sessionID );
        return SUCCESS;

    }




    StatusCode  ShimClient::setArrayMD ( string arrayname, std::map< std::string, std::string > kv, string domain )
    {

        stringstream key_array_str;
        stringstream val_array_str;

        uint32_t i = 0;
        for ( std::map<string, string>::iterator it = kv.begin(); it != kv.end(); ++it ) {
            key_array_str <<  it->first  ;
            val_array_str << it->second;
            if ( i++ < kv.size() - 1 ) {
                val_array_str << ",";
                key_array_str << ",";
            }
        }


        int sessionID = newSession();

        curlBegin();
        stringstream ss, afl;
        afl << "eo_setmd(" << arrayname << ",'" << key_array_str.str() << "','" << val_array_str.str() << "')";
        Utils::debug ( "Performing AFL Query: " +  afl.str() );
        ss << _host << SHIMENDPOINT_EXECUTEQUERY << "?" << "id=" << sessionID << "&query=" << afl.str();
        if ( _ssl && !_auth.empty() ) ss << "&auth=" << _auth; // Add auth parameter if using ssl
        curl_easy_setopt ( _curl_handle, CURLOPT_URL, ss.str().c_str() );
        curl_easy_setopt ( _curl_handle, CURLOPT_HTTPGET, 1 );
        if ( curlPerform() != CURLE_OK ) {
            curlEnd();
            Utils::warn ( "Cannot set metadata for array '" + arrayname + "'" );
            return ERR_GLOBAL_UNKNOWN;
        }
        curlEnd();
        releaseSession ( sessionID );
        return SUCCESS;
    }



    StatusCode ShimClient::getArrayMD ( std::map< string, string > &kv, string arrayname, string domain )
    {

        int sessionID = newSession();
        string response;
        {
            curlBegin();
            stringstream ss, afl;
            afl << "project(filter(eo_getmd(" << arrayname << "),attribute='' and domain='" << domain << "'), key, value)";

            Utils::debug ( "Performing AFL Query: " +  afl.str() );

            ss << _host << SHIMENDPOINT_EXECUTEQUERY << "?" << "id=" << sessionID << "&query=" << afl.str()  << "&save=" << "csv";
            if ( _ssl && !_auth.empty() ) ss << "&auth=" << _auth; // Add auth parameter if using ssl
            curl_easy_setopt ( _curl_handle, CURLOPT_URL, ss.str().c_str() );
            curl_easy_setopt ( _curl_handle, CURLOPT_HTTPGET, 1 );

            response = "";
            curl_easy_setopt ( _curl_handle, CURLOPT_WRITEFUNCTION, &responseToStringCallback );
            curl_easy_setopt ( _curl_handle, CURLOPT_WRITEDATA, &response );
            if ( curlPerform() != CURLE_OK ) {
                curlEnd();
                Utils::error ( "Cannot get metadata for array '" + arrayname + "'." );
                return ERR_READ_UNKNOWN;
            }
            curlEnd();

        }


        // Parse CSV
        stringstream ss;

        vector<string> rows;
        boost::split ( rows, response, boost::is_any_of ( "\n" ) );
        for ( vector<string>::iterator it = ++ ( rows.begin() ); it != rows.end(); ++it ) { // ignore first row
            vector<string> cols;
            boost::split ( cols, *it, boost::is_any_of ( "," ) );
            if ( cols.size() != 2 ) {
                continue;// TODO: Error handling, expected 2 cols
            }
            if ( cols[0].length() < 1 || cols[1].length() < 1 ) {
                continue;
            }

            string key = cols[0].substr ( 1, cols[0].length() - 2 );
            string val = cols[1].substr ( 1, cols[1].length() - 2 );


            kv.insert ( std::pair<string, string> ( key, val ) );

        }

        releaseSession ( sessionID );

        return SUCCESS;

    }






    StatusCode ShimClient::setAttributeMD ( string arrayname, string attribute, map< string, string > kv, string domain )
    {

        stringstream key_array_str;
        stringstream val_array_str;

        uint32_t i = 0;
        for ( std::map<string, string>::iterator it = kv.begin(); it != kv.end(); ++it ) {
            key_array_str <<  it->first  ;
            val_array_str << it->second;
            if ( i++ < kv.size() - 1 ) {
                val_array_str << ",";
                key_array_str << ",";
            }
        }


        int sessionID = newSession();

        curlBegin();
        stringstream ss, afl;
        afl << "eo_setmd(" << arrayname << ",'" << attribute << "','" << key_array_str.str() << "','" << val_array_str.str() << "')";
        Utils::debug ( "Performing AFL Query: " +  afl.str() );
        ss << _host << SHIMENDPOINT_EXECUTEQUERY << "?" << "id=" << sessionID << "&query=" << afl.str();
        if ( _ssl && !_auth.empty() ) ss << "&auth=" << _auth; // Add auth parameter if using ssl
        curl_easy_setopt ( _curl_handle, CURLOPT_URL, ss.str().c_str() );
        curl_easy_setopt ( _curl_handle, CURLOPT_HTTPGET, 1 );
        if ( curlPerform() != CURLE_OK ) {
            curlEnd();
            Utils::warn ( "Cannot set metadata for attribute '" + arrayname + "." + attribute + "'" );
            return ERR_GLOBAL_UNKNOWN;
        }
        curlEnd();
        releaseSession ( sessionID );
        return SUCCESS;
    }







    StatusCode ShimClient::getAttributeMD ( std::map< std::string, std::string > &kv, std::string arrayname, string attribute, std::string domain )
    {
        int sessionID = newSession();
        string response;
        {
            curlBegin();
            stringstream ss, afl;
            afl << "project(filter(eo_getmd(" << arrayname << "),attribute='" << attribute << "' and domain='" << domain << "'), key, value)";

            Utils::debug ( "Performing AFL Query: " +  afl.str() );

            ss << _host << SHIMENDPOINT_EXECUTEQUERY << "?" << "id=" << sessionID << "&query=" << afl.str()  << "&save=" << "csv";
            if ( _ssl && !_auth.empty() ) ss << "&auth=" << _auth; // Add auth parameter if using ssl
            curl_easy_setopt ( _curl_handle, CURLOPT_URL, ss.str().c_str() );
            curl_easy_setopt ( _curl_handle, CURLOPT_HTTPGET, 1 );

            response = "";
            curl_easy_setopt ( _curl_handle, CURLOPT_WRITEFUNCTION, &responseToStringCallback );
            curl_easy_setopt ( _curl_handle, CURLOPT_WRITEDATA, &response );
            if ( curlPerform() != CURLE_OK ) {
                curlEnd();
                Utils::error ( "Cannot get metadata for attribute '" + arrayname + "." + attribute  + "'." );
                return ERR_READ_UNKNOWN;
            }
            curlEnd();

        }


        // Parse CSV
        stringstream ss;

        vector<string> rows;
        boost::split ( rows, response, boost::is_any_of ( "\n" ) );
        for ( vector<string>::iterator it = ++ ( rows.begin() ); it != rows.end(); ++it ) { // ignore first row
            vector<string> cols;
            boost::split ( cols, *it, boost::is_any_of ( "," ) );
            if ( cols.size() != 2 ) {
                continue;// TODO: Error handling, expected 2 cols
            }
            if ( cols[0].length() < 1 || cols[1].length() < 1 ) {
                continue;
            }

            SciDBDimension dim;
            string key = cols[0].substr ( 1, cols[0].length() - 2 );
            string val = cols[1].substr ( 1, cols[1].length() - 2 );


            kv.insert ( std::pair<string, string> ( key, val ) );

        }

        releaseSession ( sessionID );

        return SUCCESS;
    }


    void ShimClient::setCreateParameters(CreationParameters &par)
    {
      _cp = &par;
      Utils::debug("Setting temporal parameters. TRS: "+_cp->dt+" and t:"+_cp->timestamp);
    }

    void ShimClient::setConnectionParameters(ConnectionParameters& par)
    {
	_conp = &par;
    }

    void ShimClient::setQueryParameters(QueryParameters& par)
    {
	_qp = &par;
    }



StatusCode ShimClient::updateTRS(SciDBTemporalArray &array)
{
        // Add temporal reference system information if available
        if ( array.getTPoint() != NULL && array.getTInterval() != NULL && array.getTInterval()->toStringISO() != "P") {
	    
            int sessionID = newSession();

            curlBegin();
            // EXECUTE QUERY  ////////////////////////////
            stringstream afl;
            afl << "st_settrs(" << array.name << ",'" << array.tdim << "','" << array.getTPoint()->toStringISO() << "','" << array.getTInterval()->toStringISO() << "')";
            Utils::debug ( "Performing AFL Query: " +  afl.str() );

            stringstream ss;
            ss << _host << SHIMENDPOINT_EXECUTEQUERY << "?" << "id=" << sessionID << "&query=" << curl_easy_escape ( _curl_handle, afl.str().c_str(), 0 );
            if ( _ssl && !_auth.empty() ) ss << "&auth=" << _auth; // Add auth parameter if using ssl
            curl_easy_setopt ( _curl_handle, CURLOPT_URL, ss.str().c_str() );
            curl_easy_setopt ( _curl_handle, CURLOPT_HTTPGET, 1 );
            curlPerform();
            curlEnd();



            releaseSession ( sessionID );
        }
        else {
            // TODO: How to remove SRS information? Is this neccessary at all?
	    return ERR_TRS_INVALID;

        }

        return SUCCESS;
}






  void ShimClient::createSHIMExecuteString(stringstream &base, int &sessionID, stringstream &query)
  {
      base << _host << SHIMENDPOINT_EXECUTEQUERY << "?" << "id=" << sessionID << "&query=" << query.str();
      if ( _ssl && !_auth.empty() ) base << "&auth=" << _auth; // Add auth parameter if using ssl

  }



}
