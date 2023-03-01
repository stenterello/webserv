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
#include <string>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#define IOV_MAX 1024

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

VirtServ::~VirtServ() { _connections.clear(); }

//////// Getters & Setters ///////////////////////////////////

int VirtServ::getSocket() { return (_sockfd); }
t_config VirtServ::getConfig() const { return _config; }

//////// Main Functions //////////////////////////////////////

/*
	Inizializza il socket, lo setta come non-blocking, lo binda e lo mette in listen per massimo 10 client;
*/

bool VirtServ::startServer()
{
	int i = 1;

	_sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (_sockfd == -1)
		return bool_error("Socket error\n");
	if (setsockopt(_sockfd, SOL_SOCKET, SO_REUSEADDR, (const char *)&i, sizeof(i)) < 0)
		return bool_error("setsockopt() error\n");
	if (bind(_sockfd, (struct sockaddr *)&_sin, sizeof(_sin)) != 0)
		return bool_error("Bind error");
	if (listen(_sockfd, 10) == -1)
		return bool_error("Listen error");
	return (true);
}

/*
	Fa il clear del vettore delle connessioni e chiude il socket;
*/

bool VirtServ::stopServer()
{
	_connections.clear();
	if (close(_sockfd))
		return bool_error("Error closing _sockfd");
	return true;
}

/*
	Dopo poll, accetta la connessione del client e la salva nel vettore delle connessioni del server. Ritorna il valore dell'fd della connessione;
*/

int VirtServ::acceptConnectionAddFd(int sockfd)
{
	socklen_t addrlen;
	struct sockaddr_storage remoteaddr;

	addrlen = sizeof remoteaddr;
	if (_connections.size() == _connections.capacity())
		_connections.reserve(1);
	_connections.push_back(t_connInfo(accept(sockfd,
							 (struct sockaddr *)&remoteaddr,
							 &addrlen)));
	if (_connections.back().connfd == -1)
	{
		_connections.pop_back();
		perror("accept");
		return -1;
	}
	fcntl(_connections.back().connfd, F_SETFL, O_NONBLOCK);
	return (_connections.back().connfd);
}

/*
	Inizia la gestione della richiesta del client;
		- recv sull'fd, salvataggio in buf (char [256]);
		- pulisce la variabile privata request di VirtServ;
		- riempie la struttura request (readRequest());
		- lancia la funzione di elaborazione della richiesta (elaborateRequest());
		- chiude la connessione e libera il valore nel vettore delle connessioni;
*/

std::vector<t_connInfo>::iterator	VirtServ::findFd(std::vector<t_connInfo>::iterator begin, std::vector<t_connInfo>::iterator end, int fd)
{
	while (begin != end)
	{
		if (begin->connfd == fd)
			return (begin);
		begin++;
	}
	return (begin);
}

int VirtServ::handleClient(int fd)
{
	char buf[512];
	static char totalBuffer[2048];

	printf("------HANDLE CLIENT------\n");
	std::vector<t_connInfo>::iterator it = findFd(_connections.begin(), _connections.end(), fd);
	if (it == _connections.end())
		return (0);
	static int c = 0;
	int nbytes, i = 0;
	while (_request.method == "") {
		nbytes = recv(fd, buf, sizeof buf, 0);
		if (nbytes <= 0)
		{
			if (nbytes == 0) {
				printf("pollserver: socket %d hung up\n", fd);
			}
			else
				perror("recv");
			_connections.erase(it);
			return (1);
		}
		else
		{
			for (i = 0; i < nbytes; i++, c++)
				totalBuffer[c] = buf[i];
			memset(buf, 0, 512);
		}
		// if we find the trailing CRLF(\r\n) we've found the headers. If not we go back to poll()
		if (!(strcmp(&totalBuffer[c - 4], "\r\n\r\n")))
			break ;
		if (nbytes < 512)
			return 0;
	}
	for (i = 0; i < c; i++) {
		if (!(strncmp(totalBuffer + i, "\r\n\r\n", 4)))
			break ;
		}
	i += 4;
	char *header;
	if (i < c) {
		int x;
		header = (char *)malloc(sizeof(*header) * i + 1);
		for (x = 0; x < i; x++)
			header[x] = totalBuffer[x];
		header[x] = '\0';
		for (x = 0; i < c; x++)
			totalBuffer[x] = totalBuffer[i++];
		for (; c > x;)
			totalBuffer[--c] = 0;
		c = 0;
		memset(totalBuffer, 0, 2048);
	}
	else {
		totalBuffer[c] = '\0';
		header = strdup(totalBuffer);
		memset(totalBuffer, 0, 2048);
		c = 0;
	}
	int (VirtServ::*execMethod[])(t_connInfo & info) = {&VirtServ::execPost, &VirtServ::execPut};
	std::string	method[] = {"POST", "PUT"};
	for (int i = 0; i < 2; i++) {
		if (method[i] == _request.method) {
			if ((this->*execMethod[i])(*it) == 1) {
				_connections.erase(it);
				return 1;
			}
			return 0;
		}
	}
	printf("------HEADER------\n%s\n------END HEADER------\n", header);
	this->cleanRequest();
	this->cleanResponse();
	if (this->readRequest(header)) {
		this->elaborateRequest(fd);
		if (_request.method == "GET" || _request.method == "HEAD") {
			this->cleanRequest();
			this->cleanResponse();
		}
	}
	else
		defaultAnswerError(400, fd, _config);
	return (0);
}

