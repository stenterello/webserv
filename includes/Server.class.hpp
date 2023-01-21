#pragma once

#include <webserv.hpp>

typedef	struct	s_location
{
	bool		regex;
	std::string	location;
}				t_location;

typedef struct	s_config
{
	unsigned short				port;
	unsigned char				host[4];
	std::vector<char*>			server_name;
	std::vector<char*>			acceptedMethods;
	char*						root;
	bool						autoindex;
	char*						index;
	std::vector<t_location>		locationRules;

	s_config() : port(0)
	{
		root = NULL;
		index = NULL;
	}
}				t_config;

class Server
{
	private:
		Server(Server const & src);
		Server&	operator=(Server const & rhs);

		std::vector<t_config>	_config;
		const char*				_filename;
		

		// Configuration File Operation

		void					openFile(const char* filename);
		void					defineConfig(std::ifstream & configFile);
		std::string				deleteComments(std::string text);
		bool					curlyBrace(std::string text, size_t pos);
		size_t					searchEndingCurlyBrace(std::string text, size_t pos);
		void					divideAndCheck(std::string text, std::vector<std::string> & serverBlocks);
		void					elaborateServerBlock(std::string serverBlock);
		bool					takeRule(std::string & text, t_config & conf);

	public:
		Server();
		Server(const char* filename);
		~Server();
};
