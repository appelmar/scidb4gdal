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


namespace scidb4gdal
{

    ShimClient::ShimClient() : _host("http://localhost"), _port(8080), _user("scidb"), _passwd("scidb"), _curl_handle(0), _curl_initialized(false)
    {
        curl_global_init(CURL_GLOBAL_ALL);
    }
    ShimClient::ShimClient(string host, uint16_t port, string user, string passwd) : _host(host), _port(port), _user(user), _passwd(passwd), _curl_handle(0), _curl_initialized(false)
    {
        curl_global_init(CURL_GLOBAL_ALL);
    }
    ShimClient::~ShimClient()
    {
        curl_global_cleanup();
        _curl_handle = 0;
    }

    void ShimClient::curlBegin()
    {
        if (_curl_initialized) curlEnd();

        _curl_handle = curl_easy_init();
        _curl_initialized = true;
        //curl_easy_setopt ( _curl_handle, CURLOPT_URL, _host.c_str() );
        curl_easy_setopt(_curl_handle, CURLOPT_PORT, _port);
        curl_easy_setopt(_curl_handle, CURLOPT_HTTPAUTH, CURLAUTH_DIGEST);
        curl_easy_setopt(_curl_handle, CURLOPT_USERNAME, _user.c_str());
        curl_easy_setopt(_curl_handle, CURLOPT_PASSWORD, _passwd.c_str());

#ifdef CURL_VERBOSE
        curl_easy_setopt(_curl_handle, CURLOPT_VERBOSE, 1L);
#endif
    }




    void ShimClient::curlEnd()
    {
        if (_curl_initialized)
        {
            curl_easy_cleanup(_curl_handle);
            _curl_initialized = false;
        }

    }


    CURLcode ShimClient::curlPerform()
    {
        CURLcode res = curl_easy_perform(_curl_handle);
        for (int i = 1; i < CURL_RETRIES && res == CURLE_COULDNT_CONNECT; ++i)
        {
            stringstream s;
            s << "Connection error, retrying ... " << "(#" << i << ")";
            Utils::warn(s.str());
            Utils::sleep(i * 100);
            res = curl_easy_perform(_curl_handle);
        }
        if (res != CURLE_OK)
        {
            Utils::error((string)("curl_easy_perform() failed: ") + curl_easy_strerror(res));
        }
        return res;
    }









    static size_t responseToStringCallback(void* ptr, size_t size, size_t count, void* stream)
    {
        ((string*) stream)->append((char*) ptr, 0, size * count);
        return size * count;
    }





    StatusCode ShimClient::testConnection()
    {

        curlBegin();

        stringstream ss;
        ss << _host << SHIMENDPOINT_VERSION;
        curl_easy_setopt(_curl_handle, CURLOPT_URL, ss.str().c_str());

        // Test connection
        string response;
        curl_easy_setopt(_curl_handle, CURLOPT_WRITEFUNCTION, &responseToStringCallback);
        curl_easy_setopt(_curl_handle, CURLOPT_WRITEDATA, &response);

        curlPerform();
        curlEnd();
        Utils::debug("SHIM Version: " + response);


        return SUCCESS;
    }







    int ShimClient::newSession()
    {

        curlBegin();

        stringstream ss;
        string response;

        // NEW SESSION ID ////////////////////////////
        ss << _host << SHIMENDPOINT_NEW_SESSION;
        curl_easy_setopt(_curl_handle, CURLOPT_URL, ss.str().c_str());
        curl_easy_setopt(_curl_handle, CURLOPT_HTTPGET, 1);


        // Test connection

        curl_easy_setopt(_curl_handle, CURLOPT_WRITEFUNCTION, &responseToStringCallback);
        curl_easy_setopt(_curl_handle, CURLOPT_WRITEDATA, &response);

        curlPerform();

        curlEnd();

        //int sessionID = boost::lexical_cast<int>(response.data());
        int sessionID = atoi(response.c_str());
        if (sessionID > 0)
            return sessionID;


        Utils::error((string)("Invalid session ID"));
        return -1;

    }


    void ShimClient::releaseSession(int sessionID)
    {
        curlBegin();
        stringstream ss;
        ss << _host << SHIMENDPOINT_RELEASE_SESSION;
        ss << "?" << "id=" << sessionID;
        curl_easy_setopt(_curl_handle, CURLOPT_URL, ss.str().c_str());
        curl_easy_setopt(_curl_handle, CURLOPT_HTTPGET, 1);
        string response;
        curl_easy_setopt(_curl_handle, CURLOPT_WRITEFUNCTION, &responseToStringCallback);
        curl_easy_setopt(_curl_handle, CURLOPT_WRITEDATA, &response);
        curlPerform();
        curlEnd();

    }