/*
	Pulisce la variabile privata _request;
*/

void VirtServ::cleanRequest()
{
	_request.line = "";
	_request.body = "";
	_request.method = "";
	std::vector<std::pair<std::string, std::string> >::iterator iter = _request.headers.begin();

	for (; iter != _request.headers.end(); iter++)
		(*iter).second = "";
}

/*
	Pulisce la variabile privata _response;
*/

void VirtServ::cleanResponse()
{
	_response.line = "";
	_response.body = "";
	std::vector<std::pair<std::string, std::string> >::iterator iter = _response.headers.begin();

	for (; iter != _response.headers.end(); iter++)
		(*iter).second = "";
}

/*
	Parsing della richiesta letta dal client;
*/

int VirtServ::readRequest(std::string req)
{
	std::string key;
	std::vector<std::pair<std::string, std::string> >::iterator header;
	std::string comp[] = {"GET", "POST", "DELETE", "PUT", "HEAD"};

	_request.line = req.substr(0, req.find_first_of("\n"));
	int i;
	for (i = 0; i < 5; i++)
	{
		if (_request.line.find(comp[i]) != std::string::npos)
			break;
	}
	if (i == 5)
		return 0;
	req = req.substr(req.find_first_of("\n") + 1);

	while (req.find_first_not_of(" \t\n\r") != std::string::npos && std::strncmp(req.c_str(), "\n\n", 2))
	{
		key = req.substr(0, req.find_first_of(":"));
		req = req.substr(req.find_first_of(":") + 2);
		header = findKey(_request.headers, key);
		if (header != _request.headers.end())
			(*header).second = req.substr(0, req.find_first_of("\r\n"));
		if (req.find_first_of("\r") != std::string::npos && std::strlen(req.substr(req.find_first_of("\r")).c_str()) > 4 && !std::strncmp(req.substr(req.find_first_of("\r")).c_str(), "\r\n\r\n", 4))
			break;
		req = req.substr(req.find_first_of("\n") + 1);
	}

	if (req.find_first_of("\r") != std::string::npos && std::strlen(req.substr(req.find_first_of("\r")).c_str()) > 4 && !std::strncmp(req.substr(req.find_first_of("\r")).c_str(), "\r\n\r\n", 4))
	{
		if (header != _request.headers.end())
			(*header).second = req.substr(0, req.find_first_of("\r\n"));
		req = req.substr(req.find_first_of("\r") + 4);
		_request.body = req;
	}

	if (req.find_first_not_of("\r\n") != std::string::npos)
		_request.body = req.substr(req.find_first_not_of("\r\n"));

	if (findKey(_request.headers, "Expect") != _request.headers.end())
	{
		return (1);
	}
	// Check request parsed
	std::cout << "REQUEST LINE " + _request.line << std::endl;
	std::vector<std::pair<std::string, std::string> >::iterator iter = _request.headers.begin();
	while (iter != _request.headers.end())
	{
		std::cout << (*iter).first << ": " << (*iter).second << std::endl;
		iter++;
	}
	std::cout << _request.body << std::endl;
	return (0);
}

/*
	Cerca il location block corretto nella configurazione del virtual server e lancia la funzione executeLocationRules;
*/

void VirtServ::elaborateRequest(int dest_fd)
{
	std::string path;
	t_location *location;

	// _response.headers.insert(std::make_pair("set-cookie", "ciao=ciao"));
	_request.method = _request.line.substr(0, _request.line.find_first_of(" "));
	if (std::strlen(_request.method.c_str()) < std::strlen(_request.line.c_str()))
	{
		_request.line = _request.line.substr(_request.method.length() + 1);
		path = _request.line.substr(0, _request.line.find_first_of(" "));
	}
	else
	{
		_request.line = _request.method;
		path = "/";
	}

	location = searchLocationBlock(_request.method, path, dest_fd);
	if (!location)
		return;
	executeLocationRules(location->location, location->text, dest_fd);
	// close(dest_fd);
}

/*
	Elabora il location block implementando le diverse configurazioni espresse nel location block, andando a cercare quando deve il file.
*/

