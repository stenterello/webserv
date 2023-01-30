#include <VirtServ.class.hpp>


//////// Constructors & Destructor //////////////////////////////

VirtServ::VirtServ(t_config config) : _config(config)
{
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

		elaborateRequest();

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
	_request.line = "";
	_request.body = "";
	std::map<std::string, std::string>::iterator	iter = _request.headers.begin();

	while (iter != _request.headers.end())
	{
		(*iter).second = "";
		iter++;
	}
}

void	VirtServ::readRequest(std::string req)
{
	std::string	key;

	_request.line = req.substr(0, req.find_first_of("\n"));
	req = req.substr(req.find_first_of("\n") + 1);

	while (req.find_first_not_of(" \t\n") != std::string::npos && std::strncmp(req.c_str(), "\n\n", 2))
	{
		key = req.substr(0, req.find_first_of(":"));
		req = req.substr(req.find_first_of(":") + 2);
		(*_request.headers.find(key)).second = req.substr(0, req.find_first_of("\n"));
		req = req.substr(req.find_first_of("\n") + 1);
	}

	if (req.find_first_not_of("\n") != std::string::npos)
		_request.body = req.substr(req.find_first_not_of("\n"));


	// Check request parsed

	std::cout << "PARSED REQUEST CHECKING" << std::endl;

	std::cout << _request.line << std::endl;

	std::map<std::string, std::string>::iterator	iter = _request.headers.begin();

	while (iter != _request.headers.end())
	{
		std::cout << (*iter).first << ": " << (*iter).second << std::endl;
		iter++;
	}

	std::cout << _request.body << std::endl;
}

void		VirtServ::elaborateRequest()
{
	std::string	method;
	std::string	path;
	t_location*	location;

	method = _request.line.substr(0, _request.line.find_first_of(" "));
	_request.line = _request.line.substr(method.length() + 1);
	path = _request.line.substr(0, _request.line.find_first_of(" "));

	std::cout << "method: " << method << std::endl;
	std::cout << "path: " << path << std::endl;

	location = searchLocationBlock(method, path);
	(void)location;

	/// 404 RESPONSE IF
	// if (!location)

	// interpretLocationBlock(location);
	// searchResource(location);
}

t_location*	VirtServ::searchLocationBlock(std::string method, std::string path)
{
	std::vector<t_location>::iterator	iter = _config.locationRules.begin();
	t_location*							ret = NULL;

	// Exact corrispondence
	while (iter != _config.locationRules.end())
	{
		if (!std::strncmp(path.c_str(), (*iter).location.c_str(), (*iter).location.length()) && !(*iter).regex)
		{
			if (ret == NULL || (*iter).location.length() > ret->location.length())
				ret = &(*iter);
		}
		iter++;
	}

	// Prefix
	if (ret == NULL)
	{
		iter = _config.locationRules.begin();
		while (iter != _config.locationRules.end())
		{
			if (!std::strncmp(path.c_str(), (*iter).location.c_str(), path.length()) && !(*iter).regex)
			{
				if (ret == NULL || (*iter).location.length() > ret->location.length())
					ret = &(*iter);
			}
			iter++;
		}
	}

	// Regex
	if (ret == NULL)
	{
		iter = _config.locationRules.begin();
		while (iter != _config.locationRules.end())
		{
			if ((*iter).regex && !std::strncmp((*iter).location.c_str(), path.substr(path.length() - (*iter).location.length()).c_str(), (*iter).location.length()))
			{
				if (ret == NULL || (*iter).location.length() > ret->location.length())
					ret = &(*iter);
			}
			iter++;
		}
	}

	if (ret != NULL)
		std::cout << ret->location << std::endl;
	else
		std::cout << "not found" << std::endl;
		
	(void)method;

	return (ret);
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
	(void)bytesSent;
}