    StatusCode  ShimClient::getAttributeDesc(const string& inArrayName, vector< SciDBAttribute >& out)
    {


        out.clear();


        int sessionID = newSession();

        string response;
        stringstream ss;
        // EXECUTE QUERY  //////////////////////////// http://localhost:8080/execute_query?id=${s}&query=project(attributes(inArrayName),name,type_id,nullable);&save=dcsv


        {

            stringstream afl;
            ss.str("");
            curlBegin();
            afl << "project(attributes(" << inArrayName << "),name,type_id,nullable)";
            Utils::debug("Performing AFL Query: " +  afl.str());
            ss << _host << SHIMENDPOINT_EXECUTEQUERY;
            ss << "?" << "id=" << sessionID << "&query=" << afl.str() << "&save=" << "csv";

            curl_easy_setopt(_curl_handle, CURLOPT_URL, ss.str().c_str());
            curl_easy_setopt(_curl_handle, CURLOPT_HTTPGET, 1);

            response = "";
            curl_easy_setopt(_curl_handle, CURLOPT_WRITEFUNCTION, &responseToStringCallback);
            curl_easy_setopt(_curl_handle, CURLOPT_WRITEDATA, &response);

            if (curlPerform() != CURLE_OK)
            {
                curlEnd();
                Utils::error("Cannot get attribute information for array '" + inArrayName + "'. Does it exist?");
                return ERR_READ_UNKNOWN;
            }
            curlEnd();

        }

        {

            curlBegin();
            ss.str("");
            // READ BYTES  ///////////////////////////
            ss << _host << SHIMENDPOINT_READ_BYTES << "?" << "id=" << sessionID << "&n=0";
            curl_easy_setopt(_curl_handle, CURLOPT_URL, ss.str().c_str());
            curl_easy_setopt(_curl_handle, CURLOPT_HTTPGET, 1);

            response = "";
            curl_easy_setopt(_curl_handle, CURLOPT_WRITEFUNCTION, &responseToStringCallback);
            curl_easy_setopt(_curl_handle, CURLOPT_WRITEDATA, &response);
            if (curlPerform() != CURLE_OK)
            {
                curlEnd();
                Utils::error("Cannot get attribute information for array '" + inArrayName + "'. Does it exist?");
                return ERR_READ_UNKNOWN;
            }
            curlEnd();
        }


        // Parse CSV
        ss.str("");
        vector<string> rows;
        boost::split(rows, response, boost::is_any_of("\n"));
        for (vector<string>::iterator it = ++ (rows.begin()); it != rows.end(); ++it)       // ignore first row
        {

            vector<string> cols;
            boost::split(cols, *it, boost::is_any_of(","));
            if (cols.size() != 3)
            {
                continue;// TODO: Error handling, expected 3 cols
            }
            if (cols[0].length() < 2 || cols[1].length() < 2 || cols[2].length() < 2) continue;

            SciDBAttribute attr;

            attr.name = cols[0].substr(1, cols[0].length() - 2);    // Remove quotes
            attr.typeId = cols[1].substr(1, cols[1].length() - 2);    // Remove quotes
            attr.nullable = (cols[2].compare("TRUE") * cols[2].compare("true") == 0);

            // Assert attr has datatype that is supported by GDAL
            if (Utils::scidbTypeIdToGDALType(attr.typeId) == GDT_Unknown)
            {
                ss.str("");
                ss << "SciDB GDAL driver does not support data type " << attr.typeId << " of attribute " << attr.name;
                Utils::error(ss.str());
                return ERR_READ_UNKNOWN;
            }

            out.push_back(attr);

        }


        releaseSession(sessionID);

        return SUCCESS;

    }







