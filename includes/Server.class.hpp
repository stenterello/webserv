#pragma once

#include <webserv.hpp>

typedef	struct	s_location
{
	bool				regex;
	std::string			location;
	std::vector<char*>	try_files;
}				t_location;

typedef	struct	s_cases
{
	std::vector<std::string>	c;

	s_cases()
	{
		c.push_back("listen");
		c.push_back("server_name");
		c.push_back("root");
		c.push_back("autoindex");
		c.push_back("index");
		c.push_back("error_pages");
		c.push_back("client_body_max_size");
		c.push_back("location");
	}
}				t_cases;

typedef struct	s_config
{
	unsigned short				port;					// listen
	unsigned char				host[4];				// listen
	std::vector<char*>			server_name;			// server_name
	std::vector<char*>			acceptedMethods;		
	char*						root;					// root
	bool						autoindex;				// autoindex
	char*						index;					// index
	std::vector<char*>			errorPages;				// error_pages
	unsigned long				client_body_max_size;	// client_body_max_size
	std::vector<t_location>		locationRules;			// location

	s_config() : port(0)
	{
		root = NULL;
		index = NULL;
		host[0] = 0;
		host[1] = 0;		
		host[2] = 0;
		host[3] = 0;
	}
}				t_config;

class Server
{
	private:
		Server(Server const & src);
		Server&	operator=(Server const & rhs);

		std::vector<t_config>	_config;
		const char*				_filename;
		t_cases					_cases;
		

		// Configuration File Operations

		void					openFile(const char* filename);
		void					defineConfig(std::ifstream & configFile);
		std::string				deleteComments(std::string text);
		bool					curlyBrace(std::string text, size_t pos);
		size_t					searchEndingCurlyBrace(std::string text, size_t pos);
		void					divideAndCheck(std::string text, std::vector<std::string> & serverBlocks);
		void					elaborateServerBlock(std::string serverBlock);
		bool					prepareRule(std::string & text, t_config & conf);
		void					fillConf(std::string key, std::string value, t_config & conf);
		void					checkHostPort(std::string value, t_config & conf);
		void					checkValidIP(std::string value);

	public:
		Server();
		Server(const char* filename);
		~Server();
};
