#include <Parser.class.hpp>
#include <VirtServ.class.hpp>


//////// Constructors & Destructor //////////////////////////////

VirtServ::VirtServ(t_config config) : _config(config)
{
	memset(&_sin, '\0', sizeof(_sin));
	memset(&_client, '\0', sizeof(_client));
	this->_sin.sin_family = AF_INET;
	this->_sin.sin_addr.s_addr = inet_addr(_config.host.c_str());
	this->_sin.sin_port = htons(_config.port);

	_connfd = 0;

	if(!this->startServer())
	{
		die("Server Failed to Start. Aborting... :(");
	}
	if(!this->startListen())
	{
		die("Server Failed to Listen. Aborting... :(", *this);
	}
}

VirtServ::~VirtServ()
{

}

//////// Getters & Setters ///////////////////////////////////

int		VirtServ::getSocket() { return (_sockfd); }
int		VirtServ::getConnectionFd() { return (_connfd); }


//////// Main Functions //////////////////////////////////////

bool	VirtServ::startServer()
{
	// int	i = 1;

	_sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (_sockfd == -1)
	{
		std::cerr << "Socket error" << std::endl;
		return (false);
	}
	// fcntl(_sockfd, F_SETFL, O_NONBLOCK);
	// if (setsockopt(_sockfd, SOL_SOCKET, SO_REUSEADDR, (const char *)&i, sizeof(i)) < 0)
	// {
	// 	std::cerr << "setsockopt error" << std::endl;
	// 	return (false);
	// }
	if (bind(_sockfd, (struct sockaddr *)&_sin, sizeof(_sin)) != 0)
	{
		std::cerr << "Bind error" << std::endl;
		return (false);
	}
	return (true);
}