    StatusCode ShimClient::getDimensionDesc(const string& inArrayName, vector< SciDBDimension>& out)
    {

        out.clear();

        int sessionID = newSession();

        string response;


        {
            curlBegin();
            stringstream ss;
            stringstream afl;
            afl << "project(dimensions(" << inArrayName << "),name,low,high,type,chunk_interval,start,length)";  // project(dimensions(chicago2),name,low,high,type)
            Utils::debug("Performing AFL Query: " +  afl.str());

            ss << _host << SHIMENDPOINT_EXECUTEQUERY << "?" << "id=" << sessionID << "&query=" << afl.str()  << "&save=" << "csv";
            curl_easy_setopt(_curl_handle, CURLOPT_URL, ss.str().c_str());
            curl_easy_setopt(_curl_handle, CURLOPT_HTTPGET, 1);

            response = "";
            curl_easy_setopt(_curl_handle, CURLOPT_WRITEFUNCTION, &responseToStringCallback);
            curl_easy_setopt(_curl_handle, CURLOPT_WRITEDATA, &response);
            if (curlPerform() != CURLE_OK)
            {
                curlEnd();
                Utils::error("Cannot get dimension information for array '" + inArrayName + "'. Does it exist?");
                return ERR_READ_UNKNOWN;
            }
            curlEnd();

        }

        {
            curlBegin();
            stringstream ss;
            response = "";
            ss << _host << SHIMENDPOINT_READ_BYTES << "?" << "id=" << sessionID << "&n=0";
            curl_easy_setopt(_curl_handle, CURLOPT_URL, ss.str().c_str());
            curl_easy_setopt(_curl_handle, CURLOPT_HTTPGET, 1);
            curl_easy_setopt(_curl_handle, CURLOPT_WRITEFUNCTION, &responseToStringCallback);
            curl_easy_setopt(_curl_handle, CURLOPT_WRITEDATA, &response);
            if (curlPerform() != CURLE_OK)
            {
                curlEnd();
                Utils::error("Cannot get dimension information for array '" + inArrayName + "'. Does it exist?");
                return ERR_READ_UNKNOWN;
            }
            curlEnd();
        }

        // Parse CSV
        stringstream ss;

        vector<string> rows;
        boost::split(rows, response, boost::is_any_of("\n"));
        for (vector<string>::iterator it = ++ (rows.begin()); it != rows.end(); ++it)       // ignore first row
        {
            vector<string> cols;
            boost::split(cols, *it, boost::is_any_of(","));
            if (cols.size() != 7)
            {

                continue;// TODO: Error handling, expected 4 cols
            }
            if (cols[0].length() < 2 || cols[1].length() < 1 || cols[2].length() < 1 || cols[3].length() < 1 || cols[4].length() < 1 || cols[5].length() < 1 || cols[6].length() < 1)
            {
                continue;
            }

            SciDBDimension dim;
            dim.name = cols[0].substr(1, cols[0].length() - 2);
            dim.low = boost::lexical_cast<int64_t> (cols[1].c_str());
            dim.high = boost::lexical_cast<int64_t> (cols[2].c_str());
            dim.typeId = cols[3].substr(1, cols[3].length() - 2);    // Remove quotes
            dim.chunksize = boost::lexical_cast<uint32_t> (cols[4].c_str());

            if (dim.high == SCIDB_MAX_DIM_INDEX || dim.low == SCIDB_MAX_DIM_INDEX  || dim.high == -SCIDB_MAX_DIM_INDEX || dim.low == -SCIDB_MAX_DIM_INDEX)     // yet unspecified e.g. for newly created arrays
            {
                dim.low = boost::lexical_cast<int64_t> (cols[5].c_str());
                dim.high = dim.low + boost::lexical_cast<int64_t> (cols[6].c_str()) - 1;
            }


            // Assert  dim.typeId is integer
            if (!(dim.typeId == "int32" || dim.typeId == "int64" || dim.typeId == "int16" || dim.typeId == "int8" ||
                    dim.typeId == "uint32" || dim.typeId == "uint64" || dim.typeId == "uint16" || dim.typeId == "uint8"))
            {
                ss.str("");
                ss << "SciDB GDAL driver works with integer dimensions only. Got dimension " << dim.name << ":" << dim.typeId;
                Utils::error(ss.str());
                return ERR_READ_UNKNOWN;
            }

            out.push_back(dim);

        }

        releaseSession(sessionID);

        return SUCCESS;

    }








    StatusCode ShimClient::getSRSDesc(const string& inArrayName, SciDBSpatialReference& out)
    {




        int sessionID = newSession();
        string response;


        {
            curlBegin();
            stringstream ss;
            stringstream afl;
            afl << "project(st_getsrs(" << inArrayName << "),name,xdim,ydim,srtext,proj4text,A)";  // project(dimensions(chicago2),name,low,high,type)
            Utils::debug("Performing AFL Query: " +  afl.str());
            ss << _host << SHIMENDPOINT_EXECUTEQUERY << "?" << "id=" << sessionID << "&query=" << afl.str() << "&save=" << "csv";
            curl_easy_setopt(_curl_handle, CURLOPT_URL, ss.str().c_str());
            curl_easy_setopt(_curl_handle, CURLOPT_HTTPGET, 1);
            response = "";
            curl_easy_setopt(_curl_handle, CURLOPT_WRITEFUNCTION, &responseToStringCallback);
            curl_easy_setopt(_curl_handle, CURLOPT_WRITEDATA, &response);
            if (curlPerform() != CURLE_OK)
            {
                curlEnd();
                Utils::warn("Cannot find spatial reference information for array '" + inArrayName + "'");
                return ERR_SRS_NOSPATIALREFFOUND;
            };
            curlEnd();
        }


        {
            curlBegin();
            stringstream ss;
            // READ BYTES  ////////////////////////////
            ss.str("");
            ss << _host << SHIMENDPOINT_READ_BYTES << "?" << "id=" << sessionID << "&n=0";

            curl_easy_setopt(_curl_handle, CURLOPT_URL, ss.str().c_str());
            curl_easy_setopt(_curl_handle, CURLOPT_HTTPGET, 1);

            response = "";
            curl_easy_setopt(_curl_handle, CURLOPT_WRITEFUNCTION, &responseToStringCallback);
            curl_easy_setopt(_curl_handle, CURLOPT_WRITEDATA, &response);
            if (curlPerform() != CURLE_OK)
            {
                curlEnd();
                Utils::warn("Cannot read spatial reference information for array '" + inArrayName + "'");
                return ERR_SRS_NOSPATIALREFFOUND;
            };
            curlEnd();
        }


        // Parse CSV

        vector<string> rows;
        boost::split(rows, response, boost::is_any_of("\n"));

        if (rows.size() < 2 || rows.size() > 3)     // Header + 1 data row
        {
            Utils::warn("Cannot extract SRS information from response: "  + response);
            return ERR_SRS_NOSPATIALREFFOUND;
        }

        string row = rows[1];
        vector <string> cols;
        int cur_start = -1;
        for (uint32_t i = 0; i < row.length(); ++i)     // Find items between single quotes 'XXX', only works if all SciDB attributes of query result are strings
        {
            if (row[i] == '\'')
            {
                if (cur_start < 0) cur_start = i;
                else
                {
                    cols.push_back(row.substr(cur_start + 1, i - cur_start - 1));
                    cur_start = -1;

                }
            }
        }


        if (cols.size() != 6)
        {
            Utils::warn("Cannot extract SRS information from response: "  + response);
            return ERR_SRS_NOSPATIALREFFOUND;
//             out.affineTransform = * ( new AffineTransform() );
//             out.ydim  =  out.xdim  = out.proj4text  = out.srtext = "";

        }
        else
        {
            out.xdim = cols[1];
            out.ydim = cols[2];
            out.srtext = cols[3];
            out.proj4text = cols[4];
            out.affineTransform = * (new AffineTransform(cols[5]));      // Should be released
        }

        releaseSession(sessionID);

        return SUCCESS;

    }