void VirtServ::executeLocationRules(std::string locationName, std::string text, int dest_fd)
{
	t_config tmpConfig(_config);
	std::string line;
	std::string key;
	std::string value;
	std::string toCompare[8] = {"root", "autoindex", "index", "error_page", "client_max_body_sizes", "allowed_methods", "try_files", "return"};
	int i;

	std::cout << "EXECUTE LOCATION RULES\n";
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
		{
			tmpConfig.root = value;
			break;
		}
		case 1:
		{
			std::cout << "------CASE 1------\n";
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
			// sistemare questo caso
			std::cout << "------CASE 2------\n";
			tmpConfig.index.clear();
			while (value.find_first_not_of(" \n\t") != std::string::npos)
			{
				tmpConfig.index.push_back(value.substr(0, value.find_first_of(" \t")));
				value = value.substr(0, value.find_first_of(" \t"));
				value = value.substr(0, value.find_first_not_of(" \t"));
			}
			break;
		}
		case 3:
		{
			// error_page
			std::cout << "------CASE 3------\n";
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
		{
			std::cout << "------CHECK CLIENT BODY MAX SIZE------\n";
			Parser::checkClientBodyMaxSize(value, tmpConfig);
			break;
		}
		case 5:
		{
			std::cout << "------INSERT METHOD------\n";
			insertMethod(tmpConfig, value);
			break;
		}
		case 6:
		{
			std::cout << "------TRY FILES------\n";
			tryFiles(value, tmpConfig, dest_fd, locationName);
			return;
		}
		case 7:
		{
			std::cout << "------CHECK AND REDIRECT------\n";
			checkAndRedirect(value, dest_fd);
			return;
		}
		default:
			die("Unrecognized location rule. Aborting", *this);
		}
		text = text.substr(text.find("\n") + 1);
		text = text.substr(text.find_first_not_of(" \t\n"));
	}
}

/*
	Handle redirection
*/

void VirtServ::checkAndRedirect(std::string value, int dest_fd)
{
	std::string code;
	std::string phrase;
	std::string phrases[1] = {"301"};
	std::string tmpString;
	std::stringstream output;
	size_t i;

	if (value.find_first_of(" \t") == std::string::npos)
		die("Return rule must be written as: 'return code address'. Aborting");

	code = value.substr(0, value.find_first_of(" \t"));

	for (i = 0; i < phrases->size(); i++)
	{
		if (!code.compare(phrases[i]))
			break;
	}

	if (i == phrases->size())
		die("Return code unrecognized. Aborting");

	switch (i)
	{
	case 0:
		phrase = "Moved Permanently";
		break;
	default:
		break;
	}

	value = value.substr(value.find_first_of(" \t"));
	value = value.substr(value.find_first_not_of(" \t"));
	if (std::strncmp("http://", value.c_str(), 7))
		die("Protocol HTTP is not specified in the given redirection address. Aborting");

	_response.line = "HTTP/1.1 " + code + " " + phrase;
	findKey(_response.headers, "Location")->second = value;

	output << _response.line << "\r" << std::endl;
	for (std::vector<std::pair<std::string, std::string> >::iterator iter = _response.headers.begin(); iter != _response.headers.end(); iter++)
	{
		if ((*iter).second.length())
			output << (*iter).first << ": " << (*iter).second << "\r" << std::endl;
	}
	output << "\r\n"
		   << _response.body;

	tmpString = output.str();

	send(dest_fd, tmpString.c_str(), tmpString.size(), 0);
}

/*
	Parsing del location block:> modifica dei metodi permessi.
*/

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
			if (*iter == methods[i])
				break;
		}
		if (i == 4)
			die("Method not recognized in location block. Aborting");
	}
}

/*
	Parsing del location block:> effettiva ricerca del file con try_file.
*/

void VirtServ::tryFiles(std::string value, t_config tmpConfig, int dest_fd, std::string locationName)
{
	std::vector<std::string> files;
	std::string defaultFile;

	if (tmpConfig.client_max_body_size && std::strlen(_request.body.c_str()) > tmpConfig.client_max_body_size)
	{
		defaultAnswerError(413, dest_fd, tmpConfig);
		return;
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
		if (tryGetResource(*iter, tmpConfig, dest_fd, locationName))
			return;
		iter++;
	}
}

/*
	Apre la cartella di riferimento: se l'autoindex è su off, cerca il file di default (tmpConfig.index [vector]), ritorna 404 in caso non sia presente.
									 se l'autoindex è su on, ritorna l'autoindex della cartella.
*/

