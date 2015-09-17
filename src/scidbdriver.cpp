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


// TODO:
// * Add different modes for handling chunks as different images
// * Add query params like trimming, projecting, ..
// * Add minimum chunk size (e.g. 1 MB), min(1MB, image size)
// * Test writing for image size < chunk size
// * Add chunksize and overlap parameter in connection string for array creation
// * Insert to existing array
// * (!!!) Define and implement test cases!
// * (!!!) Improved error handling, e.g. for writing to already existing arrays or including removal of temporary (load) arrays


#include "scidbdriver.h"
#include "shimclient.h"

#include "utils.h"
#include <iomanip>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/assign.hpp>
#include "shim_client_structs.h"
#include "scidb_structs.h"

CPL_C_START
void GDALRegister_SciDB ( void );
CPL_C_END

/**
 * GDAL driver registration function
 */
void GDALRegister_SciDB()
{
    GDALDriver  *poDriver;

    if ( GDALGetDriverByName ( "SciDB" ) == NULL ) {
        poDriver = new GDALDriver();
        poDriver->SetDescription ( "SciDB" );
        //poDriver->SetMetadataItem ( GDAL_DCAP_RASTER, "YES");
        poDriver->SetMetadataItem ( GDAL_DMD_LONGNAME, "SciDB array driver" );
        poDriver->SetMetadataItem ( GDAL_DMD_HELPTOPIC, "frmt_scidb.html" );
        poDriver->pfnOpen = scidb4gdal::SciDBDataset::Open;
        poDriver->pfnIdentify = scidb4gdal::SciDBDataset::Identify;
        // poDriver->pfnCreate = scidb4gdal::SciDBDataset::Create;
        poDriver->pfnDelete = scidb4gdal::SciDBDataset::Delete;

        poDriver->pfnCreateCopy = scidb4gdal::SciDBDataset::CreateCopy;

        GetGDALDriverManager()->RegisterDriver ( poDriver );
    }
}


namespace scidb4gdal
{

    SciDBRasterBand::SciDBRasterBand ( SciDBDataset *poDS, SciDBSpatialArray *array, int nBand )
    {
        this->poDS = poDS;
        this->nBand = nBand;
        this->_array = array;
	
        eDataType = Utils::scidbTypeIdToGDALType ( _array->attrs[nBand].typeId ); // Data type is mapped from SciDB's attribute data type

        /* GDAL interprets x dimension as image rows and y dimension as image cols whereas our
         * implementation of spatial reference systems assumes x being easting and y being northing.
         * This makes the following code pretty messy in mixing x and y. */
        uint32_t nImgYSize ( 1 + _array->getXDim().high - _array->getXDim().low );
        uint32_t nImgXSize ( 1 + _array->getYDim().high - _array->getYDim().low );
        nBlockYSize = ( _array->getXDim().chunksize < nImgYSize ) ? _array->getXDim().chunksize : nImgYSize;
        nBlockXSize = ( _array->getYDim().chunksize < nImgXSize ) ? _array->getYDim().chunksize : nImgXSize;
    }


    SciDBRasterBand::~SciDBRasterBand()
    {
        FlushCache();
    }



    CPLErr SciDBRasterBand::GetStatistics ( int bApproxOK, int bForce, double *pdfMin, double *pdfMax, double *pdfMean, double *pdfStdDev )
    {

        SciDBAttributeStats stats ;
        ( ( SciDBDataset * ) poDS )->getClient()->getAttributeStats ( *_array, this->nBand - 1, stats );

        *pdfMin = stats.min;
        *pdfMax = stats.max;
        *pdfMean = stats.mean;
        *pdfStdDev = stats.stdev;

        return CE_None;
    }


