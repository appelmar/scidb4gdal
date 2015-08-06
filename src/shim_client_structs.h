
#ifndef SHIM_CLIENT_STRUCTS_H
#define SHIM_CLIENT_STRUCTS_H

#include "utils.h"
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/assign.hpp>


using namespace boost::assign;

namespace scidb4gdal {

    enum ConStringParameter {
      HOST, ARRAY, PORT,USER, PASSWORD, SSL
    };

    enum Properties {
      T_INDEX
    };
    
    struct TemporalQueryParameters {
	  int temp_index = -1;
	  int lower_bound;
	  int upper_bound;
	  string dim_name;
	  
	  static TemporalQueryParameters* parsePropertiesString ( const string &propstr ) {
	      TemporalQueryParameters *properties = new TemporalQueryParameters();
	      //mapping between the constant variables and their string representation for using a switch/case statement later
	      std::map<string,Properties> propResolver = map_list_of ("t",T_INDEX);
	      vector<string> parts;
	      boost::split ( parts, propstr, boost::is_any_of ( ";" ) ); // Split at semicolon and comma for refering to a whole KVP
	      //example for filename with properties variable    "src_win:0 0 50 50;..."
	      for ( vector<string>::iterator it = parts.begin(); it != parts.end(); ++it ) {
		  vector<string> kv, c;
		  boost::split ( kv, *it, boost::is_any_of ( ":=" ) ); 
		  if ( kv.size() != 2 ) {
		      continue;
		  }
		  else {
		    if(propResolver.find(std::string(kv[0])) != propResolver.end()) {
		      switch(propResolver[kv[0]]) {
			  case T_INDEX:
			      properties->temp_index = boost::lexical_cast<int>(kv[1]);
			    //TODO maybe we allow also selecting multiple slices (that will later be saved separately as individual files)
			    break;
			  default:
			    continue;
			    
		      }
		    } else {
			Utils::debug("unused parameter \""+string(kv[0])+ "\" with value \""+string(kv[1])+"\"");
		    }
		  }
	      }
	      
	      return properties;
	}
	
	void parseArrayName (string& array) {
// 	  string array = pars->arrayname;
	  size_t length = array.size();
	  size_t t_start = array.find_first_of('<');
	  size_t t_end = array.find_last_of('>');
	  if (t_start < 0 || t_end < 0) {
	    Utils::error("No or invalid temporal information in array name.");
	    return;
	  }
	  //extract temporal string
	  if ((length-1) == t_end) {
	    string t_expression = array.substr(t_start+1,(t_end-t_start)-1);
	    //remove the temporal part of the array name otherwise it messes up the concrete array name in scidb
	    string temp = array.substr(0,t_start);
	    array = temp;
	    vector<string> kv;
	    boost::split(kv, t_expression, boost::is_any_of(","));
	    if (kv.size() < 2) {
	      Utils::error("Temporal query not complete. Please state the dimension name and the temporal index / interval");
	      return;
	    }
	    this->dim_name = kv[0];
	    string t_interval = kv[1];
	    if (t_interval.length() > 0) {
	      size_t pos = t_interval.find(":");
	      if (pos < t_interval.length()) {
		//interval
		vector<string> attr;
		boost::split(attr,t_interval,boost::is_any_of(":"));
		this->lower_bound = boost::lexical_cast<int>(attr[0]);
		this->upper_bound = boost::lexical_cast<int>(attr[1]);
		//TODO for now we parse it correctly, but we will just use the lower_bound... change this later
		Utils::debug("Currently interval query is not supported. Using the lower bound instead");
		this->temp_index = this->lower_bound;
	      } else {
		//temporal index
		this->temp_index = boost::lexical_cast<int>(t_interval);
	      }
	    } else {
		Utils::error("No temporal information stated");
		return;
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

        void parseConnectionString ( const string &connstr) {
	    std::map<string,ConStringParameter> paramResolver = map_list_of ("host", HOST)("port", PORT) ("array", ARRAY) ("user",USER) ("password", PASSWORD);
	    
	    vector<string> connparts;
            boost::split ( connparts, connstr, boost::is_any_of ( "; " ) ); // Split at whitespace, comma, semicolon
            for ( vector<string>::iterator it = connparts.begin(); it != connparts.end(); ++it ) {
                vector<string> kv;
                boost::split ( kv, *it, boost::is_any_of ( "=" ) ); // No colon because uf URL
                if ( kv.size() != 2 ) {
                    continue;
                } else {
		    if(paramResolver.find(std::string(kv[0])) != paramResolver.end()) {
			switch(paramResolver[kv[0]]) {
			    case HOST:
			      host  = ( kv[1] );
			      ssl = (kv[1].substr ( 0, 5 ).compare ( "https" ) == 0 );
			      break;
			    case PORT:
			      port = boost::lexical_cast<int> ( kv[1] );
			      break;
			    case ARRAY:
			      arrayname = ( kv[1] );
			      break;
			    case USER:
			      user = ( kv[1] );
			      break;
			    case PASSWORD:
			      passwd = ( kv[1] );
			      break;
			    default:
			      Utils::debug("haven't found stuff");
			      continue; 
			}
		    } else {
			Utils::debug("unused parameter \""+string(kv[0])+ "\" with value \""+string(kv[1])+"\"");
		    }
                }
            }
        }

	void parseOpeningOptions (GDALOpenInfo *poOpenInfo) {
	    std::map<string,ConStringParameter> paramResolver = map_list_of ("host", HOST)("port", PORT) ("user",USER) ("password", PASSWORD) ("ssl",SSL);
// 	    char* filename = poOpenInfo->pszFilename;
// 	    Utils::debug(filename);
// 	    this->parseConnectionString(filename);
	    
	    char** options = poOpenInfo->papszOpenOptions;
	    int count = CSLCount(options);
	    for (int i = 0; i < count; i++) {
		const char* s = CSLGetField(options,i);
		char* key;
		const char* value = CPLParseNameValue(s,&key);
		
		if(paramResolver.find(std::string(key)) != paramResolver.end()) {
		  switch(paramResolver[std::string(key)]) {
		    case HOST:
		      host = value;
		      ssl = (std::string(value).substr ( 0, 5 ).compare ( "https" ) == 0 );
		      break;
		    case USER:
		      user = value;
		      break;
		    case PORT:
		      port = boost::lexical_cast<int>(value);
		      break;
		    case PASSWORD:
		      passwd = value;
		      break;
		    case SSL:
		      ssl = CSLTestBoolean(value);
		      break;
		    default:
		      continue;
		  }
		} else {
		    Utils::debug("unused parameter \""+string(key)+ "\" with value \""+string(value)+"\"");
		}
	    }

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