    StatusCode  ShimClient::getArrayDesc(const string& inArrayName, SciDBSpatialArray& out)
    {

        bool exists;
        arrayExists(inArrayName, exists);
        if (!exists)
        {
            Utils::error("Array '" + inArrayName + "' does not exist in SciDB database");
            return ERR_READ_ARRAYUNKNOWN;
        }


        out.name = inArrayName;
        // Get dimensions: project(dimensions(inArrayName),name,low,high,type)
        StatusCode res;
        res = getDimensionDesc(inArrayName, out.dims);
        if (res != SUCCESS)
        {
            Utils::error("Cannot extract array dimension metadata");
            return res;
        }


        // Get attributes: project(attributes(inArrayName),name,type_id,nullable);
        res = getAttributeDesc(inArrayName, out.attrs);
        if (res != SUCCESS)
        {
            Utils::error("Cannot extract array attribute metadata");
            return res;
        }

        /* Get spatial metadata: project(st_getsrs(chicago2),name,xdim,ydim,srtext); If array is not spatially referenced,
        a default srs instance is used (representing nonspatial) */

        //  try {
        SciDBSpatialReference srs;
        getSRSDesc(inArrayName, srs);
        out.affineTransform = srs.affineTransform;
        out.xdim = srs.xdim;
        out.ydim = srs.ydim;
        out.proj4text = srs.proj4text;
        out.srtext = srs.srtext;
//         }
//         catch ( ... ) { // if spatial reference system retreival failed for whatever reason, ignore it
//             out.affineTransform = ( new AffineTransform() )->toString();
//             out.ydim  =  out.xdim  = out.proj4text  = out.srtext = "";
//         }



        return SUCCESS;

    }




    /**
     * Helper structure filled while fetching scidb binary stream
     */
    struct SingleAttributeChunk
    {
        char* memory;
        size_t size;

        /*
            template <typename T> T get ( int64_t i ) {
                return ( ( T * ) memory ) [i]; // No overflow checks!
            }*/
    };



    /**
     * Callback function for receiving scidb binary data
     */
    static size_t responseBinaryCallback(void* ptr, size_t size, size_t count, void* stream)
    {

        size_t realsize = size * count;
        struct SingleAttributeChunk* mem = (struct SingleAttributeChunk*) stream;

//  stringstream info;
//  info << "RETREIVED " << realsize << " bytes";
//  Utils::debug(info.str());

        memcpy(& (mem->memory[mem->size]), ptr, realsize);
        mem->size += realsize;

        return realsize;
    }







