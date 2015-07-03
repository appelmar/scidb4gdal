
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
    SRC_WIN
  };
  
  struct ImageProperties {
	float src_coords [4]; //usually image coordinates in the order xmin,ymin,xsize,ysize
	
	static ImageProperties *parsePropertiesString ( const string &propstr ) {
	    std::map<string,Properties> propResolver = map_list_of ("src_win", SRC_WIN);
	    ImageProperties *out = new ImageProperties();
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
			  out->src_coords[i] = boost::lexical_cast<float> ( c[i] );
			}
			
			break;
		      default:
			continue;
			
		  }
                }
            }
            
            return out;
      }
    };
    
    struct ConnectionPars {

        string arrayname;
        string host;
        int port;
        string user;
        string passwd;
        bool ssl;
	ImageProperties *properties;

        ConnectionPars() : arrayname ( "" ), host ( "https://localhost" ), port ( 8083 ), user ( "scidb" ), passwd ( "scidb" ) {}

        string toString() {
            stringstream s;
            s << "array= " << arrayname << " host=" << host << " port=" << port << "  user=" << user << " passwd=" << passwd;
            return s.str();
        }

        static ConnectionPars *parseConnectionString ( const string &connstr ) {
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
	      out->properties = ImageProperties::parsePropertiesString(propertiesString);
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
}
#endif