#include <Parser.class.hpp>
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

bool    VirtServ::startListen()
{
    this->_fd_size = 5;
    struct sockaddr_storage remoteaddr;   // Client address
    socklen_t               addrlen;
    int                     newfd;        // Newly accept()ed socket descriptor
    char                    buf[256];     // Buffer for client data
    this->_pfds = (struct pollfd*)malloc(sizeof(*_pfds) * _fd_size);  // We  start creating arbitrary 5 sockets
    // Set up and get a listening socket
    _sockfd = get_listener_socket(_config.host.c_str(), "12372");
    if (_sockfd == -1) {
        fprintf(stderr, "error getting listening socket\n");
        exit(1);
    }
     // Add the listener to set
    _pfds[0].fd = _sockfd;
    _pfds[0].events = POLLIN; // Report ready to read on incoming connection
    this->_fd_count = 1; // For the listener
    for(;;) {
        int poll_count = poll(_pfds, _fd_count, -1);
        if (poll_count == -1) {
            perror("poll");
            exit(1);
        }
        // Run through the existing connections looking for data to read
        for(int i = 0; i < _fd_count; i++) {
            // Check if someone's ready to read
            if (_pfds[i].revents & POLLIN) { // We got one!!
                if (_pfds[i].fd == _sockfd) {
                    // If listener is ready to read, handle new connection
                    addrlen = sizeof remoteaddr;
                    newfd = accept(_sockfd,
                        (struct sockaddr *)&remoteaddr,
                        &addrlen);
                    if (newfd == -1) {
                        perror("accept");
                    } else {
                        add_to_pfds(&_pfds, newfd, &_fd_count, &_fd_size);
                        printf("pollserver: new connection from %s on "
                            "socket %d\n", "localhost",
                            newfd);
                    }
                } else {
                    // If not the listener, we're just a regular client
                    int nbytes = recv(_pfds[i].fd, buf, sizeof buf, 0);
                    int sender_fd = _pfds[i].fd;
                    if (nbytes <= 0) {
                        // Got error or connection closed by client
                        if (nbytes == 0) {
                            // Connection closed
                            printf("pollserver: socket %d hung up\n", sender_fd);
                        } else {
                            perror("recv");
                        }
                        close(_pfds[i].fd); // Bye!
                        del_from_pfds(_pfds, i, &_fd_count);
                    } else {
                        // We got some good data from a client
                        for(int j = 0; j < _fd_count; j++) {
                            // Send to everyone!
                            int dest_fd = _pfds[j].fd;
                            // Except the listener and ourselves
                            if (dest_fd != _sockfd && dest_fd != sender_fd) {
                                if (send(dest_fd, buf, nbytes, 0) == -1) {
                                    perror("send");
                                }
                            }
                        }
                    }
                } // END handle data from client
            } // END got ready-to-read from poll()
        } // END looping through file descriptors
    } // END for(;;)--and you thought it would never end!
    return (true);
}

// Add a new file descriptor to the set
void VirtServ::add_to_pfds(struct pollfd *pfds[], int newfd, int *fd_count, int *fd_size)
{
    // If we don't have room, add more space in the pfds array
    if (*fd_count == *fd_size) {
        *fd_size *= 2; // Double it
        *pfds = (struct pollfd *)realloc(*pfds, sizeof(**pfds) * (*fd_size));
    }
    (*pfds)[*fd_count].fd = newfd;
    (*pfds)[*fd_count].events = POLLIN; // Check ready-to-read
    (*fd_count)++;
}

// Remove an index from the set
void VirtServ::del_from_pfds(struct pollfd pfds[], int i, int *fd_count)
{
    // Copy the one from the end over this one
    pfds[i] = pfds[*fd_count-1];
    (*fd_count)--;
}

