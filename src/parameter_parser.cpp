#include "parameter_parser.h"

# include "utils.h"
# include <boost/algorithm/string.hpp>
# include <boost/lexical_cast.hpp>
# include <boost/assign.hpp>

namespace scidb4gdal
{
    using namespace std;
    
    ParameterParser::ParameterParser(string scidbFile, char** optionKVP,  SciDBOperation op) :
      _operation (op)
    {
      _scidb_filename = scidbFile;
      _options = optionKVP;
      if (!init()) {
	throw SCIDB_PARSING_ERROR;
      }  
    }
    
    void ParameterParser::parseConnectionString () {
      map<string,ConStringParameter> paramResolver = map_list_of ("host", HOST)("port", PORT) ("array", ARRAY) ("user",USER) ("password", PASSWORD);
      
      vector<string> connparts;
      // Split at whitespace, comma, semicolon
      boost::split ( connparts, _connection_string, boost::is_any_of ( "; " ) );
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
		_con->host  = ( kv[1] );
		_con->ssl = (kv[1].substr ( 0, 5 ).compare ( "https" ) == 0 );
		break;
	      case PORT:
		_con->port = boost::lexical_cast<int> ( kv[1] );
		break;
	      case ARRAY:
		_con->arrayname = ( kv[1] );
		break;
	      case USER:
		_con->user = ( kv[1] );
		break;
	      case PASSWORD:
		_con->passwd = ( kv[1] );
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
   
   void ParameterParser::parseOpeningOptions () {
      map<string,ConStringParameter> paramResolver = map_list_of ("host", HOST)("port", PORT) ("user",USER) ("password", PASSWORD) ("ssl",SSL);

      int count = CSLCount(_options);
      for (int i = 0; i < count; i++) {
	const char* s = CSLGetField(_options,i);
	char* key;
	const char* value = CPLParseNameValue(s,&key);
	
	if(paramResolver.find(std::string(key)) != paramResolver.end()) {
	  switch(paramResolver[std::string(key)]) {
	    case HOST:
	      _con->host = value;
	      _con->ssl = (std::string(value).substr ( 0, 5 ).compare ( "https" ) == 0 );
	      break;
	    case USER:
	      _con->user = value;
	      break;
	    case PORT:
	      _con->port = boost::lexical_cast<int>(value);
	      break;
	    case PASSWORD:
	      _con->passwd = value;
	      break;
	    case SSL:
	      _con->ssl = CSLTestBoolean(value);
	      break;
	    default:
	      continue;
	  }
	} else {
	  Utils::debug("unused parameter \""+string(key)+ "\" with value \""+string(value)+"\"");
	}
      }
    }
    
    void ParameterParser::parseCreateOptions(){
      map<string,ConStringParameter> paramResolver = map_list_of ("host", HOST)("port", PORT) ("user",USER) ("password", PASSWORD) ("ssl",SSL);
      map<string,Properties> propResolver = map_list_of ("trs",TRS) ("timestamp",TIMESTAMP);
      int count = CSLCount(_options);
      for (int i = 0; i < count; i++) {
	const char* s = CSLGetField(_options,i);
	char* key;
	const char* value = CPLParseNameValue(s,&key);
	
	if(paramResolver.find(std::string(key)) != paramResolver.end()) {
	  switch(paramResolver[std::string(key)]) {
	    case HOST:
	      _con->host = value;
	      _con->ssl = (std::string(value).substr ( 0, 5 ).compare ( "https" ) == 0 );
	      break;
	    case USER:
	      _con->user = value;
	      break;
	    case PORT:
	      _con->port = boost::lexical_cast<int>(value);
	      break;
	    case PASSWORD:
	      _con->passwd = value;
	      break;
	    case SSL:
	      _con->ssl = CSLTestBoolean(value);
	      break;
	  }
	} else {
	  if(propResolver.find(std::string(key)) != propResolver.end()) {
	    switch(propResolver[std::string(key)]) {
	      case TRS:
		_create->trs = value;
		break;
	      case TIMESTAMP:
		_create->timestamp = value;
		break;
	      default:
		continue;
	    }
	  } else {
	    Utils::debug("unused parameter \""+string(key)+ "\" with value \""+string(value)+"\"");
	  }
	}
      }
    }

    
    bool ParameterParser::splitPropertyString () {
      	string propToken = "properties=";
	int start = _scidb_filename.find(propToken);
	bool found = (start >= 0);

	//if there then split it accordingly and fill the ConnectionPars and the SelectProperties
	if (found) {
	  // if we find the 'properties=' in the connection string we treat those string part as
	  // the database open parameters 
	  int end = start + propToken.length();
	  _connection_string = _scidb_filename.substr(0,start-1);
	  _properties_string = _scidb_filename.substr(end,_scidb_filename.length()-1);
	  
	} else {
	  _connection_string = _scidb_filename;
	}
	return found;
    }

    void ParameterParser::parsePropertiesString () {
      //mapping between the constant variables and their string representation for using a switch/case statement later
      std::map<string,Properties> propResolver = map_list_of ("t",T_INDEX);
      
      vector<string> parts;
      boost::split ( parts, _properties_string, boost::is_any_of ( ";" ) ); // Split at semicolon and comma for refering to a whole KVP
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
		_query->temp_index = boost::lexical_cast<int>(kv[1]);
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
    
    void ParameterParser::parseArrayName() {
      // 	  string array = pars->arrayname;
      size_t length = _con->arrayname.size();
      size_t t_start = _con->arrayname.find_first_of('[');
      size_t t_end = _con->arrayname.find_last_of(']');
      
      if (t_start < 0 || t_end < 0) {
	Utils::error("No or invalid temporal information in array name.");
	return;
      }
      //extract temporal string
      if ((length-1) == t_end) {
	string t_expression = _con->arrayname.substr(t_start+1,(t_end-t_start)-1);
	//remove the temporal part of the array name otherwise it messes up the concrete array name in scidb
	string temp = _con->arrayname.substr(0,t_start);
	_con->arrayname = temp;
	vector<string> kv;
	
	boost::split(kv, t_expression, boost::is_any_of(","));
	if (kv.size() < 2) {
	  Utils::error("Temporal query not complete. Please state the dimension name and the temporal index / interval");
	  return;
	}
	//first part is the dimension identifier
	_query->dim_name = kv[0];
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
	    _query->timestamp = t_interval;
	    _query->hasTemporalIndex = false;
	  } else {                                               
	    //simply extract the temporal index
	    size_t pos = t_interval.find(":");
	    if (pos < t_interval.length()) {
	      //interval
	      vector<string> attr;
	      boost::split(attr,t_interval,boost::is_any_of(":"));
	      _query->lower_bound = boost::lexical_cast<int>(attr[0]);
	      _query->upper_bound = boost::lexical_cast<int>(attr[1]);
	      //TODO for now we parse it correctly, but we will just use the lower_bound... change this later
	      Utils::debug("Currently interval query is not supported. Using the lower bound instead");
	      _query->temp_index = _query->lower_bound;
	    } else {
	      //temporal index
	      _query->temp_index = boost::lexical_cast<int>(t_interval);
	    }
	    _query->hasTemporalIndex = true;
	  }
	} else {
	  Utils::error("No temporal information stated");
	  return;
	}
      }
    }
    
