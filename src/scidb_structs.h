#ifndef SCIDB_STRUCTS_H
#define SCIDB_STRUCTS_H

#include "affinetransform.h"
#include "utils.h"
#include <vector>
#include <sstream>
#include "TemporalReference.h"
#include "shimclient.h"

namespace scidb4gdal
{
    using namespace std;
    using namespace scidb4geo;
    
    typedef map<string, string>   MD;
    typedef map<string, MD>      DomainMD;
    
    /**
     * A structure for storing metadata of an attribute of a SciDB array
     */
    struct SciDBAttribute {
	/** the name of the attribute as stored in SciDB */
        string name;
	/** the name of the data type that the values will represent */
        string typeId;
	/** a flag if the attribute is allowed to have null values */
        bool nullable;
	/** metadata about the domain of the attribute */
	DomainMD md;
    };
    
    /**
     * A structure to store the statistical values for a band
     */
    struct SciDBAttributeStats {
        double min, max, mean, stdev;
    };

    /**
      * A structure for storing metadata of a dimension of a SciDB Array
      */
    struct SciDBDimension {
	/** name of the dimension */
        string name; 
	
	/**  lowest value for this axis */
        int64_t low;
	
	/**  highest value for this axis */
        int64_t high;
	
	/**  The intended chunksize for this Dimension */
        uint32_t chunksize;
	
	/**  GDAL type specification as string */
        string typeId;
	
	/** The dimensions allowed minimal value  */
	int64_t start;
	
	/** The range of the dimension. E.g. start + length = maximal allowed value */
	int64_t length;

    };


    /**
    * A structure for storing general metadata of an array in SciDB. The most basic representation is considered
    * to hold at least information about the array name, its attributes and its dimensions.
    * 
    * Dimensions:
    * The dimensions describe the conditions unber which the information was stored. For example dimensions be
    * temporal, spatial (e.g. image coordinates) or they describe other attributal information (e.g. gender, a category, a sensor...)
    * 
    * Attributes:
    * The attributes hold the information of the cell value in each band.
    */
    struct SciDBArray {
	/** the name of the array under which it is (or will be) stored in SciDB */
        string name;
	/** a list of scidb4gdal::Attribute that contain metadata about the attributes */
        vector<SciDBAttribute> attrs;
	/** a list of scidb4gdal::Dimension that contain metadata about the dimensions */
        vector<SciDBDimension> dims;
	/** a map of strings holding domain metadata */
	DomainMD md;
	
	/**
	 * @brief toString method
	 * 
	 * The resulting output will be like the following:
	 * 'src_arrayname':<'dimname',min:max,type>['attributename',type,nullable]
	 * The parts in the brackets can show multiple times.
	 * 
	 * @return std::string
	 */
	virtual string toString() {
            stringstream s;
            s << "'" << name << "'" << ":";
            for ( uint32_t i = 0; i < dims.size(); ++i ) s << "<'" << dims[i].name << "'," << dims[i].low << ":" << dims[i].high << "," << dims[i].typeId << ">";
            for ( uint32_t i = 0; i < attrs.size(); ++i ) s << "['" << attrs[i].name << "'," << attrs[i].typeId << "," << attrs[i].nullable << "]";
            s << "\n";
            return s.str();
        }

