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

VirtServ::~VirtServ() { _connfd.clear(); }

//////// Getters & Setters ///////////////////////////////////

int VirtServ::getSocket() { return (_sockfd); }
t_config VirtServ::getConfig() const { return _config; }
std::vector<int> VirtServ::getConnfd() { return _connfd; };

//////// Main Functions //////////////////////////////////////

bool VirtServ::startServer()
{
	int i = 1;

	_sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (_sockfd == -1)
		return bool_error("Socket error\n");
	fcntl(_sockfd, F_SETFL, O_NONBLOCK);
	if (setsockopt(_sockfd, SOL_SOCKET, SO_REUSEADDR, (const char *)&i, sizeof(i)) < 0)
		return bool_error("setsockopt() error\n");
	if (bind(_sockfd, (struct sockaddr *)&_sin, sizeof(_sin)) != 0)
		return bool_error("Bind error");
	if (listen(_sockfd, 10) == -1)
		return bool_error("Listen error");
	return (true);
}

bool VirtServ::stopServer()
{
	_connfd.clear();
	if (close(_sockfd))
		return bool_error("Error closing _sockfd");
	return true;
}

int VirtServ::acceptConnectionAddFd(int sockfd)
{
	socklen_t addrlen;
	struct sockaddr_storage remoteaddr;

	// If listener is ready to read, handle new connection
	addrlen = sizeof remoteaddr;
	if (_connfd.size() == _connfd.capacity())
		_connfd.reserve(1);
	_connfd.push_back(accept(sockfd,
							 (struct sockaddr *)&remoteaddr,
							 &addrlen));
	if (_connfd.back() == -1)
	{
		_connfd.pop_back();
		perror("accept");
		return -1;
	}
	return (_connfd.back());
}

int VirtServ::handleClient(int fd)
{
	char buf[256];

	std::vector<int>::iterator it = std::find(_connfd.begin(), _connfd.end(), fd);
	if (it == _connfd.end())
		return (1);
	int nbytes = recv(fd, buf, sizeof buf, 0);
	if (nbytes <= 0)
	{
		if (nbytes == 0)
		{
			printf("pollserver: socket %d hung up\n", fd);
		}
		else
		{
			perror("recv");
		}
		close(fd);
		return (1);
	}
	else
	{
		this->cleanRequest();
		this->readRequest(buf);
		this->elaborateRequest(fd);
		_connfd.erase(it);
	}
	return (0);
}

void VirtServ::cleanRequest()
{
	_request.line = "";
	_request.body = "";
	std::map<std::string, std::string>::iterator iter = _request.headers.begin();

	for (; iter != _request.headers.end(); iter++)
		(*iter).second = "";
}

void VirtServ::readRequest(std::string req)
{
	std::string key;
	std::map<std::string, std::string>::iterator header;

	_request.line = req.substr(0, req.find_first_of("\n"));
	std::cout << "REQUESTE.LINE " << _request.line << "\n";
	req = req.substr(req.find_first_of("\n") + 1);

	while (req.find_first_not_of(" \t\n\r") != std::string::npos && std::strncmp(req.c_str(), "\n\n", 2))
	{
		key = req.substr(0, req.find_first_of(":"));
		req = req.substr(req.find_first_of(":") + 2);
		header = _request.headers.find(key);
		if (header != _request.headers.end())
			(*header).second = req.substr(0, req.find_first_of("\n"));
		if (!std::strncmp(req.substr(req.find_first_of("\r")).c_str(), "\r\n\r\n", 4))
			break ;
		req = req.substr(req.find_first_of("\n") + 1);
	}

	if (!std::strncmp(req.substr(req.find_first_of("\r")).c_str(), "\r\n\r\n", 4))
	{
		req = req.substr(req.find_first_of("\r") + 4);
		_request.body = req;
	}

	if (req.find_first_not_of("\n") != std::string::npos)
		_request.body = req.substr(req.find_first_not_of("\n"));

	// Check request parsed
	std::cout << "PARSED REQUEST CHECKING" << std::endl;
	std::cout << _request.line << std::endl;
	std::map<std::string, std::string>::iterator iter = _request.headers.begin();
	while (iter != _request.headers.end())
	{
		std::cout << (*iter).first << ": " << (*iter).second << std::endl;
		iter++;
	}
	std::cout << _request.body << std::endl;
}

