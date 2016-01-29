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

#ifndef SCIDB_DRIVER_H
#define SCIDB_DRIVER_H

#include <iostream>
#include "utils.h"
#include "shimclient.h"
#include "tilecache.h"
#include "TemporalReference.h"



namespace scidb4gdal
{
    using namespace scidb4geo;

    class SciDBRasterBand;
    class SciDBDataset;


    /**
     * @brief GDALDataset subclass implementing core GDAL functionality
     * 
     * The SciDBDataset is a GDAL representation of a SciDB array. The data set holds information about the arrays features like dimensions,
     * attributes and their statistics, as well as the ShimClient that is used to communicate with the Shim web client and also a TileCache that
     * caches the chunked data, while downloading.
     * The SciDBDataset is a subclass of the GDALDataset and it implements GDALs functions to Open, Identify and CreateCopy.
     * 
     * @author Marius Appel, IfGI Muenster
     * @author Florian Lahn, IfGI Muenster
     */
    class SciDBDataset : public GDALDataset
    {
        friend class SciDBRasterBand;

    private:
	/**
	 * Spatial array or a spatio-temporal array to store the meta data about the SciDB representation of the array.
	 */
        SciDBSpatialArray& _array; 
	/**
	 * pointer to Shim client class that is used to interact with the SciDB
	 */
        ShimClient *_client;
	/**
	 * the tile cache used for downloading chunked array data and to temporarily storing it before writing to a file
	 */
        TileCache _cache;
	
	/**
	 * The key-value pairs of the meta data retreived from the source (either a file when creating or from SciDB when querying)
	 */
	char** papszMetadata;

    public:

      /**
       * @brief The constructor of a SciDBDataset
       * 
       * This constructor needs a spatial array representation and a SHIM client representation to create a data set. Those classes will be created during
       * different steps on the SciDBDriver (namely GDALDataset::Open and GDAL::CreateCopy). Those information are passed on to the data set in order to allow
       * further processing steps like downloading an existing array,
       * 
       * @param array A scidb4gdal::SciDBSpatialArray that holds meta data of the SciDBArray.
       * @param client The scidb4gdal::ShimClient that holds the connection information to the SHIM web client.
       */
        SciDBDataset ( SciDBSpatialArray &array, ShimClient *client);

        /**
         * Destructor for SciDBDatasets
         */
        ~SciDBDataset();

        /**
         * Function called by GDAL once a SciDB dataset is requested
         * @see GDALDataset::Open
         */
        static GDALDataset *Open ( GDALOpenInfo *poOpenInfo );

	/**
	 * @brief Checks if the data can be opened as a SciDB dataset
	 * 
	 * Decides whether a dataset is a SciDB dataset or not. It mainly depends whether or not th connection string
	 * starts with the prefix "SCIDB:".
	 * 
	 * @param poOpenInfo GDAL opening options
	 * @return int 0 or 1 depending if it can be Identified or not
	 */
	static int Identify ( GDALOpenInfo *poOpenInfo );
	
	/**
	 * @brief Returns a pointer to the Shim client object
	 * 
	 * @return scidb4gdal::ShimClient*
	 */
	ShimClient* getClient();

	/**
	 * @brief Returns affine transformation parameters
	 * 
	 * @see GDALDataset::GetGeoTransform
	 * @param padfTransform a pointer to a double array with the size of 6 to store the affine transform parameters are stored.
	 * @return CPLErr
	 */
	CPLErr GetGeoTransform ( double *padfTransform );

	/**
	 * @brief Returns WKT spatial reference string
	 * 
	 * @see GDALDataset::GetProjectionRef
	 * @return const char*
	 */
	const char *GetProjectionRef();

	
	/**
	 * @brief Fetch Metadata for a domain.
	 * 
	 * @see GDALPamDataset::GetMetadata
	 * @param pszDomain the domain of interest. Use "" or NULL for the default domain
	 * @return char** NULL or a string list. 
	 */
	char ** GetMetadata ( const char * pszDomain = "");
	
	/**
	 * @brief Set single metadata item. 
	 * 
	 * @see GDALPamDataset::GetMetadataItem
	 * @param pszName the key for the metadata item to fetch. 
	 * @param pszDomain the value to assign to the key. 
	 * @return const char*
	 */
	const char * GetMetadataItem (const char * pszName, const char * pszDomain = "");

        /**
        * Function for creating a new SciDB array based on an existing GDAL dataset.
        * @see GDALDriver::CreateCopy()
        */
        static GDALDataset *CreateCopy ( const char *pszFilename, GDALDataset *poSrcDS, int bStrict, char **papszOptions, GDALProgressFunc pfnProgress, void *pProgressData );


        
	/**
	 * @brief Delete named dataset.
	 * 
	 * @see GDALDriver::Delete
	 * @param pszName name of dataset to delete.
	 * @return CPLErr
	 */
	static CPLErr Delete ( const char *pszName );
	
	
    protected:
      
