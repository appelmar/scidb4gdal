#ifndef PARAMETER_PARSER_H
#define PARAMETER_PARSER_H

#include "shim_client_structs.h"

#define SCIDB_OPEN 1
#define SCIDB_CREATE 2
#define SCIDB_PARSING_ERROR 100

namespace scidb4gdal
{
    using namespace std;
    
    class ParameterParser {
      public:
	ParameterParser();
	ParameterParser(string scidbFile, char** optionKVP);
	void parseConnectionString( const string &connstr, ConnectionPars *con);
	bool isValid();
// 	parseQueryString();
// 	parseCreateString();
	void parseOpeningOptions (char** options, ConnectionPars* con);
	bool splitPropertyString (string &input, string &constr, string &propstr);
	void parsePropertiesString ( const string &propstr, QueryParameters* query );
	void parseArrayName (string& array, QueryParameters* query);
	ConnectionPars* getConnectionParameter();
	QueryParameters* getQueryParameter();
      protected:
	bool init();
      private:
	ConnectionPars* _con;
	QueryParameters* _query;
	char** _options;
	string _scidb_filename;
    };
}

#endif