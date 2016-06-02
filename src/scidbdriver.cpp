/*
Copyright (c) 2016 Marius Appel <marius.appel@uni-muenster.de>

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

#include "scidbdriver.h"
#include "shimclient.h"
#include "utils.h"
#include <iomanip>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/assign.hpp>
#include <limits.h>
#include "shim_client_structs.h"
#include "scidb_structs.h"
#include "parameter_parser.h"

CPL_C_START
void GDALRegister_SciDB(void);
CPL_C_END

/**
 * GDAL driver registration function
 * Links the specific functions for Open, Identify, Delete and Create Copy on a
 * GDALDriver stub.
 */
void GDALRegister_SciDB() {
    GDALDriver* poDriver;

    if (GDALGetDriverByName("SciDB") == NULL) {
        poDriver = new GDALDriver();
        poDriver->SetDescription("SciDB");
        poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
        std::stringstream driverlongname;
        driverlongname << "SciDB array driver(" << "BUILD " << __DATE__ << " " << __TIME__ << ")";
        poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, driverlongname.str().c_str());
        poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "frmt_scidb.html");
        poDriver->SetMetadataItem(GDAL_DMD_CREATIONDATATYPES,"Byte UInt16 Int16 Uint32 Int32 Float32 Float64");

        /* Create and opening options description */
        std::stringstream co_descr, general_descr,  oo_descr;
        general_descr << "    <Option name='host' type='string' default='http://localhost' description='hostname or IP address of SciDB coordinator instance'/>";
        general_descr << "    <Option name='port' type='string' default='8083' description='port number shim listens on'/>";
        general_descr << "    <Option name='user' type='string' default='scidb' description='username to connect to SciDB'/>";
        general_descr << "    <Option name='password' type='string' default='scidb' description='password to connect to SciDB'/>";
        general_descr << "    <Option name='ssl'  type='boolean' description='Use SSL connection'/>";
        general_descr << "    <Option name='trust' default='true'  type='boolean' description='ignore certificate checks'/>";           
        
        co_descr <<  "<CreationOptionList>" <<  general_descr.str();            
        oo_descr <<  "<OpenOptionList>" <<  general_descr.str();            
                    
        co_descr << "    <Option name='CHUNKSIZE_SP' type='int' description='chunk size of spatial dimensions in pixels'/>";
        co_descr << "    <Option name='CHUNKSIZE_T' type='int' description='chunk size of temporal dimension in pixels'/>";
        co_descr << "    <Option name='type' type='string-select' description='type of array to be created (S=2d space,  ST=3d spacetime, STS=3d spacetime with unbounded temporal dimension)'>"
                    "       <Value>S</Value>"
                    "       <Value>ST</Value>"
                    "       <Value>STS</Value>"
                    "    </Option>";
        co_descr << "    <Option name='bbox' type='string' description='spatial boundaries of the target array'/>";
        
        
        // co_descr <<  "   <Option name='srs' type='string'  description='spatial
        // reference system '/>" // TODO: remove srs parameter from driver
        co_descr << "    <Option name='srs' type='string'  description='spatial reference system (deprecated)'/>";
        co_descr << "    <Option name='t' type='string'  description='datetime as ISO8601 string'/>";
        co_descr << "    <Option name='dt' type='string' description='temporal resolution as ISO8601 period string'/>";
        co_descr << "</CreationOptionList>";
        poDriver->SetMetadataItem(GDAL_DMD_CREATIONOPTIONLIST, co_descr.str().c_str());
        
        
        oo_descr << "    <Option name='timestamp' type='string' description='datetime as ISO8601 string to query a temporal slice of a spacetime array'/>";
        oo_descr << "    <Option name='t' type='int' description='temporal array index to query a temporal slice of a spacetime array'/>";
        oo_descr <<  "</OpenOptionList>";
        poDriver->SetMetadataItem(GDAL_DMD_OPENOPTIONLIST, oo_descr.str().c_str());
        
        

        poDriver->pfnOpen = scidb4gdal::SciDBDataset::Open;
        poDriver->pfnIdentify = scidb4gdal::SciDBDataset::Identify;
        // poDriver->pfnCreate = scidb4gdal::SciDBDataset::Create;
        poDriver->pfnDelete = scidb4gdal::SciDBDataset::Delete;

        poDriver->pfnCreateCopy = scidb4gdal::SciDBDataset::CreateCopy;
        GetGDALDriverManager()->RegisterDriver(poDriver);
    }
}

namespace scidb4gdal {
    /* =============================================
    *  SciDBRasterBand
    * =============================================
    */
    SciDBRasterBand::SciDBRasterBand(SciDBDataset* poDS, SciDBSpatialArray* array,int nBand) 
    {
        this->poDS = poDS;
        this->nBand = nBand;
        this->_array = array;

        eDataType = Utils::scidbTypeIdToGDALType( _array->attrs[nBand].typeId); // Data type is mapped from SciDB's attribute data type

        uint32_t nImgYSize(1 + _array->getYDim()->high - _array->getYDim()->low);
        uint32_t nImgXSize(1 + _array->getXDim()->high - _array->getXDim()->low);
        nBlockYSize = (_array->getYDim()->chunksize < nImgYSize)
                        ? _array->getYDim()->chunksize
                        : nImgYSize;
        nBlockXSize = (_array->getXDim()->chunksize < nImgXSize)
                        ? _array->getXDim()->chunksize
                        : nImgXSize;
    }

    SciDBRasterBand::~SciDBRasterBand() { FlushCache(); }

    CPLErr SciDBRasterBand::GetStatistics(int bApproxOK, int bForce, double* pdfMin,
                                        double* pdfMax, double* pdfMean,
                                        double* pdfStdDev) {
        SciDBAttributeStats stats;
        ((SciDBDataset*)poDS)
            ->getClient()
            ->getAttributeStats(*_array, this->nBand - 1, stats);

        *pdfMin = stats.min;
        *pdfMax = stats.max;
        *pdfMean = stats.mean;
        *pdfStdDev = stats.stdev;

        return CE_None;
    }

