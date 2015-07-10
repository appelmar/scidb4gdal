#ifndef SCIDB_STRUCTS_H
#define SCIDB_STRUCTS_H

#include "affinetransform.h"
#include "utils.h"
#include <vector>
#include <sstream>

namespace scidb4gdal
{
    using namespace std;

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

        string toString() {
            stringstream s;
            s << "'" << name << "'" << ":";
            for ( uint32_t i = 0; i < dims.size(); ++i ) s << "<'" << dims[i].name << "'," << dims[i].low << ":" << dims[i].high << "," << dims[i].typeId << ">";
            for ( uint32_t i = 0; i < attrs.size(); ++i ) s << "['" << attrs[i].name << "'," << attrs[i].typeId << "," << attrs[i].nullable << "]";
            s << "\n";
            return s.str();
        }

        string getFormatString() {
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
     * A structure for storing metadata of a spatially referenced SciDB array
     */
    struct SciDBSpatialArray : SciDBArray, SciDBSpatialReference {

        SciDBSpatialArray() : _x_idx ( -1 ), _y_idx ( -1 ) {}

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

    private:
        int _x_idx;
        int _y_idx;


        void deriveDimensionIndexes() {
            _x_idx = 0;
            _y_idx = 1;
            if ( xdim != "" && ydim != "" ) {
                for ( int i = 0; i < 2; ++i ) { // Assuming 2 dimensions!!!
                    if ( dims[i].name == xdim ) _x_idx = i;
                    if ( dims[i].name == ydim ) _y_idx = i;
                }
                // TODO: Assert x_idx != y_idx
            }
            else { // Try default dimension names
                for ( int i = 0; i < 2; ++i ) { // Assuming 2 dimensions!!!
                    if ( dims[i].name == SCIDB4GDAL_DEFAULT_XDIMNAME ) _x_idx = i;
                    if ( dims[i].name == SCIDB4GDAL_DEFAULT_YDIMNAME ) _y_idx = i;
                }
                // TODO: Assert x_idx != y_idx
            }
        }
    };
}

#endif