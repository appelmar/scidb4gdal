#ifndef PARAMETER_PARSER_H
#define PARAMETER_PARSER_H

#include "shim_client_structs.h"

#define SCIDB_PARSING_ERROR 1001

namespace scidb4gdal
{
  typedef enum {
      SCIDB_OPEN, SCIDB_CREATE
  } SciDBOperation;
  
  template <typename T>
  struct Resolver  {
      map<string, T> mapping;
      
      public:
	  bool contains (string key);
	  T getKey (string s);
  };
  
    using namespace std;
    
    class ParameterParser {
      public:
	ParameterParser(string scidbFile, char** optionKVP, SciDBOperation op  = SCIDB_OPEN);
	bool isValid();
	ConnectionParameters& getConnectionParameters();
	QueryParameters& getQueryParameters();
	CreationParameters& getCreationParameters();
      protected:
	bool init();
	void validate();
	void parseOpeningOptions ();
	void parseCreateOptions ();
	bool splitPropertyString ();
	void parsePropertiesString ();
	void parseConnectionString();
	void parseArrayName ();
      private:
	ConnectionParameters* _con;
	QueryParameters* _query;
	CreationParameters* _create;
	char** _options;
	string _scidb_filename;
	string _connection_string;
	string _properties_string;
	bool _isValid;
	SciDBOperation _operation;
	Resolver<ConStringParameter> _conKeyResolver; 
	Resolver<Properties> _propKeyResolver;
	Resolver<CreationType> _creationTypeResolver;
	void assignConectionParameter(string key, string value);
	void assignCreateParameter(string key, string value);
	void assignQueryParameter(string key, string value);
    };
}

#endif