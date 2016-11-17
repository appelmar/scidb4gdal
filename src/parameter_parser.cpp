#include "parameter_parser.h"

#include "utils.h"
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/assign.hpp>
#include <string>

namespace scidb4gdal {

using namespace std;

template <typename T>
bool Resolver<T>::contains(string key) {
    return mapping.find(key) != mapping.end();
}

template <typename T>
T Resolver<T>::getKey(string s) { return mapping[s]; }

ParameterParser::ParameterParser(string scidbFile, char** optionKVP, SciDBOperation op)
    : _operation(op) {
    // 2016-11-17: VC++ 2013 complains about ambigous = operator with map_list_of()
    // set the resolver a.k.a the key value pairs in the connection / property
    // string or the create / opening options
    //_creationTypeResolver.mapping = map_list_of("S", S_ARRAY)("ST", ST_ARRAY)("STS", ST_SERIES);
    _creationTypeResolver.mapping.insert ( std::pair<string,CreationType>("S",S_ARRAY) );
    _creationTypeResolver.mapping.insert ( std::pair<string,CreationType>("ST",ST_ARRAY) );
    _creationTypeResolver.mapping.insert ( std::pair<string,CreationType>("STS",ST_SERIES) );
    
    // 2016-11-17: VC++ 2013 complains about ambigous = operator with map_list_of()
    //_propKeyResolver.mapping = map_list_of("dt", TRS)("timestamp", TIMESTAMP)(
    //    "t", TIMESTAMP)("type", TYPE)("i", T_INDEX)("bbox", BBOX)("srs", SRS)(
    //    "CHUNKSIZE_SP", CHUNKSIZE_SPATIAL)("chunksize_sp", CHUNKSIZE_SPATIAL)(
    //    "CHUNKSIZE_T", CHUNKSIZE_TEMPORAL)("chunksize_t", CHUNKSIZE_TEMPORAL);
    
    _propKeyResolver.mapping.insert ( std::pair<string,Properties>("dt",TRS) );
    _propKeyResolver.mapping.insert ( std::pair<string,Properties>("timestamp",TIMESTAMP) );
    _propKeyResolver.mapping.insert ( std::pair<string,Properties>("t",TIMESTAMP) );
    _propKeyResolver.mapping.insert ( std::pair<string,Properties>("type",TYPE) );
    _propKeyResolver.mapping.insert ( std::pair<string,Properties>("i",T_INDEX) );
    _propKeyResolver.mapping.insert ( std::pair<string,Properties>("bbox",BBOX) );
    _propKeyResolver.mapping.insert ( std::pair<string,Properties>("srs",SRS) );
    _propKeyResolver.mapping.insert ( std::pair<string,Properties>("CHUNKSIZE_SP",CHUNKSIZE_SPATIAL) );
    _propKeyResolver.mapping.insert ( std::pair<string,Properties>("chunksize_sp",CHUNKSIZE_SPATIAL) );
    _propKeyResolver.mapping.insert ( std::pair<string,Properties>("CHUNKSIZE_T",CHUNKSIZE_TEMPORAL) );
    _propKeyResolver.mapping.insert ( std::pair<string,Properties>("chunksize_t",CHUNKSIZE_TEMPORAL) );

    // 2016-11-17: VC++ 2013 complains about ambigous = operator with map_list_of()
    //_conKeyResolver.mapping = map_list_of("host", HOST)("port", PORT)(
    //    "user", USER)("password", PASSWORD)("ssl", SSL)("trust", SSLTRUST)("array", ARRAY)(
    //    "confirmDelete", CONFIRM_DELETE);

    _conKeyResolver.mapping.insert( std::pair<string,ConnectionStringKey>("host",HOST) );
    _conKeyResolver.mapping.insert( std::pair<string,ConnectionStringKey>("port",PORT) );
    _conKeyResolver.mapping.insert( std::pair<string,ConnectionStringKey>("user",USER) );
    _conKeyResolver.mapping.insert( std::pair<string,ConnectionStringKey>("password",PASSWORD) );
    _conKeyResolver.mapping.insert( std::pair<string,ConnectionStringKey>("ssl",SSL) );
    _conKeyResolver.mapping.insert( std::pair<string,ConnectionStringKey>("trust",SSLTRUST) );
    _conKeyResolver.mapping.insert( std::pair<string,ConnectionStringKey>("array",ARRAY) );
    _conKeyResolver.mapping.insert( std::pair<string,ConnectionStringKey>("confirmDelete",CONFIRM_DELETE) );
      
    _scidb_filename = scidbFile;
    _options = optionKVP;
    if (!init()) {
        throw ERR_GLOBAL_PARSE;
    }
}

void ParameterParser::parseConnectionString() {
    vector<string> connparts;
    // Split at whitespace, comma, semicolon
    boost::split(connparts, _connection_string, boost::is_any_of("; "));
    for (vector<string>::iterator it = connparts.begin(); it != connparts.end();
         ++it) {
        vector<string> kv;
        // No colon because uf URL
        boost::split(kv, *it, boost::is_any_of("="));
        if (kv.size() != 2) {
            continue;
        } else {
            if (_conKeyResolver.contains(string(kv[0]))) {
                assignConnectionParameter(kv[0], kv[1]);
            } else {
                Utils::debug("unused parameter \"" + string(kv[0]) + "\" with value \"" + string(kv[1]) + "\"");
            }
        }
    }
}

void ParameterParser::parseOpeningOptions() {
    int count = CSLCount(_options);
    for (int i = 0; i < count; i++) {
        const char* s = CSLGetField(_options, i);
        char* key;
        const char* value = CPLParseNameValue(s, &key);

        if (_conKeyResolver.contains(std::string(key))) {
            assignConnectionParameter(std::string(key), value);
        } else {
            if (_propKeyResolver.contains(std::string(key))) {
                assignQueryParameter(std::string(key), std::string(value));
            } else {
                Utils::debug("unused parameter \"" + string(key) + "\" with value \"" + string(value) + "\"");
            }
        }
    }
}

void ParameterParser::parseCreateOptions() {
    int count = CSLCount(_options);
    for (int i = 0; i < count; i++) {
        const char* s = CSLGetField(_options, i);
        char* key;
        const char* value = CPLParseNameValue(s, &key);

        if (_conKeyResolver.contains(std::string(key))) {
            assignConnectionParameter(std::string(key), value);
        } else {
            if (_propKeyResolver.contains(std::string(key))) {
                assignCreateParameter(std::string(key), value);
            } else {
                Utils::debug("unused parameter \"" + string(key) + "\" with value \"" + string(value) + "\"");
            }
        }
    }
}

bool ParameterParser::splitPropertyString() {
    string propToken = "properties=";
    int start = _scidb_filename.find(propToken);
    bool found = (start >= 0);

    // if there then split it accordingly and fill the ConnectionPars and the
    // SelectProperties
    if (found) {
        // if we find the 'properties=' in the connection string we treat those
        // string part as
        // the database open parameters
        int end = start + propToken.length();
        _connection_string = _scidb_filename.substr(0, start - 1);
        _properties_string = _scidb_filename.substr(end, _scidb_filename.length() - 1);
    } 
    else {
        _connection_string = _scidb_filename;
    }
    return found;
}

void ParameterParser::parsePropertiesString() {
    // mapping between the constant variables and their string representation for
    // using a switch/case statement later
    // std::map<string,Properties> propResolver = map_list_of ("t",T_INDEX);

    vector<string> parts;
    boost::split(parts, _properties_string, boost::is_any_of(";")); // Split at semicolon and comma for refering to a whole KVP
        // example for filename with properties variable    "src_win:0 0 50 50;..."
        for (vector<string>::iterator it = parts.begin(); it != parts.end(); ++it) {
            vector<string> kv, c;
            boost::split(kv, *it, boost::is_any_of(":="));
            if (kv.size() < 2) {
                continue;
            } else {
                if (_propKeyResolver.contains(std::string(kv[0]))) {
                    assignQueryParameter(kv[0], kv[1]);
                } else {
                    Utils::debug("unused parameter \"" + string(kv[0]) + "\" with value \"" + string(kv[1]) + "\"");
                }
            }
        }
    }

