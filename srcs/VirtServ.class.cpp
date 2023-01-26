#include <VirtServ.class.hpp>


//////// Constructors & Destructor //////////////////////////////

VirtServ::VirtServ(t_config config) : _config(config)
{
	// Inizializzazione Request
	_request.insert(std::make_pair("Method", ""));
	_request.insert(std::make_pair("Path", ""));
	_request.insert(std::make_pair("Protocol", ""));
	_request.insert(std::make_pair("Host", ""));
	_request.insert(std::make_pair("User-Agent", ""));
	_request.insert(std::make_pair("Accept", ""));
	_request.insert(std::make_pair("Accept-Language", ""));
	_request.insert(std::make_pair("Accept-Encoding", ""));
	_request.insert(std::make_pair("Connection", ""));
	_request.insert(std::make_pair("Upgrade-Insecure-Requests", ""));
	_request.insert(std::make_pair("Sec-Fetch-Dest", ""));
	_request.insert(std::make_pair("Sec-Fetch-Mode", ""));
	_request.insert(std::make_pair("Sec-Fetch-Site", ""));
	_request.insert(std::make_pair("Sec-Fetch-User", ""));

	// Inizializzazione Response
	_response.insert(std::make_pair("Protocol", ""));
	_response.insert(std::make_pair("Status-Code", ""));
	_response.insert(std::make_pair("Reason-Phrase", ""));
	_response.insert(std::make_pair("Content-Type", ""));
	_response.insert(std::make_pair("Content-Lenght", ""));
	_response.insert(std::make_pair("Body", ""));

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
	int	bytes;

	if (listen(_sockfd, 20) < 0)
	{
		std::cout << "Listen failed" << std::endl;
		return (false);
	}
	while (true)
	{

		// Accept connection

		_size = sizeof(_client);
		_connfd = accept(_sockfd, (struct sockaddr *)&_client, &_size);
		if (_connfd < 0)
		{
			std::cout << "Error while accepting connection." << std::endl;
			return (false);
		}

		// Read communication from client
		char	buffer[1048576] = { 0 };
		bytes = recv(_connfd, buffer, 1048576, 0);
		if (bytes == -1)
		{
			std::cout << "Error receiving" << std::endl;
			return (false);
		}
		std::cout << "Output without newline" << std::endl;
		std::cout << "_________________________" << std::endl;
		std::cout << buffer;

		// Parse request

		cleanRequest();
		readRequest(buffer);

		// Parse Response da implementare in base al path della Request
		// dove si andrà a popolare _response con i vari campi descritti in costruzione
		// Potrebbero servire altri campi, questi sono quelli essenziali ! 
		// selectRepsonse();

		// Send Response
		sendResponse();

		// Close connection
		close(_connfd);
	}
	return (true);
}

bool	VirtServ::stopServer()
{
	if (close(_sockfd))
	{
		std::cout << "Error closing _sockfd" << std::endl;
		return (false);
	}
	return (true);
}

void	VirtServ::cleanRequest()
{
	std::map<std::string, std::string>::iterator	iter = _request.begin();

	while (iter != _request.end())
	{
		(*iter).second = "";
		iter++;
	}
}

void	VirtServ::readRequest(std::string req)
{
	std::string	key;

	(*_request.find("Method")).second = req.substr(0, req.find_first_of(" "));
	req = req.substr(req.find_first_of(" ") + 1);
	(*_request.find("Path")).second = req.substr(0, req.find_first_of(" "));
	req = req.substr(req.find_first_of(" ") + 1);
	(*_request.find("Protocol")).second = req.substr(0, req.find_first_of(" \n"));
	req = req.substr(req.find_first_of(" \n") + 1);

	while (req.find_first_not_of(" \t\n") != std::string::npos)
	{
		key = req.substr(0, req.find_first_of(":"));
		req = req.substr(req.find_first_of(":") + 2);
		(*_request.find(key)).second = req.substr(0, req.find_first_of("\n"));
		req = req.substr(req.find_first_of("\n") + 1);
	}


	// Check request parsed

	std::cout << "PARSED REQUEST CHECKING" << std::endl;

	std::map<std::string, std::string>::iterator	iter = _request.begin();

	while (iter != _request.end())
	{
		std::cout << (*iter).first << ": " << (*iter).second << std::endl;
		iter++;
	}
}

void	VirtServ::sendResponse()
{

	// Questo era un test semplice prendendo come esempio la roba di appunti.
	// Qui andranno trasformate le varie parti di _response in una c_string da inviare con send.

	std::string	message;
	std::string	sendMessage;
	long		bytesSent;

	message = "<!DOCTYPE html><html lang=\"en\"><body><h1> HOME </h1><p> Hello from your Server :) </p></body></html>";
    std::ostringstream ss;
    ss << "HTTP/1.1 200 OK\nContent-Type: text/html\nContent-Length: " << message.size() << "\n\n" << message;

	sendMessage = ss.str();

	bytesSent = send(_connfd, sendMessage.c_str(), sendMessage.size(), 0);

}