    CPLErr SciDBRasterBand::IReadBlock(int nBlockXOff, int nBlockYOff,
                                    void* pImage) {
        // TODO: Improve error handling
        SciDBDataset* poGDS = (SciDBDataset*)poDS;

        // parse the temporal index from query string...
        // 	int32_t t_index = poGDS->_client->
        //
        // 	->_query->temp_index;
        uint32_t tileId =
            TileCache::getBlockId(nBlockXOff, nBlockYOff, nBand - 1, nBlockXSize,
                                nBlockYSize, poGDS->GetRasterCount());

        ArrayTile tile;
        tile.id = tileId;

        // Check whether chunk is in cache
        if (poGDS->_cache.has(tileId)) {
            tile = *poGDS->_cache.get(tileId);
        } else {
            int xmin = nBlockXOff * this->nBlockXSize + _array->getXDim()->low;
            int xmax = xmin + this->nBlockXSize - 1;
            if (xmax > _array->getXDim()->high)
                xmax = _array->getXDim()->high;

            int ymin = nBlockYOff * this->nBlockYSize + _array->getYDim()->low;
            int ymax = ymin + this->nBlockYSize - 1;
            if (ymax > _array->getYDim()->high)
                ymax = _array->getYDim()->high;

            // Read  and fetch data
            bool use_subarray = true; // This is safer, between might lead to
            // inconsistent results in some instances
            // bool use_subarray = ! ( ( xmin % ( int ) _array->getXDim()->chunksize ==
            // 0 ) && ( ymin % ( int ) _array->getYDim()->chunksize == 0 ) );

            tile.size =
                nBlockXSize * nBlockYSize *
                Utils::scidbTypeIdBytes(
                    _array->attrs[nBand - 1].typeId); // Always allocate full block size
            tile.data = malloc(tile.size);            // will be freed automatically by cache

            if ((nBlockXOff + 1) * this->nBlockXSize > poGDS->nRasterXSize) {
                // This is a bit of a hack...
                size_t dataSize =
                    (1 + xmax - xmin) * (1 + ymax - ymin) *
                    Utils::scidbTypeIdBytes(
                        _array->attrs[nBand - 1]
                            .typeId); // This is smaller than the block size!
                void* buf = malloc(dataSize);

                // Write to temporary buffer first
                // TODO  t_index is set as zero for compiler testing... this must be
                // changed
                poGDS->getClient()->getData(*_array, nBand - 1, buf, xmin, ymin, xmax,
                                            ymax, use_subarray); // GDAL bands start with
                // 1, scidb attribute
                // indexes with 0
                for (int i = 0; i < (1 + ymax - ymin); ++i) {
                    uint8_t* src = &((uint8_t*)buf)[i * (1 + xmax - xmin) *
                                                    Utils::scidbTypeIdBytes(
                                                        _array->attrs[nBand - 1].typeId)];
                    uint8_t* dest =
                        &((uint8_t*)tile.data)[i * this->nBlockXSize *
                                            Utils::scidbTypeIdBytes(
                                                _array->attrs[nBand - 1].typeId)];
                    memcpy(dest, src,
                        (1 + xmax - xmin) *
                            Utils::scidbTypeIdBytes(_array->attrs[nBand - 1].typeId));
                }
                free(buf);
            } else {
                // This is the most efficient!
                poGDS->getClient()->getData(*_array, nBand - 1, tile.data, xmin, ymin,
                                            xmax, ymax,
                                            use_subarray); // GDAL bands start
                                                        // with 1, scidb
                                                        // attribute
                                                        // indexes with 0
            }
        }

        poGDS->_cache.add(tile); // Add to cache

        // Copy from tile.data to pImage
        memcpy(pImage, tile.data, tile.size);

        return CE_None;
    }

    double SciDBRasterBand::GetNoDataValue(int* pbSuccess) {
        string key = "NODATA";
        double result;
        MD md = _array->attrs[nBand - 1].md[""];
        if (md.find(key) == md.end()) {
            if (pbSuccess != NULL)
                *pbSuccess = false;
            return Utils::defaultNoDataGDAL(this->GetRasterDataType());
        }
        string value = md[key];
        boost::algorithm::trim(value);

        if (value.length() == 0) {
            if (pbSuccess != NULL)
                *pbSuccess = false;
            return Utils::defaultNoDataGDAL(this->GetRasterDataType());
        }

        // it is assured that there is an entry for NODATA
        if (pbSuccess != NULL)
            *pbSuccess = true;
        result = boost::lexical_cast<double>(value);
        return result;
    }

    double SciDBRasterBand::GetMaximum(int* pbSuccess) {
        string key = "MAX";
        MD md = _array->attrs[nBand - 1].md[""]; // TODO: Add domain
        if (md.find(key) == md.end()) {
            if (pbSuccess != NULL)
                *pbSuccess = false;
            return DBL_MAX;
        }
        if (pbSuccess != NULL)
            *pbSuccess = true;
        return boost::lexical_cast<double>(md[key]);
    }

    double SciDBRasterBand::GetMinimum(int* pbSuccess) {
        string key = "MIN";
        MD md = _array->attrs[nBand - 1].md[""]; // TODO: Add domain
        if (md.find(key) == md.end()) {
            if (pbSuccess != NULL)
                *pbSuccess = false;
            return DBL_MAX;
        }
        if (pbSuccess != NULL)
            *pbSuccess = true;
        return boost::lexical_cast<double>(md[key]);
    }

    double SciDBRasterBand::GetOffset(int* pbSuccess) {
        string key = "OFFSET";
        MD md = _array->attrs[nBand - 1].md[""]; // TODO: Add domain
        if (md.find(key) == md.end()) {
            if (pbSuccess != NULL)
                *pbSuccess = false;
            return 0;
        }
        if (pbSuccess != NULL)
            *pbSuccess = true;
        return boost::lexical_cast<double>(md[key]);
    }

    double SciDBRasterBand::GetScale(int* pbSuccess) {
        string key = "SCALE";
        MD md = _array->attrs[nBand - 1].md[""]; // TODO: Add domain
        if (md.find(key) == md.end()) {
            if (pbSuccess != NULL)
                *pbSuccess = false;
            return 1;
        }
        if (pbSuccess != NULL)
            *pbSuccess = true;
        return boost::lexical_cast<double>(md[key]);
    }

    const char* SciDBRasterBand::GetUnitType() {
        string key = "UNIT";
        MD md = _array->attrs[nBand - 1].md[""]; // TODO: Add domain
        if (md.find(key) == md.end()) {
            return "";
        }
        return md[key].c_str();
    }

    /* =============================================
    *  SciDBDataset
    * =============================================
    */
    SciDBDataset::SciDBDataset(SciDBSpatialArray& array, ShimClient* client)
        : _array(array), _client(client) {
        // TODO check if the +1 is really needed or if this leeds to the one pixel
        // borders
        this->nRasterXSize = 1 + _array.getXDim()->high - _array.getXDim()->low;
        this->nRasterYSize = 1 + _array.getYDim()->high - _array.getYDim()->low;

        map<string, string> kv;

        // TODO check if this is needed or if metadata is already stored in the array
        // (it should be), this would save one unnecessary scidb call
        _client->getArrayMD(kv, _array.name, ""); // get metadata of default domain
        map<string, string>::const_iterator itr;

        // when constructing the data set set each of the stored metadata coming from
        // scidb
        for (itr = kv.begin(); itr != kv.end(); ++itr) {
            this->SetMetadataItem((*itr).first.c_str(), (*itr).second.c_str());
        }

        // Create GDAL Bands
        for (uint32_t i = 0; i < _array.attrs.size(); ++i)
            this->SetBand(i + 1, new SciDBRasterBand(this, &_array, i));

        this->SetDescription(_array.toString().c_str());

        SciDBSpatialArray* arr_ptr = &this->_array;
        SciDBSpatioTemporalArray* st_arr_ptr = dynamic_cast<SciDBSpatioTemporalArray*>(arr_ptr);

        // check if dynamic cast was successfull. if so then check for the temporal
        // index and then calculate the timestamp according to the resolution
        if (st_arr_ptr) {
            int tmin = st_arr_ptr->getTDim()->low;
            int tmax = st_arr_ptr->getTDim()->high;
            int tindex = -1;
            if (_client->_qp->hasTemporalIndex) {
                tindex = _client->_qp->temp_index; // TODO find the place where the
                // temporal index is stored in the
                // array

                TPoint time = st_arr_ptr->datetimeAtIndex(tindex);
                time._resolution = st_arr_ptr->getTInterval()->_resolution;
                this->SetMetadataItem("TIMESTAMP", time.toStringISO().c_str());
            } else {
                TPoint start = st_arr_ptr->datetimeAtIndex(tmin);
                start._resolution = st_arr_ptr->getTInterval()->_resolution;
                this->SetMetadataItem("TS_START", start.toStringISO().c_str());

                if (tmin != tmax) {
                    TPoint end = st_arr_ptr->datetimeAtIndex(tmax);
                    end._resolution = st_arr_ptr->getTInterval()->_resolution;
                    this->SetMetadataItem("TS_END", end.toStringISO().c_str());
                    this->SetMetadataItem(
                        "TINTERVAL", st_arr_ptr->getTInterval()->toStringISO().c_str());
                }
            }
        }

        // Set Metadata

        // TODO: Overviews?
        // poDS->oOvManager.Initialize ( poDS, poOpenInfo->pszFilename );
    }