    void ParameterParser::loadParsFromEnv(ConnectionParameters* con) {
        char* x;

        x = std::getenv("SCIDB4GDAL_HOST");
        if (x != NULL) {
            con->host = x;
            con->ssl = (con->host.substr(0, 5).compare("https") == 0);
        }

        x = std::getenv("SCIDB4GDAL_PASSWD");
        if (x != NULL)
            con->passwd = x;

        x = std::getenv("SCIDB4GDAL_USER");
        if (x != NULL)
            con->user = x;

        x = std::getenv("SCIDB4GDAL_PORT");
        if (x != NULL)
            con->port = boost::lexical_cast<int>(x);
    }

    void ParameterParser::parseSlicedArrayName() {
        // 	  string array = pars->arrayname;
        size_t length = _con->arrayname.size();
        size_t t_start = _con->arrayname.find_first_of('[');
        size_t t_end = _con->arrayname.find_last_of(']');

        if (t_start == std::string::npos || t_end == std::string::npos) {
            Utils::debug("No temporal information in array name.");
            return;
        }
        // extract temporal string
        if ((length - 1) == t_end) {
            string t_expression =
                _con->arrayname.substr(t_start + 1, (t_end - t_start) - 1);
            // remove the temporal part of the array name otherwise it messes up the
            // concrete array name in scidb
            string temp = _con->arrayname.substr(0, t_start);
            _con->arrayname = temp;
            vector<string> kv;

            boost::split(kv, t_expression, boost::is_any_of(","));
            if (kv.size() < 2) {
                Utils::error("Temporal query not complete. Please state the dimension "
                            "name and the temporal index / interval");
                return;
            }
            // first part is the dimension identifier
            _query->dim_name = kv[0];
            // seceond part is either a temporal index (or temporal index interval) or a
            // timestamp is used (or timestamp interval)
            string t_interval = kv[1];

            /* Test with Regex, failed due to gcc version 4.8.4 (4.9 implements regex
            * completely) */
            // 	    std::smatch match;
            //
            // 	    std::regex
            // re("/^[\d]{4}(\/|-)[\d]{2}(\/|-)[\d]{2}(\s|T)[\d]{2}:[\d]{2}:[\d]{2}((\+|-)[0-1][\d]:?(0|3)0)?$/");
            // 	    if (std::regex_search(t_interval,match,re)) {
            // 	      ...
            // 	    }

            if (t_interval.length() > 0) {
                // if string is a timestamp then translate it into the temporal index
                if (Utils::validateTimestampString(t_interval)) {
                    // store timestamp for query when calling scidb
                    _query->timestamp = t_interval;
                    _query->hasTemporalIndex = false;
                } else {
                    // simply extract the temporal index
                    size_t pos = t_interval.find(":");
                    if (pos < t_interval.length()) {
                        // interval
                        vector<string> attr;
                        boost::split(attr, t_interval, boost::is_any_of(":"));
                        _query->lower_bound = boost::lexical_cast<int>(attr[0]);
                        _query->upper_bound = boost::lexical_cast<int>(attr[1]);
                        // TODO for now we parse it correctly, but we will just use the
                        // lower_bound... change this later
                        Utils::debug("Currently interval query is not supported. Using the "
                                    "lower bound instead");
                        _query->temp_index = _query->lower_bound;
                    } else {
                        // temporal index
                        _query->temp_index = boost::lexical_cast<int>(t_interval);
                    }
                    if (_query->temp_index > 0) {
                        _query->hasTemporalIndex = true;
                    }
                }
            } else {
                Utils::error("No temporal information stated");
                return;
            }
        }
    }