        /**
	  * @brief creates a string about the data type for each attribute 
	  * 
	  * For each attribute its data type will be appended to an output string.
	  * 
	  * @return std::string
	  */
        virtual string getFormatString() {
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
    * A structure for storing spatial reference of a SciDB src_array
    */
    struct SciDBSpatialReference {
        string srtext;
        string proj4text; // TODO: Fill while reading a dataset or already done?
        string xdim;
        string ydim;
        string auth_name; // TODO: Fill while reading a dataset
        uint32_t auth_srid; // TODO: Fill while reading a dataset

        AffineTransform affineTransform;

        bool isSpatial() {
            return ( xdim != "" && ydim != "" && ( srtext != "" || proj4text != "" ) );
        }
    };
    
    /**
      *  A structure for storing temporal reference of a SciDB src_array
      */
    struct SciDBTemporalReference: public TReference {
	/**
	 * The assigned name of the temporal axis.
	 */
	string tdim;
	 
	SciDBTemporalReference(): TReference::TReference() {
	  tdim = SCIDB4GDAL_DEFAULT_TDIMNAME;
	}
	
	SciDBTemporalReference( string t0text, string dttext ): TReference::TReference( t0text, dttext ) { 
	  tdim = SCIDB4GDAL_DEFAULT_TDIMNAME;
	}
	
	~SciDBTemporalReference() {
	  
	}
	
	bool isTemporal() {
	    return ( tdim != "" && _t0 != NULL && _dt != NULL);
	}
	
	void createTRS (string const &t0, string const &dt) {
	    _t0 = new TPoint(t0);
	    _dt = new TInterval(dt);
	    _r = _dt->_resolution;
	}
	
	void setTPoint(TPoint *point) {
	    _t0 = point;
	}
	
	void setTInterval(TInterval *interval) {
	    _dt = interval;
	    _r = interval->_resolution;
	}
	
	TPoint* getTPoint() {
	    return _t0;
	}
	
	TInterval* getTInterval() {
	void isValid();
	    return _dt;
	}
    };

    /**
     * This src_array is the representation of a SciDB src_array with a temporal reference that manages the
     * correct addressing of the temporal dimension in SciDB, e.g. naming and alike.
     */
    struct SciDBTemporalArray: public virtual SciDBArray, public SciDBTemporalReference {
	
	SciDBTemporalArray(): SciDBTemporalReference() {
	  initTemporalDim();
	}
	
	SciDBTemporalArray(string t0text, string dttext) : SciDBTemporalReference(t0text,dttext) {
	  initTemporalDim();
	};
	/**
	 * The index pointing to the temporal dimension
	 */
	int _t_idx;
	
	/**
	 * Returns the designated temporal dimension of the SciDB dataset.
	 */
	SciDBDimension* getTDim() {
            if ( _t_idx < 0 ) deriveTemporalDimensionIndex();
            return &dims[_t_idx];
        }

	/**
	 * Returns the index in SciDBArray's dimension list that represents the temporal dimension.
	 */
        int getTDimIdx() {
            if ( _t_idx < 0 ) deriveTemporalDimensionIndex();
            return _t_idx;
        }
	
	protected:
	  void initTemporalDim() {
	      SciDBDimension dimt;
	      dimt.low = 0;
	      dimt.high = 0;
	      dimt.name = SCIDB4GDAL_DEFAULT_TDIMNAME;
	      dimt.chunksize = SCIDB4GDAL_DEFAULT_BLOCKSIZE;
	      dimt.typeId = "int64";
	      
	      dims.push_back ( dimt );
	      _t_idx = dims.size() - 1;
	  }
	
	void deriveTemporalDimensionIndex() {
            if ( tdim != "") {
                for ( size_t i = 0; i < dims.size(); ++i ) {
                    if ( dims[i].name == tdim ) {
			_t_idx = i;
			break;
		    }
                }
            }
            else { // Try default dimension names
                for ( size_t i = 0; i < dims.size(); ++i ) {
                    if ( dims[i].name == SCIDB4GDAL_DEFAULT_TDIMNAME ) {
			_t_idx = i;
			break;
		    }
                }
            }
        }
    };
    
    /**
     * A structure for storing metadata of a spatially referenced SciDB src_array.
     * 
     * If no SpatialReference is sepcified, then the SpatialArray will be a simple cartesian reference system.
     */
    struct SciDBSpatialArray : public virtual SciDBArray, public SciDBSpatialReference {
        SciDBSpatialArray()  {
	  initSpatialDims();
	}
        
        SciDBSpatialArray(SciDBArray &parent, SciDBSpatialReference *sr = NULL){
	  attrs = parent.attrs;
	  dims = parent.dims;
	  md = parent.md;
	  name = parent.name;
	  
	  if (sr != NULL) {
	    affineTransform = sr->affineTransform; 
	    auth_name = sr->auth_name;
	    auth_srid= sr->auth_srid;
	    proj4text = sr->proj4text;
	    srtext = sr->srtext;
	    xdim = sr->xdim;
	    ydim = sr->ydim;
	  }
	}
	
	virtual ~SciDBSpatialArray() {
	  
	}
	
	int _x_idx, _y_idx;


        SciDBDimension* getYDim() {
            if ( _y_idx < 0 ) deriveDimensionIndexes();

            return &dims[_y_idx];;
        }
        SciDBDimension* getXDim() {
            if ( _x_idx < 0 ) deriveDimensionIndexes();

            return &dims[_x_idx];
        }

        int getXDimIdx() {
            if ( _x_idx < 0 ) deriveDimensionIndexes();
            return _x_idx;
        }


        int getYDimIdx() {
            if ( _y_idx < 0 ) deriveDimensionIndexes();
            return _y_idx;
        }

    protected:
        void initSpatialDims() {
	    SciDBDimension dimx;
	    dimx.low = 0;
	    dimx.start = 0;
	    dimx.high = 0; //adapted later
	    dimx.name = SCIDB4GDAL_DEFAULT_XDIMNAME;
	    dimx.chunksize = SCIDB4GDAL_DEFAULT_BLOCKSIZE; //should be adapted
	    dimx.typeId = "int64"; //per default

	    SciDBDimension dimy;
	    dimy.low = 0;
	    dimy.start = 0;
	    dimy.high = 0;
	    dimy.name = SCIDB4GDAL_DEFAULT_YDIMNAME;
	    dimy.chunksize = SCIDB4GDAL_DEFAULT_BLOCKSIZE;
	    dimy.typeId = "int64";
	    
	    // This order is more efficient as it fits row major image format (does not require transposing during downloads)
	    dims.push_back ( dimy );
	    _y_idx = dims.size() - 1;
	    dims.push_back ( dimx );
	    _x_idx = dims.size() - 1; 


	}

	/**
	 * assigns the spatial indices. As default it uses x as the first
	 * parameter in the dimension src_array (index 0) and y as the second
	 * index (1)
	 */
        void deriveDimensionIndexes() {
            _x_idx = 0;
            _y_idx = 1;
            if ( xdim != "" && ydim != "" ) {
                for ( size_t i = 0; i < dims.size(); ++i ) { // Assuming 2 dimensions!!!
                    if ( dims[i].name == xdim ) _x_idx = i;
                    if ( dims[i].name == ydim ) _y_idx = i;
                }
                // TODO: Assert x_idx != y_idx
            }
            else { // Try default dimension names
                for ( size_t i = 0; i < dims.size(); ++i ) { // Assuming 2 dimensions!!!
                    if ( dims[i].name == SCIDB4GDAL_DEFAULT_XDIMNAME ) _x_idx = i;
                    if ( dims[i].name == SCIDB4GDAL_DEFAULT_YDIMNAME ) _y_idx = i;
                }
                // TODO: Assert x_idx != y_idx
            }
        }
    };
    
    struct SciDBSpatioTemporalArray: public SciDBSpatialArray, public SciDBTemporalArray  {
	SciDBSpatioTemporalArray() :  SciDBSpatialArray(), SciDBTemporalArray() {
	  
	}
	
	SciDBSpatioTemporalArray(string t0text, string dttext ) : SciDBSpatialArray(),SciDBTemporalArray(t0text,dttext) {
	
	}
    };
}

#endif