    void SciDBDataset::gdalMDtoMap(char** strlist, map<string, string>& kv) {
        kv.clear();

        if (strlist == NULL) {
            Utils::debug(
                "Could not find a string list for metadata in the source data set.");
            return;
        }
        int i = 0;
        char* it = strlist[0];

        while (it != NULL) {
            string s(strlist[i]);
            size_t p = string::npos;
            p = (s.find('=') != string::npos) ? s.find('=') : p;
            p = (s.find(':') != string::npos) ? s.find(':') : p;
            if (p != string::npos) {
                string key = s.substr(0, p);
                string value = s.substr(p + 1, s.length() - p - 1);
                if (value.length() > 0)
                    kv.insert(pair<string, string>(key, value));
            }
            it = strlist[++i];
        }
    }

    char** SciDBDataset::mapToGdalMD(map<string, string>& kv) {
        CPLStringList* out = new CPLStringList();
        for (map<string, string>::iterator it = kv.begin(); it != kv.end(); ++it) {
            const char* k = it->first.c_str();
            const char* v = it->second.c_str();
            out->AddNameValue(k, v);
        }
        return out->List();
    }

    SciDBDataset::~SciDBDataset() {
        FlushCache();
        delete _client;
    }

    CPLErr SciDBDataset::GetGeoTransform(double* padfTransform) {
        // If array dimensions do not start at 0, change transformation parameters
        // accordingly
        AffineTransform::double2 p0(_array.getXDim()->low, _array.getYDim()->low);
        _array.affineTransform.f(p0);
        // padfTransform[0] = _array.affineTransform._x0;
        padfTransform[0] = p0.x;
        padfTransform[1] = _array.affineTransform._a11;
        padfTransform[2] = _array.affineTransform._a12;
        // padfTransform[3] = _array.affineTransform._y0;
        padfTransform[3] = p0.y;
        padfTransform[4] = _array.affineTransform._a21;
        padfTransform[5] = _array.affineTransform._a22;
        return CE_None;
    }

    const char* SciDBDataset::GetProjectionRef() { return _array.srtext.c_str(); }

    /*
    char **SciDBDataset::GetMetadata ( const char *pszDomain )
    {
        map <string, string> kv;
        if ( pszDomain == NULL ) {
            _client->getArrayMD ( kv, _array.name, "" );
        }
        // DOMAINS NOT YET IMPLEMENTED
    //         else {
    //             _client->getArrayMD ( kv, _array.name, pszDomain );
    //         }

        //check if there are also
        return mapToGdalMD ( kv );
    }

    const char *SciDBDataset::GetMetadataItem ( const char *pszName, const char
    *pszDomain )
    {
        map <string, string> kv;
        if ( pszDomain == NULL ) {
            _client->getArrayMD ( kv, _array.name, "" );
        }
        // DOMAINS NOT YET IMPLEMENTED
    //         else {
    //             _client->getArrayMD ( kv, _array.name, pszDomain );
    //         }
        if ( kv.find ( pszName ) == kv.end() ) return NULL;
        return kv.find ( pszName )->second.c_str();

    }
    */

    ShimClient* SciDBDataset::getClient() { return _client; }