    // TODO: Add manual caching for efficient non-redundant reads when writing line-oriented file formats
    CPLErr SciDBRasterBand::IReadBlock ( int nBlockXOff, int nBlockYOff, void *pImage )
    {
	std::stringstream stream;
	stream << "Offsets: " << nBlockXOff << " " << nBlockYOff;
        Utils::debug(stream.str());
	//TODO: Improve error handling
        SciDBDataset *poGDS = ( SciDBDataset * ) poDS;
	
	//TODO for interval queries make adaptions here
	int t_index = poGDS->_query->temp_index;
	
        uint32_t tileId = TileCache::getBlockId ( nBlockXOff, nBlockYOff, nBand - 1, nBlockXSize, nBlockXSize, poGDS->GetRasterCount() );

        ArrayTile tile;
        tile.id = tileId;

        // Check whether chunk is in cache
        if ( poGDS->_cache.has ( tileId ) ) {
            tile = *poGDS->_cache.get ( tileId );
        }

        else {
            /* GDAL interprets x dimension as image rows and y dimension as image cols whereas our
            * implementation of spatial reference systems assumes x being easting and y being northing.
            * This makes the following code pretty messy in mixing x and y. */
            int xmin = nBlockYOff * this->nBlockYSize + _array->getXDim().low;
            int xmax = xmin + this->nBlockYSize - 1;
            if ( xmax > _array->getXDim().high ) xmax = _array->getXDim().high;

            int ymin = nBlockXOff * this->nBlockXSize + _array->getYDim().low;
            int ymax = ymin + this->nBlockXSize - 1;
            if ( ymax > _array->getYDim().high ) ymax = _array->getYDim().high;
	    

            // Read  and fetch data
            tile.size = nBlockXSize * nBlockYSize * Utils::scidbTypeIdBytes ( _array->attrs[nBand - 1].typeId ); // Always allocate full block size
            tile.data = malloc ( tile.size ); // will be freed automatically by cache


            // Last blocks must be treated separately if covering area outside actual array: (0 | 1 | 2 | 3 || 4 | 5 | 6 | 7) vs. (0 | 1 | - | - || 3 | 4 | - | -) for 4x2 block with only 2x2 data
            // TODO: Check whether this works also for nBlockXSize != nBlockYSize
            if ( ( nBlockXOff + 1 ) * this->nBlockXSize > poGDS->nRasterXSize ) { //end of the blocks bigger than the whole image, then fetch next block row
                // This is a bit of a hack...
                size_t dataSize = ( 1 + xmax - xmin ) * ( 1 + ymax - ymin ) * Utils::scidbTypeIdBytes ( _array->attrs[nBand - 1].typeId ); // This is smaller than the block size!
                void *buf = malloc ( dataSize );

                // Write to temporary buffer first
                poGDS->getClient()->getData ( *_array, nBand - 1, buf, xmin, ymin, xmax, ymax, t_index ); // GDAL bands start with 1, scidb attribute indexes with 0
                for ( uint32_t i = 0; i < ( 1 + xmax - xmin ); ++i ) {
                    uint8_t *src  = & ( ( uint8_t * ) buf ) [i * ( 1 + ymax - ymin ) * Utils::scidbTypeIdBytes ( _array->attrs[nBand - 1].typeId )];
                    // TODO: In the following call, check whether this->nBlockXSize must be replaced with this->nBlockYSize for unequally sized chunk dimensions
                    uint8_t *dest = & ( ( uint8_t * ) tile.data ) [i * this->nBlockXSize * Utils::scidbTypeIdBytes ( _array->attrs[nBand - 1].typeId )];
                    memcpy ( dest, src, ( 1 + ymax - ymin ) *Utils::scidbTypeIdBytes ( _array->attrs[nBand - 1].typeId ) );
                }
                free ( buf );
            }
            else poGDS->getClient()->getData ( *_array, nBand - 1, tile.data, xmin, ymin, xmax, ymax, t_index ); // GDAL bands start with 1, scidb attribute indexes with 0

            poGDS->_cache.add ( tile ); // Add to cache

        }

        // Copy from tile.data to pImage
        memcpy ( pImage, tile.data, tile.size );

        return CE_None;
    }


    /*

        CPLErr SciDBRasterBand::IWriteBlock ( int nBlockXOff, int nBlockYOff, void *pImage )
        {
            //TODO: Improve error handling

            SciDBDataset *poGDS = ( SciDBDataset * ) poDS;


            // 1. Compute array bounds from block offsets
            int xmin = nBlockYOff * this->nBlockYSize + _array->getXDim().low;
            int xmax = xmin + this->nBlockYSize - 1;
            if ( xmax > _array->getXDim().high ) xmax = _array->getXDim().high;

            int ymin = nBlockXOff * this->nBlockXSize + _array->getYDim().low;
            int ymax = ymin + this->nBlockXSize - 1;
            if ( ymax > _array->getYDim().high ) ymax = _array->getYDim().high;



            // Last blocks must be treated separately if covering area outside actual array: (0 | 1 | 2 | 3 || 4 | 5 | 6 | 7) vs. (0 | 1 | - | - || 3 | 4 | - | -) for 4x2 block with only 2x2 data
            bool xOuter = ( nBlockXOff + 1 ) * this->nBlockXSize > poGDS->nRasterXSize; // or >= ???
            if ( xOuter ) {
                // This is a bit of a hack...
                size_t pixelSize = 0;
                uint32_t nx = ( 1 + xmax - xmin );
                uint32_t ny = ( 1 + ymax - ymin );
                for ( uint16_t i = 0; i < _array->attrs.size(); ++i ) pixelSize += Utils::scidbTypeIdBytes ( _array->attrs[i].typeId );
                size_t dataSize = pixelSize *  nx * ny;
                void *buf = malloc ( dataSize );

                stringstream v;
                v << "Write to bounds: x=(" << xmin << "," << xmax << ") y=(" << ymin << "," << ymax << ") nx=" << nx << " ny=" << ny << " BLOCKSIZE X " << this->nBlockXSize << " BLOCKSIZE Y " << this->nBlockYSize;
                Utils::debug ( v.str() );

                // Write to temporary buffer first
                for ( uint32_t i = 0; i < nx; ++i ) {
                    // TODO: In the following call, check whether this->nBlockXSize must be replaced with this->nBlockYSize for unequally sized chunk dimensions
                    uint8_t *src  = & ( ( uint8_t * ) pImage ) [i * this->nBlockXSize * pixelSize];
                    uint8_t *dest  = & ( ( uint8_t * ) buf ) [i * ny * pixelSize];
                    memcpy ( dest, src, ny * pixelSize );
                }
                // Write from buf instead of pImage
                poGDS->getClient()->insertData ( *_array, buf, xmin, ymin, xmax, ymax );

                free ( buf );
            }
            else poGDS->getClient()->insertData ( *_array, pImage, xmin, ymin, xmax, ymax );


            return CE_None;
        }*/


    