      /**
       * @brief Converts a list of strings (key-value pairs) into a map of strings
       * 
       * This function splits the strings of the strlist (KVP) into a map of strings as keys and values, e.g. 
       * "key=value" --> ("key", "value")
       * 
       * @param strlist a list of key-value pairs as individual strings
       * @param kv the string map object (output)
       * @return void
       */
      static void gdalMDtoMap(char **strlist, map<string,string> &kv);
      
      /**
       * @brief Returns a list of strings mit key-value pairs from a map of strings
       * 
       * This function transforms a map of strings into a list of KVP strings:
       * e.g. ("key","value") --> "key=value"
       * 
       * @param kv The map of strings that represent keys and values
       * @return char** The list of string representation for key value pairs.
       */
      static char** mapToGdalMD(map<string,string> &kv);
      
      /**
       * @brief Copys the meta data from a source to the SciDB array representation
       * 
       * This function will be used to extract the meta data from the source dataset and copy this information
       * to the target SciDBArray.
       * 
       * @param poSrcDS a pointer to a GDALDataset
       * @param array a SciDBSpatialArray that will be modified (output)
       * @return void
       */
      static void copyMetadataToArray(GDALDataset* poSrcDS, SciDBSpatialArray &array);
      
      /**
       * @brief Transmits and stores an image into a temporal SciDB array
       * 
       * @param client the ShimClient holding the necessary information to connect to the web client
       * @param array the array representation that will be used to create an array an SciDB from
       * @param poSrcDS the source GDAL data set in which the data is stored
       * @param pfnProgress the progress function
       * @param pProgressData the progress data
       * @return void
       */
      static void uploadImageIntoTempArray(ShimClient *client, SciDBSpatialArray &array, GDALDataset *poSrcDS, GDALProgressFunc pfnProgress, void *pProgressData);

      /**
       * @brief Checks if an array can be inserted into another array
       * 
       * This functions queries the SciDB data base whether or not an array can be inserted into 
       * another by checking various conditions.
       * 
       * @param src_array The source array metdata representation
       * @param tar_array the target array metadata representation
       * @return bool
       */
      static bool arrayIntegrateable(SciDBSpatialArray &src_array, SciDBSpatialArray &tar_array);

    };

   /**
    * GDALRasterBand subclass implementing core GDAL functionality for single bands
    */
    class SciDBRasterBand : public GDALPamRasterBand
    {
        friend class SciDBDataset;

        SciDBSpatialArray *_array; //!< associated array metadata object
        char** papszMetadata;

    public:

	/**
	 * @brief Default constructor for SciDB attribute bands
	 * 
	 * @param poDS the parent dataset
	 * @param array the metadata representation of the array
	 * @param nBand the number of the band in the data set
	 */
	SciDBRasterBand ( SciDBDataset *poDS, SciDBSpatialArray *array, int nBand );

        /**
         * @brief Band destructor
         */
        ~SciDBRasterBand();

	/**
	 * @brief GDAL function called as array attribute data is requested, loads data from SciDB server
	 * 
	 * This function loads a block data from the server and it stores it temporally in a Tile cache if needed, before 
	 * the data is written into the stated image.
	 * 
	 * @param nBlockXOff the column offset as a number
	 * @param nBlockYOff the row offset as a number 
	 * @param pImage the image where the data is written into
	 * @return CPLErr
	 */
	virtual CPLErr IReadBlock ( int nBlockXOff, int nBlockYOff, void *pImage );

        /*
        * GDAL function called as array attribtue data shall be written, uploads data to SciDB and thus might take some time
         */
        //virtual CPLErr IWriteBlock ( int nBlockXOff, int nBlockYOff, void *pImage );

        /** @copydoc GDALRasterBand::GetStatistics */
        virtual CPLErr GetStatistics ( int bApproxOK, int bForce, double *pdfMin, double *pdfMax, double *pdfMean, double *pdfStdDev );

	/** @copydoc GDALPamRasterBand::GetNoDataValue */
        virtual double GetNoDataValue ( int *pbSuccess = NULL );
	
	/** @copydoc GDALPamRasterBand::GetMinimum */
        virtual double  GetMinimum ( int *pbSuccess = NULL );
	
	/** @copydoc GDALPamRasterBand::GetMaximum */
        virtual double  GetMaximum ( int *pbSuccess = NULL );
	
	/** @copydoc GDALPamRasterBand::GetOffset */
        virtual double  GetOffset ( int *pbSuccess = NULL );
	
	/** @copydoc GDALPamRasterBand::GetScale */
        virtual double  GetScale ( int *pbSuccess = NULL );
	
	/** @copydoc GDALPamRasterBand::GetUnitType */
	virtual const char *GetUnitType ();

	static void loadParsFromEnv (ConnectionPars* con);

    };
}

#endif