    void SciDBDataset::copyMetadataToArray(GDALDataset* poSrcDS,
                                        SciDBSpatialArray& array,
                                        CreationParameters* options) {
        // Attributes
        size_t pixelsize =
            0; // size in bytes of one pixel, i.e. sum of attribute sizes
        vector<SciDBAttribute> attrs;
        for (int i = 0; i < poSrcDS->GetRasterCount(); ++i) {
            SciDBAttribute a;
            a.nullable = false; // TODO
            a.typeId = Utils::gdalTypeToSciDBTypeId(
                poSrcDS->GetRasterBand(i + 1)->GetRasterDataType()); // All attribtues
            // must have the
            // same data type
            stringstream aname;
            // pixelsize += Utils::gdalTypeBytes ( poSrcDS->GetRasterBand ( i + 1
            // )->GetRasterDataType() );

            // scidb stores attributes in separate chunks, so simply use the highest
            // data type storage value
            size_t byte = Utils::gdalTypeBytes(
                poSrcDS->GetRasterBand(i + 1)->GetRasterDataType());
            if (byte > pixelsize)
                pixelsize = byte;

            aname << "band" << i + 1;
            // aname << poSrcDS->GetRasterBand ( i + 1 )->GetDescription();
            a.name = aname.str();
            attrs.push_back(a);
        }
        array.attrs = attrs;

        // overwrite the dimension chunksizes with the ones stated in the create
        // options or recalculate them
        SciDBSpatialArray* array_ptr = &array;
        SciDBSpatioTemporalArray* st_ptr = dynamic_cast<SciDBSpatioTemporalArray*>(array_ptr);
        bool isST = (st_ptr) ? true : false;

        // create Dimensions // Derive chunksize from attribute sizes
        if (options->chunksize_spatial > 0 && options->chunksize_temporal > 0) {
            Utils::debug("Using assigned spatial and temporal chunksizes.");
            // use both chunksizes as given 
        } else if (options->chunksize_spatial > 0 &&
                options->chunksize_temporal < 0 && isST) {
            stringstream ss;
            ss << "Calculating temporal chunksize: ";

            // recalculate temporal chunksize based on the MB restriction
            int t_calc = (int)((SCIDB4GEO_DEFAULT_CHUNKSIZE_MB * 1024 * 1024) /
                            (options->chunksize_spatial *
                                options->chunksize_spatial * pixelsize));
            options->chunksize_temporal = t_calc;
            ss << "t=" << t_calc;

            Utils::debug(ss.str());
        } else if (options->chunksize_spatial < 0 &&
                options->chunksize_temporal > 0) {
            stringstream ss;
            // recalculate the spatial chunksizes
            ss << "Calculating spatial chunksize: s=";

            int divident = (isST) ? (options->chunksize_temporal * pixelsize) : (pixelsize);
            int s_calc = (int)sqrt((SCIDB4GEO_DEFAULT_CHUNKSIZE_MB * 1024 * 1024) / divident);
            options->chunksize_spatial = s_calc;
            ss << s_calc;
            Utils::debug(ss.str());
        } else if (options->chunksize_spatial < 0 &&
                options->chunksize_temporal < 0) {
            // calculate both or use defaults
            Utils::debug("Calculating spatial chunksize and taking the default temporal chunksize (t=1).");
            int divident =
                (isST) ? (SCIDB4GDAL_DEFAULT_TDIM_BLOCKSIZE * pixelsize) : (pixelsize);

            uint32_t blocksize =
                Utils::nextPow2((uint32_t)sqrt(
                    (((double)(SCIDB4GEO_DEFAULT_CHUNKSIZE_MB * 1024 * 1024)) /
                    ((double)(divident))))) / 2;
            stringstream ss;
            ss << "Using chunksize " << blocksize << "x" << blocksize << " --> "
            << (int)(((double)(blocksize * blocksize * pixelsize)) /
                        ((double)1024.0)) << " kilobytes";
            Utils::debug(ss.str());

            options->chunksize_spatial = blocksize;

            if (isST)
                options->chunksize_temporal = SCIDB4GDAL_DEFAULT_TDIM_BLOCKSIZE;
        }

        if (isST) {
            st_ptr->getTDim()->chunksize = options->chunksize_temporal;
        }

        SciDBDimension* dimx;
        SciDBDimension* dimy;

        dimx = array.getXDim();
        dimx->high = poSrcDS->GetRasterXSize() - 1;
        dimx->length = poSrcDS->GetRasterXSize();
        dimx->chunksize = options->chunksize_spatial;

        dimy = array.getYDim();
        dimy->high = poSrcDS->GetRasterYSize() - 1;
        dimy->length = poSrcDS->GetRasterYSize();
        dimy->chunksize = options->chunksize_spatial;

        // extracting SRS information from source
        double padfTransform[6];
        string wkt(poSrcDS->GetProjectionRef());
        if (wkt != "" && poSrcDS->GetGeoTransform(padfTransform) == CE_None) {
            AffineTransform a(padfTransform[0], padfTransform[3], padfTransform[1],
                            padfTransform[5], padfTransform[2], padfTransform[4]);

            array.affineTransform = a;

            array.srtext = wkt;
            OGRSpatialReference srs(wkt.c_str());
            if (srs.AutoIdentifyEPSG() == OGRERR_UNSUPPORTED_SRS) {
                Utils::warn(
                    "Unsupported spatial reference system, ignoring spatial reference.");
            } else {
                array.auth_name = srs.GetAuthorityName(NULL);
                array.auth_srid =
                    boost::lexical_cast<uint32_t>(srs.GetAuthorityCode(NULL));
                char* proj4;
                srs.exportToProj4(&proj4);
                array.proj4text.assign(proj4);
                CPLFree(proj4);

                array.xdim = SCIDB4GDAL_DEFAULT_XDIMNAME;
                array.ydim = SCIDB4GDAL_DEFAULT_YDIMNAME;
            }
        }

        Utils::debug("Reading metadata from source dataset");

        // Get Metadata // TODO: Add domains
        map<string, string> kv;
        gdalMDtoMap(poSrcDS->GetMetadata(), kv);

        // in case the temporal information was stated in the image metadata parse the
        // metadata now  and assign it to the create options and the spatio temporal
        // array
        if (st_ptr && options->timestamp == "" && options->dt == "") {
            // find TIMESTAMP and TINTERVAL in the metadata
            string timestamp = "";
            string dt = "";

            for (std::map<string, string>::iterator itr = kv.begin(); itr != kv.end();
                ++itr) {
                if (itr->first == "TIMESTAMP") {
                    timestamp = itr->second;
                    kv.erase(itr);
                    continue;
                }

                if (itr->first == "TINTERVAL") {
                    dt = itr->second;
                    kv.erase(itr);
                    continue;
                }
            }
            // assign it to the create options
            options->timestamp = timestamp;
            options->dt = dt;

            // create the TPoint and TInterval from those information and assign it to
            // the SpatioTemporal array
            st_ptr->createTRS(timestamp, dt);
        }

        array.md.insert(pair<string, MD>("", kv));

        for (int i = 0; i < poSrcDS->GetRasterCount(); ++i) {
            map<string, string> kv2;
            //  gdalMDtoMap(  poSrcDS->GetRasterBand ( i + 1 )->GetMetadata(), kv2);
            stringstream s;
            double v;
            int ret = -1;
            v = poSrcDS->GetRasterBand(i + 1)->GetNoDataValue(&ret);

            Utils::debug("Reading metadata from source dataset");
            if (ret != 0) {
                s << std::setprecision(numeric_limits<double>::digits10) << v;
                kv2.insert(pair<string, string>("NODATA", s.str()));
            }

            s.str("");
            v = poSrcDS->GetRasterBand(i + 1)->GetOffset(&ret);
            if (ret != 0) {
                s << std::setprecision(numeric_limits<double>::digits10) << v;
                kv2.insert(pair<string, string>("OFFSET", s.str()));
                s.str("");
            }
            s.str("");
            v = poSrcDS->GetRasterBand(i + 1)->GetScale(&ret);
            if (ret != 0) {
                s << std::setprecision(numeric_limits<double>::digits10) << v;
                kv2.insert(pair<string, string>("SCALE", s.str()));
                s.str("");
            }

            s.str("");
            v = poSrcDS->GetRasterBand(i + 1)->GetMinimum(&ret);
            if (ret != 0) {
                s << std::setprecision(numeric_limits<double>::digits10) << v;
                kv2.insert(pair<string, string>("MIN", s.str()));
            }
            s.str("");
            v = poSrcDS->GetRasterBand(i + 1)->GetMaximum(&ret);
            if (ret != 0) {
                s << std::setprecision(numeric_limits<double>::digits10) << v;
                kv2.insert(pair<string, string>("MAX", s.str()));
            }
            s.str("");
            s << std::setprecision(numeric_limits<double>::digits10)
            << poSrcDS->GetRasterBand(i + 1)->GetUnitType();
            kv2.insert(pair<string, string>("UNIT", s.str()));

            array.attrs[i].md.insert(pair<string, MD>("", kv2));
        }
    }