    SciDBDataset::SciDBDataset ( SciDBSpatialArray array, ShimClient *client, TemporalQueryParameters *props ) : _array ( array ), _client ( client ), _query(props)
    {


        /* GDAL interprets x dimension as image rows and y dimension as image cols whereas our
         * implementation of spatial reference systems assumes x being easting and y being northing.
         * This makes the following code pretty messy in mixing x and y. */
        this->nRasterYSize = 1 + _array.getXDim().high - _array.getXDim().low ;
        this->nRasterXSize = 1 + _array.getYDim().high - _array.getYDim().low ;


        // Create GDAL Bands
        for ( uint32_t i = 0; i < _array.attrs.size(); ++i )
            this->SetBand ( i + 1, new SciDBRasterBand ( this, &_array, i ) );


        this->SetDescription ( _array.toString().c_str() );



        // TODO: Overviews?
        //poDS->oOvManager.Initialize ( poDS, poOpenInfo->pszFilename );



    }


    /*
        CPLErr SciDBDataset::SetProjection ( const char *wkt )
        {
            _array.srtext = wkt;
            OGRSpatialReference srs ( wkt );
            srs.AutoIdentifyEPSG();
            _array.auth_name = srs.GetAuthorityName ( NULL );
            _array.auth_srid = boost::lexical_cast<uint32_t> ( srs.GetAuthorityCode ( NULL ) );
            char *proj4;
            srs.exportToProj4 ( &proj4 );
            _array.proj4text.assign ( proj4 );
            CPLFree ( proj4 );

            _array.xdim = SCIDB4GDAL_DEFAULT_XDIMNAME;
            _array.ydim = SCIDB4GDAL_DEFAULT_YDIMNAME;


            // Only perform database update, if affine transformation has already been set
            if ( !_array.affineTransform.isIdentity() ) {
                _client->updateSRS ( _array );
            }

            return CE_None;
        }


        CPLErr SciDBDataset::SetGeoTransform ( double *padfTransform )
        {

            AffineTransform a ( padfTransform[0], padfTransform[3], padfTransform[1], padfTransform[5], padfTransform[2], padfTransform[4] );
            _array.affineTransform = a;

            // Only perform database update, if srs has already been set
            if ( !_array.srtext.empty() ) {
                _client->updateSRS ( _array );

            }
            return CE_None;
        }*/