DIR *VirtServ::dirAnswer(std::string fullPath, struct dirent *dirent, int dest_fd, t_config tmpConfig)
{
	std::cout << "------DIR ANSWER------\n";
	DIR *dir;

	if (*fullPath.rbegin() != '/')
		fullPath.append(("/"));

	std::string path = dirent ? fullPath + dirent->d_name + "/" : fullPath;

	dir = opendir(path.c_str());
	if (!tmpConfig.autoindex || tmpConfig.index.size())
	{
		struct dirent *tmp;
		tmp = readdir(dir);
		for (std::vector<std::string>::iterator it = tmpConfig.index.begin(); it != tmpConfig.index.end(); it++)
		{
			while (tmp)
			{
				if (!(std::strcmp(tmp->d_name, (*it).c_str())))
				{
					std::cout << "PATH " << path << std::endl;
					std::cout << "TMP->DIR NAME " << tmp->d_name << std::endl;
					answer(path, tmp, dest_fd);
					return dir;
				}
				tmp = readdir(dir);
			}
			closedir(dir);
			dir = opendir(path.c_str());
			tmp = readdir(dir);
		}
		defaultAnswerError(404, dest_fd, tmpConfig);
		return dir;
	}
	else
	{
		answerAutoindex(path, dir, dest_fd);
	}
	return (dir);
}

/*
	Prende il valore Content-Length dalla richiesta e lo usa come benchmark per la lettura del file da uploadare.
	Apre il file (deve inserire corretttamente il nome)
	Riceve e contestualmente scrive il body ricevuto nel file
	Chiude il file e la connessione
*/

int	VirtServ::launchCGI()
{
	if (!fork()) {
		system("fake_site/cgi_tester");
		exit (0);
	}
	else
		waitpid(-1, 0, 0);
	return 0;
}

/*
	Funzione principale di indirizzamento della gestione della richiesta: da qui il server decide effettivamente cosa tornare al client.

	Cerca il filename.
	Crea il path completo del file per verificarne permessi e accessibilità
	Controlli effettuati:
		- Se il metodo non è permesso, ritorna errore 405;
		- Se il metodo utilizzato è POST, chiama la funzione execPost() e ritorna;
		- Se il file richiesto non presenta permessi adatti per l'utente ritorna 403;
		- Se il file richiesto non è presente nella cartella, ritorna 404;
		- Se il filename è definito, lo cerca nella directory prevista;
		- Se il filename non è definito e l'autoindex è settato su on, restituisce l'autoindex;
		- In caso contrario ritorna 404;
*/

bool VirtServ::tryGetResource(std::string filename, t_config tmpConfig, int dest_fd, std::string locationName)
{
	std::cout << "------TRY GET RESOURCE------\n";
	std::string fullPath = tmpConfig.root;
	char rootPath[tmpConfig.root.size()];
	std::string	rootPath2;
	DIR *directory;
	struct dirent *dirent;

	tmpConfig.root.copy(rootPath, tmpConfig.root.size());
	rootPath[tmpConfig.root.size()] = '\0';
	rootPath2 = rootPath;
	if (_request.method == "POST" || _request.method == "PUT")
		return true;
	if (tmpConfig.allowedMethods.size() && std::find(tmpConfig.allowedMethods.begin(), tmpConfig.allowedMethods.end(), _request.method) == tmpConfig.allowedMethods.end())
	{
		defaultAnswerError(405, dest_fd, _config);
		return (true);
	}
	if (!filename.compare("$uri"))
		filename = _request.line.substr(0, _request.line.find_first_of(" \t"));
	if (!std::strncmp(locationName.c_str(), filename.c_str(), std::strlen(locationName.c_str())))
	{
		filename = filename.substr(std::strlen(locationName.c_str()));
		if (*filename.begin() == '/' && filename.length() > 1)
			filename = filename.substr(1);
		else if (*filename.begin() == '/')
			filename = "";
		if (filename.find('/') != std::string::npos)
		{
			if (*rootPath2.rbegin() != '/')
				rootPath2 = std::string(rootPath) + "/" + filename.substr(0, filename.find_last_of("/"));
			else
				rootPath2 = std::string(rootPath) + filename.substr(0, filename.find_last_of("/"));
			filename = filename.substr(filename.find_last_of("/"));
			if (*filename.begin() == '/' && filename.length() > 1)
				filename = filename.substr(1);
			else if (*filename.begin() == '/')
				filename = "";
		}
	}
	if (!(directory = opendir(rootPath2.c_str())))
	{
		if (errno == EACCES)
			defaultAnswerError(403, dest_fd, tmpConfig);
		if (errno == ENOENT || errno == ENOTDIR)
			defaultAnswerError(404, dest_fd, tmpConfig);
		return (true);
	}
	if (filename.length())
	{
		std::cout << "FILENAME " << filename << std::endl;
		std::cout << "ROOTPATH " << rootPath2 << std::endl;
		while ((dirent = readdir(directory)))
		{
			if (!filename.compare(dirent->d_name) || !(filename.compare("directory")))
			{
				if (!(filename.compare("directory")))
				{
					closedir(dirAnswer(rootPath2, NULL, dest_fd, tmpConfig));
					break;
				}
				if (dirent->d_type == DT_DIR)
				{
					closedir(dirAnswer(rootPath2, dirent, dest_fd, tmpConfig));
					break;
				}
				else
				{
					answer(rootPath2, dirent, dest_fd);
					break;
				}
			}
		}
		if (!dirent)
			defaultAnswerError(404, dest_fd, tmpConfig);
		closedir(directory);
		return (true);
	}
	else if (tmpConfig.autoindex)
	{
		answerAutoindex(rootPath2, directory, dest_fd);
		closedir(directory);
		return (true);
	}
	else if (!tmpConfig.autoindex)
	{
		closedir(directory);
		directory = dirAnswer(rootPath2, NULL, dest_fd, tmpConfig);
		closedir(directory);
		return true;
	}
	else
	{
		defaultAnswerError(404, dest_fd, tmpConfig);
		return (true);
	}
	closedir(directory);
	return (false);
}