    void SciDBDataset::uploadImageIntoTempArray(ShimClient* client,
                                                SciDBSpatialArray& array,
                                                GDALDataset* poSrcDS,
                                                GDALProgressFunc pfnProgress,
                                                void* pProgressData) {
        int nBands = poSrcDS->GetRasterCount();
        int nXSize = poSrcDS->GetRasterXSize();
        int nYSize = poSrcDS->GetRasterYSize();

        size_t pixelSize = 0;
        for (uint32_t i = 0; i < array.attrs.size(); ++i)
            pixelSize += Utils::scidbTypeIdBytes(array.attrs[i].typeId);

        size_t totalSize =
            pixelSize * array.getXDim()->chunksize * array.getYDim()->chunksize;

        uint8_t* bandInterleavedChunk =
            (uint8_t*)malloc(totalSize); // This is a byte array

        uint32_t nBlockX = (uint32_t)(nXSize / array.getXDim()->chunksize);
        if (nXSize % array.getXDim()->chunksize != 0)
            ++nBlockX;

        uint32_t nBlockY = (uint32_t)(nYSize / array.getYDim()->chunksize);
        if (nYSize % array.getYDim()->chunksize != 0)
            ++nBlockY;

        // upload array in chunks
        for (uint32_t bx = 0; bx < nBlockX; ++bx) {
            for (uint32_t by = 0; by < nBlockY; ++by) {
                size_t bandOffset = 0; // sum of bytes taken by previous bands, will be
                // updated in loop over bands

                if (!pfnProgress(((double)(bx * nBlockY + by)) /
                                    ((double)(nBlockX * nBlockY)),
                                NULL, pProgressData)) {
                    Utils::debug("Interruption by user requested, trying to clean up");
                    // Clean up intermediate arrays
                    client->removeArray(array.name);
                    throw ERR_CREATE_TERMINATEDBYUSER;
                }

                // 1. Compute array bounds from block offsets
                int xmin = bx * array.getXDim()->chunksize + array.getXDim()->low;
                int xmax = xmin + array.getXDim()->chunksize - 1;
                if (xmax > array.getXDim()->high)
                    xmax = array.getXDim()->high;
                if (xmin > array.getXDim()->high)
                    xmin = array.getXDim()->high;

                int ymin = by * array.getYDim()->chunksize + array.getYDim()->low;
                int ymax = ymin + array.getYDim()->chunksize - 1;
                if (ymax > array.getYDim()->high)
                    ymax = array.getYDim()->high;
                if (ymin > array.getYDim()->high)
                    ymin = array.getYDim()->high;

                // We assume reading whole blocks of individual bands first is more
                // efficient than reading single band pixels subsequently
                for (uint16_t iBand = 0; iBand < nBands; ++iBand) {
                    void* blockBandBuf =
                        malloc((1 + xmax - xmin) * (1 + ymax - ymin) *
                            Utils::scidbTypeIdBytes(array.attrs[iBand].typeId));

                    // Using nPixelSpace and nLineSpace arguments could maybe automatically
                    // write to bandInterleavedChunk properly
                    GDALRasterBand* poBand = poSrcDS->GetRasterBand(iBand + 1);
                    // poBand->RasterIO ( GF_Read, ymin, xmin, 1 + ymax - ymin, 1 + xmax -
                    // xmin, ( void * ) blockBandBuf,  1 + ymax - ymin, 1 + xmax - xmin,
                    // Utils::scidbTypeIdToGDALType ( array.attrs[iBand].typeId ), 0, 0 );
                    poBand->RasterIO(
                        GF_Read, xmin, ymin, 1 + xmax - xmin, 1 + ymax - ymin,
                        (void*)blockBandBuf, 1 + xmax - xmin, 1 + ymax - ymin,
                        Utils::scidbTypeIdToGDALType(array.attrs[iBand].typeId), 0, 0,
                        NULL);

                    /* SciDB load file format is band interleaved by pixel / cell, whereas
                    common GDAL functions are rather band sequential. In the following, we perform
                    block-wise interleaving manually. */

                    // Variable (unknown) data types and band numbers make this somewhat ugly
                    for (int i = 0; i < (1 + xmax - xmin) * (1 + ymax - ymin); ++i) {
                        memcpy(&((char*)bandInterleavedChunk)[i * pixelSize + bandOffset],
                            &((char*)blockBandBuf)[i * Utils::scidbTypeIdBytes(
                                                            array.attrs[iBand].typeId)],
                            Utils::scidbTypeIdBytes(array.attrs[iBand].typeId));
                    }
                    free(blockBandBuf);
                    bandOffset += Utils::scidbTypeIdBytes(array.attrs[iBand].typeId);
                }

                if (client->insertData(array, bandInterleavedChunk, xmin, ymin, xmax,
                                    ymax) != SUCCESS) {
                    Utils::debug("Copying data to SciDB array failed, trying to recover "
                                "initial state...");
                    if (client->removeArray(array.name) != SUCCESS) {
                        throw ERR_CREATE_AUTOCLEANUPFAILED;
                    } else {
                        throw ERR_CREATE_AUTOCLEANUPSUCCESS;
                    }
                }
            }
        }

        free(bandInterleavedChunk);
    }

    bool SciDBDataset::arrayIntegrateable(SciDBSpatialArray& src_array, SciDBSpatialArray& tar_array) {
        bool sameSRS = (src_array.auth_srid == tar_array.auth_srid) &&  boost::iequals(src_array.auth_name, tar_array.auth_name);

        if (sameSRS)  Utils::debug("Check whether source and target array have the same spatial reference system... OK");
        else Utils::warn("Check whether source and target array have the same spatial reference system... FAILED");

        // source arrays bounding box
        AffineTransform::double2 src_ul = AffineTransform::double2(src_array.getXDim()->low, src_array.getYDim()->high);
        AffineTransform::double2 src_lr = AffineTransform::double2(src_array.getXDim()->high, src_array.getYDim()->low);
        src_array.affineTransform.f(src_ul);
        src_array.affineTransform.f(src_lr);
        double src_x_min, src_x_max, src_y_min, src_y_max;
        if (src_ul.x < src_lr.x) {
            src_x_min = src_ul.x;
            src_x_max = src_lr.x;
        } else {
            src_x_max = src_ul.x;
            src_x_min = src_lr.x;
        }

        if (src_lr.y < src_ul.y) {
            src_y_min = src_lr.y;
            src_y_max = src_ul.y;
        } else {
            src_y_max = src_lr.y;
            src_y_min = src_ul.y;
        }
    
        AffineTransform::double2 tar_ul = AffineTransform::double2(
            tar_array.getXDim()->start,
            tar_array.getYDim()->start + tar_array.getYDim()->length);
        AffineTransform::double2 tar_lr = AffineTransform::double2(
            tar_array.getXDim()->start + tar_array.getXDim()->length,
            tar_array.getYDim()->start);
        tar_array.affineTransform.f(tar_ul);
        tar_array.affineTransform.f(tar_lr);

        double tar_x_min, tar_x_max, tar_y_min, tar_y_max;
        if (tar_ul.x < tar_lr.x) {
            tar_x_min = tar_ul.x;
            tar_x_max = tar_lr.x;
        } else {
            tar_x_max = tar_ul.x;
            tar_x_min = tar_lr.x;
        }

        if (tar_lr.y < tar_ul.y) {
            tar_y_min = tar_lr.y;
            tar_y_max = tar_ul.y;
        } else {
            tar_y_max = tar_lr.y;
            tar_y_min = tar_ul.y;
        }

//         stringstream s1;
//         s1 << "BBOX source image: " << setprecision(numeric_limits<double>::digits10) << " " << src_x_min << " " << src_y_min << " " << src_x_max << " " << src_y_max;
//         s1 <<  "- BBOX target image: " << setprecision(numeric_limits<double>::digits10) <<  tar_x_min << " " << tar_y_min << " " << tar_x_max << " " << tar_y_max;
//         Utils::debug(s1.str());
        
        bool isTPP = (src_x_min >= tar_x_min) && (src_y_min >= tar_y_min) &&
                    (src_x_max <= tar_x_max) && (src_y_max <= tar_y_max);

        if (isTPP) {
            Utils::debug("Checking whether source array is within the spatial extent of the target array... OK");
        } else
            Utils::warn("Checking whether source array is within the spatial extent of the target array... FAILED");

        return (sameSRS && isTPP);
    }

