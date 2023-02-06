#include <Parser.class.hpp>
#include <Server.class.hpp>
#include <VirtServ.class.hpp>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>
#define PORT 8888

//////// Constructors & Destructor //////////////////////////////

VirtServ::VirtServ(t_config config) : _config(config)
{
	memset(&_sin, '\0', sizeof(_sin));
	memset(&_client, '\0', sizeof(_client));
	this->_sin.sin_family = AF_INET;
	this->_sin.sin_addr.s_addr = inet_addr(_config.host.c_str());
	this->_sin.sin_port = htons(_config.port);

	if (!this->startServer())
		die("Server Failed to Start. Aborting... :(");
}

VirtServ::VirtServ(const VirtServ & cpy, unsigned short port)
{
	this->_config = cpy.getConfig();
	memset(&_sin, '\0', sizeof(_sin));
	memset(&_client, '\0', sizeof(_client));
	this->_sin.sin_family = AF_INET;
	this->_sin.sin_addr.s_addr = inet_addr(_config.host.c_str());
	this->_sin.sin_port = htons(port);

	if (!this->startServer())
		die("Server Failed to Start. Aborting... :(");
}

VirtServ::~VirtServ()
{
	_connfd.clear();
}

//////// Getters & Setters ///////////////////////////////////

int					VirtServ::getSocket() { return (_sockfd); }
t_config			VirtServ::getConfig() const { return _config; }
std::vector<int>	VirtServ::getConnfd() { return _connfd; };

//////// Main Functions //////////////////////////////////////

bool	VirtServ::startServer()
{
	int	i = 1;

	_sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (_sockfd == -1)
	{
		std::cerr << "Socket error" << std::endl;
		return (false);
	}
	// fcntl(_sockfd, F_SETFL, O_NONBLOCK);
	if (setsockopt(_sockfd, SOL_SOCKET, SO_REUSEADDR, (const char *)&i, sizeof(i)) < 0)
	{
		std::cerr << "setsockopt() error" << std::endl;
		return (false);
	}
	if (bind(_sockfd, (struct sockaddr *)&_sin, sizeof(_sin)) != 0)
	{
		std::cerr << "Bind error" << std::endl;
		return (false);
	}
	if (listen(_sockfd, 10) == -1) {
		std::cerr << "Listen error" << std::endl;
		return (false);
	}
	return (true);
}

bool	VirtServ::stopServer()
{
	_connfd.clear();
	if (close(_sockfd))
	{
		std::cout << "Error closing _sockfd" << std::endl;
		return (false);
	}
	return (true);
}

int	VirtServ::acceptConnectionAddFd(int sockfd)
{
	socklen_t				addrlen;
	struct sockaddr_storage	remoteaddr;

	// If listener is ready to read, handle new connection
	addrlen = sizeof remoteaddr;
	if (_connfd.size() == _connfd.capacity())
		_connfd.reserve(1);
	_connfd.push_back(accept(sockfd,
		(struct sockaddr *)&remoteaddr,
		&addrlen));
	if (_connfd.back() == -1) {
		_connfd.pop_back();
		perror("accept");
		return -1;
	} 
	return (_connfd.back());
}

int	VirtServ::handleClient(int fd)
{
	char	buf[256];

	std::vector<int>::iterator it = std::find(_connfd.begin(), _connfd.end(), fd);
	if (it == _connfd.end())
		return (1);
	int nbytes = recv(fd, buf, sizeof buf, 0);
	if (nbytes <= 0) {
		if (nbytes == 0) {
			printf("pollserver: socket %d hung up\n", fd);
		} else {
			perror("recv");
		}
		close(fd);
		return (1);
	} else {
		this->cleanRequest();
		this->readRequest(buf);
		this->elaborateRequest(fd);
		_connfd.erase(it);
	}
	return (0);
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

void		VirtServ::elaborateRequest(int dest_fd)
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
	executeLocationRules(location->text, dest_fd);
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

void		VirtServ::executeLocationRules(std::string text, int dest_fd)
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
				tryFiles(value, tmpConfig, dest_fd); return ;
			default: die("Unrecognized location rule. Aborting", *this);
		}
		text = text.substr(text.find("\n") + 1);
		text = text.substr(text.find_first_not_of(" \t\n"));
	}
	(void)tmpConfig;
}

void		VirtServ::tryFiles(std::string value, t_config tmpConfig, int dest_fd)
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
		resource = tryGetResource(*iter, tmpConfig, dest_fd);
		if (resource)
		{
			std::cout << "Eccola" << std::endl;
			return ;
		}
		iter++;
	}
	resource = tryGetResource(defaultFile, tmpConfig, dest_fd);
	// if (!resource)
		// ritorna 404
}