    GDALDataset *SciDBDataset::CreateCopy ( const char *pszFilename, GDALDataset *poSrcDS, int bStrict, char **papszOptions, GDALProgressFunc pfnProgress, void *pProgressData )
    {
        int  nBands = poSrcDS->GetRasterCount();
        int  nXSize = poSrcDS->GetRasterXSize();
        int  nYSize = poSrcDS->GetRasterYSize();

        ConnectionPars *pars = new ConnectionPars();
	GDALOpenInfo* info = new GDALOpenInfo(pszFilename,0);
	parseOpeningOptions(info, pars);
        
//         parseConnectionString ( pszFilename);

        // Create array metadata structure
        SciDBSpatialArray array;
        array.name = pars->arrayname;

        // Attributes
        size_t pixelsize = 0; // size in bytes of one pixel, i.e. sum of attribute sizes
        vector<SciDBAttribute> attrs;
        for ( int i = 0; i < nBands; ++i ) {
            SciDBAttribute a;
            a.nullable = false; // TODO
            a.typeId = Utils::gdalTypeToSciDBTypeId ( poSrcDS->GetRasterBand ( i + 1 )->GetRasterDataType() ); // All attribtues must have the same data type
            stringstream aname;
            pixelsize += Utils::gdalTypeBytes ( poSrcDS->GetRasterBand ( i + 1 )->GetRasterDataType() );
            aname << "band" << i + 1;
            a.name = aname.str();
            attrs.push_back ( a );
        }
        array.attrs = attrs;



        // Dimensions

        vector<SciDBDimension> dims;


        // Derive chunksize from attribute sizes
        uint32_t blocksize = Utils::nextPow2 ( ( uint32_t ) sqrt ( ( ( ( double ) ( SCIDB4GEO_DEFAULT_CHUNKSIZE_MB * 1024 * 1024 ) ) / ( ( double ) ( pixelsize ) ) ) ) ) / 2;
        stringstream ss;
        ss << "Using chunksize " << blocksize << "x" << blocksize << " --> " << ( int ) ( ( ( double ) ( blocksize * blocksize * pixelsize ) ) / ( ( double ) 1024.0 ) ) << " kilobytes";
        Utils::debug ( ss.str() );

        SciDBDimension dimx;
        dimx.low = 0;
        dimx.high = nYSize - 1;
        dimx.name = SCIDB4GDAL_DEFAULT_XDIMNAME;
        // dimx.chunksize = SCIDB4GDAL_DEFAULT_BLOCKSIZE_X;
        dimx.chunksize = blocksize;
        dimx.typeId = "int64";

        SciDBDimension dimy;
        dimy.low = 0;
        dimy.high = nXSize - 1;
        dimy.name = SCIDB4GDAL_DEFAULT_YDIMNAME;
        //dimy.chunksize = SCIDB4GDAL_DEFAULT_BLOCKSIZE_Y;
        dimy.chunksize = blocksize;
        dimy.typeId = "int64";


        dims.push_back ( dimx );
        dims.push_back ( dimy );

        array.dims = dims;





        // Create shim client
        ShimClient *client = new ShimClient ( pars->host, pars->port, pars->user, pars->passwd, pars->ssl );



        // Create array in SciDB
        if ( client->createTempArray ( array ) != SUCCESS ) {
            Utils::error ( "Could not create temporary SciDB array" );
            return NULL;
        }


        // Copy data and write to SciDB
        size_t pixelSize = 0;
        for ( uint32_t i = 0; i < array.attrs.size(); ++i ) pixelSize += Utils::scidbTypeIdBytes ( array.attrs[i].typeId );
        size_t totalSize = pixelSize *  dimx.chunksize * dimy.chunksize;

        uint8_t *bandInterleavedChunk = ( uint8_t * ) malloc ( totalSize ); // This is a byte array


        uint32_t nBlockX = ( uint32_t ) ( nXSize / dimx.chunksize );
        if ( nXSize % dimx.chunksize != 0 ) ++nBlockX;

        uint32_t nBlockY = ( uint32_t ) ( nYSize / dimy.chunksize );
        if ( nYSize % dimy.chunksize != 0 ) ++nBlockY;




        for ( uint32_t bx = 0; bx < nBlockY; ++bx ) {
            for ( uint32_t by = 0; by < nBlockX; ++by ) {

                size_t bandOffset = 0; // sum of bytes taken by previous bands, will be updated in loop over bands



                if ( !pfnProgress ( ( ( double ) ( bx * nBlockY + by ) ) / ( ( double ) ( nBlockX * nBlockY ) ), NULL, pProgressData ) ) {
                    Utils::debug ( "Interruption by user requested, trying to clean up" );
                    // Clean up intermediate arrays
                    client->removeArray ( array.name );
                    Utils::error ( "TERMINATED BY USER" );
                    return NULL;
                }


                // 1. Compute array bounds from block offsets
                int xmin = bx *  dimx.chunksize + dimx.low; // TODO: Check whether x, y is correct
                int xmax = xmin +  dimx.chunksize - 1; // TODO: Check whether x, y is correct
                if ( xmax > dimx.high ) xmax = dimx.high; // TODO: Check whether x, y is correct
				if ( xmin > dimx.high ) xmin = dimx.high; // TODO: Check whether x, y is correct

                int ymin = by *  dimy.chunksize + dimy.low; // TODO: Check whether x, y is correct
                int ymax = ymin + dimy.chunksize - 1; // TODO: Check whether x, y is correct
                if ( ymax > dimy.high ) ymax = dimy.high; // TODO: Check whether x, y is correct
				if ( ymin > dimy.high ) ymin = dimy.high; // TODO: Check whether x, y is correct
				
                // We assume reading whole blocks of individual bands first is more efficient than reading single band pixels subsequently
                for ( uint16_t iBand = 0; iBand < nBands; ++iBand ) {
                    void *blockBandBuf =  malloc ( ( 1 + xmax - xmin ) * ( 1 + ymax - ymin ) * Utils::scidbTypeIdBytes ( array.attrs[iBand].typeId ) );
					
                    // Using nPixelSpace and nLineSpace arguments could maybe automatically write to bandInterleavedChunk properly
                    GDALRasterBand *poBand = poSrcDS->GetRasterBand ( iBand + 1 );
		    
		    //TODO in 2.0 (released June 2015) this function expects also 'psExtraArg' as additional argument. Here it is passed with NULL
		    // in order to avoid "make" errors.
                    poBand->RasterIO ( GF_Read, ymin, xmin, 
				       1 + ymax - ymin, 1 + xmax - xmin, 
				       ( void * ) blockBandBuf,  
				       1 + ymax - ymin, 1 + xmax - xmin, 
				       Utils::scidbTypeIdToGDALType ( array.attrs[iBand].typeId ), 
				       0, 0,NULL);

                    /* SciDB load file format is band interleaved by pixel / cell, whereas common
                    GDAL functions are rather band sequential. In the following, we perform block-wise interleaving
                    manually. */

                    // Variable (unknown) data types and band numbers make this somewhat ugly
                    for ( int i = 0; i < ( 1 + xmax - xmin ) * ( 1 + ymax - ymin ); ++i ) {
                        memcpy ( & ( ( char * ) bandInterleavedChunk ) [i * pixelSize + bandOffset], & ( ( char * ) blockBandBuf ) [i * Utils::scidbTypeIdBytes ( array.attrs[iBand].typeId )], Utils::scidbTypeIdBytes ( array.attrs[iBand].typeId ) );
                    }
                    free ( blockBandBuf );
                    bandOffset += Utils::scidbTypeIdBytes ( array.attrs[iBand].typeId );

                }

                if ( client->insertData ( array, bandInterleavedChunk, xmin, ymin, xmax, ymax ) != SUCCESS ) {
                    Utils::error ( "Copying data to SciDB array failed, trying to recover initial state..." );
                    if ( client->removeArray ( array.name ) != SUCCESS ) {
                        Utils::error ( "Recovery faild, could not delete array '" + array.name + "'. Please do this manually in SciDB" );
                    }
                    return NULL;
                }
            }
        }



        free ( bandInterleavedChunk );

        Utils::debug ( "Persisting temporary array '" + array.name + SCIDB4GDAL_ARRAYSUFFIX_TEMP + "'" );
        client->copyArray ( array.name + SCIDB4GDAL_ARRAYSUFFIX_TEMP, array.name );

        Utils::debug ( "Removing temporary array '" + array.name + SCIDB4GDAL_ARRAYSUFFIX_TEMP + "'" );
        client->removeArray ( array.name + SCIDB4GDAL_ARRAYSUFFIX_TEMP );



        // Set Spatial reference
        double padfTransform[6];
        string wkt ( poSrcDS->GetProjectionRef() );
        if ( wkt != "" && poSrcDS->GetGeoTransform ( padfTransform ) == CE_None ) {
            AffineTransform a ( padfTransform[0], padfTransform[3], padfTransform[1], padfTransform[5], padfTransform[2], padfTransform[4] );

            array.affineTransform = a;

            array.srtext = wkt;
            OGRSpatialReference srs ( wkt.c_str() );
            srs.AutoIdentifyEPSG();
            array.auth_name = srs.GetAuthorityName ( NULL );
            array.auth_srid = boost::lexical_cast<uint32_t> ( srs.GetAuthorityCode ( NULL ) );
            char *proj4;
            srs.exportToProj4 ( &proj4 );
            array.proj4text.assign ( proj4 );
            CPLFree ( proj4 );

            array.xdim = SCIDB4GDAL_DEFAULT_XDIMNAME;
            array.ydim = SCIDB4GDAL_DEFAULT_YDIMNAME;



            client->updateSRS ( array );
        }





        pfnProgress ( 1.0, NULL, pProgressData );


        delete client;

        return ( GDALDataset * ) GDALOpen ( pszFilename, GA_ReadOnly );

    }



