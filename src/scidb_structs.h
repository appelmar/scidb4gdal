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

    /**
     * A structure for storing metadata of a SciDB array attribute
     */
    struct SciDBAttribute {
        string name;
        string typeId;
        bool nullable;
    };

    struct SciDBAttributeStats {
        double min, max, mean, stdev;
    };

    /**
    * A structure for storing metadata of a SciDB array dimension
    */
    struct SciDBDimension {
        string name;
        int64_t low;
        int64_t high;
        uint32_t chunksize;
        string typeId;

    };


    /**
    * A structure for storing general metadata of a SciDB array
    */
    struct SciDBArray {
        string name;
        vector<SciDBAttribute> attrs;
        vector<SciDBDimension> dims;
	
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

    struct SciDBTemporalArray: public virtual SciDBArray, public SciDBTemporalReference {
	
	SciDBTemporalArray(): _t_idx (-1), SciDBTemporalReference() {}
	
	int _t_idx;
	
	SciDBDimension getTDim() {
            if ( _t_idx < 0 ) deriveTemporalDimensionIndex();
            return dims[_t_idx];
        }

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
     * A structure for storing metadata of a spatially referenced SciDB array
     */
    struct SciDBSpatialArray : public virtual SciDBArray, public SciDBSpatialReference {
        SciDBSpatialArray() : _x_idx ( -1 ), _y_idx ( -1 ) {}
	
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