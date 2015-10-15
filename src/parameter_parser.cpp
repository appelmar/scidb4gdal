#include "parameter_parser.h"

# include "utils.h"
# include <boost/algorithm/string.hpp>
# include <boost/lexical_cast.hpp>
# include <boost/assign.hpp>

namespace scidb4gdal
{
    using namespace std;

    ParameterParser::ParameterParser() {
      init();
    }
    
    ParameterParser::ParameterParser(string scidbFile, char** optionKVP)
    {
      _scidb_filename = scidbFile;
      _options = optionKVP;
      if (!init()) {
	throw SCIDB_PARSING_ERROR;
      }  
    }


// 	parseQueryString();
// 	parseCreateString();
    
    void ParameterParser::parseConnectionString ( const string &connstr, ConnectionPars *con) {
      map<string,ConStringParameter> paramResolver = map_list_of ("host", HOST)("port", PORT) ("array", ARRAY) ("user",USER) ("password", PASSWORD);
      
      vector<string> connparts;
      // Split at whitespace, comma, semicolon
      boost::split ( connparts, connstr, boost::is_any_of ( "; " ) );
      for ( vector<string>::iterator it = connparts.begin(); it != connparts.end(); ++it ) {
	vector<string> kv;
	// No colon because uf URL
	boost::split ( kv, *it, boost::is_any_of ( "=" ) );
	if ( kv.size() != 2 ) {
	  continue;
	} else {
	  if(paramResolver.find(string(kv[0])) != paramResolver.end()) {
	    switch(paramResolver[kv[0]]) {
	      case HOST:
		con->host  = ( kv[1] );
		con->ssl = (kv[1].substr ( 0, 5 ).compare ( "https" ) == 0 );
		break;
	      case PORT:
		con->port = boost::lexical_cast<int> ( kv[1] );
		break;
	      case ARRAY:
		con->arrayname = ( kv[1] );
		break;
	      case USER:
		con->user = ( kv[1] );
		break;
	      case PASSWORD:
		con->passwd = ( kv[1] );
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
   
   void ParameterParser::parseOpeningOptions (char** options, ConnectionPars* con) {
      map<string,ConStringParameter> paramResolver = map_list_of ("host", HOST)("port", PORT) ("user",USER) ("password", PASSWORD) ("ssl",SSL);
      
      //char** options = poOpenInfo->papszOpenOptions;
      int count = CSLCount(options);
      for (int i = 0; i < count; i++) {
	const char* s = CSLGetField(options,i);
	char* key;
	const char* value = CPLParseNameValue(s,&key);
	
	if(paramResolver.find(std::string(key)) != paramResolver.end()) {
	  switch(paramResolver[std::string(key)]) {
	    case HOST:
	      con->host = value;
	      con->ssl = (std::string(value).substr ( 0, 5 ).compare ( "https" ) == 0 );
	      break;
	    case USER:
	      con->user = value;
	      break;
	    case PORT:
	      con->port = boost::lexical_cast<int>(value);
	      break;
	    case PASSWORD:
	      con->passwd = value;
	      break;
	    case SSL:
	      con->ssl = CSLTestBoolean(value);
	      break;
	    default:
	      continue;
	  }
	} else {
	  Utils::debug("unused parameter \""+string(key)+ "\" with value \""+string(value)+"\"");
	}
      }
    }
    
    bool ParameterParser::splitPropertyString (string &input, string &constr, string &propstr) {
      	string propToken = "properties=";
	int start = input.find(propToken);
	bool found = (start >= 0);

	//if there then split it accordingly and fill the ConnectionPars and the SelectProperties
	if (found) {
	  // if we find the 'properties=' in the connection string we treat those string part as
	  // the database open parameters 
	  int end = start + propToken.length();
	  constr = input.substr(0,start-1);
	  propstr = input.substr(end,input.length()-1);
	  
	} else {
	  constr = input;
	}
	return found;
    }

    void ParameterParser::parsePropertiesString ( const string &propstr, QueryParameters* query ) {
      //mapping between the constant variables and their string representation for using a switch/case statement later
      std::map<string,Properties> propResolver = map_list_of ("t",T_INDEX);
      vector<string> parts;
      boost::split ( parts, propstr, boost::is_any_of ( ";" ) ); // Split at semicolon and comma for refering to a whole KVP
      //example for filename with properties variable    "src_win:0 0 50 50;..."
      for ( vector<string>::iterator it = parts.begin(); it != parts.end(); ++it ) {
	vector<string> kv, c;
	boost::split ( kv, *it, boost::is_any_of ( ":=" ) ); 
	if ( kv.size() < 2 ) {
	  continue;
	} else {
	  if(propResolver.find(std::string(kv[0])) != propResolver.end()) {
	    switch(propResolver[kv[0]]) {
	      case T_INDEX:
		query->temp_index = boost::lexical_cast<int>(kv[1]);
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
    }
    
    void ParameterParser::parseArrayName(string& array, QueryParameters* query) {
      // 	  string array = pars->arrayname;
      size_t length = array.size();
      size_t t_start = array.find_first_of('[');
      size_t t_end = array.find_last_of(']');
      
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
	//first part is the dimension identifier
	query->dim_name = kv[0];
	//seceond part is either a temporal index (or temporal index interval) or a timestamp is used (or timestamp interval)
	string t_interval = kv[1];
	
	/* Test with Regex, failed due to gcc version 4.8.4 (4.9 implements regex completely) */
	// 	    std::smatch match;
	// 	    
	// 	    std::regex re("/^[\d]{4}(\/|-)[\d]{2}(\/|-)[\d]{2}(\s|T)[\d]{2}:[\d]{2}:[\d]{2}((\+|-)[0-1][\d]:?(0|3)0)?$/");
	// 	    if (std::regex_search(t_interval,match,re)) {
	// 	      ...
	// 	    }
	
	if (t_interval.length() > 0) {
	  //if string is a timestamp then translate it into the temporal index
	  if (Utils::validateTimestampString(t_interval)) {
	    //store timestamp for query when calling scidb
	    query->timestamp = t_interval;
	    query->hasTemporalIndex = false;
	  } else {                                               
	    //simply extract the temporal index
	    size_t pos = t_interval.find(":");
	    if (pos < t_interval.length()) {
	      //interval
	      vector<string> attr;
	      boost::split(attr,t_interval,boost::is_any_of(":"));
	      query->lower_bound = boost::lexical_cast<int>(attr[0]);
	      query->upper_bound = boost::lexical_cast<int>(attr[1]);
	      //TODO for now we parse it correctly, but we will just use the lower_bound... change this later
	      Utils::debug("Currently interval query is not supported. Using the lower bound instead");
	      query->temp_index = query->lower_bound;
	    } else {
	      //temporal index
	      query->temp_index = boost::lexical_cast<int>(t_interval);
	    }
	    query->hasTemporalIndex = true;
	  }
	} else {
	  Utils::error("No temporal information stated");
	  return;
	}
      }
    }
    
    ConnectionPars* ParameterParser::getConnectionParameter()
    {
      return _con;
    }
    
    QueryParameters* ParameterParser::getQueryParameter()
    {
      return _query;
    }
    
    bool ParameterParser::init()
    {
      if (!isValid()) return false;
      
      _query = new QueryParameters();
      _con = new ConnectionPars();
      
      string connectionString, propertiesString;
	
      if (splitPropertyString(_scidb_filename, connectionString, propertiesString)){
	parsePropertiesString(propertiesString, _query);
      }
      
      //first extract information from connection string and afterwards check for the opening options and overwrite values if double
      parseConnectionString(connectionString, _con);
      parseOpeningOptions(_options, _con);
      
      parseArrayName(_con->arrayname, _query); //array name will be modified if a temporal query is detected (for the ConnectionPars)
      
      return true;
    }
    
    bool ParameterParser::isValid()
    {
	if ( _scidb_filename == "" || _scidb_filename.substr ( 0, 6 ).compare ( "SCIDB:" ) != 0 ) return false;
	_scidb_filename = _scidb_filename.substr ( 6, _scidb_filename.length() - 6 ); // Remove SCIDB: from connection string
	return true;
    }


}