bool	VirtServ::startListen()
{
	int	bytes;

	if (listen(_sockfd, 20) < 0)
	{
		std::cerr << "Listen failed" << std::endl;
		return (false);
	}
	while (true)
	{

		// Accept connection

		_size = sizeof(_client);
		_connfd = accept(_sockfd, (struct sockaddr *)&_client, &_size);
		if (_connfd < 0)
		{
			std::cerr << "Error while accepting connection." << std::endl;
			return (false);
		}

		// Read communication from client
		char	buffer[1048576] = { 0 };
		bytes = recv(_connfd, buffer, 1048576, 0);
		if (bytes == -1)
		{
			std::cerr << "Error receiving" << std::endl;
			return (false);
		}

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
		_connfd = 0;
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
	std::string										key;
	std::map<std::string, std::string>::iterator	header;

	_request.line = req.substr(0, req.find_first_of("\n"));
	req = req.substr(req.find_first_of("\n") + 1);

	while (req.find_first_not_of(" \t\n\r") != std::string::npos && std::strncmp(req.c_str(), "\n\n", 2))
	{
		key = req.substr(0, req.find_first_of(":"));
		req = req.substr(req.find_first_of(":") + 2);
		header = _request.headers.find(key);
		if (header != _request.headers.end())
			(*header).second = req.substr(0, req.find_first_of("\n"));
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

	location = searchLocationBlock(method, path);
	// if (!location)
	// RETURN 404
	executeLocationRules(location->text);
}

void		VirtServ::executeLocationRules(std::string text)
{
	t_config	tmpConfig(_config);
	std::string	line;
	std::string	key;
	std::string	value;
	int			i;

	while (text.find_first_not_of(" \t\r\n") != std::string::npos)
	{
		line = text.substr(0, text.find("\n"));
		if (line.at(line.length() - 1) != ';')
			die("Location rules must end with semicolon. Aborting", *this);
		line = line.substr(0, line.length() - 1);
		if (line.find_first_of(" \t") == std::string::npos)
			die("Location rules must be given in format 'key value'. Aborting", *this);
		key = line.substr(0, line.find_first_of(" \t"));
		value = line.substr(line.find_first_not_of(" \t", key.length()));
		if (value.find_first_not_of(" \t\n") == std::string::npos)
			die("Location rule without value. Aborting", *this);

		for (i = 0; i < 6; i++)
		{
			if (!key.compare(_cases.c[i]))
				break ;
		}

		switch (i)
		{
			case 0:
				tmpConfig.root = value; break ;
			case 1:
			{
				if (!value.compare("on"))
					tmpConfig.autoindex = true;
				else if (!value.compare("off"))
					tmpConfig.autoindex = false;
				else
					die("Autoindex rule must have on|off value. Aborting", *this);
				break ;
			}
			case 2:
			{
				tmpConfig.index.clear();
				while (value.find_first_not_of(" \n\t") != std::string::npos)
				{
					tmpConfig.index.push_back(value.substr(0, value.find_first_of(" \t")));
					value = value.substr(value.find_first_of(" \t"));
					value = value.substr(value.find_first_not_of(" \t"));
				}
				break ;
			}
			case 3:
			{
				tmpConfig.errorPages.clear();
				while (value.find_first_not_of(" \n\t") != std::string::npos)
				{
					tmpConfig.errorPages.push_back(value.substr(0, value.find_first_of(" \t")));
					value = value.substr(value.find_first_of(" \t"));
					value = value.substr(value.find_first_not_of(" \t"));
				}
				break ;
			}
			case 4:
				Parser::checkClientBodyMaxSize(value, tmpConfig); break ;
			case 5:
				tryFiles(value, tmpConfig); return ;
			default: die("Unrecognized location rule. Aborting", *this);
		}
		text = text.substr(text.find("\n") + 1);
		text = text.substr(text.find_first_not_of(" \t"));
	}
	(void)tmpConfig;
}

void		VirtServ::tryFiles(std::string value, t_config tmpConfig)
{
	std::vector<std::string>	files;
	std::string					defaultFile;
	FILE*						resource;

	while (value.find_first_of(" \t") != std::string::npos)
	{
		files.push_back(value.substr(0, value.find_first_of(" \t")));
		value = value.substr(value.find_first_of(" \t"));
		value = value.find_first_not_of(" \t");
	}
	defaultFile = value;

	std::vector<std::string>::iterator	iter = files.begin();
	while (iter != files.end())
	{
		resource = tryGetResource(*iter, tmpConfig);
		if (resource)
		{
			std::cout << "Eccola" << std::endl;
			return ;
		}
		iter++;
	}
	resource = tryGetResource(defaultFile, tmpConfig);
	// if (!resource)
		// ritorna 404
}

FILE*		VirtServ::tryGetResource(std::string filename, t_config tmpConfig)
{
	std::string		fullPath = tmpConfig.root;
	DIR*			directory;
	struct dirent*	dirent;

	if (!filename.compare("$uri"))
		filename = _request.line.substr(0, _request.line.find_first_of(" \t"));
	if (filename.find("/") != std::string::npos)
	{
		fullPath += filename.substr(0, filename.find_last_of("/") + 1);
		if (filename.find_last_of("/") != filename.length() - 1)
			filename = filename.substr(filename.find_last_of("/") + 1);
		else
			filename = "";
	}
	directory = opendir(fullPath.c_str());
	if (directory == NULL)
	{
		if (errno == EACCES)
			std::cout << "cannot access path" << std::endl;
		if (errno == ENOENT)
			std::cout << "path is non-existent" << std::endl;
		if (errno == ENOTDIR)
			std::cout << "path is not a directory" << std::endl;
	}
	if (filename.length())
	{
		dirent = readdir(directory);
		while (dirent != NULL)
		{
			if (!filename.compare(dirent->d_name))
				answer(fullPath, dirent);
			dirent = readdir(directory);
		}
	}
	else if (tmpConfig.autoindex)
	{
		std::cout << "ready to autoindex directory" << std::endl;
	}
	else
	{
		std::cout << "404 because there is no autoindex" << std::endl;
	}
	return (NULL);
}

void		VirtServ::answer(std::string fullPath, struct dirent* dirent)
{
	std::stringstream	stream;
	std::string			tmpBody;
	std::ostringstream	responseStream;
	std::string			responseString;

	std::ifstream	resource((fullPath + std::string(dirent->d_name)).c_str());
	if (!resource.is_open())
	{
		std::cout << "cannot read file" << std::endl;
		return ;
	}
	_response.line += std::string(dirent->d_name) + " 200 OK";
	stream << resource.rdbuf();
	tmpBody = stream.str();
	_response.headers.find("Content-length")->second = tmpBody.length();
	_response.body = tmpBody;

	responseStream << _response.line << std::endl;
	std::map<std::string, std::string>::iterator	iter = _response.headers.begin();

	while (iter != _response.headers.end())
	{
		responseStream << (*iter).first << ": " << (*iter).second << std::endl;
		iter++;
	}
	responseStream << std::endl << std::endl;
	responseStream << _response.body;

	responseString = responseStream.str();

	send(_connfd, responseString.c_str(), responseString.length(), 0);

	std::cout << "SENT RESPONSE" << std::endl;
	std::cout << responseString << std::endl;
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

void		VirtServ::interpretLocationBlock(t_location* location)
{
	std::string	uri = _request.line.substr(0, _request.line.find_first_of(" \t"));
	std::string::iterator	iter = location->text.begin();

	while (location->text.find("$uri") != std::string::npos)
	{
		location->text.replace(iter + location->text.find("$uri"), iter + location->text.find("$uri") + 4, uri);
	}
}

void		VirtServ::sendResponse()
{

	// Questo era un test semplice prendendo come esempio la roba di appunti.
	// Qui andranno trasformate le varie parti di _response in una c_string da inviare con send.

	std::string	message;
	std::string	sendMessage;
	long		bytesSent;

	message = "FUNCTION sendResponse TEXT:\n\n<!DOCTYPE html><html lang=\"en\"><body><h1> HOME </h1><p> Hello from your Server :) </p></body></html>";
    std::ostringstream ss;
    ss << "HTTP/1.1 200 OK\nContent-Type: text/html\nContent-Length: " << message.size() << "\n\n" << message;

	sendMessage = ss.str();

	bytesSent = send(_connfd, sendMessage.c_str(), sendMessage.size(), 0);
	(void)bytesSent;
}
