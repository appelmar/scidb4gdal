
#ifndef SHIM_CLIENT_STRUCTS_H
# define SHIM_CLIENT_STRUCTS_H

# include "utils.h"
# include <boost/algorithm/string.hpp>
# include <boost/lexical_cast.hpp>
# include <boost/assign.hpp>

using namespace boost::assign;

namespace scidb4gdal {
  
  /**
   * Enum for allowed parameter keys in the connection string for switch-case statement when parsing
   */
  enum ConStringParameter {
    HOST, ARRAY, PORT,USER, PASSWORD, SSL
  };
  
  /**
   * Enum for a switch-case statement when parsing the properties part of a connection string
   */
  enum Properties {
    T_INDEX, TRS, TIMESTAMP, TYPE, BBOX, SRS
  };
  
  enum CreationType {
    S_ARRAY, ST_ARRAY, ST_SERIES
  };
  
  struct Parameters {
    
  };
  
  struct CreationParameters :  Parameters {
    string dt;
    string timestamp;
    double bbox[4];
    string auth_name;
    int32_t srid;
    CreationType type;
    bool hasBBOX;
    
    StatusCode error;
    
    bool isValid() {
	bool valid = true;
	
	if (hasBBOX && (auth_name == "" || srid == 0)) {
	  valid = false;
	  error = ERR_READ_BBOX_SRS_MISSING;
	}
	
	return valid;
    }
  };
  
  struct QueryParameters :  Parameters {
    int temp_index;
    int lower_bound;
    int upper_bound;
    string dim_name;
    string timestamp;
    bool hasTemporalIndex;
    
    QueryParameters(): temp_index(-1) {}
  };
  
  /**
   * Structure to pass and store all the connection parameters defined in the connection string that
   * was passed to a gdal function using the filename
   */
  struct ConnectionParameters :  Parameters {
    
    string arrayname;
    string host;
    int port;
    string user;
    string passwd;
    bool ssl;
    int error_code;
    
    
    ConnectionParameters() : arrayname ( "" ), host ( "" ), port ( 0 ), user ( "" ), passwd ( "" ) {}
    
    string toString() {
      stringstream s;
      s << "array=" << arrayname << " host=" << host << " port=" << port << "  user=" << user << " passwd=" << passwd;
      return s.str();
    };
    
    bool isComplete() {
      if (arrayname == "" || host == "") {
	error_code = ERR_READ_ARRAYUNKNOWN;
	return false;
      }
      
      return true;
    };
  };
  
  /**
   * Helper structure filled while fetching scidb binary stream
   */
  struct SingleAttributeChunk {
    char *memory;
    size_t size;
    
    /*
     t emp*late <typename T> T get ( int64_t i ) {
     return ( ( T * ) memory ) [i]; // No overflow checks!
  }*/
  };
}
#endif