FILE*		VirtServ::tryGetResource(std::string filename, t_config tmpConfig, int dest_fd)
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
				answer(fullPath, dirent, dest_fd);
			dirent = readdir(directory);
		}
	}
	else if (tmpConfig.autoindex)
		answerAutoindex(fullPath, directory, dest_fd);
	else
		std::cout << "404 because there is no autoindex" << std::endl;
	return (NULL);
}

void		VirtServ::answerAutoindex(std::string fullPath, DIR* directory, int dest_fd)
{
	std::string			body;
	std::ostringstream	convert;
	struct dirent*		dirent;
	struct stat			attr;
	std::string			tmpString;
	std::stringstream	output;

	_response.line = "HTTP/1.1 200 OK";
	body = "<html>\n<head><title>Index of " + _request.line.substr(0, _request.line.find_first_of(" ")) + "</title></head>\n<body>\n<h1>Index of " + _request.line.substr(0, _request.line.find_first_of(" ")) + "</h1><hr><pre>";
	dirent = readdir(directory);
	while (dirent != NULL)
	{
		stat((fullPath + dirent->d_name).c_str(), &attr);
		convert << attr.st_size;
		body += "<a href=\"" + std::string(dirent->d_name) + "\">" + dirent->d_name + "</a>\t\t\t\t\t\t\t\t\t\t" + ctime(&attr.st_mtime) + "\t\t\t\t\t" + convert.str() + "\n";
		convert.str("");
		dirent = readdir(directory);
	}
	body += "</pre><hr></body>\n</html>";
	_response.body = body;
	convert << _response.body.length();
	tmpString = convert.str();
	_response.headers.find("Content-length")->second = tmpString;

	output << _response.line << "\r" << std::endl;
	std::map<std::string, std::string>::iterator	iter = _response.headers.begin();

	while (iter != _response.headers.end())
	{
		output << (*iter).first << ": " << (*iter).second << "\r" << std::endl;
		iter++;
	}
	output << "\r" << std::endl;
	output << _response.body;

	tmpString = output.str();

	send(dest_fd, tmpString.c_str(), tmpString.size(), 0);
	std::cout << "SENT RESPONSE" << std::endl;
	std::cout << tmpString << std::endl;
}

void		VirtServ::answer(std::string fullPath, struct dirent* dirent, int dest_fd)
{
	std::stringstream	stream;
	std::string			tmpString;
	std::string			tmpBody;
	std::ostringstream	responseStream;
	std::string			responseString;

	std::ifstream	resource((fullPath + std::string(dirent->d_name)).c_str());
	if (!resource.is_open())
	{
		std::cout << "cannot read file" << std::endl;
		return ;
	}
	_response.line += "200 OK";
	stream << resource.rdbuf();
	tmpBody = stream.str();
	stream.str("");
	std::map<std::string, std::string>::iterator	iter2 = _response.headers.find("Content-length");
	stream << tmpBody.length();
	stream >> tmpString;
	(*iter2).second = tmpString;
	_response.body = tmpBody;

	responseStream << _response.line << "\r" << std::endl;
	std::map<std::string, std::string>::iterator	iter = _response.headers.begin();

	while (iter != _response.headers.end())
	{
		responseStream << (*iter).first << ": " << (*iter).second << "\r" << std::endl;
		iter++;
	}
	responseStream << "\r" << std::endl;
	responseStream << _response.body;

	responseString = responseStream.str();

	send(dest_fd, responseString.c_str(), responseString.size(), 0);

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

bool	VirtServ::sendAll(int socket, char*buf, size_t *len)
{
	size_t total = 0;        // how many bytes we've sent
    int bytesleft = *len; // how many we have left to send
    int n;

    while(total < *len) {
        n = send(socket, buf+total, bytesleft, 0);
        if (n == -1) 
			break;
        total += n;
        bytesleft -= n;
    }

    *len = total; // return number actually sent here

    return (n == -1 ? false : true);
}

// void		VirtServ::sendResponse()
// {

// 	// Questo era un test semplice prendendo come esempio la roba di appunti.
// 	// Qui andranno trasformate le varie parti di _response in una c_string da inviare con send.

// 	std::string	message;
// 	std::string	sendMessage;
// 	long		bytesSent;

// 	message = "FUNCTION sendResponse TEXT:\n\n<!DOCTYPE html><html lang=\"en\"><body><h1> HOME </h1><p> Hello from your Server :) </p></body></html>";
// 	std::ostringstream ss;
// 	ss << "HTTP/1.1 200 OK\nContent-Type: text/html\nContent-Length: " << message.size() << "\n\n" << message;

// 	sendMessage = ss.str();

// 	bytesSent = send(_connfd, sendMessage.c_str(), sendMessage.size(), 0);
// 	(void)bytesSent;
// }
