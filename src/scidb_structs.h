#ifndef SCIDB_STRUCTS_H
#define SCIDB_STRUCTS_H

#include "affinetransform.h"
#include "utils.h"
#include <vector>
#include <sstream>
#include "TemporalReference.h"

namespace scidb4gdal {
    using namespace std;
    using namespace scidb4geo;

    /**
    * @typedef MD
    *
    * This is a type definition to hold metadata as a map of strings that relate to key-value pairs
    */
    typedef map<string, string> MD;

    /**
    * @typedef DomainMD
    *
    * This type is definition to collect metadata MD types under a certain domain (string). It will store the
    * data in a map of string and MD objects
    */
    typedef map<string, MD> DomainMD;

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
    * @brief A structure to hold general information about the structure of an array stored in SciDB.
    *
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
        
        virtual ~SciDBArray() {};
        
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
        virtual string toString();

        /**
        * @brief creates a string about the data type for each attribute
        *
        * For each attribute its data type will be appended to an output string.
        *
        * @return std::string
        */
        virtual string getFormatString();

        /**
        * @brief derives the SciDB array schema string
        * @return std::string
        */
        virtual string getSchemaString();
    };

    /**
    * @brief A carrier structure for holding information about the spatial reference
    *
    * A structure for storing spatial reference that is to be obtained from either the source
    * file or from the SciDB database. This reference is restricted to a two dimensional projection
    * with the dimension x and y. X can be thought of the west-east direction and Y is the north-south
    * direction.
    */
    struct SciDBSpatialReference {
        /**
        * The WKT text representation of a spatial reference that can be used in an
        * OGRSpatialReference class, e.g.
        * "PROJCS["NAD27 / UTM zone
        * 16N",GEOGCS["NAD27",DATUM["North_American_Datum_1927",SPHEROID["Clarke
        * 1866",6378206.4,294.9786982139006,AUTHORITY["EPSG","7008"]],AUTHORITY["EPSG","6267"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]],AUTHORITY["EPSG","4267"]],UNIT["metre",1,AUTHORITY["EPSG","9001"]],PROJECTION["Transverse_Mercator"],PARAMETER["latitude_of_origin",0],PARAMETER["central_meridian",-87],PARAMETER["scale_factor",0.9996],PARAMETER["false_easting",500000],PARAMETER["false_northing",0],AUTHORITY["EPSG","26716"],AXIS["Easting",EAST],AXIS["Northing",NORTH]]"
        */
        string srtext;
        /**
        * The proj4 text representation of a spatial reference, e.g. "+proj=utm
        * +zone=16 +datum=NAD27 +units=m +no_defs". Intentially from this information
        * the srtext can also be obtained by using the PROJ4 library.
        */
        string proj4text;
        /** the name of the W-E dimension that is stored in SciDB */
        string xdim;
        /** the name of the N-S dimension that is stored in SciDB */
        string ydim;
        /** The authority name, e.g. "EPSG" */
        string auth_name;
        /** The ID of the spatial reference system of the stated authority, e.g. 26716
        * of the identifier "EPSG:26716" */
        uint32_t auth_srid;

        /** The affine transformation to transform image coordinates into real world
        * coordinates using the parameter of the stated SRS */
        AffineTransform affineTransform;

        /**
        * @brief Checks if the information is complete
        *
        * This function checks the class' properties if there is sufficient
        *information to have a spatial reference.
        *
        * @return bool Whether or not the class is suffienciently spatially
        *referenced
        */
        inline bool isSpatial() {
            return (xdim != "" && ydim != "" && (srtext != "" || proj4text != ""));
        }
    };

    /**
    *  @brief A structure for storing temporal reference of an array stored in SciDB
    *
    * This structure holds information about the temporal reference of the array in SciDB. It inherits
    * from the class scidb4geo::TReference, which is also used in the SciDB plugin for transforming between dates
    * or date time statements into the discrete dimension value used in the SciDB array.
    *
    * Similar to scidb4gdal::SciDBSpatialReference this struct simply extends the prior mentioned scidb4geo::TReference, by
    * carrying also the name of the temporal dimension stated in SciDB
    */
    struct SciDBTemporalReference : public TReference {
        /**
        * The assigned name of the temporal dimension in SciDB.
        */
        string tdim;

        /**
        * @brief Standard constructor
        *
        * This is the standard constructor which invokes the parents standard
        *constructor and sets the name of the
        * temporal dimension to the scidb4gdals default temporal dimension name
        *(scidb4gdal::SCIDB4GDAL_DEFAULT_TDIMNAME)
        */
        SciDBTemporalReference() : TReference::TReference() {
            tdim = SCIDB4GDAL_DEFAULT_TDIMNAME;
        }

        /**
        * @brief The constructor using a temporal datum and an interval period
        *
        * This constructor uses valid ISO 8601 strings to state the temporal datum (t0 value) and the intented
        * temporal resolution by stating a temporal interval statement. The mentioned strings are passed to the
        * parent class scidb4geo::TReference.
        *
        * @param t0text temporal datum of the data set as a ISO 8601 conform date or date-time string, e.g. "2004-06-14" (date),"2004-06-14T23:34:30" (date-time) or "2004W28" (weeks)
        * @param dttext interval statement conform to ISO 8601 time periods, e.g. "P1D" (one day interval) or "P14D" (14 days)
        */
        SciDBTemporalReference(string t0text, string dttext)
            : TReference::TReference(t0text, dttext) {
            tdim = SCIDB4GDAL_DEFAULT_TDIMNAME;
        }

        /**
        * @brief Standard destructor
        *
        * Standard destructor
        */
        ~SciDBTemporalReference() {}

        /**
        * @brief Checks if the temporal reference information are set
        *
        * The function checks whether the information about the dimension name, the
        *temporal datum and the temporal
        * resolution was set.
        *
        * @return bool if the class has sufficient temporal information
        */
        bool isTemporal() { return (tdim != "" && _t0 != NULL && _dt != NULL); }

        /**
        * @brief Creates and sets the temporal datum and resolution
        *
        * If the standard constructor was used the temporal datum and resolution are not set. To do this after
        * creating the object this function will achieve this by taking valid ISO 8601 strings.
        *
        * @param t0 temporal datum of the data set as a ISO 8601 conform date or date-time string, e.g. "2004-06-14" (date),"2004-06-14T23:34:30" (date-time) or "2004W28" (weeks)
        * @param dt interval statement conform to ISO 8601 time periods, e.g. "P1D" (one day interval) or "P14D" (14 days)
        *
        * @return void
        */
        void createTRS(string const& t0, string const& dt) {
            _t0 = new TPoint(t0);
            _dt = new TInterval(dt);
            _r = _dt->_resolution;
            _t0->_resolution = _r;
        }

        /**
        * @brief Sets the temporal datum
        *
        * If a scidb4ge::TPoint was created manually, this function overwrites the temporal datum with the stated temporal point.
        *
        * @return void
        */
        void setTPoint(TPoint* point) { _t0 = point; }

        /**
        * @brief Sets the temporal resolution
        *
        * If a scidb4geo::TInterval was created manually, this function overwrites the temporal interval with the steated temporal interval.
        *
        * @return void
        */
        void setTInterval(TInterval* interval) {
            _dt = interval;
            _r = interval->_resolution;
            if (_t0) {
                _t0->_resolution = _r;
            }
        }
        /**
        * @brief Getter for the temporal datum
        *
        * Returns a pointer to the scidb4geo::TPoint that is assumed to be the temporal datum of the SciDB array.
        *
        * @return scidb4geo::TPoint* The temporal datum.
        */
        TPoint* getTPoint() { return _t0; }

        /**
        * @brief Getter for the temporal resolution
        *
        * Returns a pointer to the scidb4geo::TInterval carried by the parent class that represents the temporal
        * resolution of the SciDB array.
        *
        * @return scidb4geo::TInterval* the temporal resolution
        */
        TInterval* getTInterval() { return _dt; }
    };

    /**
    * @brief structure to carry information about a temporally referenced SciDB array.
    *
    * This structure represents a SciDB array with a temporal reference. By inheriting from SciDBArray and
    * SciDBTemporalReference the structure allows to identify the temporal dimension in GDAL, as well as holding
    * information about the temporal reference used in SciDB and it allows also the translation between
    * dates and data-time strings and the discrete dimension values of the SciDB array.
    */
    struct SciDBTemporalArray : public virtual SciDBArray,
                                public SciDBTemporalReference {
        /**
        * @brief Standard constructor
        *
        */
        SciDBTemporalArray() : SciDBTemporalReference() { initTemporalDim(); }

        /**
        * @brief constructor using a temporal datum and a temporal resolution
        *
        * Initalize the temporal array by invoking the parents constructor with the two ISO 8601 strings.
        *
        * @param t0text temporal datum of the data set as a ISO 8601 conform date or date-time string, e.g. "2004-06-14" (date),"2004-06-14T23:34:30" (date-time) or "2004W28" (weeks)
        * @param dttext interval statement conform to ISO 8601 time periods, e.g. "P1D" (one day interval) or "P14D" (14 days)
        */
        SciDBTemporalArray(string t0text, string dttext)
            : SciDBTemporalReference(t0text, dttext) {
            initTemporalDim();
        };

        /**
        * The index pointing to the correct temporal dimension in
        * scidb4gdal::SciDBArray::dims
        */
        int _t_idx;

        /**
        * @brief Returns the designated temporal dimension of the SciDB dataset.
        *
        * This function returns a pointer to the scidb4gdal::Dimension which is assigned to be the temporal dimension. If the
        * temporal index was not already set, it will be derived automatically.
        *
        * @return scidb4gdal::SciDBDimension* A pointer to the temporal dimension
        */
        SciDBDimension* getTDim() {
            if (_t_idx < 0)
                deriveTemporalDimensionIndex();
            return &dims[_t_idx];
        }

        /**
        * @brief Returns the position in the scidb4gdal::SciDBArray::dims that is the temporal dimension.
        *
        * Returns the index in SciDBArray's dimension list that represents the temporal dimension.
        *
        * @return int The position of the temporal dimension in the dimensions list.
        */
        int getTDimIdx() {
            if (_t_idx < 0)
                deriveTemporalDimensionIndex();
            return _t_idx;
        }

    protected:
        /**
        * @brief initializes the temporal dimension
        *
        * This function is usually called from the constructor in order to create the temporal dimension at initialization. It
        * will construct a new "temporal" dimension and stores it in the dimension list and it saves the position index of this dimension
        * in the list.
        *
        * @return void
        */
        void initTemporalDim() {
            SciDBDimension dimt;
            dimt.low = 0;
            dimt.high = 0;
            dimt.name = SCIDB4GDAL_DEFAULT_TDIMNAME;
            dimt.chunksize = SCIDB4GDAL_DEFAULT_TDIM_BLOCKSIZE;
            dimt.typeId = "int64";

            dims.push_back(dimt);
            _t_idx = dims.size() - 1;
        }

        /**
        * @brief Derives the position of the temporal dimension of the dimension list
        *
        * This function is searching for the temporal dimension in the list of all dimensions, by looping through that list
        * and searching for the dimension that is using the prior specified name of the dimension or the default temporal
        * dimension name.
        *
        * @return void
        */
        void deriveTemporalDimensionIndex() {
            if (tdim != "") {
                for (size_t i = 0; i < dims.size(); ++i) {
                    if (dims[i].name == tdim) {
                        _t_idx = i;
                        break;
                    }
                }
            } else { // Try default dimension names
                for (size_t i = 0; i < dims.size(); ++i) {
                    if (dims[i].name == SCIDB4GDAL_DEFAULT_TDIMNAME) {
                        _t_idx = i;
                        break;
                    }
                }
            }
        }
    };

    /**
    * @brief A structure for storing metadata of a spatially referenced SciDB array.
    *
    * The SciDBSpatial array extends the SciDBArray with a SciDBSpatialReference, meaning that some dimensions
    * will be assigned to refer to a West-East dimension and a North-South dimension.
    *
    * If no SpatialReference is sepcified, then the SpatialArray will be a simple cartesian reference system.
    */
    struct SciDBSpatialArray : public virtual SciDBArray,
                            public SciDBSpatialReference {
        /**
        * @brief Basic constructor
        *
        * A basic constructor with a simple cartesian reference system that will initialize the two spatial dimensions.
        */
        SciDBSpatialArray() { initSpatialDims(); }

        /**
        * @brief A constructor using a SciDBArray and a SciDBSpatialReference
        *
        * This constructor will merge a SciDBArray with a SciDBSpatialReference in order to create a SciDBSpatialArray.
        *
        * @param parent a SciDBArray
        * @param sr A pointer to a SciDBSpatialReference which default value is NULL
        */
        SciDBSpatialArray(SciDBArray& parent, SciDBSpatialReference* sr = NULL) {
            attrs = parent.attrs;
            dims = parent.dims;
            md = parent.md;
            name = parent.name;

            if (sr != NULL) {
                affineTransform = sr->affineTransform;
                auth_name = sr->auth_name;
                auth_srid = sr->auth_srid;
                proj4text = sr->proj4text;
                srtext = sr->srtext;
                xdim = sr->xdim;
                ydim = sr->ydim;
            }
        }

        /**
        * @brief Basic destructor
        *
        */
        virtual ~SciDBSpatialArray() {}

        /** The position index of the designated x dimension (West-East) in the dimension list */
        int _x_idx;
        /** the position of the designated y dimension (North-South) in the dimension list*/
        int _y_idx;

        /**
        * @brief Getter for the y dimension
        *
        * Returns a pointer to the SciDBDimension that represents the y dimension.
        *
        * @return scidb4gdal::SciDBDimension* the SciDBDimension representing the y dimension
        */
        SciDBDimension* getYDim() {
            if (_y_idx < 0)
                deriveDimensionIndexes();

            return &dims[_y_idx];
            ;
        }
        /**
        * @brief Getter for the x dimension
        *
        * Returns a pointer to the SciDBDimension that represents the x dimension.
        *
        * @return scidb4gdal::SciDBDimension* The SciDBDimension representing the x dimension.
        */
        SciDBDimension* getXDim() {
            if (_x_idx < 0)
                deriveDimensionIndexes();

            return &dims[_x_idx];
        }

        /**
        * @brief Getter function for the x dimension position in the dimension list
        *
        * This function returns the position of the x dimension in the list of dimensions of the array (scidb4gdal::SciDBArray::dims).
        *
        * @return int The position of the x dimension in the dimensions list
        */
        int getXDimIdx() {
            if (_x_idx < 0)
                deriveDimensionIndexes();
            return _x_idx;
        }

        /**
        * @brief Getter function for the y dimension position in the dimension list
        *
        * This function returns the position of the y dimension in the list of dimensions of the array (scidb4gdal::SciDBArray::dims).
        *
        * @return int The position of the y dimension in the dimensions list
        */
        int getYDimIdx() {
            if (_y_idx < 0)
                deriveDimensionIndexes();
            return _y_idx;
        }

    protected:
        /**
        * @brief initializes the two spatial dimensions
        *
        * The function is usually called from the constructor in order to create the two representation of spatial dimensions (x and y). The function
        * also adds those SciDBDimensions to the dimension list and stores their positions in that list to access them later.
        *
        * @return void
        */
        void initSpatialDims() {
            SciDBDimension dimx;
            dimx.low = 0;
            dimx.start = 0;
            dimx.high = 0; // adapted later
            dimx.name = SCIDB4GDAL_DEFAULT_XDIMNAME;
            dimx.chunksize = SCIDB4GDAL_DEFAULT_BLOCKSIZE; // should be adapted
            dimx.typeId = "int64";                         // per default

            SciDBDimension dimy;
            dimy.low = 0;
            dimy.start = 0;
            dimy.high = 0;
            dimy.name = SCIDB4GDAL_DEFAULT_YDIMNAME;
            dimy.chunksize = SCIDB4GDAL_DEFAULT_BLOCKSIZE;
            dimy.typeId = "int64";

            // This order is more efficient as it fits row major image format (does not
            // require transposing during downloads)
            dims.push_back(dimy);
            _y_idx = dims.size() - 1;
            dims.push_back(dimx);
            _x_idx = dims.size() - 1;
        }

        /**
        * @brief Derives the positions of the spatial dimensions
        *
        * This function loops through the dimension list and searches for the assigned names of the
        * x and y dimension. If there is no differing dimension name statement, then the default names
        * for the X and Y dimension are used.As default it uses x as the first
        * parameter in the dimensions (index 0) and y as the second
        * index (1).
        */
        void deriveDimensionIndexes() {
            _x_idx = 0;
            _y_idx = 1;
            if (xdim != "" && ydim != "") {
                for (size_t i = 0; i < dims.size(); ++i) { // Assuming 2 dimensions!!!
                    if (dims[i].name == xdim)
                        _x_idx = i;
                    if (dims[i].name == ydim)
                        _y_idx = i;
                }
                // TODO: Assert x_idx != y_idx
            } else {                                       // Try default dimension names
                for (size_t i = 0; i < dims.size(); ++i) { // Assuming 2 dimensions!!!
                    if (dims[i].name == SCIDB4GDAL_DEFAULT_XDIMNAME)
                        _x_idx = i;
                    if (dims[i].name == SCIDB4GDAL_DEFAULT_YDIMNAME)
                        _y_idx = i;
                }
                // TODO: Assert x_idx != y_idx
            }
        }
    };

    /**
    * @brief A structure that incorporates the information of a spatial and a temporal array
    *
    * This structure simply combines the SciDBSpatialArray and the SciDBTemporalArray to hold information on
    * all of the three dimensions.
    */

    struct SciDBSpatioTemporalArray : public SciDBSpatialArray,
                                    public SciDBTemporalArray {
        /**
        * @brief Basic constructor
        *
        * Basic constructor that calls the basic constructors of the parent structures
        */
        SciDBSpatioTemporalArray() : SciDBSpatialArray(), SciDBTemporalArray() {}

        /**
        * @brief Constructor using temporal information
        *
        * This constructor creates a spatio-temporal array with a cartesian reference system and a specified temporal reference. The temporal reference
        * is established by stating the temporal datum and the temporal resolution as ISO 8601 strings.
        *
        * @param t0text temporal datum of the data set as a ISO 8601 conform date or date-time string, e.g. "2004-06-14" (date),"2004-06-14T23:34:30" (date-time) or "2004W28" (weeks)
        * @param dttext interval statement conform to ISO 8601 time periods, e.g. "P1D" (one day interval) or "P14D" (14 days)
        */
        SciDBSpatioTemporalArray(string t0text, string dttext)
            : SciDBSpatialArray(), SciDBTemporalArray(t0text, dttext) {}
    };
}

#endif