void VirtServ::elaborateRequest(int dest_fd)
{
	std::string path;
	t_location *location;

	// _response.headers.insert(std::make_pair("set-cookie", "ciao=ciao"));
	_request.method = _request.line.substr(0, _request.line.find_first_of(" "));
	_request.line = _request.line.substr(_request.method.length() + 1);
	path = _request.line.substr(0, _request.line.find_first_of(" "));

	location = searchLocationBlock(_request.method, path, dest_fd);
	if (!location)
		return;
	executeLocationRules(location->text, dest_fd);
	// close(dest_fd);
}

void VirtServ::executeLocationRules(std::string text, int dest_fd)
{
	t_config tmpConfig(_config);
	std::string line;
	std::string key;
	std::string value;
	std::string toCompare[8] = {"root", "autoindex", "index", "error_pages", "client_body_max_sizes", "allowed_methods", "client_body_max_size", "try_files"};
	int i;

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

		for (i = 0; i < 8; i++)
			if (key == toCompare[i])
				break;

		switch (i)
		{
			case 0:
				tmpConfig.root = value;
				break;
			case 1:
			{
				if (!value.compare("on"))
					tmpConfig.autoindex = true;
				else if (!value.compare("off"))
					tmpConfig.autoindex = false;
				else
					die("Autoindex rule must have on|off value. Aborting", *this);
				break;
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
				break;
			}
			case 3:
			{
				// Error_pages
				tmpConfig.errorPages.clear();
				while (value.find_first_not_of(" \n\t") != std::string::npos)
				{
					tmpConfig.errorPages.push_back(value.substr(0, value.find_first_of(" \t")));
					if (value.find_first_of(" \t") == std::string::npos)
						break;
					value = value.substr(value.find_first_of(" \t"));
					value = value.substr(value.find_first_not_of(" \t"));
				}
				break;
			}
			case 4:
				Parser::checkClientBodyMaxSize(value, tmpConfig);
				break;
			case 5:
				insertMethod(tmpConfig, value);
				break ;
			case 6:
				Parser::checkClientBodyMaxSize(value, tmpConfig);
				break ;
			case 7:
				tryFiles(value, tmpConfig, dest_fd);
				return;
			default:
				die("Unrecognized location rule. Aborting", *this);
		}
		text = text.substr(text.find("\n") + 1);
		text = text.substr(text.find_first_not_of(" \t\n"));
	}
}

void VirtServ::insertMethod(t_config &tmpConfig, std::string value)
{
	std::string methods[4] = {"GET", "POST", "DELETE", "PUT"};
	std::string tmp;
	int i;

	tmpConfig.allowedMethods.clear();
	while (value.find_first_of(" \t\n") != std::string::npos)
	{
		tmp = value.substr(0, value.find_first_of(" \t\n"));
		tmpConfig.allowedMethods.push_back(tmp);
		value = value.substr(value.find_first_of(" \t\n"));
		value = value.substr(value.find_first_not_of(" \t\n"));
	}

	for (std::vector<std::string>::iterator iter = tmpConfig.allowedMethods.begin(); iter != tmpConfig.allowedMethods.end(); iter++)
	{
		for (i = 0; i < 4; i++)
		{
			if (!(*iter).compare(methods[i]))
				break;
		}
		if (i == 4)
			die("Method not recognized in location block. Aborting");
	}
}

void VirtServ::tryFiles(std::string value, t_config tmpConfig, int dest_fd)
{
	std::vector<std::string> files;
	std::string defaultFile;

	if (tmpConfig.client_body_max_size && std::strlen(_request.body.c_str()) > tmpConfig.client_body_max_size)
	{
		defaultAnswerError(413, dest_fd, tmpConfig);
		return ;
	}

	while (value.find_first_of(" \t") != std::string::npos)
	{
		files.push_back(value.substr(0, value.find_first_of(" \t")));
		value = value.substr(value.find_first_of(" \t"));
		value = value.substr(value.find_first_not_of(" \t"));
	}

	defaultFile = value;

	std::vector<std::string>::iterator iter = files.begin();
	while (iter != files.end())
	{
		if (tryGetResource(*iter, tmpConfig, dest_fd))
			return;
		iter++;
	}
}

