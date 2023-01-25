#ifndef WEBSERV_HPP
# define WEBSERV_HPP

# include <iostream>
# include <sstream>
# include <vector>
# include <fstream>
# include <cstring>
# include <algorithm>
# include <unistd.h>
# include <dirent.h>
# include <stdlib.h>
# include <sys/types.h>
# include <sys/socket.h>
# include <netdb.h>
# include <netinet/in.h>
# include <arpa/inet.h>


typedef	struct	s_location
{
	bool				regex;		// ~
	bool				exact_path;	// =
	std::vector<char*>	acceptedMethods;
	std::string			location;	// path
	std::string			text;		// block
}				t_location;

typedef struct	s_config
{
	unsigned short				port;					// listen
	// unsigned char				host[4];				// listen
	std::string					host;
	std::vector<std::string>	server_name;			// server_nam
	std::string					root;					// root
	bool						autoindex;				// autoindex
	std::vector<std::string>	index;					// index
	std::vector<std::string>	errorPages;				// error_pages
	unsigned long				client_body_max_size;	// client_body_max_size
	std::vector<t_location>		locationRules;			// location

	s_config() : port(0)
	{
		root = "";
		host = "0.0.0.0";
		// host[0] = 0;
		// host[1] = 0;
		// host[2] = 0;
		// host[3] = 0;
	}
}				t_config;

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

void	die(std::string const err);
void	usage();

#endif
