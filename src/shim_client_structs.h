
#ifndef SHIM_CLIENT_STRUCTS_H
# define SHIM_CLIENT_STRUCTS_H
#define ERROR_NO_ARRAYNAME 1002

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
    T_INDEX, TRS, TIMESTAMP
  };
  
  struct Parameters {
    
  };
  
  struct CreationParameters :  Parameters{
    string trs;
    string timestamp;
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
    
    
    ConnectionParameters() : arrayname ( "" ), host ( "https://localhost" ), port ( 8083 ), user ( "scidb" ), passwd ( "scidb" ) {}
    
    string toString() {
      stringstream s;
      s << "array= " << arrayname << " host=" << host << " port=" << port << "  user=" << user << " passwd=" << passwd;
      return s.str();
    };
    
    bool isComplete() {
      if (arrayname == "") {
	error_code = ERROR_NO_ARRAYNAME;
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