    /*
        GDALDataset *SciDBDataset::Create ( const char *pszFilename, int nXSize, int nYSize, int nBands, GDALDataType eType, char   **papszParmList )
        {

            ConnectionPars *pars = ConnectionPars::parseConnectionString ( pszFilename );

            // Create array metadata structure
            SciDBSpatialArray array;
            array.name = pars->arrayname;

            // Attributes
            vector<SciDBAttribute> attrs;
            for ( int i = 0; i < nBands; ++i ) {
                SciDBAttribute a;
                a.nullable = false; // TODO
                a.typeId = Utils::gdalTypeToSciDBTypeId ( eType ); // All attribtues must have the same data type
                stringstream aname;
                aname << "band" << i + 1;
                a.name = aname.str();
                attrs.push_back ( a );
            }
            array.attrs = attrs;



            // Dimensions

            vector<SciDBDimension> dims;

            SciDBDimension dimx;
            dimx.low = 0;
            dimx.high = nYSize - 1;
            dimx.name = SCIDB4GDAL_DEFAULT_XDIMNAME;
            dimx.chunksize = SCIDB4GDAL_DEFAULT_BLOCKSIZE_X;
            dimx.typeId = "int64";

            SciDBDimension dimy;
            dimy.low = 0;
            dimy.high = nXSize - 1;
            dimy.name = SCIDB4GDAL_DEFAULT_YDIMNAME;
            dimy.chunksize = SCIDB4GDAL_DEFAULT_BLOCKSIZE_Y;
            dimy.typeId = "int64";

            // TODO: Check order
            dims.push_back ( dimx );
            dims.push_back ( dimy );

            array.dims = dims;

            // Spatial reference is handled automatically by GDAL calling SetGeoTransform() and SetProjection()


            // Create shim client
            ShimClient *client = new ShimClient ( pars->host, pars->port, pars->user, pars->passwd );


            // Create array in SciDB
            client->createArray ( array );


            delete client;
            return ( GDALDataset * ) GDALOpen ( pszFilename, GA_Update );
        }*/





