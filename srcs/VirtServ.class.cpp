#include <VirtServ.class.hpp>


//////// Constructors & Destructor //////////////////////////////

VirtServ::VirtServ(t_config config) : _config(config)
{
	memset(&_sin, '\0', sizeof(_sin));
	memset(&_client, '\0', sizeof(_client));
	this->_sin.sin_family = AF_INET;
	this->_sin.sin_addr.s_addr = inet_addr(_config.host.c_str());
	this->_sin.sin_port = htons(_config.port);

	if(!this->startServer())
	{
		die("Server Failed to Start. Aborting... :(");
	}
	if(!this->startListen())
	{
		die("Server Failed to Listen. Aborting... :(");
	}

}

VirtServ::~VirtServ()
{

}


//////// Main Functions //////////////////////////////////////

bool	VirtServ::startServer()
{
	_sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (_sockfd == -1)
	{
		std::cout << "Socket error" << std::endl;
		return (false);
	}
	if (bind(_sockfd, (struct sockaddr *)&_sin, sizeof(_sin)) != 0)
	{
		std::cout << "Bind error" << std::endl;
		return (false);
	}
	return (true);
}

bool	VirtServ::startListen()
{
	if (listen(_sockfd, 20) < 0)
	{
		std::cout << "Listen failed" << std::endl;
		return (false);
	}
	while (true)
	{
		_size = sizeof(_client);
		_connfd = accept(_sockfd, (struct sockaddr *)&_client, &_size);
		if (_connfd < 0)
		{
			std::cout << "Error while accepting connection." << std::endl;
			return (false);
		}
		char	buffer[_config.client_body_max_size];
		if (recv(_connfd, buffer, 100, 0) == -1)
		{
			std::cout << "Error receiving" << std::endl;
			return (false);
		}
		std::cout << buffer << std::endl;
	}
	return (true);
}

bool	VirtServ::stopServer()
{
	if (!close(_connfd))
	{
		std::cout << "Error closing connection with fd " << _connfd << std::endl;
		return (false);
	}

	if (!close(_sockfd))
	{
		std::cout << "Error closing _sockfd" << std::endl;
		return (false);
	}
	return (true);
}