    GDALDataset* SciDBDataset::CreateCopy(const char* pszFilename,
                                        GDALDataset* poSrcDS, int bStrict,
                                        char** papszOptions,
                                        GDALProgressFunc pfnProgress,
                                        void* pProgressData) {
        int nBands = poSrcDS->GetRasterCount();

        string connstr = pszFilename;
        ConnectionParameters* con_pars;
        CreationParameters* create_pars;
        QueryParameters* query_pars = new QueryParameters();
        ParameterParser* pp;
        ShimClient* client;
        SciDBSpatialArray* src_array;
        SciDBSpatialArray* tar_arr;

        try {
            Utils::debug("** Parse options and parameters **");
            // 1. Parse Options and connection string
            pp = new ParameterParser(connstr, papszOptions,
                                    SCIDB_CREATE); // connstr is the file name,
            // papszOptions are the -co options

            con_pars = &pp->getConnectionParameters();
            create_pars = &pp->getCreationParameters();

            if (!create_pars->isValid())
                throw create_pars->error;

            delete pp;
            Utils::debug("-- DONE");

            Utils::debug("** Check validity of connection parameters and set options "
                        "for SHIM client **");
            if (!con_pars->isValid()) {
                throw con_pars->error_code;
            } else {
                Utils::debug("Using connection parameters: " + con_pars->toString());
            }

            // 3. create client and set parameters
            client = new ShimClient(con_pars); // connection parameters are set
            
            client->setCreateParameters(*create_pars); // create parameters regarding time
           

            // in order to open the array after creation the temporal information needs
            // to be transferred also
            query_pars->timestamp = create_pars->timestamp;
            client->setQueryParameters(*query_pars);
            Utils::debug("-- DONE");

            Utils::debug("** Check if array already exists in the database **");
            bool exists = false;
            StatusCode code = client->arrayExists(con_pars->arrayname, exists);
            if (code != SUCCESS) {
                throw code;
            }
            Utils::debug(exists ? "Referenced array already exists." : "New array will be created.");
            Utils::debug("-- DONE");

            if (exists && create_pars->hasBBOX) {
                // currently we only support the fresh creation of a coverage by stating a bounding box
                Utils::error("Currently the operation to overwrite an existing image with a bounding box statement is not allowed.");
                throw ERR_CREATE_ARRAYEXISTS;
            }
            
            
   
            Utils::debug( "** Create source array representation in GDAL for input image **");
            // 4. Collect metadata from source data and store in SciDBSpatialArray
            // (happens only in GDAL)
            switch (create_pars->type) {
                case S_ARRAY:
                    src_array = new SciDBSpatialArray();
                    Utils::debug("SciDBSpatialArray created.");
                    break;
                case ST_ARRAY:
                    if (create_pars->timestamp == "" && create_pars->dt == "") {
                        src_array = new SciDBSpatioTemporalArray();
                    } else {
                        src_array = new SciDBSpatioTemporalArray(create_pars->timestamp,
                                                                create_pars->dt);
                    }
                    ((SciDBSpatioTemporalArray*)src_array)->getTDim()->high = 0; 
                    ((SciDBSpatioTemporalArray*)src_array)->getTDim()->start = 0; 
                    ((SciDBSpatioTemporalArray*)src_array)->getTDim()->low = 0; 
                    ((SciDBSpatioTemporalArray*)src_array)->getTDim()->length = 1 ;
                     Utils::debug("SciDB spacetime array with single temporal slice created.");
                    break;
                case ST_SERIES:
                    if (create_pars->timestamp == "" && create_pars->dt == "") {
                        src_array = new SciDBSpatioTemporalArray();
                    } else {
                        src_array = new SciDBSpatioTemporalArray(create_pars->timestamp, create_pars->dt);
                    }
                    ((SciDBSpatioTemporalArray*)src_array)->getTDim()->high = SCIDB_MAX_DIM_INDEX; // set the dimension to maximum = indefinite, will be adapted when creating the temporal src_array
                    ((SciDBSpatioTemporalArray*)src_array)->getTDim()->start = 0; 
                    ((SciDBSpatioTemporalArray*)src_array)->getTDim()->low = 0; 
                    ((SciDBSpatioTemporalArray*)src_array)->getTDim()->length = SCIDB_MAX_DIM_INDEX  - 0 + 1;
                    Utils::debug("SciDB spacetime array with unbdounded temporal dimension created.");
                    break;
            }
            Utils::debug("-- DONE");

            src_array->name = con_pars->arrayname;

            Utils::debug("** Fetch metadata from the source image and copy it to the "
                        "source array representation **");
            copyMetadataToArray(poSrcDS, *src_array,
                                create_pars); // copies the information from the source
            // data file into the ST_Array
            // representation
            Utils::debug("Testing metadata copy: " + src_array->toString());
            Utils::debug("-- DONE");

            
            
            
            if (!client->hasSCIDB4GEO()) 
            {
                if (exists) {
                    Utils::error("Inserting images to an existing array needs SciDB spacetime extension. Please install the extension on the SciDB server.");
                    throw ERR_GLOBAL_NO_SCIDB4GEO;
                } 
                if (create_pars->hasBBOX) {
                    Utils::error("Creating arrays with a bounding box statement needs SciDB spacetime extension. Please install the extension on the SciDB server."); 
                    throw ERR_GLOBAL_NO_SCIDB4GEO;
                }
                if (create_pars->type ==  ST_ARRAY ||  create_pars->type ==  ST_SERIES) {
                    Utils::error("Inserting images as spatiotemporal (3d) arrays needs the SciDB spacetime extension. Please install the extension on the SciDB server.");
                    throw ERR_GLOBAL_NO_SCIDB4GEO;
                }
                //  TODO: Are there more cases to be catched here?
               
            }
            
            
            
            
            
            // prepare the target array
            Utils::debug("** Prepare the target array representation **");
            if (exists) {
                // get the information on the target array
                client->getArrayDesc(src_array->name, tar_arr);
                Utils::debug("target array was fetched from SciDB, because the array "
                            "already exists");
            } else {
                if (create_pars->hasBBOX) {
                    // sort the coordinates if not sorted (e.g. bbox parameter with ll/ur or
                    // simlar instead of ul/lr)
                    double tar_x_min, tar_x_max, tar_y_min, tar_y_max;
                    if (create_pars->bbox[0] < create_pars->bbox[2]) {
                        tar_x_min = create_pars->bbox[0];
                        tar_x_max = create_pars->bbox[2];
                    } else {
                        tar_x_min = create_pars->bbox[2];
                        tar_x_max = create_pars->bbox[0];
                    }
                    if (create_pars->bbox[1] < create_pars->bbox[3]) {
                        tar_y_min = create_pars->bbox[1];
                        tar_y_max = create_pars->bbox[3];
                    } else {
                        tar_y_min = create_pars->bbox[3];
                        tar_y_max = create_pars->bbox[1];
                    }

                    // otherwise if an additional bounding box is stated, then copy the
                    // source array and reassign the boundaries
                    switch (create_pars->type) {
                        case S_ARRAY:
                            tar_arr = new SciDBSpatialArray(*src_array);
                            break;
                        case ST_ARRAY:
                        case ST_SERIES:
                            tar_arr = new SciDBSpatioTemporalArray(
                                *(SciDBSpatioTemporalArray*)src_array);
                            ((SciDBSpatioTemporalArray*)src_array)->getTDim()->high = 0;
                            break;
                    }
                    // create "bigger" array
                    SciDBDimension* x = tar_arr->getXDim();
                    SciDBDimension* y = tar_arr->getYDim();

                    AffineTransform af = src_array->affineTransform;
                    AffineTransform::double2 ul = AffineTransform::double2(tar_x_min, tar_y_max);
                    AffineTransform::double2 lr = AffineTransform::double2(tar_x_max, tar_y_min);

                    // calculate image coordinates
                    af.fInv(ul);
                    af.fInv(lr);

                    double img_x_min, img_x_max, img_y_min, img_y_max;
                    if (ul.x < lr.x) {
                        img_x_min = ul.x;
                        img_x_max = lr.x;
                    } else {
                        img_x_max = ul.x;
                        img_x_min = lr.x;
                    }

                    if (lr.y < ul.y) {
                        img_y_min = lr.y;
                        img_y_max = ul.y;
                    } else {
                        img_y_max = lr.y;
                        img_y_min = ul.y;
                    }

                    // new image coordinates for target array
                    x->low = (int64_t)floor(img_x_min);
                    x->high = (int64_t)ceil(img_x_max);
                    x->start = x->low;
                    x->length = (x->high  - x->low) + 1;
                    y->low = (int64_t)floor(img_y_min);
                    y->high = (int64_t)ceil(img_y_max);
                    y->start = y->low;
                    y->length = (y->high  - y->low) + 1;

                    // transfer the stated SRS information from create options to the target array
                    tar_arr->auth_name = create_pars->auth_name;
                    tar_arr->auth_srid = create_pars->srid;
                    Utils::debug("target array was created with the extent of the stated bbox");
                } else {
                    tar_arr = src_array;
                    Utils::debug("target array is a copy of the source array");
                }
            }
            Utils::debug("-- DONE");

            Utils::debug("** Check if the source array can be inserted into the target "
                        "array **");
            if (!arrayIntegrateable(*src_array, *tar_arr)) {
                Utils::debug("Array not integrateable");
                throw ERR_CREATE_ARRAY_NOT_INSERTABLE;
            }
            Utils::debug("-- DONE");

            string arrayName = src_array->name;
            string tempArrayName = src_array->name + SCIDB4GDAL_ARRAYSUFFIX_TEMP;
            string insertableName = src_array->name + "_insertable";
            string insertableTempName = insertableName + SCIDB4GDAL_ARRAYSUFFIX_TEMP;

            if (src_array != tar_arr) {
                src_array->name = insertableName;
            }

            
            
            
            Utils::debug("** Creating the array structure for the uploaded image in SciDB **");
            if (client->createTempArray(*src_array) != SUCCESS) {
                throw ERR_CREATE_TEMPARRAY;
            }
            Utils::debug("-- DONE");
            // at this point the target array for the upload has been created in GDAL

            // now upload the source array into SciDB with the original coordinates as
            // temporary
            // Copy data and write to SciDB as a temporary array
            Utils::debug("** Upload the source image into the temporary array **");
            uploadImageIntoTempArray(client, *src_array, poSrcDS, pfnProgress, pProgressData);
            Utils::debug("-- DONE");

            if (src_array == tar_arr) {
                // normal case: image does not exist and has no special boundary

                // simply persist the uploaded image, set references and metadata
                Utils::debug("Persisting temporary array '" + tempArrayName + "'");
                StatusCode persist_res =
                    client->persistArray(tempArrayName, src_array->name);
                if (persist_res == SUCCESS) {
                    Utils::debug("Removing temporary array '" + tempArrayName + "'");
                    client->removeArray(
                        tempArrayName); // deletes the temporary array in SciDB

                    client->updateSRS(*src_array);

                    if (create_pars->type == ST_ARRAY || create_pars->type == ST_SERIES) {
                        client->updateTRS(*((SciDBSpatioTemporalArray*)src_array));
                    }

                    Utils::debug("Trying to persist array metadata in SciDB system catalog");
                    // Set Metadata in database
                    {
                        // set general image information
                        for (DomainMD::iterator it = src_array->md.begin();
                            it != src_array->md.end(); ++it) {
                            client->setArrayMD(src_array->name, it->second, it->first);
                        }

                        // set attribute data for each band
                        for (int i = 0; i < nBands; ++i) {
                            SciDBAttribute attr = src_array->attrs[i];
                            for (DomainMD::iterator it = attr.md.begin(); it != attr.md.end(); ++it) {
                                client->setAttributeMD(src_array->name, attr.name, it->second, it->first);
                            }
                        }
                    }
                } else {
                    throw persist_res;
                }
            } else { // new target array case
                // update spatial (and temporal) reference for temp source
                src_array->name = insertableTempName;
                client->updateSRS(*src_array);
                if (create_pars->type == ST_ARRAY || create_pars->type == ST_SERIES) {
                    client->updateTRS(*((SciDBSpatioTemporalArray*)src_array));
                }

                // create temporary target array if not exists
                if (!exists) {
                    if (client->createTempArray(*tar_arr) != SUCCESS) {
                        throw ERR_CREATE_TEMPARRAY;
                    }
                    tar_arr->name = tempArrayName;

                    client->updateSRS(*tar_arr);
                    if (create_pars->type == ST_ARRAY || create_pars->type == ST_SERIES) {
                        client->updateTRS(*((SciDBSpatioTemporalArray*)tar_arr));
                    }
                }

                // insert data into target array
                client->insertInto(*src_array, *tar_arr);
                client->removeArray(src_array->name); // insertable

                // persist target array
                if (!exists) {
                    StatusCode persist_res = client->persistArray(tempArrayName, arrayName);
                    if (persist_res == SUCCESS) {
                        tar_arr->name = arrayName;
                        Utils::debug(
                            "Trying to persist array metadata in SciDB system catalog");
                        // update final image if it was not created yet

                        client->updateSRS(*tar_arr);
                        if (create_pars->type == ST_ARRAY || create_pars->type == ST_SERIES) {
                            client->updateTRS(*((SciDBSpatioTemporalArray*)tar_arr));
                        }

                        // Set Metadata in database
                        {
                            // set general image information
                            for (DomainMD::iterator it = tar_arr->md.begin();
                                it != tar_arr->md.end(); ++it) {
                                client->setArrayMD(tar_arr->name, it->second, it->first);
                            }

                            // set attribute data for each band
                            for (int i = 0; i < nBands; ++i) {
                                for (DomainMD::iterator it = tar_arr->attrs[i].md.begin();
                                    it != tar_arr->attrs[i].md.end(); ++it) {
                                    client->setAttributeMD(tar_arr->name, tar_arr->attrs[i].name,
                                                        it->second, it->first);
                                }
                            }
                        }
                        client->removeArray(tempArrayName); // temp array
                    } else {
                        throw persist_res;
                    }
                }
                src_array = tar_arr;
            }

            pfnProgress(1.0, NULL, pProgressData);

            if (tar_arr && tar_arr != src_array)
                delete tar_arr;

            return new SciDBDataset(*src_array, client);
        } catch (StatusCode e) {
            // catch exceptions and give information back to the user
            switch (e) {
                case ERR_GLOBAL_PARSE:
                    Utils::error("This is not a scidb4gdal connection string");
                    break;
                case ERR_READ_ARRAYUNKNOWN:
                    Utils::error("No array name stated.");
                    break;
                case ERR_CREATE_AUTOCLEANUPFAILED:
                    Utils::error("Recovery failed, could not delete array. Please do this "
                                "manually in SciDB");
                    break;
                case ERR_CREATE_TERMINATEDBYUSER:
                    Utils::error("TERMINATED BY USER");
                    break;
                case ERR_CREATE_AUTOCLEANUPSUCCESS:
                    Utils::debug("Progress terminated. Successfully deleted temporary array "
                                "in SciDB.");
                    break;
                case ERR_CREATE_TEMPARRAY:
                    Utils::error("Could not create temporary SciDB array");
                    break;
                case ERR_READ_BBOX:
                    Utils::error("Paramater BBOX was used but the stated BBOX is incorrect. "
                                "Please use 4 coordinates and separate with space, eg. "
                                "\"bbox=left top right bottom\"");
                    break;
                case ERR_CREATE_ARRAY_NOT_INSERTABLE:
                    Utils::error("Source array is not insertable into the target array. "
                                "Please make sure that the source and target array have "
                                "coordinates in the same coordinate reference system and "
                                "that the source is tangetial proper part of the target "
                                "array.");
                    break;
                case ERR_READ_BBOX_SRS_MISSING:
                    Utils::error("Cannot create target array, because the stated coordinates "
                                "of \"bbox\" are not interpretable. Please state the SRS "
                                "with parameter \"srs\", e.g. EPSG:4326");
                    break;
                case ERR_CREATE_ARRAYEXISTS:
                    Utils::error("The array shall not be overwriten. Please delete the array "
                                "or rename it first and then run the command again.");
                    break;
                case ERR_CREATE_NOARRAY:
                    Utils::error("Could not create an internal representation for the "
                                "temporary array.");
                    break;
                case ERR_GLOBAL_INVALIDCONNECTIONSTRING:
                    Utils::error("The connection parameters are invalid. Please make sure to "
                                "state the parameters either as opening or create options, "
                                "as connection string in the file name or as environment "
                                "parameter.");
                    break;
                case ERR_GLOBAL_NO_SCIDB4GEO:
                    Utils::error("To perform the requested operation, the SciDB server must run the spacetime extension. Without the extension, it "
                        "is only possible to upload / download single images to / from a single two-dimensional arrays with the same image boundaries");
                    break;
            
                default:
                    Utils::error("Uncaught error: " + boost::lexical_cast<string>(e));
                    break;
            }
            if (client)
                delete client;
            if (con_pars)
                delete con_pars;
            if (create_pars)
                delete create_pars;
            if (src_array)
                delete src_array;
            if (tar_arr)
                delete tar_arr;

            return NULL;
        }
    }

