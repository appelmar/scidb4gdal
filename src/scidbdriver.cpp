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
        //TODO: Improve error handling
        SciDBDataset *poGDS = ( SciDBDataset * ) poDS;

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
            if ( ( nBlockXOff + 1 ) *this->nBlockXSize > poGDS->nRasterXSize ) {
                // This is a bit of a hack...
                size_t dataSize = ( 1 + xmax - xmin ) * ( 1 + ymax - ymin ) * Utils::scidbTypeIdBytes ( _array->attrs[nBand - 1].typeId ); // This is smaller than the block size!
                void *buf = malloc ( dataSize );

                // Write to temporary buffer first
                poGDS->getClient()->getData ( *_array, nBand - 1, buf, xmin, ymin, xmax, ymax ); // GDAL bands start with 1, scidb attribute indexes with 0
                for ( uint32_t i = 0; i < ( 1 + xmax - xmin ); ++i ) {
                    uint8_t *src  = & ( ( uint8_t * ) buf ) [i * ( 1 + ymax - ymin ) * Utils::scidbTypeIdBytes ( _array->attrs[nBand - 1].typeId )];
                    // TODO: In the following call, check whether this->nBlockXSize must be replaced with this->nBlockYSize for unequally sized chunk dimensions
                    uint8_t *dest = & ( ( uint8_t * ) tile.data ) [i * this->nBlockXSize * Utils::scidbTypeIdBytes ( _array->attrs[nBand - 1].typeId )];
                    memcpy ( dest, src, ( 1 + ymax - ymin ) *Utils::scidbTypeIdBytes ( _array->attrs[nBand - 1].typeId ) );
                }
                free ( buf );
            }
            else poGDS->getClient()->getData ( *_array, nBand - 1, tile.data, xmin, ymin, xmax, ymax ); // GDAL bands start with 1, scidb attribute indexes with 0

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




    SciDBDataset::SciDBDataset ( SciDBSpatialArray array, ShimClient *client ) : _array ( array ), _client ( client )
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

        ConnectionPars *pars = ConnectionPars::parseConnectionString ( pszFilename );

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
        ConnectionPars *pars = ConnectionPars::parseConnectionString ( connstr );
        Utils::debug ( "Using connection parameters: host:" + pars->toString() );
	
//TODO remove
// 	std::stringstream sstm;
// 	sstm << "Test 1: " << pars->properties->src_coords[0] << " " << pars->properties->src_coords[1]
// 	   << " " << pars->properties->src_coords[2] << " " << pars->properties->src_coords[3];
// 	string result = sstm.str();
// 	Utils::debug( result );

        // 2. Check validity of parameters
        if ( pars->arrayname == "" ) Utils::error ( "No array specified, currently not supported" );

        // 3. Create shim client
        ShimClient *client = new ShimClient ( pars->host, pars->port, pars->user, pars->passwd, pars->ssl, pars->properties);



        SciDBSpatialArray array;
        // 4. Request array metadata
        if ( client->getArrayDesc ( pars->arrayname, array ) != SUCCESS ) {
            Utils::error ( "Cannot fetch array metadata" );
            return NULL;
        }

        if ( array.dims.size() != 2 ) {
            Utils::error ( "GDAL works with two-dimensional arrays only" );
            return NULL;
        }



        delete pars;


        // Create the dataset

        SciDBDataset *poDS;
        poDS = new SciDBDataset ( array, client );
        return ( poDS );
    }



}