// Return a listening socket
int VirtServ::get_listener_socket(std::string addr, const char *port)
{
    int listener;     // Listening socket descriptor
    int yes=1;        // For setsockopt() SO_REUSEADDR, below
    int rv;
    addr.begin();
    struct addrinfo hints, *ai, *p;
    // Get us a socket and bind it
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    if ((rv = getaddrinfo(NULL, port, &hints, &ai)) != 0) {
        fprintf(stderr, "selectserver: %s\n", gai_strerror(rv));
        exit(1);
    }
    for(p = ai; p != NULL; p = p->ai_next) {
        listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (listener < 0) {
            continue;
        }
        // Lose the pesky "address already in use" error message
        setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
        if (bind(listener, p->ai_addr, p->ai_addrlen) < 0) {
            close(listener);
            continue;
        }
        break;
    }
    freeaddrinfo(ai); // All done with this
    // If we got here, it means we didn't get bound
    if (p == NULL) {
        return -1;
    }
    // Listen
    if (listen(listener, 10) == -1) {
        return -1;
    }
    return listener;
}

// bool	VirtServ::startListen()
// {
// 	int	bytes;

// 	if (listen(_sockfd, 20) < 0)
// 	{
// 		std::cerr << "Listen failed" << std::endl;
// 		return (false);
// 	}
// 	while (true)
// 	{

// 		// Accept connection

// 		_size = sizeof(_client);
// 		_connfd = accept(_sockfd, (struct sockaddr *)&_client, &_size);
// 		if (_connfd < 0)
// 		{
// 			std::cerr << "Error while accepting connection." << std::endl;
// 			return (false);
// 		}

// 		// Read communication from client
// 		char	buffer[1048576] = { 0 };
// 		bytes = recv(_connfd, buffer, 1048576, 0);
// 		if (bytes == -1)
// 		{
// 			std::cerr << "Error receiving" << std::endl;
// 			return (false);
// 		}

// 		// Parse request
// 		cleanRequest();
// 		readRequest(buffer);
// 		elaborateRequest();

// 		// Parse Response da implementare in base al path della Request
// 		// dove si andrà a popolare _response con i vari campi descritti in costruzione
// 		// Potrebbero servire altri campi, questi sono quelli essenziali ! 
// 		// selectRepsonse();

// 		// Send Response
// 		sendResponse();

// 		// Close connection
// 		close(_connfd);
// 		_connfd = 0;
// 	}
// 	return (true);
// }

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
		answerAutoindex(fullPath, directory);
	}
	else
	{
		std::cout << "404 because there is no autoindex" << std::endl;
	}
	return (NULL);
}

void		VirtServ::answerAutoindex(std::string fullPath, DIR* directory)
{
	std::stringstream	stream;
	struct dirent*		dirent;
	struct stat			attr;
	std::string			tmpString;

	stream << "<html>\n<head><title>Index of " \
		<< _request.line.substr(0, _request.line.find_first_of(" ")) << "</title></head>\n<body>\n<h1>Index of " << _request.line.substr(0, _request.line.find_first_of(" ")) << "</h1><hr><pre>";
	dirent = readdir(directory);
	while (dirent != NULL)
	{
		stat((fullPath + dirent->d_name).c_str(), &attr);
		stream << "<a href=\"" << dirent->d_name << "\">" << dirent->d_name << "</a>\t\t\t\t\t\t\t\t\t\t" << ctime(&attr.st_mtime) << "\t\t\t\t\t" << attr.st_size << "\n";
		dirent = readdir(directory);
	}
	stream << "</pre><hr></body>\n</html>";
	_response.body = stream.str();
	stream.str("");
	stream << _request.body.length();
	stream >> tmpString;
	_request.headers.find("Content-length")->second = tmpString;

	stream.str("");
	stream << _response.line << "\r" << std::endl;
	std::map<std::string, std::string>::iterator	iter = _response.headers.begin();

	while (iter != _response.headers.end())
	{
		stream << (*iter).first << ": " << (*iter).second << "\r" << std::endl;
		iter++;
	}
	stream << "\r" << std::endl;
	stream << _response.body;

	tmpString = stream.str();

	send(_connfd, tmpString.c_str(), tmpString.size(), 0);
	std::cout << "SENT RESPONSE" << std::endl;
	std::cout << tmpString << std::endl;
}

void		VirtServ::answer(std::string fullPath, struct dirent* dirent)
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

	send(_connfd, responseString.c_str(), responseString.size(), 0);

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
