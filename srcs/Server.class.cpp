#include <webserv.hpp>
#include <Server.class.hpp>

//////// Constructors & Destructor //////////////////////////////

Server::Server(const char* filename) : _filename(filename == NULL ? "default.conf" : filename)
{
	_config.reserve(10);
	_virtServs.reserve(10);
	Parser(_filename, _config);
	std::vector<VirtServ>::iterator		iter = _virtServs.begin();
	std::vector<t_config>::iterator		iterConfig = _config.begin();

	std::cout << "TEST 0 !!!" << std::endl;
	while (iterConfig != _config.end())
	{
		std::cout << "TEST 1 !!!" << std::endl;
		_virtServs.push_back(VirtServ(*iterConfig));
		iter++;
		iterConfig++;
	}
}

Server::~Server() {}