void VirtServ::dirAnswer(std::string fullPath, struct dirent *dirent, int dest_fd, t_config tmpConfig)
{
	DIR *dir;
	std::string path = fullPath + dirent->d_name + "/";

	dir = opendir(path.c_str());
	if (!tmpConfig.autoindex)
	{
		struct dirent *tmp;
		tmp = readdir(dir);
		for (std::vector<std::string>::iterator it = tmpConfig.index.begin(); it != tmpConfig.index.end(); it++)
		{
			while (tmp)
			{
				if (!(std::strcmp(tmp->d_name, (*it).c_str())))
				{
					answer(path, tmp, dest_fd);
					return;
				}
				tmp = readdir(dir);
			}
			closedir(dir);
			dir = opendir(path.c_str());
			tmp = readdir(dir);
		}
		if (!tmp)
		{
			defaultAnswerError(404, dest_fd, tmpConfig);
			return;
		}
	}
	else
	{
		answerAutoindex(path, dir, dest_fd);
	}
}

bool VirtServ::tryGetResource(std::string filename, t_config tmpConfig, int dest_fd)
{
	std::string fullPath = tmpConfig.root;
	DIR *directory;
	struct dirent *dirent;

	if (tmpConfig.allowedMethods.size() && std::find(tmpConfig.allowedMethods.begin(), tmpConfig.allowedMethods.end(), _request.method) == tmpConfig.allowedMethods.end())
	{
		defaultAnswerError(405, dest_fd, _config);
		return (true);
	}

	if (!filename.compare("$uri"))
		filename = _request.line.substr(0, _request.line.find_first_of(" \t"));
	if (filename.find("/") != std::string::npos)
	{
		fullPath += filename.substr(0, filename.find_last_of("/") + 1);
		if (filename.find_last_of("/") != filename.length() - 1)
		{
			filename = filename.substr(filename.find_last_of("/") + 1);
		}
		else
			filename = "";
	}
	if (!(directory = opendir(fullPath.c_str())))
	{
		if (errno == EACCES)
			defaultAnswerError(403, dest_fd, tmpConfig);
		if (errno == ENOENT || errno == ENOTDIR)
			defaultAnswerError(404, dest_fd, tmpConfig);
		return (true);
	}
	if (filename.length())
	{
		while ((dirent = readdir(directory)))
		{
			if (!filename.compare(dirent->d_name))
			{
				if (dirent->d_type == DT_DIR)
				{
					dirAnswer(fullPath, dirent, dest_fd, tmpConfig);
					break;
				}
				answer(fullPath, dirent, dest_fd);
				break;
			}
		}
		if (!dirent)
			defaultAnswerError(404, dest_fd, tmpConfig);
		closedir(directory);
		return (true);
	}
	else if (tmpConfig.autoindex)
	{
		answerAutoindex(fullPath, directory, dest_fd);
		closedir(directory);
		return (true);
	}
	else if (!tmpConfig.autoindex)
	{
		dirent = readdir(directory);
		dirAnswer(fullPath, dirent, dest_fd, tmpConfig);
	}
	else
	{
		defaultAnswerError(404, dest_fd, tmpConfig);
		return (true);
	}
	closedir(directory);
	return (false);
}