    StatusCode ShimClient::getData(SciDBSpatialArray& array, uint8_t nband, void* outchunk, int32_t x_min, int32_t y_min, int32_t x_max, int32_t y_max)
    {


        if (x_min < array.getXDim().low || x_min > array.getXDim().high ||
                x_max < array.getXDim().low || x_max > array.getXDim().high ||
                y_min < array.getYDim().low || y_min > array.getYDim().high ||
                y_max < array.getYDim().low || y_max > array.getYDim().high)
        {
            Utils::error("Requested array subset is outside array boundaries");
        }

        if (nband >= array.attrs.size())
            Utils::error("Requested array band does not exist");

        int8_t x_idx = array.getXDimIdx();
        int8_t y_idx = array.getYDimIdx();


        stringstream ss;
        string response;
        SciDBSpatialReference out;



        int sessionID = newSession();



        /* Depending on dimension ordering, array must be transposed. Since GDAL assumes latitude / northing dimension
         * to move fastest, an array must be transposed if dimension order is (lat, lon). */


        curlBegin();
        // EXECUTE QUERY  ////////////////////////////
        ss.str();
        ss << _host << SHIMENDPOINT_EXECUTEQUERY << "?" << "id=" << sessionID;

        stringstream afl;
        if (x_idx > y_idx)   // TODO: need to check performance of differend ordering
            afl << "transpose(project(subarray(" << array.name << "," << y_min << "," << x_min << "," << y_max << "," << x_max << ")," << array.attrs[nband].name << "))";
        else
            afl << "project(subarray(" << array.name << "," << x_min << "," << y_min << "," << x_max << "," << y_max << ")," << array.attrs[nband].name << ")";

        Utils::debug("Performing AFL Query: " +  afl.str());

        ss << "&query=" << afl.str() << "&save=" << "(" << array.attrs[nband].typeId << ")"; //  //if (array.attrs[nband].nullable) ss << " null";  TODO: Null value handling

        curl_easy_setopt(_curl_handle, CURLOPT_URL, ss.str().c_str());
        curl_easy_setopt(_curl_handle, CURLOPT_HTTPGET, 1);
        response = "";
        curl_easy_setopt(_curl_handle, CURLOPT_WRITEFUNCTION, &responseToStringCallback);
        curl_easy_setopt(_curl_handle, CURLOPT_WRITEDATA, &response);
        curlPerform();
        curlEnd();

        curlBegin();
        // READ BYTES  ////////////////////////////
        ss.str("");
        ss << _host << SHIMENDPOINT_READ_BYTES << "?" << "id=" << sessionID << "&n=0";
        curl_easy_setopt(_curl_handle, CURLOPT_URL, ss.str().c_str());
        curl_easy_setopt(_curl_handle, CURLOPT_HTTPGET, 1);

        response = "";
        struct SingleAttributeChunk data;
        data.memory = (char*) outchunk;
        data.size = 0;

        curl_easy_setopt(_curl_handle, CURLOPT_WRITEFUNCTION, responseBinaryCallback);
        curl_easy_setopt(_curl_handle, CURLOPT_WRITEDATA, (void*) &data);
        curlPerform();
        curlEnd();

        releaseSession(sessionID);

        outchunk = (void*) data.memory;

        return SUCCESS;
    }



    StatusCode ShimClient::createArray(SciDBSpatialArray& array)
    {

        if (array.name == "")
        {
            Utils::error("Cannot create unnamed arrays");
            return ERR_CREATE_INVALIDARRAYNAME;

        }
        bool exists;
        arrayExists(array.name, exists);
        if (exists)
        {
            Utils::error("Array '" + array.name + "' already exists in SciDB database");
            return ERR_CREATE_ARRAYEXISTS;
        }

        if (array.dims.size() != 2)
        {
            Utils::error("Only two-dimensional arrays can be created currently");
            return ERR_CREATE_WRONGDIMENSIONALITY;
        }

        if (array.attrs.size() == 0)
        {
            Utils::error("No array attributes specified");
            return ERR_CREATE_UNKNOWN;
        }

        stringstream ss;

        int sessionID = newSession();

        // Build afl query, e.g. CREATE ARRAY A <x: double, err: double> [i=0:99,10,0, j=0:99,10,0];
        stringstream afl;
        afl << "CREATE ARRAY " << array.name;
        afl << " <";
        for (uint32_t i = 0; i < array.attrs.size() - 1; ++i)
        {
            afl << array.attrs[i].name << ": " << array.attrs[i].typeId << ",";
        }
        afl << array.attrs[array.attrs.size() - 1].name << ": " << array.attrs[array.attrs.size() - 1].typeId << ">";
        afl << " [";
        for (uint32_t i = 0; i < array.dims.size() - 1; ++i)
        {
            afl << array.dims[i].name << "=" << array.dims[i].low << ":" << array.dims[i].high << "," << array.dims[i].chunksize << "," << 0  << ", "; // TODO: Overlap
        }
        afl << array.dims[array.dims.size() - 1].name << "=" << array.dims[array.dims.size() - 1].low << ":" << array.dims[array.dims.size() - 1].high << "," << array.dims[array.dims.size() - 1].chunksize << "," << 0 ; // TODO: Overlap
        afl << "]";
        //afl << ";";
        string aflquery = afl.str();
        Utils::debug("Performing AFL Query: " +  afl.str());

        //Utils::debug("AFL QUERY: " + afl.str());

        curlBegin();
        // EXECUTE QUERY  ////////////////////////////
        ss.str("");
        ss << _host << SHIMENDPOINT_EXECUTEQUERY  << "?" << "id=" << sessionID << "&query=" << curl_easy_escape(_curl_handle, aflquery.c_str(), 0);
        curl_easy_setopt(_curl_handle, CURLOPT_URL, ss.str().c_str());
        curl_easy_setopt(_curl_handle, CURLOPT_HTTPGET, 1);
        if (curlPerform() != CURLE_OK)
        {
            curlEnd();
            return ERR_CREATE_UNKNOWN;
        }
        curlEnd();


        releaseSession(sessionID);


        return SUCCESS;

    }


