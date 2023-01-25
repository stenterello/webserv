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

	// QUA BISOGNA RIEMPIRE LA STRUCT _addStruct

		_addrStruct->ai_family = AF_INET;  // Address family, AF_INET
    	_addrStruct->ai_socktype = 0; 
		_addrStruct->ai_protocol = 0;   // Port number
		_addrStruct->ai_family = AI_PASSIVE;
    	_addrStruct->ai_addr->sa_family = AF_INET;
		//_addrStruct->ai_addr->sa_data = inet_addr("IP ADDRESS HERE")
}

Server::~Server() {}

bool	Server::startServer()
{
	_sockfd = socket(_addrStruct->ai_family, _addrStruct->ai_socktype, _addrStruct->ai_protocol);
	if (_sockfd < 0)
	{
		std::cout << "socket error" << std::endl;
		return false;
	}
	if((bind(_sockfd, _addrStruct->ai_addr, _addrStruct->ai_addrlen)) < 0)
	{
		std::cout << "bind error" << std::endl;
		return false;
	}
	return true;
}	

bool	Server::listenServer()
{
	if (listen(_sockfd, 20) < 0)
	{
		std::cout << "listen failed" << std::endl;
		return false;
	}
	return true;
}