    SciDBDataset::~SciDBDataset()
    {
        FlushCache();
        delete _client;
    }
    

    CPLErr SciDBDataset::GetGeoTransform ( double *padfTransform )
    {
        padfTransform[0] = _array.affineTransform._x0;
        padfTransform[1] = _array.affineTransform._a11;
        padfTransform[2] = _array.affineTransform._a12;
        padfTransform[3] = _array.affineTransform._y0;
        padfTransform[4] = _array.affineTransform._a21;
        padfTransform[5] = _array.affineTransform._a22;
        return CE_None;
    }


    const char *SciDBDataset::GetProjectionRef()
    {
        return _array.srtext.c_str();
    }
    


    int SciDBDataset::Identify ( GDALOpenInfo *poOpenInfo )
    {
        if ( poOpenInfo->pszFilename == NULL || !EQUALN ( poOpenInfo->pszFilename, "SCIDB:", 6 ) ) {
            return FALSE;
        }
        return TRUE;
    }


    CPLErr SciDBDataset::Delete ( const char *pszName )
    {
        // TODO
        Utils::debug ( "Deleting SciDB arrays from GDAL is currently not allowed..." );
        return CE_None;
    }
  

    GDALDataset *SciDBDataset::Open ( GDALOpenInfo *poOpenInfo )
    {

        string connstr = poOpenInfo->pszFilename;


        // Check whether dataset matches SciDB dataset
        if ( !Identify ( poOpenInfo ) ) {
            return NULL;
        }

        // Do not allow update access
        if ( poOpenInfo->eAccess == GA_Update ) {
            CPLError ( CE_Failure, CPLE_NotSupported, "scidb4gdal currently does not support update access to existing arrays.\n" );
            return NULL;
        }

        
        // 1. parse connection string and extract the following values
        //TODO should be one function at the scidb driver
        TemporalQueryParameters *sp = new TemporalQueryParameters();
	ConnectionPars *pars = new ConnectionPars();
	
	if ( connstr.substr ( 0, 6 ).compare ( "SCIDB:" ) != 0 ) {
	    Utils::error ( "This is not a scidb4gdal connection string" );
	}
	string astr = connstr.substr ( 6, connstr.length() - 6 ); // Remove SCIDB: from connection string
	
	//1. search for "properties=" in the string
	string connectionString, propertiesString;
	
	if (splitPropertyString(astr, connectionString, propertiesString)){
	  parsePropertiesString(propertiesString, sp);
	}
	
	//first extract information from connection string and afterwards check for the opening options and overwrite values if double
	parseConnectionString(connectionString, pars);
	parseOpeningOptions(poOpenInfo, pars);
	
	parseArrayName(pars->arrayname, sp); //array name will be modified if a temporal query is detected (for the ConnectionPars)
	
	// 2. Check validity of parameters
        if ( pars->arrayname == "" ) {
	  Utils::error ( "No array specified, currently not supported" );
	  return NULL;
	}
	
        //ConnectionPars *pars = ConnectionPars::parseConnectionString ( connstr );
        Utils::debug ( "Using connection parameters:" + pars->toString() );
	
        // 3. Create shim client
        ShimClient *client = new ShimClient ( pars->host, pars->port, pars->user, pars->passwd, pars->ssl);

        SciDBSpatioTemporalArray array;
        // 4. Request array metadata
        if ( client->getArrayDesc ( pars->arrayname, array ) != SUCCESS ) {
            Utils::error ( "Cannot fetch array metadata" );
            return NULL;
        }
	
		//check if the temporal index was set or if a timestamp was used
	if (sp->hasTemporalIndex) {
	  char* t_index_key = new char[1];
	  t_index_key[0] = 't';
	  if (CSLFindName(poOpenInfo->papszOpenOptions,t_index_key) >= 0) {
	    string value = CSLFetchNameValue(poOpenInfo->papszOpenOptions,t_index_key);
	    sp->temp_index = boost::lexical_cast<int>(value);
	  } 
	} else {

	  //convert date to temporal index
	  TPoint time = TPoint(sp->timestamp);
	  sp->temp_index = array.indexAtDatetime(time);
	  sp->hasTemporalIndex = true;
	}
	
	
	
	
	//TODO check also if temporal index is within time dimension range
	if (array.dims.size() > 2) {
	  if ( !array.isTemporal() ) {
	      Utils::debug ( "No time statement defined in the connection string. Can't decide which image to fetch in the temporal data set." );
// 	      return NULL;
	  } else {
	      //get dimension for time
	      SciDBDimension *dim;
	      dim = &array.dims[array.getTDimIdx()];

	      if (sp->temp_index < dim->low || sp->temp_index > dim->high) {
		  Utils::error ( "Specified temporal index out of bounce. Temporal Index stated or calculated: " + boost::lexical_cast<string>(sp->temp_index) +
		  ", Lower bound: " + boost::lexical_cast<string>(dim->low) + " (" + array.datetimeAtIndex(dim->low).toStringISO() + "), " +
		    "Upper bound: " + boost::lexical_cast<string>(dim->high) + " (" + array.datetimeAtIndex(dim->high).toStringISO() + ")"
		  );
		  return NULL;
	      }
	  }
	}


        delete pars;


        // Create the dataset

        SciDBDataset *poDS;
        poDS = new SciDBDataset ( array, client, sp );
        return ( poDS );
    }
    
