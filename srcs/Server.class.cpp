#include <webserv.hpp>
#include <Server.class.hpp>

//////// Constructors & Destructor //////////////////////////////

Server::Server(const char* filename) : _filename(filename == NULL ? "default.conf" : filename)
{
	_config.reserve(10);
	Parser(_filename, _config);
}

Server::~Server() {}