    // This is probably incomplete but should be enough for shim generated filenames on the server
    bool isIllegalFilenameCharacter(char c)
    {
        return !(std::isalnum(c) || c == '/' || c == '_' || c == '-' || c == '.');
    }

    StatusCode ShimClient::insertData(SciDBSpatialArray& array, void* inChunk, int32_t x_min, int32_t y_min, int32_t x_max, int32_t y_max)
    {
        // TODO: Do some checks

        string tempArray = array.name + "_tempload"; // SciDB 14.12 marks nested load operations as deprecated, so we create a temporary array first
        // If temporary load array exists, it will be automatically removed
        bool exists;
        arrayExists(tempArray, exists);
        if (exists)
        {
            removeArray(tempArray); // TODO: Error checks
        }


        // Shim create session
        int sessionID = newSession();


        // Shim upload file from binary stream
        string format = array.getFormatString();

        // Get total size in bytes of one pixel, i.e. sum of attribute sizes
        size_t pixelSize = 0;
        uint32_t nx = (1 + x_max - x_min);
        uint32_t ny = (1 + y_max - y_min);
        for (uint32_t i = 0; i < array.attrs.size(); ++i) pixelSize += Utils::scidbTypeIdBytes(array.attrs[i].typeId);
        size_t totalSize = pixelSize *  nx * ny;

        //Utils::debug ( "TOTAL SIZE TO UPLOAD: " + boost::lexical_cast<string> ( totalSize ) );




        // UPLOAD FILE ////////////////////////////
        stringstream ss;
        ss.str("");
        ss << _host << ":" << _port <<  SHIMENDPOINT_UPLOAD_FILE; // neccessary to put port in URL due to  HTTP 100-continue
        ss << "?" << "id=" << sessionID;


        struct curl_httppost* formpost = NULL;
        struct curl_httppost* lastptr = NULL;

        // Load file from buffer instead of file!
        curl_formadd(&formpost, &lastptr, CURLFORM_COPYNAME, "file", CURLFORM_BUFFER, SCIDB4GDAL_DEFAULT_UPLOAD_FILENAME, CURLFORM_BUFFERPTR, inChunk, CURLFORM_BUFFERLENGTH, totalSize,  CURLFORM_CONTENTTYPE, "application/octet-stream", CURLFORM_END);

        curlBegin();
        string remoteFilename = "";
        //curl_easy_setopt(_curl_handle, CURLOPT_FOLLOWLOCATION, 1L);

        curl_easy_setopt(_curl_handle, CURLOPT_URL, ss.str().c_str());
        curl_easy_setopt(_curl_handle, CURLOPT_HTTPPOST, formpost);
        curl_easy_setopt(_curl_handle, CURLOPT_WRITEFUNCTION, &responseToStringCallback);
        curl_easy_setopt(_curl_handle, CURLOPT_WRITEDATA, &remoteFilename);

        if (curlPerform() != CURLE_OK)
        {
            curlEnd();
            return ERR_CREATE_UNKNOWN;
        }
        curlEnd();
        curl_formfree(formpost);


        // Remove special characters from remote filename
        remoteFilename.erase(std::remove_if(remoteFilename.begin(), remoteFilename.end(), isIllegalFilenameCharacter), remoteFilename.end());



        //Utils::sleep(3000);

        {
            /*
             * CREATE ARRAY tempArray < ... > [i=0:*,SCIDB4GDAL_DEFAULT_BLOCKSIZE_X*SCIDB4GDAL_DEFAULT_BLOCKSIZE_Y,0];
             */

            stringstream afl;
            afl << "CREATE ARRAY " << tempArray;
            afl << " <";
            for (uint32_t i = 0; i < array.attrs.size() - 1; ++i)
            {
                afl << array.attrs[i].name << ": " << array.attrs[i].typeId << ",";
            }
            afl << array.attrs[array.attrs.size() - 1].name << ": " << array.attrs[array.attrs.size() - 1].typeId << ">";
            afl << " [" << "i=0:*," << SCIDB4GDAL_DEFAULT_BLOCKSIZE_X* SCIDB4GDAL_DEFAULT_BLOCKSIZE_Y << ",0]";
            Utils::debug("Performing AFL Query: " +  afl.str());

            curlBegin();
            ss.str("");
            ss << _host << SHIMENDPOINT_EXECUTEQUERY << "?" << "id=" << sessionID << "&query=" << curl_easy_escape(_curl_handle, afl.str().c_str(), 0);
            curl_easy_setopt(_curl_handle, CURLOPT_URL, ss.str().c_str());
            curl_easy_setopt(_curl_handle, CURLOPT_HTTPGET, 1);
            if (curlPerform() != CURLE_OK)
            {
                curlEnd();
                if (removeArray(tempArray) != SUCCESS)
                {
                    Utils::error("Could not delete temporary load array '" + tempArray + "'. This may result in an inconsistent state, please check in SciDB.");
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
            Utils::debug("Performing AFL Query: " +  afl.str());
            curlBegin();
            ss.str("");
            ss << _host << SHIMENDPOINT_EXECUTEQUERY << "?" << "id=" << sessionID << "&query=" << curl_easy_escape(_curl_handle, afl.str().c_str(), 0);
            curl_easy_setopt(_curl_handle, CURLOPT_URL, ss.str().c_str());
            curl_easy_setopt(_curl_handle, CURLOPT_HTTPGET, 1);
            if (curlPerform() != CURLE_OK)
            {
                curlEnd();
                Utils::warn("Loading binary data to temporary SciDB array'" + tempArray + "' failed. Trying to recover initial state.");
                if (removeArray(tempArray) != SUCCESS)
                {
                    Utils::warn("Could not delete temporary load array '" + tempArray + "'. This may result in an inconsistent state, please check in SciDB.");
                }
                return ERR_CREATE_UNKNOWN;
            }
            curlEnd();
        }


        {
            /*
             * insert(
             *     redimension(
             *         apply(TEMP_ARRAY, array.dims[xidx].name,  (int64)xmin +  ((int64)i % (int64)nx),          array.dims[yidx].name,  (int64)ymin +  (int64)((int64)i / (int64)ny))
             *         ,array.name)
             *     ,a)
            */
            stringstream afl;
            afl << "insert(redimension(apply(" << tempArray << ","; // TODO: Check dimension indices + ordering...
            afl << array.dims[array.getYDimIdx()].name << "," << "int64(" << y_min  << ") + int64(i) % int64(" << ny << ")";
            afl << ",";
            afl << array.dims[array.getXDimIdx()].name << "," << "int64(" << x_min  << ") + int64(int64(i) / int64(" << ny << "))";
            afl << "), " << array.name << "), " << array.name << ")";

            Utils::debug("Performing AFL Query: " +  afl.str());


            curlBegin();
            ss.str("");
            ss << _host << SHIMENDPOINT_EXECUTEQUERY << "?" << "id=" << sessionID << "&query=" << curl_easy_escape(_curl_handle, afl.str().c_str(), 0);
            curl_easy_setopt(_curl_handle, CURLOPT_URL, ss.str().c_str());
            curl_easy_setopt(_curl_handle, CURLOPT_HTTPGET, 1);
            curl_easy_setopt(_curl_handle, CURLOPT_HTTPGET, 1);
            if (curlPerform() != CURLE_OK)
            {
                curlEnd();
                Utils::warn("Insertion or redimensioning of temporary array '" + tempArray + "' failed. Trying to recover initial state.");
                if (removeArray(tempArray) != SUCCESS)
                {
                    Utils::warn("Could not delete temporary load array '" + tempArray + "'. This may result in an inconsistent state, please check in SciDB.");
                }
                return ERR_CREATE_UNKNOWN;
            }
            curlEnd();
        }



        {
            /*
             * remove (tempArray);
             */
            stringstream afl;
            afl << "remove(" << tempArray << ")";
            Utils::debug("Performing AFL Query: " +  afl.str());

            curlBegin();
            ss.str("");
            ss << _host << SHIMENDPOINT_EXECUTEQUERY << "?" << "id=" << sessionID << "&query=" << curl_easy_escape(_curl_handle, afl.str().c_str(), 0);
            curl_easy_setopt(_curl_handle, CURLOPT_URL, ss.str().c_str());
            curl_easy_setopt(_curl_handle, CURLOPT_HTTPGET, 1);
            if (curlPerform() != CURLE_OK)
            {
                curlEnd();
                Utils::warn("Insertion or redimensioning of temporary array '" + tempArray + "' failed. Trying to recover initial state.");
                if (removeArray(tempArray) != SUCCESS)        // This is somewhat redundant...
                {
                    Utils::warn("Could not delete temporary load array '" + tempArray + "'. This may result in an inconsistent state, please check in SciDB.");
                }
                return ERR_CREATE_UNKNOWN;
            }
            curlEnd();
        }



        // Release session
        releaseSession(sessionID);

        return SUCCESS;
    }



    StatusCode ShimClient::getAttributeStats(SciDBSpatialArray& array, uint8_t nband, SciDBAttributeStats& out)
    {


        if (nband >= array.attrs.size())
        {
            Utils::error("Invalid attribute index");
        }

        int sessionID = newSession();

        stringstream ss, afl;
        string aname = array.attrs[nband ].name;

        afl << "aggregate(" << array.name << ",min(" << aname << "),max(" << aname << "),avg(" << aname << "),stdev(" << aname << "))";
        Utils::debug("Performing AFL Query: " +  afl.str());

        ss.str();
        ss << _host << SHIMENDPOINT_EXECUTEQUERY << "?" << "id=" << sessionID << "&query=" << afl.str() << "&save=" << "(double,double,double,double)";

        curlBegin();
        curl_easy_setopt(_curl_handle, CURLOPT_URL, ss.str().c_str());
        curl_easy_setopt(_curl_handle, CURLOPT_HTTPGET, 1);
        curlPerform();
        curlEnd();



        curlBegin();
        // READ BYTES  ////////////////////////////
        ss.str("");
        ss << _host << SHIMENDPOINT_READ_BYTES << "?" << "id=" << sessionID << "&n=0";
        curl_easy_setopt(_curl_handle, CURLOPT_URL, ss.str().c_str());
        curl_easy_setopt(_curl_handle, CURLOPT_HTTPGET, 1);

        struct SingleAttributeChunk data;
        data.memory = (char*) malloc(sizeof(double) * 4);
        data.size = 0;

        curl_easy_setopt(_curl_handle, CURLOPT_WRITEFUNCTION, responseBinaryCallback);
        curl_easy_setopt(_curl_handle, CURLOPT_WRITEDATA, (void*) &data);
        curlPerform();
        curlEnd();


        out.min = ((double*) data.memory) [0];
        out.max = ((double*) data.memory) [1];
        out.mean = ((double*) data.memory) [2];
        out.stdev = ((double*) data.memory) [3];

        free(data.memory);


        releaseSession(sessionID);

        return SUCCESS;


    }





    StatusCode ShimClient::arrayExists(const string& inArrayName, bool& out)
    {
        stringstream ss, afl;


        int sessionID = newSession();

        // There might be less complex queries but this one always succeeds and does not give HTTP 500 SciDB errors
        afl << "aggregate(filter(list('arrays'),name='" << inArrayName << "'),count(name))";
        Utils::debug("Performing AFL Query: " +  afl.str());

        ss.str();
        ss << _host << SHIMENDPOINT_EXECUTEQUERY << "?" << "id=" << sessionID << "&query=" << afl.str() << "&save=" << "(int64)";

        curlBegin();
        curl_easy_setopt(_curl_handle, CURLOPT_URL, ss.str().c_str());
        curl_easy_setopt(_curl_handle, CURLOPT_HTTPGET, 1);
        curlPerform();
        curlEnd();



        curlBegin();
        // READ BYTES  ////////////////////////////
        ss.str("");
        ss << _host << SHIMENDPOINT_READ_BYTES << "?" << "id=" << sessionID << "&n=0";
        curl_easy_setopt(_curl_handle, CURLOPT_URL, ss.str().c_str());
        curl_easy_setopt(_curl_handle, CURLOPT_HTTPGET, 1);

        struct SingleAttributeChunk data;
        data.memory = (char*) malloc(sizeof(int64_t) * 1);          // Expect just one integer
        data.size = 0;

        curl_easy_setopt(_curl_handle, CURLOPT_WRITEFUNCTION, responseBinaryCallback);
        curl_easy_setopt(_curl_handle, CURLOPT_WRITEDATA, (void*) &data);
        curlPerform();
        curlEnd();


        uint64_t count = ((int64_t*) data.memory) [0];

        if (count > 0) out = true;
        else out = false;


        free(data.memory);


        releaseSession(sessionID);

        return SUCCESS;

    }



    StatusCode ShimClient::updateSRS(SciDBSpatialArray& array)
    {
        // Add spatial reference system information if available
        if (array.srtext != "")
        {

            int sessionID = newSession();


            /* In the following, we assume the SRS to be already known by scidb4geo.
               This might only work for EPSG codes and in the future, this should be checked and
               unknown SRS should be registered via st_regnewsrs() automatically */
            curlBegin();
            // EXECUTE QUERY  ////////////////////////////
            stringstream afl;
            afl << "st_setsrs(" << array.name << ",'" << array.getXDim().name << "','" << array.getYDim().name << "','" << array.auth_name << "'," << array.auth_srid << ",'" <<  array.affineTransform.toString() << "')";
            Utils::debug("Performing AFL Query: " +  afl.str());

            stringstream ss;
            ss << _host << SHIMENDPOINT_EXECUTEQUERY << "?" << "id=" << sessionID << "&query=" << curl_easy_escape(_curl_handle, afl.str().c_str(), 0);

            curl_easy_setopt(_curl_handle, CURLOPT_URL, ss.str().c_str());
            curl_easy_setopt(_curl_handle, CURLOPT_HTTPGET, 1);
            curlPerform();
            curlEnd();



            releaseSession(sessionID);
        }
        else
        {
            // TODO: How to remove SRS information? Is this neccessary at all?


        }

        return SUCCESS;
    }


    StatusCode ShimClient::removeArray(const string& inArrayName)
    {
        int sessionID = newSession();

        stringstream ss, afl;
        afl << "remove(" << inArrayName << ")";
        Utils::debug("Performing AFL Query: " +  afl.str());
        ss << _host << SHIMENDPOINT_EXECUTEQUERY << "?" << "id=" << sessionID << "&query=" << afl.str();
        curl_easy_setopt(_curl_handle, CURLOPT_URL, ss.str().c_str());
        curl_easy_setopt(_curl_handle, CURLOPT_HTTPGET, 1);
        if (curlPerform() != CURLE_OK)
        {
            curlEnd();
            Utils::warn("Cannot remove array '" + inArrayName + "'");
            return ERR_GLOBAL_UNKNOWN;
        }
        curlEnd();
        releaseSession(sessionID);
        return SUCCESS;

    }




}