    bool SciDBDataset::splitPropertyString (string &input, string &constr, string &propstr) {
      	string propToken = "properties=";
	int start = input.find(propToken);
	bool found = (start >= 0);

	//if there then split it accordingly and fill the ConnectionPars and the SelectProperties
	if (found) {
	  // if we find the 'properties=' in the connection string we treat those string part as
	  // the database open parameters 
	  int end = start + propToken.length();
	  constr = input.substr(0,start-1);
	  propstr = input.substr(end,input.length()-1);
	  
	} else {
	  constr = input;
	}
	return found;
    }

    void SciDBDataset::parsePropertiesString ( const string &propstr, TemporalQueryParameters* query ) {
      //mapping between the constant variables and their string representation for using a switch/case statement later
      std::map<string,Properties> propResolver = map_list_of ("t",T_INDEX);
      vector<string> parts;
      boost::split ( parts, propstr, boost::is_any_of ( ";" ) ); // Split at semicolon and comma for refering to a whole KVP
      //example for filename with properties variable    "src_win:0 0 50 50;..."
      for ( vector<string>::iterator it = parts.begin(); it != parts.end(); ++it ) {
	vector<string> kv, c;
	boost::split ( kv, *it, boost::is_any_of ( ":=" ) ); 
	if ( kv.size() < 2 ) {
	  continue;
	} else {
	  if(propResolver.find(std::string(kv[0])) != propResolver.end()) {
	    switch(propResolver[kv[0]]) {
	      case T_INDEX:
		query->temp_index = boost::lexical_cast<int>(kv[1]);
		//TODO maybe we allow also selecting multiple slices (that will later be saved separately as individual files)
		break;
	      default:
		continue;
		
	    }
	  } else {
	    Utils::debug("unused parameter \""+string(kv[0])+ "\" with value \""+string(kv[1])+"\"");
	  }
	}
      }
    }
    