    ConnectionParameters& ParameterParser::getConnectionParameter() {
      return *_con;
    }
    
    QueryParameters& ParameterParser::getQueryParameter(){
      return *_query;
    }
    
    CreationParameters& ParameterParser::getCreationParameter(){
      return *_create;
    }

    void ParameterParser::validate(){
	if ( _scidb_filename == "" || _scidb_filename.substr ( 0, 6 ).compare ( "SCIDB:" ) != 0 ) {
	  _isValid = false;
	  return;
	}
	
	_scidb_filename = _scidb_filename.substr ( 6, _scidb_filename.length() - 6 ); // Remove SCIDB: from connection string
	_isValid = true;
    }

    bool ParameterParser::init(){
      _isValid = false;
      validate();
      
      if (!isValid()) return false;
      _con = new ConnectionParameters();  
      if (_operation == SCIDB_OPEN) {
	_query = new QueryParameters();
      }
      if (_operation == SCIDB_CREATE) {
	
	_create = new CreationParameters();
      }
      
      // if the connection string has an additional properties string then split it from the connection string
      if (splitPropertyString()){
	parsePropertiesString();
      }
      
      //first extract information from connection string and afterwards check for the opening options and overwrite values if double
      parseConnectionString();
      
      if (_operation == SCIDB_OPEN) {
	parseOpeningOptions();
	parseArrayName(); //array name will be modified if a temporal query is detected (for the ConnectionPars)
      }
      if (_operation == SCIDB_CREATE) {
	parseCreateOptions();
      }
      return true;
    }
    
    bool ParameterParser::isValid(){
	return _isValid;
    }


}