    ConnectionParameters& ParameterParser::getConnectionParameters() {
        return *_con;
    }

    QueryParameters& ParameterParser::getQueryParameters() { return *_query; }

    CreationParameters& ParameterParser::getCreationParameters() {
        return *_create;
    }

    void ParameterParser::validate() {
        if (_scidb_filename == "" ||
            _scidb_filename.substr(0, 6).compare("SCIDB:") != 0) {
            _isValid = false;
            _con->error_code = ERR_GLOBAL_INVALIDCONNECTIONSTRING;
            return;
        }

        _scidb_filename = _scidb_filename.substr(
            6, _scidb_filename.length() - 6); // Remove SCIDB: from connection string
        _isValid = true;
    }

    bool ParameterParser::init() {
        _isValid = false;
        validate();

        // create the parameter objects
        if (!isValid())
            return false;

        _con = new ConnectionParameters();
        if (_operation == SCIDB_OPEN) {
            _query = new QueryParameters();
        } else if (_operation == SCIDB_CREATE) {
            _create = new CreationParameters();
        }

        // if the connection string has an additional properties string then split it
        // from the connection string
        if (splitPropertyString()) {
            parsePropertiesString();
        }

        // first extract information from connection string and afterwards check for
        // the opening options and overwrite values if double
        parseConnectionString();

        // TODO check if connection parameter are set. if not then try to parse
        // parameter from global environment
        if (!_con->isValid()) {
            loadParsFromEnv(_con);
        }
        if (!_con->isValid()) {
            stringstream s;
            s << "Failed to extract connection information. host: " << _con->host
            << ", array: " << _con->arrayname;
            Utils::error(s.str());
        }
        //       if (!_con->isValid()) {
        // 	throw ERR_GLOBAL_INVALIDCONNECTIONSTRING;
        //       }

        if (_operation == SCIDB_OPEN) {
            parseOpeningOptions();
            parseSlicedArrayName(); // array name will be modified if a temporal query is
                            // detected (for the ConnectionPars)
        }
        if (_operation == SCIDB_CREATE) {
            parseCreateOptions();
        }
        return true;
    }

