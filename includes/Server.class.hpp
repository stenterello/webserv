#pragma once

#include <webserv.hpp>

typedef struct	s_config
{
	unsigned short				port;
	unsigned char				host[4];
	std::vector<char*>			server_name;
	std::vector<char*>			acceptedMethods;
	char*						root;
	bool						autoindex;
	char*						index;
	std::vector<char*>			locationRules;

	s_config() : port(0)
	{
		root = NULL;
		index = NULL;
	}
}				t_config;

class Server
{
	private:
		Server();
		Server(Server const & src);
		Server&	operator=(Server const & rhs);

		std::vector<t_config>	_config;
		

		// Configuration File Operation

		void	openFile(const char* filename);
		void	defineConfig(std::ifstream & configFile);
		void	parseLine(std::string line, bool serverMatched, bool inBlock, t_config n_conf);
		void	findServerBlock(std::string line, bool serverMatched, bool inBlock);
		void	findOpeningOfBlock(std::string line, bool inBlock);
		void	searchRule(std::string line, t_config n_conf);

	public:
		Server(const char* filename);
		~Server();
};