#include <webserv.hpp>
#include <Server.class.hpp>

//////// Constructors & Destructor //////////////////////////////

Server::Server(const char* filename) : _filename(filename == NULL ? "default.conf" : filename)
{
	_config.reserve(10);
	Parser(_filename, _config);
	std::vector<VirtServ*>::iterator	iter = _virtServs.begin();
	std::vector<t_config>::iterator		iterConfig = _config.begin();

	while (iterConfig != _config.end())
	{
		_virtServs.push_back(new VirtServ(*iterConfig));
		iter++;
		iterConfig++;
	}
	std::cout << "ciao" << std::endl;
}

Server::~Server() {}

