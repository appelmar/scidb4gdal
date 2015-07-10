
#ifndef SHIM_CLIENT_STRUCTS_H
#define SHIM_CLIENT_STRUCTS_H

#include "utils.h"
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/assign.hpp>


using namespace boost::assign;

namespace scidb4gdal {

  enum ConStringParameter {
    HOST, ARRAY, PORT,USER, PASSWORD
  };

  enum Properties {
    SRC_WIN, T_INDEX
  };
  
  struct SelectProperties {
	float src_coords [4]; //usually image coordinates in the order xmin,ymin,xsize,ysize
	int temp_index;
	
	static void parsePropertiesString (SelectProperties *properties,  const string &propstr ) {
	    std::map<string,Properties> propResolver = map_list_of ("src_win", SRC_WIN)("t",T_INDEX);
            vector<string> parts;
            boost::split ( parts, propstr, boost::is_any_of ( ";," ) ); // Split at semicolon and comma for refering to a whole KVP
	    //example for filename with properties variable    "src_win:0 0 50 50;..."
            for ( vector<string>::iterator it = parts.begin(); it != parts.end(); ++it ) {
                vector<string> kv, c;
                boost::split ( kv, *it, boost::is_any_of ( ":=" ) ); 
                if ( kv.size() != 2 ) {
                    continue;
                }
                else {
		  switch(propResolver[kv[0]]) {
		      case SRC_WIN:
			boost::split ( c, kv[1], boost::is_any_of ( " " ) ); 
			
			for (int i = 0; i < c.size(); i++) {
			  properties->src_coords[i] = boost::lexical_cast<float> ( c[i] );
			}
			
			break;
		      case T_INDEX:
			  properties->temp_index = boost::lexical_cast<int>(kv[1]);
			//TODO maybe we allow also selecting multiple slices (that will later be saved separately as individual files)
			break;
		      default:
			continue;
			
		  }
                }
            }
      }
    };
    
    /**
     * Structure to pass and store all the connection parameters defined in the connection string that
     * was passed to a gdal function using the filename
     */
    struct ConnectionPars {

        string arrayname;
        string host;
        int port;
        string user;
        string passwd;
        bool ssl;
	

        ConnectionPars() : arrayname ( "" ), host ( "https://localhost" ), port ( 8083 ), user ( "scidb" ), passwd ( "scidb" ) {}

        string toString() {
            stringstream s;
            s << "array= " << arrayname << " host=" << host << " port=" << port << "  user=" << user << " passwd=" << passwd;
            return s.str();
        }

        static ConnectionPars *parseConnectionString ( const string &connstr, SelectProperties *properties ) {
	    std::map<string,ConStringParameter> paramResolver = map_list_of ("host", HOST)("port", PORT) ("array", ARRAY) ("user",USER) ("password", PASSWORD);
            if ( connstr.substr ( 0, 6 ).compare ( "SCIDB:" ) != 0 ) {
                Utils::error ( "This is not a scidb4gdal connection string" );
            }

            ConnectionPars *out = new ConnectionPars();

            string astr = connstr.substr ( 6, connstr.length() - 6 ); // Remove SCIDB: from connection string
            //first split the connection string from the properties
	    string connectionString, propertiesString;
	    string propToken = "properties=";
	    int start = astr.find(propToken);
	    bool found = (start >= 0);
	    
	    if (found) {
	      // if we find the 'properties=' in the connection string we treat those string part as
	      // the database open parameters 
	      //TODO replace this somehow with the GDAL -oo options
	      int end = start + propToken.length();
	      connectionString = astr.substr(0,start-1);
	      propertiesString = astr.substr(end,astr.length()-1);
	      SelectProperties::parsePropertiesString(properties, propertiesString);
	    } else {
	      connectionString = astr;
	    }
	    
	    //part 1 is the connection string
	    vector<string> connparts;
            boost::split ( connparts, connectionString, boost::is_any_of ( ",; " ) ); // Split at whitespace, comma, semicolon
            for ( vector<string>::iterator it = connparts.begin(); it != connparts.end(); ++it ) {
                vector<string> kv;
                boost::split ( kv, *it, boost::is_any_of ( "=" ) ); // No colon because uf URL
                if ( kv.size() != 2 ) {
                    continue;
                }
                else {
		  switch(paramResolver[kv[0]]) {
		      case HOST:
			out->host  = ( kv[1] );
			out->ssl = (kv[1].substr ( 0, 5 ).compare ( "https" ) == 0 );
			break;
		      case PORT:
			out->port = boost::lexical_cast<int> ( kv[1] );
			break;
		      case ARRAY:
			out->arrayname = ( kv[1] );
			break;
		      case USER:
			out->user = ( kv[1] );
			break;
		      case PASSWORD:
			out->passwd = ( kv[1] );
			break;
		      default:
			continue;
			
		  }
                }
            }

            return out;

        }


    };
    
    /**
     * Helper structure filled while fetching scidb binary stream
     */
    struct SingleAttributeChunk {
        char *memory;
        size_t size;

        /*
            template <typename T> T get ( int64_t i ) {
                return ( ( T * ) memory ) [i]; // No overflow checks!
            }*/
    };
}
#endif