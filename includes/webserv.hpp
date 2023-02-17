#ifndef WEBSERV_HPP
# define WEBSERV_HPP

# include <iostream>
# include <sstream>
# include <vector>
# include <map>
# include <tr1/unordered_map>
# include <fstream>
# include <cstring>
# include <cstdlib>
# include <algorithm>
# include <unistd.h>
# include <dirent.h>
# include <stdlib.h>
# include <sys/types.h>
# include <sys/socket.h>
# include <sys/stat.h>
# include <netdb.h>
# include <netinet/in.h>
# include <arpa/inet.h>
# include <fcntl.h>
# include <time.h>


typedef	struct	s_location
{
	bool						regex;		// ~
	bool						exact_path;	// =
	std::vector<std::string>	allowedMethods;
	std::string					location;	// path
	std::string					text;		// block
}				t_location;

typedef struct	s_config
{
	unsigned short				port;					// listen
	std::string					host;
	std::vector<std::string>	server_name;			// server_nam
	std::string					root;					// root
	bool						autoindex;				// autoindex
	std::vector<std::string>	index;					// index
	std::vector<std::string>	errorPages;				// error_page
	unsigned long				client_max_body_size;	// client_max_body_size
	std::vector<std::string>	allowedMethods;
	std::vector<t_location>		locationRules;			// location

	s_config() : port(0)
	{
		root = "";
		host = "0.0.0.0";
		client_max_body_size = 1048576;
	}
}				t_config;

typedef struct	s_request
{
	std::string										line;
	std::string										method;
	std::tr1::unordered_map<std::string, std::string>	headers;
	std::string										body;

	s_request()
	{
		headers.insert(std::make_pair("Host", ""));
		headers.insert(std::make_pair("User-Agent", ""));
		headers.insert(std::make_pair("Accept", ""));
		headers.insert(std::make_pair("Accept-Language", ""));
		headers.insert(std::make_pair("Accept-Encoding", ""));
		headers.insert(std::make_pair("Connection", ""));
		headers.insert(std::make_pair("Upgrade-Insecure-Requests", ""));
		headers.insert(std::make_pair("Sec-Fetch-Dest", ""));
		headers.insert(std::make_pair("Sec-Fetch-Mode", ""));
		headers.insert(std::make_pair("Sec-Fetch-Site", ""));
		headers.insert(std::make_pair("Sec-Fetch-User", ""));
		headers.insert(std::make_pair("Content-Length", ""));
		headers.insert(std::make_pair("Content-Type", ""));
		headers.insert(std::make_pair("Expect", ""));
	}
}				t_request;

typedef struct	s_response
{
	std::string										line;
	std::tr1::unordered_map<std::string, std::string>	headers;
	std::string										body;

	s_response()
	{
		headers.insert(std::make_pair("Server", "webserv"));
		headers.insert(std::make_pair("Date", ""));
		headers.insert(std::make_pair("Content-type", ""));
		headers.insert(std::make_pair("Content-length", ""));
		headers.insert(std::make_pair("Last-Modified", ""));
		headers.insert(std::make_pair("Connection", ""));
		headers.insert(std::make_pair("ETag", ""));
		headers.insert(std::make_pair("Accept-Ranges", ""));
		headers.insert(std::make_pair("Protocol", ""));
		headers.insert(std::make_pair("Status-Code", ""));
		headers.insert(std::make_pair("Reason-Phrase", ""));
		headers.insert(std::make_pair("Date", ""));
	}
}				t_response;

# include <VirtServ.class.hpp>

void	die(std::string const err);
void	die(std::string const err, VirtServ & serv);
void	usage();
bool	bool_error(std::string error);

#endif
