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


#include "scidbdriver.h"
#include "shimclient.h"

#include "utils.h"
#include <iomanip>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/assign.hpp>
#include <limits>
#include "shim_client_structs.h"
#include "scidb_structs.h"
#include "parameter_parser.h"

CPL_C_START
void GDALRegister_SciDB ( void );
CPL_C_END

/**
 * GDAL driver registration function
 * Links the specific functions for Open, Identify, Delete and Create Copy on a GDALDriver stub.
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

        uint32_t nImgYSize ( 1 + _array->getYDim().high - _array->getYDim().low );
        uint32_t nImgXSize ( 1 + _array->getXDim().high - _array->getXDim().low );
        nBlockYSize = ( _array->getYDim().chunksize < nImgYSize ) ? _array->getYDim().chunksize : nImgYSize;
        nBlockXSize = ( _array->getXDim().chunksize < nImgXSize ) ? _array->getXDim().chunksize : nImgXSize;
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



    CPLErr SciDBRasterBand::IReadBlock ( int nBlockXOff, int nBlockYOff, void *pImage )
    {

        //TODO: Improve error handling
        SciDBDataset *poGDS = ( SciDBDataset * ) poDS;

	//parse the temporal index from query string...
	int32_t t_index = poGDS->_query->temp_index;
        uint32_t tileId = TileCache::getBlockId ( nBlockXOff, nBlockYOff, nBand - 1, nBlockXSize, nBlockYSize, poGDS->GetRasterCount() );

	

        ArrayTile tile;
        tile.id = tileId;

        // Check whether chunk is in cache
        if ( poGDS->_cache.has ( tileId ) ) {
            tile = *poGDS->_cache.get ( tileId );
        }
        else {

            int xmin = nBlockXOff * this->nBlockXSize + _array->getXDim().low;
            int xmax = xmin + this->nBlockXSize - 1;
            if ( xmax > _array->getXDim().high ) xmax = _array->getXDim().high;

            int ymin = nBlockYOff * this->nBlockYSize + _array->getYDim().low;
            int ymax = ymin + this->nBlockYSize - 1;
            if ( ymax > _array->getYDim().high ) ymax = _array->getYDim().high;
	    

            // Read  and fetch data
            bool use_subarray = ! ( ( xmin % ( int ) _array->getXDim().chunksize == 0 ) && ( ymin % ( int ) _array->getYDim().chunksize == 0 ) );

            tile.size = nBlockXSize * nBlockYSize * Utils::scidbTypeIdBytes ( _array->attrs[nBand - 1].typeId ); // Always allocate full block size
            tile.data = malloc ( tile.size ); // will be freed automatically by cache



//      // TODO: Check which chunks are overlapped
//      int xchunk_1 = floor((double)xmin / (double)_array->getXDim().chunksize);
//      int xchunk_2 = floor((double)xmax / (double)_array->getXDim().chunksize);
//      int ychunk_1 = floor((double)ymin / (double)_array->getYDim().chunksize);
//      int ychunk_2 = floor((double)ymax / (double)_array->getYDim().chunksize);
//
//      int xchunk_n = xchunk_2 - xchunk_1 + 1;  // Number of visited chunks in x direction
//      int ychunk_n = ychunk_2 - ychunk_1 + 1; // Number of visited chunks in y direction
//
//      stringstream sd;
//      sd << "(" << xmin << "," <<  xmax << "," << ymin << "," << ymax << ")(" <<  xchunk_1 << "," << xchunk_2 << "," << ychunk_1 << "," << ychunk_2 << ")";
//      Utils::debug(sd.str());


//      if (xchunk_n != 1 || ychunk_n != 1) {
//        /* Requested block covers multiple SciDB chunks --> result is only "row-major" in individual chunks
//         * Need to iterate over all covered chunks */
//        stringstream ss;
//        ss << "Block request covers " << xchunk_n * ychunk_n << " SciDB chunks -> Fetch data chunk-wise (might be slow).";
//        Utils::debug(ss.str());
//        // TODO: Switch inner and outer loop depending on order of x and y in SciDB array, is that neccessary?
//        size_t dataSize = ( 1 + xmax - xmin ) * ( 1 + ymax - ymin ) * Utils::scidbTypeIdBytes ( _array->attrs[nBand - 1].typeId ); // This might be smaller than the block size!
//        void *buf = malloc ( dataSize );
//        int iy=0;
//        for (int icy=0; icy<ychunk_n; ++icy)
//        {
//      int ix=0;
//      int ychunk_low  = MAX(ymin,(ychunk_1+icy)   * (int)_array->getYDim().chunksize);
//      int ychunk_high = MIN(ymax,(ychunk_1+icy+1) * (int)_array->getYDim().chunksize - 1);
//      for (int icx=0; icx<xchunk_n; ++icx)
//      {
//        int xchunk_low  = MAX(xmin,(xchunk_1+icx)   * (int)_array->getXDim().chunksize);
//        int xchunk_high = MIN(xmax,(xchunk_1+icx+1) * (int)_array->getXDim().chunksize - 1);
//
//        stringstream ss;
//        ss << "Fetching data " << "[(" << xchunk_low << "," <<  xchunk_high << "),(" << ychunk_low << "," << ychunk_high << ")]";
//        Utils::debug(ss.str());
//
//
//        poGDS->getClient()->getData ( *_array, nBand - 1, buf, xchunk_low, ychunk_low, xchunk_high, ychunk_high );
//
//        ss.str("");
//        ss << "ix,iy" << "[(" << ix << "," <<  iy << ")], attribute " << nBand - 1 << " expecting " <<  ( 1 + xchunk_high - xchunk_low ) *  ( 1 + xchunk_high - xchunk_low ) << "cells";
//        Utils::debug(ss.str());
//
//        for ( uint32_t i = 0; i < ( 1 + ychunk_high - ychunk_low ); ++i ) {
//           /* ss.str("");
//            ss << " Writing " << ( 1 + xchunk_high - xchunk_low ) << " elements for attribute " << nBand - 1 << " from buf[" << i <<  ",0] to tile.data[" << (i+iy)  <<  "," << ix << "]";
//            Utils::debug(ss.str());
//           */
//           uint8_t *src  = & ( ( uint8_t * ) buf ) [i * ( 1 + xchunk_high - xchunk_low ) * Utils::scidbTypeIdBytes ( _array->attrs[nBand - 1].typeId )];
//           uint8_t *dest = & ( ( uint8_t * ) tile.data ) [((i+iy) * this->nBlockXSize + ix)* Utils::scidbTypeIdBytes ( _array->attrs[nBand - 1].typeId )];
//           memcpy ( dest, src, ( 1 + xchunk_high - xchunk_low ) *Utils::scidbTypeIdBytes ( _array->attrs[nBand - 1].typeId ) );
//        }
//        ix += 1 + xchunk_high - xchunk_low;
//
//
//      }
//      iy+= 1 + ychunk_high - ychunk_low;
//        }
//        free ( buf );
//
//      }




            if ( ( nBlockXOff + 1 ) * this->nBlockXSize > poGDS->nRasterXSize ) {
                // This is a bit of a hack...
                size_t dataSize = ( 1 + xmax - xmin ) * ( 1 + ymax - ymin ) * Utils::scidbTypeIdBytes ( _array->attrs[nBand - 1].typeId ); // This is smaller than the block size!
                void *buf = malloc ( dataSize );

		
                // Write to temporary buffer first
		//TODO  t_index is set as zero for compiler testing... this must be changed
                poGDS->getClient()->getData ( *_array, nBand - 1, buf, xmin, ymin, xmax, ymax, t_index, use_subarray ); // GDAL bands start with 1, scidb attribute indexes with 0
                for ( uint32_t i = 0; i < ( 1 + ymax - ymin ); ++i ) {
                    uint8_t *src  = & ( ( uint8_t * ) buf ) [i * ( 1 + xmax - xmin ) * Utils::scidbTypeIdBytes ( _array->attrs[nBand - 1].typeId )];
                    uint8_t *dest = & ( ( uint8_t * ) tile.data ) [i * this->nBlockXSize * Utils::scidbTypeIdBytes ( _array->attrs[nBand - 1].typeId )];
                    memcpy ( dest, src, ( 1 + xmax - xmin ) *Utils::scidbTypeIdBytes ( _array->attrs[nBand - 1].typeId ) );
                }
                free ( buf );
            }
            else {
                // This is the most efficient!
                poGDS->getClient()->getData ( *_array, nBand - 1, tile.data, xmin, ymin, xmax, ymax, t_index, use_subarray ); // GDAL bands start with 1, scidb attribute indexes with 0
            }
        }


        poGDS->_cache.add ( tile ); // Add to cache




        // Copy from tile.data to pImage
        memcpy ( pImage, tile.data, tile.size );

        return CE_None;
    }




    double SciDBRasterBand::GetNoDataValue ( int *pbSuccess )
    {
        string key = "NODATA";
        MD md = _array->attrs[nBand - 1].md[""]; // TODO: Add domain
        if ( md.find ( key ) == md.end() ) {
            if ( pbSuccess != NULL ) *pbSuccess = false;
            return Utils::defaultNoDataGDAL ( this->GetRasterDataType() );
        }
        if ( pbSuccess != NULL ) *pbSuccess = true;
        return boost::lexical_cast<double> ( md[key] );
    }


    double SciDBRasterBand::GetMaximum ( int *pbSuccess )
    {
        string key = "MAX";
        MD md = _array->attrs[nBand - 1].md[""]; // TODO: Add domain
        if ( md.find ( key ) == md.end() ) {
            if ( pbSuccess != NULL ) *pbSuccess = false;
            return DBL_MAX;

        }
        if ( pbSuccess != NULL ) *pbSuccess = true;
        return boost::lexical_cast<double> ( md[key] );
    }


    double SciDBRasterBand::GetMinimum ( int *pbSuccess )
    {
        string key = "MIN";
        MD md = _array->attrs[nBand - 1].md[""]; // TODO: Add domain
        if ( md.find ( key ) == md.end() ) {
            if ( pbSuccess != NULL ) *pbSuccess = false;
            return DBL_MAX;

        }
        if ( pbSuccess != NULL ) *pbSuccess = true;
        return boost::lexical_cast<double> ( md[key] );

    }


    double SciDBRasterBand::GetOffset ( int *pbSuccess )
    {
        string key = "OFFSET";
        MD md = _array->attrs[nBand - 1].md[""]; // TODO: Add domain
        if ( md.find ( key ) == md.end() ) {
            if ( pbSuccess != NULL ) *pbSuccess = false;
            return 0;

        }
        if ( pbSuccess != NULL ) *pbSuccess = true;
        return boost::lexical_cast<double> ( md[key] );
    }

    double SciDBRasterBand::GetScale ( int *pbSuccess )
    {
        string key = "SCALE";
        MD md = _array->attrs[nBand - 1].md[""]; // TODO: Add domain
        if ( md.find ( key ) == md.end() ) {

            if ( pbSuccess != NULL ) *pbSuccess = false;
            return 1;

        }
        if ( pbSuccess != NULL ) *pbSuccess = true;
        return boost::lexical_cast<double> ( md[key] );
    }


    const char *SciDBRasterBand::GetUnitType()
    {
        string key = "UNIT";
        MD md = _array->attrs[nBand - 1].md[""]; // TODO: Add domain
        if ( md.find ( key ) == md.end() ) {
            return "";
        }
        return md[key].c_str();
    }


    


    SciDBDataset::SciDBDataset ( SciDBSpatialArray array, ShimClient *client, QueryParameters *props ) : _array ( array ), _client ( client ), _query(props)
    {

        this->nRasterXSize = 1 + _array.getXDim().high - _array.getXDim().low ;
        this->nRasterYSize = 1 + _array.getYDim().high - _array.getYDim().low ;



        // Create GDAL Bands
        for ( uint32_t i = 0; i < _array.attrs.size(); ++i )
            this->SetBand ( i + 1, new SciDBRasterBand ( this, &_array, i ) );


        this->SetDescription ( _array.toString().c_str() );

        // Set Metadata
        //this->SetMetadataItem("NODATA_VALUES", "0", "");


        // TODO: Overviews?
        //poDS->oOvManager.Initialize ( poDS, poOpenInfo->pszFilename );



    }



    GDALDataset *SciDBDataset::CreateCopy ( const char *pszFilename, GDALDataset *poSrcDS, int bStrict, char **papszOptions, GDALProgressFunc pfnProgress, void *pProgressData )
    {
        int  nBands = poSrcDS->GetRasterCount();
        int  nXSize = poSrcDS->GetRasterXSize();
        int  nYSize = poSrcDS->GetRasterYSize();
	
	
	try {
	  string connstr = pszFilename;
	  ParameterParser pp = ParameterParser(connstr, papszOptions, SCIDB_CREATE); //connstr is the file name, papszOptions are the -co options
	  
	  //1. Parse Options and connection string to create SHIMClient
	  ConnectionParameters *con_pars;
	  CreationParameters *create_pars;
	  con_pars = &pp.getConnectionParameters();
	  create_pars = &pp.getCreationParameters();

	  ShimClient *client = new ShimClient (con_pars);
	  client->setCreateParameters(*create_pars);
	  
	  // 2. Check validity of connection parameters
	  if ( !con_pars->isComplete()) {
	    throw con_pars->error_code;
	  } else {
	    Utils::debug ( "Using connection parameters: " + con_pars->toString() );
	  }

	  
	  // Create array metadata structure
	  //TODO decide if we need to create a SArray or a STArray
	  SciDBSpatialArray array;
	  
	  
	  array.name = con_pars->arrayname;

	  copyMetadataToSciDBArray(poSrcDS, array);
	  Utils::debug("Testing metadata copy: "+array.toString());
	  
	  // Create array in SciDB
	  if ( client->createTempArray ( array ) != SUCCESS ) {
	      Utils::error ( "Could not create temporary SciDB array" );
	      return NULL;
	  }
	  // Copy data and write to SciDB
	  size_t pixelSize = 0;
	  for ( uint32_t i = 0; i < array.attrs.size(); ++i ) pixelSize += Utils::scidbTypeIdBytes ( array.attrs[i].typeId );
	  size_t totalSize = pixelSize *  array.getXDim().chunksize * array.getYDim().chunksize;
	  
	  uint8_t *bandInterleavedChunk = ( uint8_t * ) malloc ( totalSize ); // This is a byte array

	  
	  uint32_t nBlockX = ( uint32_t ) ( nXSize / array.getXDim().chunksize );
	  if ( nXSize % array.getXDim().chunksize != 0 ) ++nBlockX;

	  uint32_t nBlockY = ( uint32_t ) ( nYSize / array.getYDim().chunksize );
	  if ( nYSize % array.getYDim().chunksize != 0 ) ++nBlockY;




	  for ( uint32_t bx = 0; bx < nBlockX; ++bx ) {
	      for ( uint32_t by = 0; by < nBlockY; ++by ) {

		  size_t bandOffset = 0; // sum of bytes taken by previous bands, will be updated in loop over bands



		  if ( !pfnProgress ( ( ( double ) ( bx * nBlockY + by ) ) / ( ( double ) ( nBlockX * nBlockY ) ), NULL, pProgressData ) ) {
		      Utils::debug ( "Interruption by user requested, trying to clean up" );
		      // Clean up intermediate arrays
		      client->removeArray ( array.name );
		      Utils::error ( "TERMINATED BY USER" );
		      return NULL;
		  }

		  // 1. Compute array bounds from block offsets
		  int xmin = bx *  array.getXDim().chunksize + array.getXDim().low;
		  int xmax = xmin +  array.getXDim().chunksize - 1;
		  if ( xmax > array.getXDim().high ) xmax = array.getXDim().high;
		  if ( xmin > array.getXDim().high ) xmin = array.getXDim().high;

		  int ymin = by *  array.getYDim().chunksize + array.getYDim().low;
		  int ymax = ymin + array.getYDim().chunksize - 1;
		  if ( ymax > array.getYDim().high ) ymax = array.getYDim().high;
		  if ( ymin > array.getYDim().high ) ymin = array.getYDim().high;

		  // We assume reading whole blocks of individual bands first is more efficient than reading single band pixels subsequently
		  for ( uint16_t iBand = 0; iBand < nBands; ++iBand ) {
		      void *blockBandBuf =  malloc ( ( 1 + xmax - xmin ) * ( 1 + ymax - ymin ) * Utils::scidbTypeIdBytes ( array.attrs[iBand].typeId ) );

		      // Using nPixelSpace and nLineSpace arguments could maybe automatically write to bandInterleavedChunk properly
		      GDALRasterBand *poBand = poSrcDS->GetRasterBand ( iBand + 1 );
		      //poBand->RasterIO ( GF_Read, ymin, xmin, 1 + ymax - ymin, 1 + xmax - xmin, ( void * ) blockBandBuf,  1 + ymax - ymin, 1 + xmax - xmin, Utils::scidbTypeIdToGDALType ( array.attrs[iBand].typeId ), 0, 0 );
		      poBand->RasterIO ( GF_Read, xmin, ymin, 1 + xmax - xmin, 1 + ymax - ymin, ( void * ) blockBandBuf,  1 + xmax - xmin, 1 + ymax - ymin, Utils::scidbTypeIdToGDALType ( array.attrs[iBand].typeId ), 0, 0, NULL );


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
		      Utils::debug ( "Copying data to SciDB array failed, trying to recover initial state..." );
		      if ( client->removeArray ( array.name ) != SUCCESS ) {
			throw SCIDB_AUTOCLEANUP_FAILED;
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



	  Utils::debug ( "Trying to persist array metadata in SciDB system catalog" );

	  // Set Metadata in databse
	  for ( DomainMD::iterator it = array.md.begin(); it != array.md.end(); ++it ) {
	      client->setArrayMD ( array.name, it->second, it->first );
	  }



	  for ( int i = 0; i < nBands; ++i ) {
	      for ( DomainMD::iterator it = array.attrs[i].md.begin(); it != array.attrs[i].md.end(); ++it ) {
		  client->setAttributeMD ( array.name, array.attrs[i].name, it->second, it->first );
	      }
	  }





	  pfnProgress ( 1.0, NULL, pProgressData );




	  delete client;

	  return ( GDALDataset * ) GDALOpen ( pszFilename, GA_ReadOnly );
	} catch (int e) {
	    switch (e) {
	      case SCIDB_PARSING_ERROR:  
		Utils::error ( "This is not a scidb4gdal connection string" );
		break;
	      case ERROR_NO_ARRAYNAME:
		Utils::error ("No array name stated.");
		break;
	      case SCIDB_AUTOCLEANUP_FAILED:
		Utils::error ( "Recovery failed, could not delete array. Please do this manually in SciDB" );
		break;
	    }
	}
    }



    void SciDBDataset::gdalMDtoMap ( char **strlist, map<string, string> &kv )
    {
        kv.clear();

        int i = 0;
        char *it = strlist[0];
        while ( it != NULL ) {
            string s ( strlist[i] );
            size_t p = string::npos;
            p = ( s.find ( '=' ) != string::npos ) ? s.find ( '=' ) : p;
            p = ( s.find ( ':' ) != string::npos ) ? s.find ( ':' ) : p;
            if ( p != string::npos ) {
                kv.insert ( pair<string, string> ( s.substr ( 0, p ), s.substr ( p + 1, s.length() - p - 1 ) ) );
            }
            it = strlist[++i];

        }
    }

    char **SciDBDataset::mapToGdalMD ( map< string, string > &kv )
    {
        CPLStringList out;
        for ( map<string, string>::iterator it = kv.begin(); it != kv.end(); ++it ) {
            out.AddNameValue ( it->first.c_str(), it->second.c_str() );
        }
        return out.List();
    }




    SciDBDataset::~SciDBDataset()
    {
        FlushCache();
        delete _client;
    }
    

    CPLErr SciDBDataset::GetGeoTransform ( double *padfTransform )
    {
        // If array dimensions do not start at 0, change transformation parameters accordingly
        AffineTransform::double2 p0 ( _array.getXDim().low, _array.getYDim().low );
        _array.affineTransform.f ( p0 );
        // padfTransform[0] = _array.affineTransform._x0;
        padfTransform[0] = p0.x;
        padfTransform[1] = _array.affineTransform._a11;
        padfTransform[2] = _array.affineTransform._a12;
        //padfTransform[3] = _array.affineTransform._y0;
        padfTransform[3] = p0.y;
        padfTransform[4] = _array.affineTransform._a21;
        padfTransform[5] = _array.affineTransform._a22;
        return CE_None;
    }


    const char *SciDBDataset::GetProjectionRef()
    {
        return _array.srtext.c_str();
    }
    


    char **SciDBDataset::GetMetadata ( const char *pszDomain )
    {
        map <string, string> kv;
        if ( pszDomain == NULL ) {
            _client->getArrayMD ( kv, _array.name, "" );
        }
        else {
            _client->getArrayMD ( kv, _array.name, pszDomain );
        }
        return mapToGdalMD ( kv );
    }


    const char *SciDBDataset::GetMetadataItem ( const char *pszName, const char *pszDomain )
    {
        map <string, string> kv;
        if ( pszDomain == NULL ) {
            _client->getArrayMD ( kv, _array.name, "" );
        }
        else {
            _client->getArrayMD ( kv, _array.name, pszDomain );
        }
        if ( kv.find ( pszName ) == kv.end() ) return NULL;
        return kv.find ( pszName )->second.c_str();

    }

//     CPLErr SciDBDataset::SetMetadata ( char   **papszMetadataIn, const char *pszDomain )
//     {
//         map <string, string> kv;
//         gdalMDtoMap ( papszMetadataIn, kv );
//         _client->setArrayMD ( _array.name, kv, pszDomain );
//         return CE_None;
//     }
//
//     CPLErr SciDBDataset::SetMetadataItem ( const char *pszName, const char *pszValue, const char *pszDomain )
//     {
//         map <string, string> kv;
//         kv.insert ( pair<string, string> ( pszName, pszValue ) );
//         _client->setArrayMD ( _array.name, kv, pszDomain );
//         return CE_None;
//     }

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
	// Check whether dataset matches SciDB dataset
        if ( !Identify ( poOpenInfo ) ) {
            return NULL;
        }
        
        // Do not allow update access
        if ( poOpenInfo->eAccess == GA_Update ) {
            CPLError ( CE_Failure, CPLE_NotSupported, "scidb4gdal currently does not support update access to existing arrays.\n" );
            return NULL;
        }
        
        string connstr = poOpenInfo->pszFilename;
	
	//feed the parser with the filename and the opening options
	try {
	  ParameterParser pp = ParameterParser(connstr, poOpenInfo->papszOpenOptions);
	  

	  // 1. parse connection string and extract the following values
	  QueryParameters *query_pars;
	  ConnectionParameters *con_pars;

	  query_pars = &pp.getQueryParameters();
	  con_pars = &pp.getConnectionParameters();
	    
	  // 2. Check validity of connection parameters
	  if ( !con_pars->isComplete()) {
	    throw con_pars->error_code;
	  } else {
	    Utils::debug ( "Using connection parameters: " + con_pars->toString() );
	  }

	  // 3. Create shim client
// 	  ShimClient *client = new ShimClient ( con_pars->host, con_pars->port, con_pars->user, con_pars->passwd, con_pars->ssl);
	  ShimClient *client = new ShimClient (con_pars);
	  
	  SciDBSpatioTemporalArray array;
	  // 4. Request array metadata
	  if ( client->getArrayDesc ( con_pars->arrayname, array ) != SUCCESS ) {
	      Utils::error ( "Cannot fetch array metadata" );
	      return NULL;
	  }
	  
		  //check if the temporal index was set or if a timestamp was used
	  if (query_pars->hasTemporalIndex) {
	    char* t_index_key = new char[1];
	    t_index_key[0] = 't';
	    if (CSLFindName(poOpenInfo->papszOpenOptions,t_index_key) >= 0) {
	      string value = CSLFetchNameValue(poOpenInfo->papszOpenOptions,t_index_key);
	      query_pars->temp_index = boost::lexical_cast<int>(value);
	    } 
	  } else {

	    //convert date to temporal index
	    TPoint time = TPoint(query_pars->timestamp);
	    query_pars->temp_index = array.indexAtDatetime(time);
	    query_pars->hasTemporalIndex = true;
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

		if (query_pars->temp_index < dim->low || query_pars->temp_index > dim->high) {
		    Utils::error ( "Specified temporal index out of bounce. Temporal Index stated or calculated: " + boost::lexical_cast<string>(query_pars->temp_index) +
		    ", Lower bound: " + boost::lexical_cast<string>(dim->low) + " (" + array.datetimeAtIndex(dim->low).toStringISO() + "), " +
		      "Upper bound: " + boost::lexical_cast<string>(dim->high) + " (" + array.datetimeAtIndex(dim->high).toStringISO() + ")"
		    );
		    return NULL;
		}
	    }
	  }


	  delete con_pars;


	  // Create the dataset

	  SciDBDataset *poDS;
	  poDS = new SciDBDataset ( array, client, query_pars );
	  return ( poDS );
	} catch (int e) {
	  switch (e) {
	    case SCIDB_PARSING_ERROR:  
	      Utils::error ( "This is not a scidb4gdal connection string" );
	      break;
	    case ERROR_NO_ARRAYNAME:
	      Utils::error ("No array name stated.");
	      break;
	  }
	  
	  return NULL;
	}
    }
    
    
    void SciDBDataset::copyMetadataToSciDBArray(GDALDataset* poSrcDS, SciDBSpatialArray& array)
    {
      // Attributes
	  size_t pixelsize = 0; // size in bytes of one pixel, i.e. sum of attribute sizes
	  vector<SciDBAttribute> attrs;
	  for ( int i = 0; i < poSrcDS->GetRasterCount(); ++i ) {
	      SciDBAttribute a;
	      a.nullable = false; // TODO
	      a.typeId = Utils::gdalTypeToSciDBTypeId ( poSrcDS->GetRasterBand ( i + 1 )->GetRasterDataType() ); // All attribtues must have the same data type
	      stringstream aname;
	      pixelsize += Utils::gdalTypeBytes ( poSrcDS->GetRasterBand ( i + 1 )->GetRasterDataType() );
	      aname << "band" << i + 1;
	      //aname << poSrcDS->GetRasterBand ( i + 1 )->GetDescription();
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
	  dimx.high = poSrcDS->GetRasterXSize() - 1;
	  dimx.name = SCIDB4GDAL_DEFAULT_XDIMNAME;
	  // dimx.chunksize = SCIDB4GDAL_DEFAULT_BLOCKSIZE_X;
	  dimx.chunksize = blocksize;
	  dimx.typeId = "int64";

	  SciDBDimension dimy;
	  dimy.low = 0;
	  dimy.high = poSrcDS->GetRasterYSize() - 1;
	  dimy.name = SCIDB4GDAL_DEFAULT_YDIMNAME;
	  //dimy.chunksize = SCIDB4GDAL_DEFAULT_BLOCKSIZE_Y;
	  dimy.chunksize = blocksize;
	  dimy.typeId = "int64";


	  // This order is more efficient as it fits row major image format (does not require transposing during downloads)
	  dims.push_back ( dimy );
	  dims.push_back ( dimx );

	  array.dims = dims;




	  Utils::debug ( "Reading metadata from source dataset" );

	  // Get Metadata // TODO: Add domains
	  map<string, string> kv;
	  gdalMDtoMap ( poSrcDS->GetMetadata(), kv );
	  array.md.insert ( pair<string, MD> ( "", kv ) );


	  for ( int i = 0; i < poSrcDS->GetRasterCount(); ++i ) {
	      map<string, string> kv2;
	      //  gdalMDtoMap(  poSrcDS->GetRasterBand ( i + 1 )->GetMetadata(), kv2);
	      stringstream s;
	      double v;
	      int *ret = NULL;
	      v = poSrcDS->GetRasterBand ( i + 1 )->GetNoDataValue ( ret );
	      if ( ret != NULL && *ret != 0 ) {
		  s <<  std::setprecision ( numeric_limits< double >::digits10 ) << v;
		  kv2.insert ( pair<string, string> ( "NODATA", s.str() ) );
	      }
	      s.str ( "" );
	      v = poSrcDS->GetRasterBand ( i + 1 )->GetOffset ( ret ) ;
	      if ( ret != NULL && *ret != 0 ) {
		  s <<  std::setprecision ( numeric_limits< double >::digits10 ) << v;
		  kv2.insert ( pair<string, string> ( "OFFSET", s.str() ) );
		  s.str ( "" );
	      }
	      s.str ( "" );
	      v = poSrcDS->GetRasterBand ( i + 1 )->GetScale ( ret ) ;
	      if ( ret != NULL && *ret != 0 ) {
		  s <<  std::setprecision ( numeric_limits< double >::digits10 ) << v;
		  kv2.insert ( pair<string, string> ( "SCALE", s.str() ) );
		  s.str ( "" );
	      }
	      s.str ( "" );
	      v = poSrcDS->GetRasterBand ( i + 1 )->GetMinimum ( ret );
	      if ( ret != NULL && *ret != 0 ) {
		  s <<  std::setprecision ( numeric_limits< double >::digits10 ) << v;
		  kv2.insert ( pair<string, string> ( "MIN", s.str() ) );
	      }
	      s.str ( "" );
	      v = poSrcDS->GetRasterBand ( i + 1 )->GetMaximum ( ret );
	      if ( ret != NULL && *ret != 0 ) {
		  s <<  std::setprecision ( numeric_limits< double >::digits10 ) << v;
		  kv2.insert ( pair<string, string> ( "MAX", s.str() ) );
	      }
	      s.str ( "" );
	      s <<  std::setprecision ( numeric_limits< double >::digits10 ) << poSrcDS->GetRasterBand ( i + 1 )->GetUnitType() ;
	      kv2.insert ( pair<string, string> ( "UNIT", s.str() ) );


	      array.attrs[i].md.insert ( pair<string, MD> ( "", kv2 ) );
	  }
    }

    
    
    
    
}