    void SciDBDataset::parseArrayName (string& array, TemporalQueryParameters* query) {
      // 	  string array = pars->arrayname;
      size_t length = array.size();
      size_t t_start = array.find_first_of('[');
      size_t t_end = array.find_last_of(']');
      
      if (t_start < 0 || t_end < 0) {
	Utils::error("No or invalid temporal information in array name.");
	return;
      }
      //extract temporal string
      if ((length-1) == t_end) {
	string t_expression = array.substr(t_start+1,(t_end-t_start)-1);
	//remove the temporal part of the array name otherwise it messes up the concrete array name in scidb
	string temp = array.substr(0,t_start);
	array = temp;
	vector<string> kv;
	
	boost::split(kv, t_expression, boost::is_any_of(","));
	if (kv.size() < 2) {
	  Utils::error("Temporal query not complete. Please state the dimension name and the temporal index / interval");
	  return;
	}
	//first part is the dimension identifier
	query->dim_name = kv[0];
	//seceond part is either a temporal index (or temporal index interval) or a timestamp is used (or timestamp interval)
	string t_interval = kv[1];
	
	/* Test with Regex, failed due to gcc version 4.8.4 (4.9 implements regex completely) */
	// 	    std::smatch match;
	// 	    
	// 	    std::regex re("/^[\d]{4}(\/|-)[\d]{2}(\/|-)[\d]{2}(\s|T)[\d]{2}:[\d]{2}:[\d]{2}((\+|-)[0-1][\d]:?(0|3)0)?$/");
	// 	    if (std::regex_search(t_interval,match,re)) {
	// 	      ...
	// 	    }
	
	if (t_interval.length() > 0) {
	  //if string is a timestamp then translate it into the temporal index
	  if (Utils::validateTimestampString(t_interval)) {
	    //store timestamp for query when calling scidb
	    query->timestamp = t_interval;
	    query->hasTemporalIndex = false;
	  } else {                                               
	    //simply extract the temporal index
	    size_t pos = t_interval.find(":");
	    if (pos < t_interval.length()) {
	      //interval
	      vector<string> attr;
	      boost::split(attr,t_interval,boost::is_any_of(":"));
	      query->lower_bound = boost::lexical_cast<int>(attr[0]);
	      query->upper_bound = boost::lexical_cast<int>(attr[1]);
	      //TODO for now we parse it correctly, but we will just use the lower_bound... change this later
	      Utils::debug("Currently interval query is not supported. Using the lower bound instead");
	      query->temp_index = query->lower_bound;
	    } else {
	      //temporal index
	      query->temp_index = boost::lexical_cast<int>(t_interval);
	    }
	    query->hasTemporalIndex = true;
	  }
	} else {
	  Utils::error("No temporal information stated");
	  return;
	}
      }
    }
    
    void SciDBDataset::parseOpeningOptions (GDALOpenInfo *poOpenInfo, ConnectionPars* con) {
      std::map<string,ConStringParameter> paramResolver = map_list_of ("host", HOST)("port", PORT) ("user",USER) ("password", PASSWORD) ("ssl",SSL);
      
      char** options = poOpenInfo->papszOpenOptions;
      int count = CSLCount(options);
      for (int i = 0; i < count; i++) {
	const char* s = CSLGetField(options,i);
	char* key;
	const char* value = CPLParseNameValue(s,&key);
	
	if(paramResolver.find(std::string(key)) != paramResolver.end()) {
	  switch(paramResolver[std::string(key)]) {
	    case HOST:
	      con->host = value;
	      con->ssl = (std::string(value).substr ( 0, 5 ).compare ( "https" ) == 0 );
	      break;
	    case USER:
	      con->user = value;
	      break;
	    case PORT:
	      con->port = boost::lexical_cast<int>(value);
	      break;
	    case PASSWORD:
	      con->passwd = value;
	      break;
	    case SSL:
	      con->ssl = CSLTestBoolean(value);
	      break;
	    default:
	      continue;
	  }
	} else {
	  Utils::debug("unused parameter \""+string(key)+ "\" with value \""+string(value)+"\"");
	}
      }
    }
    
    void SciDBDataset::parseConnectionString ( const string &connstr, ConnectionPars *con) {
      std::map<string,ConStringParameter> paramResolver = map_list_of ("host", HOST)("port", PORT) ("array", ARRAY) ("user",USER) ("password", PASSWORD);
      
      vector<string> connparts;
      // Split at whitespace, comma, semicolon
      boost::split ( connparts, connstr, boost::is_any_of ( "; " ) );
      for ( vector<string>::iterator it = connparts.begin(); it != connparts.end(); ++it ) {
	vector<string> kv;
	// No colon because uf URL
	boost::split ( kv, *it, boost::is_any_of ( "=" ) );
	if ( kv.size() != 2 ) {
	  continue;
	} else {
	  if(paramResolver.find(std::string(kv[0])) != paramResolver.end()) {
	    switch(paramResolver[kv[0]]) {
	      case HOST:
		con->host  = ( kv[1] );
		con->ssl = (kv[1].substr ( 0, 5 ).compare ( "https" ) == 0 );
		break;
	      case PORT:
		con->port = boost::lexical_cast<int> ( kv[1] );
		break;
	      case ARRAY:
		con->arrayname = ( kv[1] );
		break;
	      case USER:
		con->user = ( kv[1] );
		break;
	      case PASSWORD:
		con->passwd = ( kv[1] );
		break;
	      default:
		Utils::debug("haven't found stuff");
		continue; 
	    }
	  } else {
	    Utils::debug("unused parameter \""+string(kv[0])+ "\" with value \""+string(kv[1])+"\"");
	  }
	}
      }
    }
}