void VirtServ::defaultAnswerError(int err, int dest_fd, t_config tmpConfig)
{
	std::string 		tmpString;
	std::stringstream	convert;
	std::ifstream 		file;

	if (tmpConfig.errorPages.size() && err != 500)
	{
		convert << err;
		for (std::vector<std::string>::iterator it = tmpConfig.errorPages.begin(); it != tmpConfig.errorPages.end(); it++)
		{
			if (!std::strncmp(convert.str().c_str(), (*it).c_str(), 3))
			{
				if (*(tmpConfig.root.end() - 1) != '/')
					tmpConfig.root.push_back('/');
				file.open((tmpConfig.root + *it).c_str());
				if (!file)
				{
					file.close();
					defaultAnswerError(500, dest_fd, tmpConfig);
					return;
				}
				break;
			}
		}
	}

	switch (err)
	{
		case 400: tmpString = "400 Bad Request"; break;
		case 401: tmpString = "401 Unauthorized"; break;
		case 402: tmpString = "402 Payment Required"; break;
		case 403: tmpString = "403 Forbidden"; break;
		case 404: tmpString = "404 Not Found"; break;
		case 405: tmpString = "405 Method Not Allowed"; break;
		case 406: tmpString = "406 Not Acceptable"; break;
		case 413: tmpString = "413 Request Entity Too Large"; break ;
		case 500: tmpString = "500 Internal Server Error"; break;
		default: break;
	}

	_response.line = "HTTP/1.1 " + tmpString;

	if (file.is_open())
	{
		convert.clear();
		convert << file.rdbuf();
		file.close();
		_response.body = convert.str();
		_response.body.erase(0, 3);
		convert.clear();
		convert << _response.body.length();
		_response.headers.find("Content-length")->second = convert.str();
		tmpString.clear();
		tmpString = _response.line + "\r\n";
	}
	else
	{
		_response.body = "<html>\n<head><title>" + tmpString + "</title></head>\n<body>\n<center><h1>" + tmpString + "</h1></center>\n<hr><center>webserv</center>\n</body>\n</html>\n";
		convert.str("");
		convert << _response.body.length();
		_response.headers.find("Content-length")->second = convert.str();
		tmpString.clear();
		tmpString = _response.line + "\r\n";
	}

	std::map<std::string, std::string>::iterator iter = _response.headers.begin();

	while (iter != _response.headers.end())
	{
		if ((*iter).second.length())
			tmpString += (*iter).first + ": " + (*iter).second + "\r\n";
		iter++;
	}
	tmpString += "\r\n" + _response.body;
	send(dest_fd, tmpString.c_str(), tmpString.size(), 0);
	std::cout << "SENT RESPONSE" << std::endl;
	std::cout << tmpString << std::endl;
}

struct dirent **VirtServ::fill_dirent(DIR *directory, std::string path)
{
	struct dirent **ret;
	struct dirent *tmp;
	int size = 0;
	int i = 0;

	while ((tmp = readdir(directory)))
		size++;
	closedir(directory);
	directory = opendir(path.c_str());
	ret = (struct dirent **)malloc(sizeof(*ret) * size + 1);
	int j = 0;
	while (i < size)
	{
		tmp = readdir(directory);
		if (tmp && tmp->d_type == DT_DIR)
			ret[j++] = tmp;
		i++;
	}
	i = 0;
	closedir(directory);
	directory = opendir(path.c_str());
	while (i < size)
	{
		tmp = readdir(directory);
		if (tmp && tmp->d_type != DT_DIR)
			ret[j++] = tmp;
		i++;
	}
	ret[j] = NULL;
	return (ret);
}

