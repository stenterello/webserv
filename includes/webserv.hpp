#ifndef WEBSERV_HPP
# define WEBSERV_HPP

# include <iostream>
# include <sstream>
# include <vector>
# include <map>
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
# include <sys/wait.h>
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
	s_location&	operator=(s_location const & rhs)
	{
		this->regex = rhs.regex;
		this->exact_path = rhs.exact_path;
		this->allowedMethods = rhs.allowedMethods;
		this->location = rhs.location;
		this->text = rhs.text;
		return (*this);
	}
}				t_location;

typedef struct	s_config
{
	unsigned short				port;					// listen
	std::string					host;
	std::vector<std::string>	server_name;			// server_name
	std::string					root;					// root
	bool						autoindex;				// autoindex
	std::vector<std::string>	index;					// index
	std::vector<std::string>	errorPages;				// error_page
	unsigned long				client_max_body_size;	// client_max_body_size
	std::vector<std::string>	allowedMethods;
	std::vector<t_location>		locationRules;			// location
	std::vector<std::string>	files;					// files to try
	std::string					cgi_script;
	bool						valid;

	s_config() : port(0)
	{
		root = "";
		host = "0.0.0.0";
		client_max_body_size = 1048576;
		valid = true;
		cgi_script = "";
	}
	s_config(bool val) : valid(val) {}
}				t_config;

typedef struct	s_request
{
	std::string											line;
	std::string											method;
	std::string											path;
	std::vector<std::pair<std::string, std::string> >	headers;
	std::string											body;

	s_request()
	{
		headers.push_back(std::make_pair("Host", ""));
		headers.push_back(std::make_pair("User-Agent", ""));
		headers.push_back(std::make_pair("Accept", ""));
		headers.push_back(std::make_pair("Accept-Language", ""));
		headers.push_back(std::make_pair("Accept-Encoding", ""));
		headers.push_back(std::make_pair("Connection", ""));
		headers.push_back(std::make_pair("Upgrade-Insecure-Requests", ""));
		headers.push_back(std::make_pair("Sec-Fetch-Dest", ""));
		headers.push_back(std::make_pair("Sec-Fetch-Mode", ""));
		headers.push_back(std::make_pair("Sec-Fetch-Site", ""));
		headers.push_back(std::make_pair("Sec-Fetch-User", ""));
		headers.push_back(std::make_pair("Content-Length", ""));
		headers.push_back(std::make_pair("Content-Type", ""));
		headers.push_back(std::make_pair("Expect", ""));
		headers.push_back(std::make_pair("Transfer-Encoding", ""));
	};
	s_request& operator=(s_request const & rhs)
	{
		this->line = rhs.line;
		this->method = rhs.method;
		this->path = rhs.path;
		this->headers = rhs.headers;
		this->body = rhs.body;
		return (*this);
	};
}				t_request;

typedef struct	s_response
{
	std::string											line;
	std::vector<std::pair<std::string, std::string> >	headers;
	std::string											body;

	s_response()
	{
		headers.push_back(std::make_pair("Server", "webserv"));
		headers.push_back(std::make_pair("Date", ""));
		headers.push_back(std::make_pair("Content-Type", ""));
		headers.push_back(std::make_pair("Content-Length", ""));
		headers.push_back(std::make_pair("Last-Modified", ""));
		headers.push_back(std::make_pair("ETag", ""));
		headers.push_back(std::make_pair("Accept-Ranges", ""));
		headers.push_back(std::make_pair("Protocol", ""));
		headers.push_back(std::make_pair("Status-Code", ""));
		headers.push_back(std::make_pair("Reason-Phrase", ""));
		headers.push_back(std::make_pair("Location", ""));
		headers.push_back(std::make_pair("Connection", ""));
		headers.push_back(std::make_pair("Allow", ""));
	}
}				t_response;

typedef struct	s_connInfo
{
	int			fd;
	std::string	buffer;
	std::string	headers;
	std::string	path;
	t_config	config;
	int			chunk_size;
	t_request	request;
	t_response	response;
	std::string	body;
	t_location*	location;
	
	s_connInfo() : chunk_size(-1) {};
	s_connInfo(int i) : fd(i), chunk_size(-1) {};
}			t_connInfo;

# include <VirtServ.class.hpp>

void	die(std::string const err);
void	die(std::string const err, VirtServ & serv);
void	usage();
bool	bool_error(std::string error);
typedef std::vector<std::pair<std::string, std::string> >::iterator keyIter;
	keyIter				findKey(std::vector<std::pair<std::string, std::string> > & vector, std::string key);

#endif
