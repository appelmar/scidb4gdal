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
     * A structure for storing metadata of a SciDB array attribute
     */
    struct SciDBAttribute {
        string name;
        string typeId;
        bool nullable;
	DomainMD md;
    };
    
    /**
     * A structure to store the statistical values for a band
     */
    struct SciDBAttributeStats {
        double min, max, mean, stdev;
    };

    /**
    * A structure for storing metadata of a SciDB array dimension
    */
    struct SciDBDimension {
	/**
	 * name of the dimension
	 */
        string name;
	
	/**
	 * lowest value for this axis
	 */
        int64_t low;
	
	/**
	 * highest value for this axis
	 */
        int64_t high;
	
	/**
	 * The intended chunksize for this Dimension
	 */
        uint32_t chunksize;
	
	/**
	 * GDAL type specification as string
	 */
        string typeId;
	
	int64_t start; //TODO used?
	int64_t length; //TODO used?

    };


    /**
    * A structure for storing general metadata of a SciDB array
    */
    struct SciDBArray {
        string name;
        vector<SciDBAttribute> attrs;
        vector<SciDBDimension> dims;
	DomainMD md;
	
	/**
	 * The resulting output will be like the following:
	 * 'arrayname':<'dimname',min:max,type>['attributename',type,nullable]
	 * The parts in the brackets can show multiple times.
	 */
        virtual string toString() {
            stringstream s;
            s << "'" << name << "'" << ":";
            for ( uint32_t i = 0; i < dims.size(); ++i ) s << "<'" << dims[i].name << "'," << dims[i].low << ":" << dims[i].high << "," << dims[i].typeId << ">";
            for ( uint32_t i = 0; i < attrs.size(); ++i ) s << "['" << attrs[i].name << "'," << attrs[i].typeId << "," << attrs[i].nullable << "]";
            s << "\n";
            return s.str();
        }

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
    * A structure for storing spatial reference of a SciDB array
    */
    struct SciDBSpatialReference {
        string srtext;
        string proj4text; // TODO: Fill while reading a dataset or already done?
        string xdim;
        string ydim;
        string auth_name; // TODO: Fill while reading a dataset
        uint32_t auth_srid; // TODO: Fill while reading a dataset

        AffineTransform affineTransform;

        string toString() {
            stringstream s;
            s << "SPATIAL REFERENCE (" << xdim << "," << ydim << ") :" << affineTransform.toString() << "-->" << proj4text;
            s << "\n";
            return s.str();
        }

        bool isSpatial() {
            return ( xdim != "" && ydim != "" && ( srtext != "" || proj4text != "" ) );
        }
    };
    
    /**
      *  A structure for storing temporal reference of a SciDB array
      */
    struct SciDBTemporalReference: public TReference {
	/**
	 * The assigned name of the temporal axis.
	 */
	string tdim;
	 
	SciDBTemporalReference(): TReference() {}
	
	~SciDBTemporalReference() {
	  
	}
	
	string toString() {
	    stringstream s;
	    s << "TEMPORAL REFERENCE ("<< tdim << ", "<< _t0->toStringISO() << ", " << _dt->toStringISO() <<" )";
	    s << "\n";
	    return s.str();
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
	    return _dt;
	}
    };

    /**
     * This array is the representation of a SciDB array with a temporal reference that manages the
     * correct addressing of the temporal dimension in SciDB, e.g. naming and alike.
     */
    struct SciDBTemporalArray: public virtual SciDBArray, public SciDBTemporalReference {
	
	SciDBTemporalArray(): _t_idx (-1), SciDBTemporalReference() {}
	
	/**
	 * The index pointing to the temporal dimension
	 */
	int _t_idx;
	
	/**
	 * Returns the designated temporal dimension of the SciDB dataset.
	 */
	SciDBDimension getTDim() {
            if ( _t_idx < 0 ) deriveTemporalDimensionIndex();
            return dims[_t_idx];
        }
	
	/**
	 * Returns the index in SciDBArray's dimension list that represents the temporal dimension.
	 */
        int getTDimIdx() {
            if ( _t_idx < 0 ) deriveTemporalDimensionIndex();
            return _t_idx;
        }
	
	protected:
	
	
	void deriveTemporalDimensionIndex() {
            if ( tdim != "") {
                for ( int i = 0; i < dims.size(); ++i ) {
                    if ( dims[i].name == tdim ) {
			_t_idx = i;
			break;
		    }
                }
            }
            else { // Try default dimension names
                for ( int i = 0; i < dims.size(); ++i ) {
                    if ( dims[i].name == SCIDB4GDAL_DEFAULT_TDIMNAME ) {
			_t_idx = i;
			break;
		    }
                }
            }
        }
    };
    
    /**
     * A structure for storing metadata of a spatially referenced SciDB array.
     * 
     * If no SpatialReference is sepcified, then the SpatialArray will be a simple cartesian reference system.
     */
    struct SciDBSpatialArray : public virtual SciDBArray, public SciDBSpatialReference {
        SciDBSpatialArray() : _x_idx ( -1 ), _y_idx ( -1 ) {}
        
        SciDBSpatialArray(SciDBArray &parent, SciDBSpatialReference *sr = NULL){
	  attrs = parent.attrs;
	  dims = parent.dims;
	  md = parent.md;
	  name = parent.name;
	  
	  if (sr != NULL) {
	    //TODO copy the Reference
	  }
	}
	
	int _x_idx, _y_idx;
	
        string toString() {
            stringstream s;
            s << SciDBArray::toString();
            s << SciDBSpatialReference::toString();
            s << "\n";
            return s.str();
        }

        SciDBDimension getYDim() {
            if ( _y_idx < 0 ) deriveDimensionIndexes();
            return dims[_y_idx];
        }
        SciDBDimension getXDim() {
            if ( _x_idx < 0 ) deriveDimensionIndexes();
            return dims[_x_idx];
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
        

	/**
	 * assigns the spatial indices. As default it uses x as the first
	 * parameter in the dimension array (index 0) and y as the second
	 * index (1)
	 */
        void deriveDimensionIndexes() {
            _x_idx = 0;
            _y_idx = 1;
            if ( xdim != "" && ydim != "" ) {
                for ( int i = 0; i < dims.size(); ++i ) { // Assuming 2 dimensions!!!
                    if ( dims[i].name == xdim ) _x_idx = i;
                    if ( dims[i].name == ydim ) _y_idx = i;
                }
                // TODO: Assert x_idx != y_idx
            }
            else { // Try default dimension names
                for ( int i = 0; i < dims.size(); ++i ) { // Assuming 2 dimensions!!!
                    if ( dims[i].name == SCIDB4GDAL_DEFAULT_XDIMNAME ) _x_idx = i;
                    if ( dims[i].name == SCIDB4GDAL_DEFAULT_YDIMNAME ) _y_idx = i;
                }
                // TODO: Assert x_idx != y_idx
            }
        }
    };
    
    struct SciDBSpatioTemporalArray: public SciDBTemporalArray, public SciDBSpatialArray {
	SciDBSpatioTemporalArray() :  SciDBTemporalArray(), SciDBSpatialArray() {}
	
	string toString() {
	    stringstream s;
	    s << "TEMPORAL REFERENCE ("<< tdim << ", "<< _t0->toStringISO() << ", " << _dt->toStringISO() <<" )";
	    s << "\n";
	    return s.str();
	}
    };
}

#endif