void VirtServ::answerAutoindex(std::string fullPath, DIR *directory, int dest_fd)
{
	std::ostringstream convert;
	struct dirent **store;
	struct stat attr;
	std::string tmpString;
	std::stringstream output;
	std::string name;

	store = fill_dirent(directory, fullPath);
	_response.line = "HTTP/1.1 200 OK";
	_response.body = "<html>\n<head><title>Index of " + _request.line.substr(0, _request.line.find_first_of(" ")) + "</title></head>\n<body>\n<h1>Index of " + _request.line.substr(0, _request.line.find_first_of(" ")) + "</h1><hr><pre>";
	int i = 0;
	while (store[i] != NULL)
	{
		name = std::string((store[i])->d_name);
		if (std::strncmp(".\0", name.c_str(), 2))
		{
			stat((fullPath + (store[i])->d_name).c_str(), &attr);
			convert << attr.st_size;
			if (store[i]->d_type == DT_DIR)
				name += "/";
			_response.body += "<a href=\"" + name + "\">" + name + "</a>";
			if (std::strncmp("../\0", name.c_str(), 4))
			{
				tmpString = std::string(ctime(&attr.st_mtime)).substr(0, std::string(ctime(&attr.st_mtime)).length() - 1);
				_response.body.append(52 - static_cast<int>(name.length()), ' ');
				_response.body += tmpString;
				_response.body.append(21 - convert.width(), ' ');
				_response.body += store[i]->d_type == DT_DIR ? "-" : convert.str();
			}
			_response.body += "\n";
			convert.str("");
		}
		i++;
	}
	_response.body += "</pre><hr></body>\n</html>";
	convert << _response.body.length();
	tmpString = convert.str();
	_response.headers.find("Content-length")->second = tmpString;
	output << _response.line << "\r" << std::endl;
	for (std::map<std::string, std::string>::iterator iter = _response.headers.begin(); iter != _response.headers.end(); iter++)
		output << (*iter).first << ": " << (*iter).second << "\r" << std::endl;
	output << "\r\n"
		   << _response.body;
	tmpString = output.str();
	send(dest_fd, tmpString.c_str(), tmpString.size(), 0);
	std::cout << "SENT RESPONSE" << std::endl;
	std::cout << tmpString << std::endl;
	delete[] (store);
}
void VirtServ::answer(std::string fullPath, struct dirent *dirent, int dest_fd)
{
	std::stringstream stream;
	std::string tmpString;
	std::string tmpBody;
	std::ostringstream responseStream;
	std::string responseString;

	std::ifstream resource((fullPath + std::string(dirent->d_name)).c_str());
	if (!resource.is_open())
	{
		std::cout << "cannot read file" << std::endl;
		return;
	}
	_response.line += "HTTP/1.1 200 OK";
	stream << resource.rdbuf();
	tmpBody = stream.str();
	stream.str("");
	std::map<std::string, std::string>::iterator iter2 = _response.headers.find("Content-length");
	stream << tmpBody.length();
	stream >> tmpString;
	(*iter2).second = tmpString;
	_response.body = tmpBody;

	responseStream << _response.line << "\r" << std::endl;
	std::map<std::string, std::string>::iterator iter = _response.headers.begin();

	while (iter != _response.headers.end())
	{
		responseStream << (*iter).first << ": " << (*iter).second << "\r" << std::endl;
		iter++;
	}
	responseStream << "\r\n"
				   << _response.body;
	responseString = responseStream.str();

	send(dest_fd, responseString.c_str(), responseString.size(), 0);
	std::cout << "SENT RESPONSE" << std::endl;
	std::cout << responseString << std::endl;
}

t_location *VirtServ::searchLocationBlock(std::string method, std::string path, int dest_fd)
{
	std::vector<t_location>::iterator iter = _config.locationRules.begin();
	t_location *ret = NULL;
	bool regex = false;

	(void)method;
	if (path.at(0) == '~')
		regex = true;

	// Exact corrispondence
	while (iter != _config.locationRules.end() && !regex)
	{
		if (!std::strncmp(path.c_str(), (*iter).location.c_str(), (*iter).location.length()) && !(*iter).regex)
		{
			if (ret == NULL || (*iter).location.length() > ret->location.length())
				ret = &(*iter);
		}
		iter++;
	}

	// Prefix
	if (ret == NULL && !regex)
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
	if (regex)
	{
		path = path.substr(path.find_first_of(" \t"), path.find_first_not_of(" \t"));
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

	if (!ret)
	{
		defaultAnswerError(403, dest_fd, _config);
		return (NULL);
	}
	return (ret);
}

void VirtServ::interpretLocationBlock(t_location *location)
{
	std::string uri = _request.line.substr(0, _request.line.find_first_of(" \t"));
	std::string::iterator iter = location->text.begin();

	while (location->text.find("$uri") != std::string::npos)
		location->text.replace(iter + location->text.find("$uri"), iter + location->text.find("$uri") + 4, uri);
}

bool VirtServ::sendAll(int socket, char *buf, size_t *len)
{
	size_t total = 0;	  // how many bytes we've sent
	int bytesleft = *len; // how many we have left to send
	int n;

	while (total < *len)
	{
		n = send(socket, buf + total, bytesleft, 0);
		if (n == -1)
			break;
		total += n;
		bytesleft -= n;
	}

	*len = total; // return number actually sent here

	return (n == -1 ? false : true);
}
