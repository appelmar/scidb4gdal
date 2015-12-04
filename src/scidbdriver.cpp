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
#include <limits.h>
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

        uint32_t nImgYSize ( 1 + _array->getYDim()->high - _array->getYDim()->low );
        uint32_t nImgXSize ( 1 + _array->getXDim()->high - _array->getXDim()->low );
        nBlockYSize = ( _array->getYDim()->chunksize < nImgYSize ) ? _array->getYDim()->chunksize : nImgYSize;
        nBlockXSize = ( _array->getXDim()->chunksize < nImgXSize ) ? _array->getXDim()->chunksize : nImgXSize;
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
// 	int32_t t_index = poGDS->_client->
// 	
// 	->_query->temp_index;
        uint32_t tileId = TileCache::getBlockId ( nBlockXOff, nBlockYOff, nBand - 1, nBlockXSize, nBlockYSize, poGDS->GetRasterCount() );

	

        ArrayTile tile;
        tile.id = tileId;

        // Check whether chunk is in cache
        if ( poGDS->_cache.has ( tileId ) ) {
            tile = *poGDS->_cache.get ( tileId );
        }
        else {

            int xmin = nBlockXOff * this->nBlockXSize + _array->getXDim()->low;
            int xmax = xmin + this->nBlockXSize - 1;
            if ( xmax > _array->getXDim()->high ) xmax = _array->getXDim()->high;

            int ymin = nBlockYOff * this->nBlockYSize + _array->getYDim()->low;
            int ymax = ymin + this->nBlockYSize - 1;
            if ( ymax > _array->getYDim()->high ) ymax = _array->getYDim()->high;
	    

            // Read  and fetch data
            bool use_subarray = ! ( ( xmin % ( int ) _array->getXDim()->chunksize == 0 ) && ( ymin % ( int ) _array->getYDim()->chunksize == 0 ) );

            tile.size = nBlockXSize * nBlockYSize * Utils::scidbTypeIdBytes ( _array->attrs[nBand - 1].typeId ); // Always allocate full block size
            tile.data = malloc ( tile.size ); // will be freed automatically by cache

            if ( ( nBlockXOff + 1 ) * this->nBlockXSize > poGDS->nRasterXSize ) {
                // This is a bit of a hack...
                size_t dataSize = ( 1 + xmax - xmin ) * ( 1 + ymax - ymin ) * Utils::scidbTypeIdBytes ( _array->attrs[nBand - 1].typeId ); // This is smaller than the block size!
                void *buf = malloc ( dataSize );

		
                // Write to temporary buffer first
		//TODO  t_index is set as zero for compiler testing... this must be changed
                poGDS->getClient()->getData ( *_array, nBand - 1, buf, xmin, ymin, xmax, ymax, use_subarray ); // GDAL bands start with 1, scidb attribute indexes with 0
                for ( uint32_t i = 0; i < ( 1 + ymax - ymin ); ++i ) {
                    uint8_t *src  = & ( ( uint8_t * ) buf ) [i * ( 1 + xmax - xmin ) * Utils::scidbTypeIdBytes ( _array->attrs[nBand - 1].typeId )];
                    uint8_t *dest = & ( ( uint8_t * ) tile.data ) [i * this->nBlockXSize * Utils::scidbTypeIdBytes ( _array->attrs[nBand - 1].typeId )];
                    memcpy ( dest, src, ( 1 + xmax - xmin ) *Utils::scidbTypeIdBytes ( _array->attrs[nBand - 1].typeId ) );
                }
                free ( buf );
            }
            else {
                // This is the most efficient!
                poGDS->getClient()->getData ( *_array, nBand - 1, tile.data, xmin, ymin, xmax, ymax, use_subarray ); // GDAL bands start with 1, scidb attribute indexes with 0
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


    


    SciDBDataset::SciDBDataset ( SciDBSpatialArray &array, ShimClient *client) : _array ( array ), _client ( client )
    {

        this->nRasterXSize = 1 + _array.getXDim()->high - _array.getXDim()->low ;
        this->nRasterYSize = 1 + _array.getYDim()->high - _array.getYDim()->low ;



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
	
	string connstr = pszFilename;
	ConnectionParameters *con_pars;
	CreationParameters *create_pars;
	ParameterParser *pp;
	ShimClient *client;
	SciDBSpatialArray* array;
	
	try {
	  //1. Parse Options and connection string
	  pp = new ParameterParser(connstr, papszOptions, SCIDB_CREATE); //connstr is the file name, papszOptions are the -co options

	  con_pars = &pp->getConnectionParameters();
	  create_pars = &pp->getCreationParameters();
	  
	  delete pp;
	  
	  // 2. Check validity of connection parameters
	  if ( !con_pars->isComplete()) {
	    throw con_pars->error_code;
	  } else {
	    Utils::debug ( "Using connection parameters: " + con_pars->toString() );
	  }

	  //3. create client and set parameters
	  client = new ShimClient (con_pars); //connection parameters are set
	  
	  client->setCreateParameters(*create_pars); //create parameters regarding time
	  
	  //4. Collect metadata from source data and store in SciDBSpatialArray (happens only in GDAL)
	  switch (create_pars->type) {
	    case S_ARRAY:
	      array = new SciDBSpatialArray();
	      break;
	    case ST_ARRAY:
	      array = new SciDBSpatioTemporalArray(create_pars->timestamp,create_pars->dt);
	      break;
	    case ST_SERIES:
	      
	      array = new SciDBSpatioTemporalArray(create_pars->timestamp,create_pars->dt);
	      //TODO recalculate the chunksizes
	      ((SciDBSpatioTemporalArray*)array)->getTDim()->high = INT64_MAX; //set the dimension to maximum = indefinite, will be adapted when creating the temporal array
	      break;
	  }
	  
	  array->name = con_pars->arrayname;
	  
	  copyMetadataToArray(poSrcDS, *array); //copies the information from the source data file into the ST_Array representation
	  Utils::debug("Testing metadata copy: "+array->toString());

	  
// 	  return NULL;
	  //stop here for development test in GDAL
	  
	  // Create array in SciDB using just the image pixel. This image will be integrated into a ST series or into a S array
	  if ( client->createTempArray ( *array ) != SUCCESS ) {
	      throw ERR_CREATE_TEMPARRAY;
	  }
	  
	  // Copy data and write to SciDB
	  uploadImageIntoTempArray(client, *array, poSrcDS,pfnProgress, pProgressData);
	  Utils::debug("Data uploaded into temporary array");
	  string arrayName = array->name;
	  string tempArrayName = array->name + SCIDB4GDAL_ARRAYSUFFIX_TEMP;
	  
	  Utils::debug ( "Persisting temporary array '" + tempArrayName + "'" );  
	  StatusCode persist_res = client->persistTempArray(tempArrayName, array->name);
	    if (persist_res == SUCCESS) {
	      Utils::debug ( "Removing temporary array '" + tempArrayName + "'" );
	      client->removeArray ( tempArrayName ); //deletes the temporary array in SciDB
	      
	      client->updateSRS ( *array );
	      
	      if (create_pars->type == ST_ARRAY || create_pars->type == ST_SERIES) {
		client->updateTRS (*((SciDBSpatioTemporalArray*)array));
	      }
	      
	      Utils::debug ( "Trying to persist array metadata in SciDB system catalog" );
	      // Set Metadata in database
	      {
		  // set general image information
		  for ( DomainMD::iterator it = array->md.begin(); it != array->md.end(); ++it ) {
		      client->setArrayMD ( array->name, it->second, it->first );
		  }

		  // set attribute data for each band
		  for ( int i = 0; i < nBands; ++i ) {
		      for ( DomainMD::iterator it = array->attrs[i].md.begin(); it != array->attrs[i].md.end(); ++it ) {
			  client->setAttributeMD ( array->name, array->attrs[i].name, it->second, it->first );
		      }
		  }
	      }
	    } else if ( persist_res == TEMP_ARRAY_READY_FOR_INTEGRATION) {
	      Utils::debug("Target Array exists, trying to insert source image into it.");
	      //at this point the destination array exists, get metadata for this array
	      string destName = array->name;
	      SciDBSpatialArray* destArray;
	      client->getArrayDesc(destName, destArray);
	      
	      array->name = tempArrayName;
	      //update the temporary array to prepare the insertion process for over
	      client->updateSRS ( *array );
	      if (create_pars->type == ST_ARRAY || create_pars->type == ST_SERIES) {
		client->updateTRS (*((SciDBSpatioTemporalArray*)array));
	      }
	      //temporary array has SRS (and TRS)
	      //insert array into existing array
	      client->insertInto(*array, *destArray);
	      client->removeArray( tempArrayName );
	      array->name = arrayName;
	  } else {
	      throw persist_res;
	  }
	  



	  pfnProgress ( 1.0, NULL, pProgressData );

	  //delete con_pars;
	  delete create_pars ;
	  
// 	  delete client; 
// 	  delete array;
// 	  return ( GDALDataset * ) GDALOpen ( pszFilename, GA_ReadOnly );
	  
	  return new SciDBDataset(*array, client);
	} catch (int e) {
	    //catch exceptions and give information back to the user
	    switch (e) {
	      case ERR_GLOBAL_PARSE:  
		Utils::error ( "This is not a scidb4gdal connection string" );
		break;
	      case ERR_READ_ARRAYUNKNOWN:
		Utils::error ("No array name stated.");
		break;
	      case ERR_CREATE_AUTOCLEANUPFAILED:
		Utils::error ( "Recovery failed, could not delete array. Please do this manually in SciDB" );
		break;
	      case ERR_CREATE_TERMINATEDBYUSER:
		Utils::error ( "TERMINATED BY USER" );
		break;
	      case ERR_CREATE_AUTOCLEANUPSUCCESS:
		Utils::debug ("Progress terminated. Successfully deleted temporary array in SciDB.");
		break;
	      case ERR_CREATE_TEMPARRAY:
		Utils::error ( "Could not create temporary SciDB array" );
		break;
	      default:
		Utils::error("Uncaught error: "+boost::lexical_cast<string>(e));
		break;
	    }
	    delete client; 
	    delete con_pars;
	    delete create_pars ;
	    delete array;
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
        AffineTransform::double2 p0 ( _array.getXDim()->low, _array.getYDim()->low );
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
	ParameterParser p = ParameterParser(pszName, NULL);
	ConnectionParameters c = p.getConnectionParameters();
	Utils::debug(c.toString());
	
	
	if (c.isComplete()) {
	  ShimClient client = ShimClient(&c);
	  client.removeArray(c.arrayname);
	}
	//TODO distinguish types of arrays, e.g. if array is a cube and contains multiple slices then don't remove unless 3rd dim contains just one slice
//         Utils::debug ( "Deleting SciDB arrays from GDAL is currently not allowed..." );
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
	  ShimClient *client = new ShimClient (con_pars);
	  client->setQueryParameters(*query_pars);
	  
	  //create simple array here and cast it later if needed
	  SciDBSpatialArray* array;
	  // 4. Request array metadata
	  if ( client->getArrayDesc ( con_pars->arrayname, array ) != SUCCESS ) {
	      Utils::error ( "Cannot fetch array metadata" );
	      return NULL;
	  }
	  if(!array) {
	      Utils::debug("array changes not afflicting the 'scidbdriver'");
	  }
	  
	  //try to cast the array. if not possible then it is null and the temporal parameter setting is skipped
	  SciDBSpatioTemporalArray* starray_ptr = dynamic_cast<SciDBSpatioTemporalArray*> (array);
	  if (starray_ptr) {
	    Utils::debug("Type Cast OK. Start setting up temporal information");
	      //check if the temporal index was set or if a timestamp was used
	      if (query_pars->hasTemporalIndex) {
		char* t_index_key = new char[1];
		t_index_key[0] = 't';
		if (CSLFindName(poOpenInfo->papszOpenOptions,t_index_key) >= 0) {
		  string value = CSLFetchNameValue(poOpenInfo->papszOpenOptions,t_index_key);
		  Utils::debug("Casting temp index");
		  query_pars->temp_index = boost::lexical_cast<int>(value);
		  Utils::debug("done");
		} 
	      } else {
		//convert date to temporal index
		if (query_pars->timestamp == "") {
		  Utils::debug("No temporal index and no timestamp provided. Using first temporal index instead");
		  query_pars->temp_index = 0;
		  query_pars->hasTemporalIndex = true;
		} else {
		  TPoint time = TPoint(query_pars->timestamp);
		  query_pars->temp_index = starray_ptr->indexAtDatetime(time);
		  query_pars->hasTemporalIndex = true;
		}
	      }
	      
	      //get dimension for time
	      SciDBDimension *dim;
	      dim = &starray_ptr->dims[starray_ptr->getTDimIdx()];

	      if (query_pars->temp_index < dim->low || query_pars->temp_index > dim->high) {
		  Utils::error ( "Specified temporal index out of bounce. Temporal Index stated or calculated: " + boost::lexical_cast<string>(query_pars->temp_index) +
		  ", Lower bound: " + boost::lexical_cast<string>(dim->low) + " (" + starray_ptr->datetimeAtIndex(dim->low).toStringISO() + "), " +
		    "Upper bound: " + boost::lexical_cast<string>(dim->high) + " (" + starray_ptr->datetimeAtIndex(dim->high).toStringISO() + ")"
		  );
		  return NULL;
	      }
	  }


	  delete con_pars;


	  // Create the dataset

	  SciDBDataset *poDS;
	  poDS = new SciDBDataset ( *array, client);
	  return ( poDS );
	} catch (int e) {
	  switch (e) {
	    case ERR_GLOBAL_PARSE:  
	      Utils::error ( "This is not a scidb4gdal connection string" );
	      break;
	    case ERR_READ_ARRAYUNKNOWN:
	      Utils::error ("No array name stated.");
	      break;
	  }
	  
	  return NULL;
	}
    }
    
    
    void SciDBDataset::copyMetadataToArray(GDALDataset* poSrcDS, SciDBSpatialArray& array)
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

	  //vector<SciDBDimension> dims;

	  // Derive chunksize from attribute sizes
	  //TODO adapt for t dim as well
	  uint32_t blocksize = Utils::nextPow2 ( ( uint32_t ) sqrt ( ( ( ( double ) ( SCIDB4GEO_DEFAULT_CHUNKSIZE_MB * 1024 * 1024 ) ) / ( ( double ) ( pixelsize ) ) ) ) ) / 2;
	  stringstream ss;
	  ss << "Using chunksize " << blocksize << "x" << blocksize << " --> " << ( int ) ( ( ( double ) ( blocksize * blocksize * pixelsize ) ) / ( ( double ) 1024.0 ) ) << " kilobytes";
	  Utils::debug ( ss.str() );
	  
	  SciDBDimension* dimx;
	  SciDBDimension* dimy;
	  
	  
	  dimx =  array.getXDim();
	  dimx->high = poSrcDS->GetRasterXSize() - 1;
	  dimx->chunksize = blocksize;

	  dimy = array.getYDim();
	  dimy->high = poSrcDS->GetRasterYSize() - 1;
	  dimy->chunksize = blocksize;

	  
	  //extracting SRS information from source
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
	  }


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

    void SciDBDataset::uploadImageIntoTempArray(ShimClient *client, SciDBSpatialArray& array, GDALDataset *poSrcDS, GDALProgressFunc pfnProgress, void* pProgressData)
    {
	int  nBands = poSrcDS->GetRasterCount();
	int  nXSize = poSrcDS->GetRasterXSize();
        int  nYSize = poSrcDS->GetRasterYSize();
	
	size_t pixelSize = 0;
	for ( uint32_t i = 0; i < array.attrs.size(); ++i ) pixelSize += Utils::scidbTypeIdBytes ( array.attrs[i].typeId );
	
	size_t totalSize = pixelSize *  array.getXDim()->chunksize * array.getYDim()->chunksize;
	
	uint8_t *bandInterleavedChunk = ( uint8_t * ) malloc ( totalSize ); // This is a byte array

	
	uint32_t nBlockX = ( uint32_t ) ( nXSize / array.getXDim()->chunksize );
	if ( nXSize % array.getXDim()->chunksize != 0 ) ++nBlockX;

	uint32_t nBlockY = ( uint32_t ) ( nYSize / array.getYDim()->chunksize );
	if ( nYSize % array.getYDim()->chunksize != 0 ) ++nBlockY;



	//upload array in chunks
	for ( uint32_t bx = 0; bx < nBlockX; ++bx ) {
	    for ( uint32_t by = 0; by < nBlockY; ++by ) {

		size_t bandOffset = 0; // sum of bytes taken by previous bands, will be updated in loop over bands



		if ( !pfnProgress ( ( ( double ) ( bx * nBlockY + by ) ) / ( ( double ) ( nBlockX * nBlockY ) ), NULL, pProgressData ) ) {
		    Utils::debug ( "Interruption by user requested, trying to clean up" );
		    // Clean up intermediate arrays
		    client->removeArray ( array.name );
		    throw ERR_CREATE_TERMINATEDBYUSER;
		}

		// 1. Compute array bounds from block offsets
		int xmin = bx *  array.getXDim()->chunksize + array.getXDim()->low;
		int xmax = xmin +  array.getXDim()->chunksize - 1;
		if ( xmax > array.getXDim()->high ) xmax = array.getXDim()->high;
		if ( xmin > array.getXDim()->high ) xmin = array.getXDim()->high;

		int ymin = by *  array.getYDim()->chunksize + array.getYDim()->low;
		int ymax = ymin + array.getYDim()->chunksize - 1;
		if ( ymax > array.getYDim()->high ) ymax = array.getYDim()->high;
		if ( ymin > array.getYDim()->high ) ymin = array.getYDim()->high;

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
		      throw ERR_CREATE_AUTOCLEANUPFAILED;
		    } else {
		      throw ERR_CREATE_AUTOCLEANUPSUCCESS;
		    } 
		}
	    }
	}



	free ( bandInterleavedChunk );
    }

    
    
    
}
