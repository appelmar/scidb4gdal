
#ifndef SHIM_CLIENT_STRUCTS_H
# define SHIM_CLIENT_STRUCTS_H

# include "utils.h"
# include <boost/algorithm/string.hpp>
# include <boost/lexical_cast.hpp>
# include <boost/assign.hpp>

using namespace boost::assign;

namespace scidb4gdal {
  
  /**
   * @brief enumeration for parsing connection string parameter
   * 
   * This enumeration mainly works as keys to reference to the parameter names and identifies the 
   * properties of the scidb4gdal::ConnectionParameters. It means that the connection string will be
   * split according to the strings that are mapped to this key, e.g. (HOST, "host"), (ARRAY, "array"),...
   * 
   * It relates to the connection string "host=http://a.b.c port=8080 " where the Keys correspond to the keys used
   * there.
   * 
   * @see scidb4gdal::ParameterParser
   */
  enum ConnectionStringKey {
    /** the host key */
    HOST, 
    /** the key for the array name */
    ARRAY, 
    /** the key for the server port */
    PORT,
    /** the key for the user name */
    USER, 
    /** the key for the users password */
    PASSWORD, 
    /** the key for using SSL encryption */
    SSL,
    /** the key to confirm the delete preocess */
    CONFIRM_DELETE
  };
  
  /**
   * Enum for a switch-case statement when parsing the properties part of a connection string
   */
  enum Properties {
    T_INDEX, TRS, TIMESTAMP, TYPE, BBOX, SRS,CHUNKSIZE_SPATIAL, CHUNKSIZE_TEMPORAL
  };
  
  /** 
   * @brief The informal array type that shall be created
   * 
   * An enumeration for the type of array that shall be created in SciDB. This parameter will
   * be set from user input by the "type=" statement
   * 
   * @see scidb4gdal::ParameterParser
   */
  enum CreationType {
    /** key for a spatial array */
    S_ARRAY, 
    /** key for a spatio-temporal array */
    ST_ARRAY, 
    /** key for a spatio-temporal series */
    ST_SERIES
  };
  
  /**
   * Empty structure to create an semantical inheritance for parameter
   * structures.
   */
  struct Parameters {
    
  };
  
  /**
   * @brief Structure to store the information of the user input
   */
  struct CreationParameters :  Parameters {
    /** the temporal resolution as an ISO 8601 string */
    string dt;
    /** the timestamp of the image as an ISO 8601 string */
    string timestamp;
    /** the bounding box if a spatial coverage needs to be created */
    double bbox[4];
    /** the name of the authority which SRS definition is used, e.g. EPSG */
    string auth_name;
    /** the id of the SRS */
    int32_t srid;
    /** The type of array that shall be created */
    CreationType type;
    /** whether or not a bounding box was used */
    bool hasBBOX;
    /** error code if validation fails */
    StatusCode error;
    /** the chunksize for each of the spatial dimensions*/
    int chunksize_spatial = -1;
    /** the blocksize for the temporal dimension */
    int chunksize_temporal = -1;
    
    /**
     * @brief validates the parameters for completenes
     * 
     * This function evaluates the creation parameters, for example if a BBOX was stated, then
     * there needs to be a statement about the authority and the id of the reference system.
     * 
     * @return bool whether or not the parameters are complete and valid
     */
    bool isValid() {
	bool valid = true;
	
	if (hasBBOX && (auth_name == "" || srid == 0)) {
	  valid = false;
	  error = ERR_READ_BBOX_SRS_MISSING;
	}
	
	return valid;
    }
  };
  
  /**
   * @brief Structure to store information of a query to retrieve data from the user input
   * 
   * This structure will be filled by using the ParameterParser, where the user input is parsed and
   * evaluated. This structure stores the information about querying or retrieving data from SciDB. 
   * The temporal index needs to be set if a temporally referenced image is queryied. This happens either
   * by stating a temporal index directly or by stating the timestamp string from which the index is
   * calculated.
   * 
   * @see scidb4gdal::ParameterParser
   */
  struct QueryParameters :  Parameters {
    /** the temporal index of an temporally referenced array */
    int temp_index;
    /** the lower bound of an interval query (currently not implemented) */
    int lower_bound;
    /** the upper bound of an interval query (currently not implemented) */
    int upper_bound;
    /** the dimension name pf the temporal axis */
    string dim_name;
    /** the ISO 8601 data/time string to query for */
    string timestamp;
    /** flag whether or not the temporal index was set */
    bool hasTemporalIndex;
    
    QueryParameters(): temp_index(-1) {}
  };
  
  /**
   * @brief Structure to store information to access the SHIM client based on user input
   * 
   * Structure to pass and store all the connection parameters defined in the connection string that
   * was passed to a gdal function using the filename
   * 
   * @see scidb4gdal::ParameterParser
   */
  struct ConnectionParameters :  Parameters {
    /** the array name */
    string arrayname;
    /** the url of the host */
    string host;
    /** the server port */
    int port;
    /** the user name */
    string user;
    /** the password of the stated user */
    string passwd;
    /** flag for using SSL encryption */
    bool ssl;
    /** error code if the connection parameters are invalid */
    int error_code;
    
    /** Flag whether or not to delete the Array. This value is used to prevent QuietDelete() */
    bool deleteArray;
    
    /** 
     * Default constructor to create empty connection parameters
     */
    ConnectionParameters() : arrayname ( "" ), host ( "" ), port ( 0 ), user ( "" ), passwd ( "" ), deleteArray (false) {}

    /**
     * @brief Represents the connection parameter in string form
     * 
     * Creates a string representation of the connection parameter similar to the connection string.
     * 
     * @return std::string The string representation
     */
    string toString() {
      stringstream s;
      s << "array=" << arrayname << " host=" << host << " port=" << port << "  user=" << user << " passwd=" << passwd;
      return s.str();
    };
    
    bool isValid() {
      bool valid = true;
      if (arrayname == "" || host == "") {
	error_code = ERR_READ_ARRAYUNKNOWN;
	valid = false;
      }
      
      return valid;
    };
  };
  
  /**
   * @brief Helper structure filled while fetching scidb binary stream
   * 
   * This structure is used to fetch the binary stream of data and point to the memory allocation
   * space for that data. The SingleAttributeChunks are later merged into on image
   * 
   */
  struct SingleAttributeChunk {
    char *memory;
    size_t size;
    

//     template <typename T> T get ( int64_t i ) {
//       return ( ( T * ) memory ) [i]; // No overflow checks!
//     }
  };
}
#endif