    CPLErr SciDBDataset::Delete(const char* pszName) {
        try {
            ParameterParser p = ParameterParser(pszName, NULL, SCIDB_DELETE);
            ConnectionParameters c = p.getConnectionParameters();

            if (c.isValid() && c.deleteArray) {
                Utils::debug("Deleting array: " + c.arrayname);
                ShimClient client = ShimClient(&c);
                client.removeArray(c.arrayname);
            }
        } catch (StatusCode e) {
            Utils::debug("Error while deleting. Cannot delete image. Continuing.");
        }

        return CE_None;
    }

    int SciDBDataset::Identify(GDALOpenInfo* poOpenInfo) {
        if (poOpenInfo->pszFilename == NULL ||
            !EQUALN(poOpenInfo->pszFilename, "SCIDB:", 6)) {
            return FALSE;
        }
        return TRUE;
    }

    GDALDataset* SciDBDataset::Open(GDALOpenInfo* poOpenInfo) {
        // Check whether dataset matches SciDB dataset
        if (!Identify(poOpenInfo)) {
            return NULL;
        }

        // Do not allow update access
        if (poOpenInfo->eAccess == GA_Update) {
            CPLError(CE_Failure, CPLE_NotSupported, "scidb4gdal currently does not support update access to existing arrays.\n");
            return NULL;
        }

        string connstr = poOpenInfo->pszFilename;

        // feed the parser with the filename and the opening options
        try {
            ParameterParser pp = ParameterParser(connstr, poOpenInfo->papszOpenOptions);

            // 1. parse connection string and extract the following values
            QueryParameters* query_pars;
            ConnectionParameters* con_pars;

            query_pars = &pp.getQueryParameters();
            con_pars = &pp.getConnectionParameters();

           

            // 2. Check validity of connection parameters
            if (!con_pars->isValid()) {
                throw con_pars->error_code;
            } else {
                Utils::debug("Using connection parameters: " + con_pars->toString());
            }

            // 3. Create shim client
            ShimClient* client = new ShimClient(con_pars);
            
            //  Check whether scidb4geo is installed and stop impossible operations
            if (!client->hasSCIDB4GEO()) {
                if (query_pars->timestamp.length() > 0) {  
                    Utils::error("Downloading temporal slices with given timestamp needs SciDB spacetime extension. Please install the extension on the SciDB server.");
                    throw ERR_GLOBAL_NO_SCIDB4GEO;
                }
            }
            
            

            client->setQueryParameters(*query_pars);
            

            // create simple array here and cast it later if needed
            SciDBSpatialArray* array;
            // 4. Request array metadata
            if (client->getArrayDesc(con_pars->arrayname, array) != SUCCESS) {
                Utils::error("Cannot fetch array metadata");
                return NULL;
            }
            if (!array) {
                Utils::debug("array changes not afflicting the 'scidbdriver'");
            }

            // try to cast the array. if not possible then it is null and the temporal
            // parameter setting is skipped
            SciDBSpatioTemporalArray* starray_ptr = dynamic_cast<SciDBSpatioTemporalArray*>(array);
            if (starray_ptr) {
                Utils::debug("Type Cast OK. Start setting up temporal information");
                // check if the temporal index was set or if a timestamp was used
                if (query_pars->hasTemporalIndex) {
                    Utils::debug("Has index...");
                    char* t_index_key = new char[1];
                    t_index_key[0] = 't';
                    if (CSLFindName(poOpenInfo->papszOpenOptions, t_index_key) >= 0) {
                        string value = CSLFetchNameValue(poOpenInfo->papszOpenOptions, t_index_key);
                        Utils::debug("Casting temporal index");
                        query_pars->temp_index = boost::lexical_cast<int>(value);
                        Utils::debug("done");
                    }
                } else {
                    // convert date to temporal index
                    Utils::debug("Converting date to index");
                    if (query_pars->timestamp.length() == 0) {
                        Utils::warn("No temporal index and no timestamp provided. Using first temporal index instead");
                        query_pars->temp_index = -1;
                        query_pars->hasTemporalIndex = false;
                    } else {
                        Utils::debug("Transform date to index");
                        TPoint time = TPoint(query_pars->timestamp);
                        query_pars->temp_index = starray_ptr->indexAtDatetime(time);
                        query_pars->hasTemporalIndex = true;
                    }
                }

                // get dimension for time
                SciDBDimension* dim;
                // dim = &starray_ptr->dims[starray_ptr->getTDimIdx()];
                dim = starray_ptr->getTDim();

                if (query_pars->hasTemporalIndex) {
                    if (query_pars->temp_index < dim->low ||
                        query_pars->temp_index > dim->high) {
                        Utils::error(
                            "Specified temporal index out of bounds. Temporal index stated "
                            "or calculated: " +
                            boost::lexical_cast<string>(query_pars->temp_index) +
                            ", Lower bound: " + boost::lexical_cast<string>(dim->low) + " (" +
                            starray_ptr->datetimeAtIndex(dim->low).toStringISO() + "), " +
                            "Upper bound: " + boost::lexical_cast<string>(dim->high) + " (" +
                            starray_ptr->datetimeAtIndex(dim->high).toStringISO() + ")");
                        return NULL;
                    }
                } 
                /*else {
                    //set the temporal index to 0 (the first one, if no query parameter was stated)
                    query_pars->temp_index = starray_ptr->getTDim()->low;
                    query_pars->hasTemporalIndex = true;
                }*/
            }

            // Create the dataset

            SciDBDataset* poDS;
            poDS = new SciDBDataset(*array, client);
            return (poDS);
        } catch (int e) {
            switch (e) {
                case ERR_GLOBAL_PARSE:
                    Utils::error("This is not a scidb4gdal connection string");
                    break;
                case ERR_READ_ARRAYUNKNOWN:
                    Utils::error("Array name missing");
                    break;
                case ERR_GLOBAL_NO_SCIDB4GEO:
                    Utils::error("To perform the requested operation, the SciDB server must run the spacetime extension. Without the extension, it "
                        "is only possible to upload / download single images to / from a single two-dimensional arrays with the same image boundaries");
                    break;
            }
            return NULL;
        }
    }
}
