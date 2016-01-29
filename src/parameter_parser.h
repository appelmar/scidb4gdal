#ifndef PARAMETER_PARSER_H
#define PARAMETER_PARSER_H

#include "shim_client_structs.h"


namespace scidb4gdal
{
    /**
     * The type of operation that needs to be performed with SciDB. Currrently querying and creating arrays in SciDB is
     * supported
     */
    enum SciDBOperation {
	SCIDB_OPEN, SCIDB_CREATE
    };
    
    template <typename T>
    
    /**
     * @brief class to get a specific class type (or enum) by a string
     * 
     * A Resolver class that resolves the string value of a parameter description to address the correct parameter in the respective kind
     * of parameter. The main reason to use this class is to interprete the parameter names of the create options or the connection string
     * in order to provide enums to allow a switch-case statement to assign the values of an user input (options or connection string).
     * 
     */
    struct Resolver  {
	/** a map that maps a name (string) to a generic type T (intended to be an enum) */
	map<string, T> mapping;
	
	public:

	    /**
	     * @brief is the parameter name registered for the respective kind of parameters
	     * 
	     * This function checks the existance in the class' map for the enum of a given
	     * parameter name.
	     * 
	     * @param key the String representation of a parameter
	     * @return bool whether or not the key is registered
	     */
	    bool contains (string key);
	    
	    /**
	     * @brief returns the internal enum for a named parameter
	     * 
	     * This function finds and returns the enum (key) of a given string representation.
	     * 
	     * @param s the String representation of a parameter 
	     * @return T returns the assigned generic type which is usually an enum
	     */
	    T getKey (string s);
    };
  
    using namespace std;
    
    /**
     * @brief parser for the user input
     * 
     * A class to parse the user input that is coming from the invoking command. The user input is either
     * stated in the create/opening options or it is stated in the connection string. In both cases this
     * class reads the key-value pairs and assigns it to the internal *Parameters representation (e.g. scidb4gdal::CreationParameters,
     * scidb4gdal::QueryParameters and scidb4gdal::ConnectionParameters).
     * 
     * @author Florian Lahn, IfGI Muenster
     */
    class ParameterParser {
      public:
	/**
	 * @brief the class constructor
	 * 
	 * @param scidbFile a string containing the name of the file
	 * @param optionKVP a pointer to the option key-value pairs
	 * @param op The operation to be parsed for. default it is SCIDB_OPEN
	 */
	ParameterParser(string scidbFile, char** optionKVP, SciDBOperation op  = SCIDB_OPEN);
	/**
	 * @brief checks whether the opening options or the connection string is complete
	 * 
	 * @return bool
	 */
	bool isValid();
	/**
	 * @brief Returns the connection parameters
	 * 
	 * @return scidb4gdal::ConnectionParameters&
	 */
	ConnectionParameters& getConnectionParameters();

	/**
	 * @brief Returns the query parameters
	 * 
	 * @return scidb4gdal::QueryParameters&
	 */
	QueryParameters& getQueryParameters();
	
	/**
	 * @brief Returns the creation parameters
	 * 
	 * @return scidb4gdal::CreationParameters&
	 */
	CreationParameters& getCreationParameters();
	
	
	protected:
	  /**
	 * @brief initiates the parameter parser
	 * 
	 * This function will be called in from the constructor in order to parse and assign the parameters to the 
	 * respective representations.
	 * 
	 * @return bool Whether or not the initiation was successful
	 */
	bool init();
	/**
	 * @brief validates the connection string
	 * 
	 * It validates the filename that the correct Protocol ("SCIDB:") was specified.
	 * 
	 * @return void
	 */
	void validate();
	/**
	 * @brief parses the opening options
	 * 
	 * @return void
	 */
	void parseOpeningOptions ();
	/**
	 * @brief parses the create options
	 * 
	 * @return void
	 */
	void parseCreateOptions ();
	/**
	 * @brief Splits the file name string into a connection string and a property string, in which additional options are stated.
	 * 
	 * @return void
	 */
	bool splitPropertyString ();
	/**
	 * @brief parses the property string for create or query options
	 * 
	 * @return void
	 */
	void parsePropertiesString ();
	/**
	 * @brief parses the connection string part for the connection information
	 * 
	 * If there are no create or opening options the file name needs to include the connection parameters as key-value pairs. In
	 * this case the connection string needs to contain information about the login credentials as well as the access point of
	 * SciDB (host, port, protocol, etc.)
	 * 
	 * @return void
	 */
	void parseConnectionString();
	/**
	 * @brief parses the array name for additional information
	 * 
	 * When using the opening operation it is sometimes necessary to state the temporal index or the date/time for a certain
	 * time slice. This temporal information can be written as syntactical sugar within square brackets. This function will read
	 * and remove this information from the array name.
	 * 
	 * @return void
	 */
	void parseArrayName ();
	
	/**
	* @brief loads the connection string from environment parameters
	* 
	* If there are no connection parameter explicitly stated, then this function will try to read the connection
	* parameter from the global environment parameter.
	* 
	* @param con The connection parameters, where the information shall be stored in (in/out)
	* @return void
	*/
	static void loadParsFromEnv (ConnectionParameters* con);
      private:
	/**
	 * pointer to the connection parameters
	 */
	ConnectionParameters* _con;
	/**
	 * pointer to the query parameters
	 */
	QueryParameters* _query;
	/**
	 * pointer to the create parameters
	 */
	CreationParameters* _create;
	/**
	 * pointer to store the key-value pairs from GDAL Open
	 */
	char** _options;
	/**
	 * the file name of the SciDB array from the command invoke (user input)
	 */
	string _scidb_filename;
	/**
	 * the connection string that is retrieved by calling scidb4gdal::ParameterParser::splitPropertyString
	 */
	string _connection_string;
	/**
	 * the properties string that is split when using splitPropertyString
	 */
	string _properties_string;
	/**
	 * Flag to store if the options and parameters are valid
	 */
	bool _isValid;
	/**
	 * the intended SciDB operation
	 */
	SciDBOperation _operation;
	
	/**
	 * the resolver to get the internal enum for connection parameter
	 */
	Resolver<ConnectionStringKey> _conKeyResolver; 
	/**
	 * resolver to get internal enums for query or creation parameter
	 */
	Resolver<Properties> _propKeyResolver;
	/**
	 * resolver to get interal enums for creation type (S_ARRAY,ST_ARRAY,...)
	 */
	Resolver<CreationType> _creationTypeResolver;

	/**
	 * @brief Assigns the key value pair to the connection parameters
	 * 
	 * @param key the key as a string
	 * @param value the value as a string
	 * @return void
	 */
	void assignConectionParameter(string key, string value);

	/**
	 * @brief Assigns the key-value pair to the create parameter
	 * 
	 * @param key the key as a string
	 * @param value the value as a string
	 * @return void
	 */
	void assignCreateParameter(string key, string value);
	
	/**
	 * @brief Assigns the key-value pair to the query parameter
	 * 
	 * @param key the key as a string
	 * @param value the value as a string
	 * @return void
	 */
	void assignQueryParameter(string key, string value);
    };
}

#endif