    bool ParameterParser::isValid() { return _isValid; }

    void ParameterParser::assignConnectionParameter(string key, string value) {
        ConnectionStringKey enumKey = _conKeyResolver.getKey(key);
        switch (enumKey) {
            case HOST:
                _con->host = value;   
                //  Explicitly set ssl from URL only if http or https is given
                if (value.substr(0, 8).compare("https://") == 0) _con->ssl = true;
                else if (value.substr(0, 7).compare("http://") == 0) _con->ssl = false;
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
                _con->ssl = CSLTestBoolean(value.c_str());
                break;
            case SSLTRUST:
                _con->ssltrust = CSLTestBoolean(value.c_str());
                break;
            case ARRAY:
                _con->arrayname = value;
                break;
            case CONFIRM_DELETE:
                boost::algorithm::to_lower<string>(value);
                if (strcmp(value.c_str(), "true") == 0 || strcmp(value.c_str(), "1") == 0 ||
                    strcmp(value.c_str(), "y") == 0 || strcmp(value.c_str(), "yes") == 0 ||
                    strcmp(value.c_str(), "t") == 0) {
                    _con->deleteArray = true;
                }
                break;
            default:
                break;
        }
    }

    void ParameterParser::assignCreateParameter(string key, string value) {
        Properties enumKey = _propKeyResolver.getKey(key);
        switch (enumKey) {
            case TRS: {
                _create->dt = value;
                break;
            }
            case TIMESTAMP: {
                _create->timestamp = value;
                break;
            }
            case TYPE: {
                if (_creationTypeResolver.contains(value)) {
                    _create->type = _creationTypeResolver.getKey(value);
                } else {
                    Utils::error("Array \"type\" incorrect. Please use 'S', 'ST' or 'STS'.");
                }
                break;
            }
            case SRS: {
                vector<string> code;
                boost::split(code, value, boost::is_any_of(":"));

                _create->auth_name = code.at(0);
                _create->srid = boost::lexical_cast<int32_t>(code.at(1));
                break;
            }
            case BBOX: {
                vector<string> coords;
                Utils::debug("Got a bbox statement.");
                boost::split(coords, value, boost::is_any_of(" "));
                if (coords.size() != 4) {
                    throw ERR_READ_BBOX;
                    break;
                }
                _create->bbox[0] = boost::lexical_cast<double>(coords.at(0));
                _create->bbox[1] = boost::lexical_cast<double>(coords.at(1));
                _create->bbox[2] = boost::lexical_cast<double>(coords.at(2));
                _create->bbox[3] = boost::lexical_cast<double>(coords.at(3));
                _create->hasBBOX = true;
                break;
            }
            case CHUNKSIZE_SPATIAL:
                try {
                    _create->chunksize_spatial = boost::lexical_cast<int>(value);
                } catch (boost::bad_lexical_cast e) {
                    Utils::debug(e.what());
                    throw ERR_GLOBAL_PARSE;
                }
                break;
            case CHUNKSIZE_TEMPORAL:
                try {
                    _create->chunksize_temporal = boost::lexical_cast<int>(value);
                } catch (boost::bad_lexical_cast e) {
                    Utils::debug(e.what());
                    throw ERR_GLOBAL_PARSE;
                }
                break;
        }
    }
    void ParameterParser::assignQueryParameter(string key, string value) {
        Properties enumKey = _propKeyResolver.getKey(key);
        switch (enumKey) {
            case T_INDEX:
                // this T_INDEX is for query only!
                Utils::debug("Assign query parameter for temporal index");
                _query->temp_index = boost::lexical_cast<int>(value);
                _query->hasTemporalIndex = true;
                // TODO maybe we allow also selecting multiple slices (that will later be
                // saved separately as individual files)
                break;
            default:
                break;
        }
    }
}