/*
	Funzione di restituizione della pagina di errore: prima verifica la presenza di pagine custom di errore e, in caso, torna quelle;
	altrimenti opera un crafting manuale delle stesse, per poi tornarle.
*/

void VirtServ::defaultAnswerError(int err, int dest_fd, t_config tmpConfig)
{
	std::string tmpString;
	std::stringstream convert;
	std::ifstream file;

	std::cout << "------DEFAULT ANSWER ERROR------\n";
	if (tmpConfig.errorPages.size() && err != 500)
	{
		convert << err;
		for (std::vector<std::string>::iterator it = tmpConfig.errorPages.begin(); it != tmpConfig.errorPages.end(); it++)
		{
			if (!std::strncmp(convert.str().c_str(), (*it).c_str(), 3))
			{
				if (*(_config.root.end() - 1) != '/')
					_config.root.push_back('/');
				file.open((_config.root + *it).c_str());
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
		case 100: tmpString = "100 Continue"; break ;
		case 200: tmpString = "200 OK"; break ;
		case 201: tmpString = "201 Created"; break ;
		case 202: tmpString = "202 Accepted"; break ;
		case 203: tmpString = "203 Non-Authoritative Information"; break ;
		case 204: tmpString = "204 No content"; break ;
		case 205: tmpString = "205 Reset Content"; break ;
		case 206: tmpString = "206 Partial Content"; break ;
		case 400: tmpString = "400 Bad Request"; break ;
		case 401: tmpString = "401 Unauthorized"; break ;
		case 402: tmpString = "402 Payment Required"; break ;
		case 403: tmpString = "403 Forbidden"; break ;
		case 404: tmpString = "404 Not Found"; break ;
		case 405: tmpString = "405 Method Not Allowed"; break ;
		case 406: tmpString = "406 Not Acceptable"; break ;
		case 411: tmpString = "411 Length Required"; break ;
		case 413: tmpString = "413 Request Entity Too Large"; break ;
		case 500: tmpString = "500 Internal Server Error"; break ;
		case 501: tmpString = "501 Not Implemented"; break ;
		case 510: tmpString = "510 Not Extended"; break ;
		default: break ;
	}

	_response.line = "HTTP/1.1 " + tmpString;

	if (file.is_open())
	{
		convert.clear();
		convert << file.rdbuf();
		file.close();
		_response.body = convert.str();
		_response.body.erase(0, 3);
		convert.str("");
		convert << _response.body.length();
		findKey(_response.headers, "Content-Length")->second = convert.str();
		tmpString.clear();
	}
	else if (err != 100)
	{
		_response.body = "<html>\n<head><title>" + tmpString + "</title></head>\n<body>\n<center><h1>" + tmpString + "</h1></center>\n<hr><center>webserv</center>\n</body>\n</html>\n";
		convert.str("");
		convert << _response.body.length();
		findKey(_response.headers, "Content-Length")->second = convert.str();
		tmpString.clear();
	}

	if (err == 405)
	{
		for (std::vector<std::string>::iterator iter = tmpConfig.allowedMethods.begin(); iter != tmpConfig.allowedMethods.end(); iter++)
		{
			if (findKey(_response.headers, "Allow")->second.length())
				findKey(_response.headers, "Allow")->second += " " + *iter;
			else
				findKey(_response.headers, "Allow")->second += *iter;
		}
	}

	tmpString = _response.line + "\r\n";
	std::vector<std::pair<std::string, std::string> >::iterator iter = _response.headers.begin();

	while (iter != _response.headers.end())
	{
		if ((*iter).second.length())
			tmpString += (*iter).first + ": " + (*iter).second + "\r\n";
		iter++;
	}
	if (err != 100 && _request.method != "HEAD")
		tmpString += "\r\n" + _response.body;
	else
		tmpString += "\r\n";
	send(dest_fd, tmpString.c_str(), tmpString.size(), 0);
	std::cout << "SENT RESPONSE" << std::endl;
	std::cout << tmpString << std::endl;
}

/*
	Funzione di lettura e riempimento in struttura dei file letti all'interno di una directory;
*/

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

/*
	Funzione che costruisce il campo dell'header 'Date' di ogni risposta
*/

std::string VirtServ::getDateTime()
{
	time_t tm;
	struct tm *buf;
	std::string ret;

	tm = time(NULL);
	buf = gmtime(&tm);
	ret = asctime(buf);
	ret = ret.substr(0, ret.length() - 1);
	ret += " GMT";
	return (ret);
}

/*
	Funzione di crafting manuale dell'autoindex, che ritorna al termine lo stesso autoindex;
*/

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
	output << _response.line << "\r" << std::endl;

	findKey(_response.headers, "Content-Length")->second = tmpString;
	findKey(_response.headers, "Date")->second = getDateTime();
	findKey(_response.headers, "Content-Type")->second = "text/html";

	for (std::vector<std::pair<std::string, std::string> >::iterator iter = _response.headers.begin(); iter != _response.headers.end(); iter++)
	{
		if ((*iter).second.length())
			output << (*iter).first << ": " << (*iter).second << "\r" << std::endl;
	}

	output << "\r\n"
		   << _response.body;
	tmpString = output.str();

	send(dest_fd, tmpString.c_str(), tmpString.size(), 0);

	std::cout << "SENT RESPONSE" << std::endl;
	std::cout << tmpString << std::endl;
	delete[] (store);
}

/*
	Funzione di default per rispondere al client con file specifico, trovato nella root del virtual server;
*/

void VirtServ::answer(std::string fullPath, struct dirent *dirent, int dest_fd)
{
	std::cout << "------ANSWER------\n";
	std::stringstream stream;
	std::string tmpString;
	std::string tmpBody;
	std::ostringstream responseStream;
	std::string responseString;

	if (*fullPath.rbegin() != '/')
		fullPath.append("/");
	std::ifstream resource((fullPath + std::string(dirent->d_name)).c_str());
	if (!resource.is_open())
	{
		std::cout << "cannot read file" << std::endl;
		return;
	}
	_response.line += "HTTP/1.1 200 OK";
	stream << resource.rdbuf();
	if (!(strcmp(dirent->d_name, "youpi.bad_extension")))
	{
		std::ifstream tmp("fake_site/YoupiBanane/nop/youpi.bad_extension");
		stream << tmp.rdbuf();
	}
	tmpBody = stream.str();
	stream.str("");
	std::vector<std::pair<std::string, std::string> >::iterator iter2 = findKey(_response.headers, "Content-Length");
	if (tmpBody.length())
	{
		stream << tmpBody.length();
		stream >> tmpString;
		(*iter2).second = tmpString;
	}
	else
		(*iter2).second = "0";
	findKey(_response.headers, "Content-Type")->second = defineFileType(dirent->d_name);
	_response.body = tmpBody;

	responseStream << _response.line << "\r" << std::endl;
	std::vector<std::pair<std::string, std::string> >::iterator iter = _response.headers.begin();

	while (iter != _response.headers.end())
	{
		if ((*iter).second.length())
			responseStream << (*iter).first << ": " << (*iter).second << "\r" << std::endl;
		iter++;
	}

	if (_request.method != "HEAD")
	{
		responseStream << "\r\n"
					   << _response.body;
	}
	else
		responseStream << "\r\n";

	responseString = responseStream.str();

	if (responseString.size() > IOV_MAX)
	{
		while (responseString.size() > IOV_MAX)
		{
			send(dest_fd, responseString.c_str(), IOV_MAX, 0);
			responseString = responseString.substr(IOV_MAX);
		}
		send(dest_fd, responseString.c_str(), responseString.size(), 0);
	}
	else
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

/*
	Funzione di sostituzione della variabile $uri con l'url richiesto;
*/

void VirtServ::interpretLocationBlock(t_location *location)
{
	std::string uri = _request.line.substr(0, _request.line.find_first_of(" \t"));
	std::string::iterator iter = location->text.begin();

	while (location->text.find("$uri") != std::string::npos)
		location->text.replace(iter + location->text.find("$uri"), iter + location->text.find("$uri") + 4, uri);
}

/*
	Find function translated for vector
*/

std::vector<std::pair<std::string, std::string> >::iterator VirtServ::findKey(std::vector<std::pair<std::string, std::string> > &vector, std::string key)
{
	std::vector<std::pair<std::string, std::string> >::iterator iter = vector.begin();

	for (; iter != vector.end(); iter++)
	{
		if ((*iter).first == key)
			break;
	}
	return iter;
}

/*
	Define Filetype for "Content-type" header
*/

std::string VirtServ::defineFileType(char *filename)
{
	std::string toCompareText[4] = {".html", ".css", ".csv", ".xml"};
	std::string toCompareImage[6] = {".gif", ".jpeg", ".jpg", ".png", ".tiff", ".ico"};
	std::string toCompareApp[3] = {".js", ".zip", ".pdf"};
	std::string fileExtension = std::string(filename);
	size_t i;

	fileExtension = fileExtension.substr(fileExtension.find_last_of("."));

	for (i = 0; i < toCompareText->size(); i++)
	{
		if (!fileExtension.compare(toCompareText[i]))
			break;
	}

	if (i != toCompareText->size())
	{
		switch (i)
		{
		case 0:
			return ("text/html");
		case 1:
			return ("text/css");
		case 2:
			return ("text/csv");
		case 3:
			return ("text/xml");
		default:
			break;
		}
	}

	for (i = 0; i < toCompareImage->size(); i++)
	{
		if (!fileExtension.compare(toCompareImage[i]))
			break;
	}

	if (i != toCompareImage->size())
	{
		switch (i)
		{
		case 0:
			return ("image/gif");
		case 1:
		case 2:
			return ("image/jpeg");
		case 3:
			return ("image/png");
		case 4:
			return ("image/tiff");
		case 5:
			return ("image/x-icon");
		default:
			break;
		}
	}

	for (i = 0; i < toCompareApp->size(); i++)
	{
		if (!fileExtension.compare(toCompareApp[i]))
			break;
	}

	if (i != toCompareApp->size())
	{
		switch (i)
		{
		case 0:
			return ("application/javascript");
		case 1:
			return ("application/zip");
		case 2:
			return ("application/pdf");
		default:
			break;
		}
	}

	return ("text/plain");
}

FILE* VirtServ::chunkEncoding(t_connInfo & info)
{
	FILE *ofs = NULL;
	char buffer[2048] = {0};
	int dataRead = 0;
	int totalRead = 0;

	std::string filename = _request.line.substr(0, _request.line.find_first_of(" "));
	filename = filename.substr(filename.find_last_of("/") + 1, filename.size());

	while (1){
		while (info.chunk_size < 0) {
			dataRead = recv(info.connfd, buffer + totalRead, 1, 0);
			if (dataRead <= 0) {
				perror ("recv");
				return NULL;
			}
			totalRead += dataRead;
			if ((strstr(buffer, "\r\n"))) {
				info.chunk_size = strtoul(buffer, NULL, 16);
				totalRead = 0;
				memset(buffer, 0, 2048);
			}
		}
		if (info.chunk_size > 0)
		{
			ofs = fopen(filename.c_str(), "wba+");
			char chunk[info.chunk_size];
			// chunk = recv_timeout(info);
			while (totalRead < info.chunk_size) {
				if ((dataRead = recv(info.connfd, chunk, info.chunk_size - totalRead, MSG_DONTWAIT)) < 0)
					usleep(10000);
				if (dataRead > 0) 
					totalRead += dataRead;
			}
			for (int i = 0; i < dataRead; i++)
				fwrite(chunk + i, 1, sizeof((*chunk)), ofs);
			memset(chunk, 0, dataRead);
			fclose(ofs);
			dataRead = recv(info.connfd, chunk, 2, 0);
			// recv_timeout(info);
			info.chunk_size = -1;
			totalRead = 0;
		}
		else if (info.chunk_size == 0) {
			char chunk[4];
			recv(info.connfd, chunk, sizeof chunk, 0);
			info.chunk_size = -1;
			break;
		}
	}
	return ofs;
}

bool VirtServ::chunkEncodingCleaning(t_connInfo & info)
{
	char buffer[2048] = {0};
	int dataRead = 0;
	int totalRead = 0;

	while (1){
		while (info.chunk_size < 0) {
			if ((dataRead = recv(info.connfd, buffer + totalRead, 1, 0)) < 0)
				usleep(10000);
			if (dataRead > 0) {
				totalRead += dataRead;
				if ((strstr(buffer, "\r\n"))) {
					info.chunk_size = strtoul(buffer, NULL, 16);
					totalRead = 0;
					memset(buffer, 0, 2048);
				}
			}
		}
		if (info.chunk_size > 0)
		{
			char chunk[info.chunk_size];
			while (totalRead < info.chunk_size) {
				if ((dataRead = recv(info.connfd, chunk, info.chunk_size - totalRead, MSG_DONTWAIT)) < 0)
					usleep(10000);
				if (dataRead > 0) 
					totalRead += dataRead;
			}
			memset(chunk, 0, dataRead);
			dataRead = recv(info.connfd, chunk, 2, 0);
			info.chunk_size = -1;
			totalRead = 0;
		}
		else if (info.chunk_size == 0) {
			char chunk[8];
			while ((dataRead = recv(info.connfd, chunk + totalRead, sizeof chunk - totalRead, 0)) < 0)
				usleep(10000);
			if (dataRead > 0) {
				totalRead += dataRead;
			}
			if (strstr(chunk, "\r\n")) {
				info.chunk_size = -1;
				break;
			}
		}
	}
	return true;
}

int VirtServ::execPut(t_connInfo & info)
{
	std::cout << "------EXEC PUT------\n";
	
	if (findKey(_request.headers, "Transfer-Encoding")->second == "chunked") {
		if (chunkEncoding(info) != NULL) {
			defaultAnswerError(201, info.connfd, _config);
			this->cleanRequest();
			this->cleanResponse();
		}
	}
	// else if (findKey(_request.headers, ""))
	return 0;
}

int VirtServ::execPost(t_connInfo & info)
{
	std::cout << "------EXEC POST------\n";
	if (_request.line.find(".bla HTTP/") != std::string::npos) {
		std::string filename = _request.line.substr(0, _request.line.find_first_of(" "));
		// filename = filename.substr(filename.find_last_of("/") + 1, filename.size());
		launchCGI();
		return 0;
	}
	if (_config.allowedMethods.size() && std::find(_config.allowedMethods.begin(), _config.allowedMethods.end(), "POST") == _config.allowedMethods.end())
	{
		if (chunkEncodingCleaning(info) == true) {
			this->cleanRequest();
			this->cleanResponse();
			defaultAnswerError(405, info.connfd, _config);
		}
		return 0;
	}
	std::string _contentLength = findKey(_request.headers, "Content-Length")->second;
	std::stringstream ss;
	size_t _totalLength;
	_totalLength = 0;
	if (_contentLength != "")
	{
		ss << _contentLength;
		ss >> _totalLength;
	}
	else
		_totalLength = 4096;
	FILE *ofs;
	static char totalBuffer[8192];
	char buffer[512];
	int dataRead = 0;
	int i = 0;
	static int c = 0;
	while (1)
	{
		dataRead = recv(info.connfd, buffer, sizeof buffer, 0);
		std::cout << "DATA READ " << dataRead << std::endl;
		std::cout << "BUFFER " << buffer << std::endl;
		if (dataRead <= 0)
			break;
		for (i = 0; i < dataRead; i++, c++)
			totalBuffer[c] = buffer[i];
		memset(buffer, 0, sizeof buffer);
		if (dataRead < static_cast<int>(sizeof buffer))
			break;
	}
	if (strstr(totalBuffer, "\r\n\r\n") == NULL)
		return 0;
	c = 0;
	if (_config.allowedMethods.size() && std::find(_config.allowedMethods.begin(), _config.allowedMethods.end(), "POST") == _config.allowedMethods.end())
	{
		defaultAnswerError(405, info.connfd, _config);
		memset(totalBuffer, 0, 8192);
		this->cleanRequest();
		return (true);
	}
	if (dataRead <= 0)
	{
		memset(totalBuffer, 0, 8192);
		defaultAnswerError(205, info.connfd, _config);
		return false;
	}
	std::string store(reinterpret_cast<char *>(totalBuffer));
	std::string filename;
	if (store.find("filename") != std::string::npos)
		filename = store.substr(store.find("filename"), std::string::npos);
	else
	{
		memset(totalBuffer, 0, 8192);
		defaultAnswerError(205, info.connfd, _config);
		return false;
	}
	filename = filename.substr(filename.find_first_of("\"") + 1, filename.find_first_of("\n"));
	filename = _config.root + "/uploads/" + filename.substr(0, filename.find_first_of("\""));
	if (FILE *file = fopen(filename.c_str(), "r"))
	{
		fclose(file);
		memset(totalBuffer, 0, 8192);
		defaultAnswerError(202, info.connfd, _config);
		return true;
	}
	ofs = fopen(filename.c_str(), "wb");
	if (ofs)
	{
		std::string cmp = store.substr(0, store.find_first_of("\n") - 1);
		cmp.append("--\n");
		i = 0;
		for (; i < static_cast<int>(8129); i++)
		{
			if (!(strncmp(&totalBuffer[i], "\r\n\r\n", 4)))
				break;
		}
		fwrite(totalBuffer + (i + 4), 1, 8129 - (cmp.size() + 3) - (i + 4), ofs);
		std::cout << "Save file: " << filename << std::endl;
		fclose(ofs);
		defaultAnswerError(201, info.connfd, _config);
		memset(totalBuffer, 0, 8192);
		return true;
	}
	return false;
}

char* VirtServ::recv_timeout(t_connInfo & info)
{
	int size_recv , total_size= 0;
	struct timeval begin , now;
	char *chunk = (char *)malloc(sizeof(char) * info.chunk_size);
	double timediff;
	
	fcntl(info.connfd, F_SETFL, O_NONBLOCK);
	gettimeofday(&begin , NULL);
	while(total_size < info.chunk_size)
	{
		gettimeofday(&now , NULL);
		//time elapsed in seconds
		timediff = (now.tv_sec - begin.tv_sec) + 1e-6 * (now.tv_usec - begin.tv_usec);
		//if you got some data, then break after timeout
		if( total_size > 0 && timediff > 2)
			break;
		//if you got no data at all, wait a little longer, twice the timeout
		else if( timediff > 4)
			break;
		if((size_recv =  recv(info.connfd , chunk + total_size, info.chunk_size - total_size, 0) ) < 0)
			usleep(100000);
		else
		{
			printf("ELSE\n");
			total_size += size_recv;
			//reset beginning time
			gettimeofday(&begin , NULL);
		}
	}
	return chunk;
}