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
// #ifdef linux
	#define IOV_MAX 1024
// #endif

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

VirtServ::~VirtServ() { _connections.clear(); _cookies.clear(); }

//////// Getters & Setters ///////////////////////////////////

int			VirtServ::getSocket() { return (_sockfd); }
t_config	VirtServ::getConfig() const { return _config; }

//////// Main Functions //////////////////////////////////////

/*
	Inizializza il socket, lo setta come non-blocking, lo binda e lo mette in listen per massimo 10 client;
*/

bool		VirtServ::startServer()
{
	int i = 1;

	_sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (_sockfd == -1)
		return bool_error("Socket error\n");
	if (setsockopt(_sockfd, SOL_SOCKET, SO_REUSEADDR, (const char *)&i, sizeof(i)) < 0)
		return bool_error("setsockopt() error\n");
	if (bind(_sockfd, (struct sockaddr *)&_sin, sizeof(_sin)) != 0)
		return bool_error("Bind error");
	if (listen(_sockfd, 200) == -1)
		return bool_error("Listen error");
	return (true);
}

/*
	Fa il clear del vettore delle connessioni e chiude il socket;
*/

bool		VirtServ::stopServer()
{
	_connections.clear();
	if (close(_sockfd))
		return bool_error("Error closing _sockfd");
	return true;
}

/*
	Dopo poll, accetta la connessione del client e la salva nel vettore delle connessioni del server. Ritorna il valore dell'fd della connessione;
*/

int			VirtServ::acceptConnectionAddFd(int sockfd)
{
	socklen_t addrlen;
	struct sockaddr_storage remoteaddr;
	int		tmpfd;
	addrlen = sizeof remoteaddr;

	tmpfd = accept(sockfd, (struct sockaddr *)&remoteaddr, &addrlen);
	if (tmpfd == -1)
	{
		perror("accept");
		return -1;
	}
	_connections.push_back(t_connInfo(tmpfd));
	return (tmpfd);
}

/*
	Inizia la gestione della richiesta del client;
		- recv sull'fd, salvataggio in buf (char [256]);
		- pulisce la variabile privata request di VirtServ;
		- riempie la struttura request (readRequest());
		- lancia la funzione di elaborazione della richiesta (elaborateRequest());
		- chiude la connessione e libera il valore nel vettore delle connessioni;
*/

typedef std::vector<t_connInfo>::iterator	connIter;

connIter	VirtServ::findFd(std::vector<t_connInfo>::iterator begin, std::vector<t_connInfo>::iterator end, int fd)
{
	while (begin != end)
	{
		if (begin->fd == fd)
			return (begin);
		begin++;
	}
	return (begin);
}

t_config	VirtServ::getConfig(t_connInfo & conn)
{
	t_config	ret(_config);
	std::string line;
	std::string key;
	std::string value;
	std::string toCompare[9] = {"root", "autoindex", "index", "error_page", "client_max_body_size", "allowed_methods", "try_files", "return", "cgi_pass"};
	int i;

	conn.location = interpretLocationBlock(conn.location, conn.request.path);

	while (conn.location->text.find_first_not_of(" \t\r\n") != std::string::npos)
	{
		line = conn.location->text.substr(0, conn.location->text.find("\n"));
		if (line.at(line.length() - 1) != ';')
			die("Location rules must end with semicolon. Aborting", *this);
		line = line.substr(0, line.length() - 1);
		if (line.find_first_of(" \t") == std::string::npos)
			die("Location rules must be given in format 'key value'. Aborting", *this);
		key = line.substr(0, line.find_first_of(" \t"));
		value = line.substr(line.find_first_not_of(" \t", key.length()));
		if (value.find_first_not_of(" \t\n") == std::string::npos)
			die("Location rule without value. Aborting", *this);

		for (i = 0; i < 9; i++)
			if (key == toCompare[i])
				break;

		switch (i) {
			case 0: ret.root = value; break;
			case 1: {
				if (!value.compare("on"))
					ret.autoindex = true;
				else if (!value.compare("off"))
					ret.autoindex = false;
				else
					die("Autoindex rule must have on|off value. Aborting", *this);
				break;
			}
			case 2: {
				ret.index.clear();
				while (value.find_first_not_of(" \n\t") != std::string::npos)
				{
					ret.index.push_back(value.substr(0, value.find_first_of(" \t")));
					value = value.substr(0, value.find_first_of(" \t"));
					value = value.substr(0, value.find_first_not_of(" \t"));
				}
				break;
			}
			case 3: {
				ret.errorPages.clear();
				while (value.find_first_not_of(" \n\t") != std::string::npos)
				{
					ret.errorPages.push_back(value.substr(0, value.find_first_of(" \t")));
					if (value.find_first_of(" \t") == std::string::npos)
						break;
					value = value.substr(value.find_first_of(" \t"));
					value = value.substr(value.find_first_not_of(" \t"));
				}
				break;
			}
			case 4: Parser::checkClientBodyMaxSize(value, ret); break;
			case 5: insertMethod(ret, value); break;
			case 6: {
				if (saveFiles(value, ret, conn))
					return (ret);
				return (t_config(false));
			}
			case 7: { checkAndRedirect(value, conn.fd); return (t_config(false)); }
			case 8: { ret.cgi_script = value; break ; }
			default: die("Unrecognized location rule: \"" + key + "\". Aborting", *this);
			}
		if (conn.location->text.find("\n") == conn.location->text.npos)
			return (ret);
		conn.location->text = conn.location->text.substr(conn.location->text.find("\n") + 1);
		conn.location->text = conn.location->text.substr(conn.location->text.find_first_not_of(" \t\n"));
	}
	//////////
		// AGGIUNGERE CONTROLLI SUI PARAMETRI OBBLIGATORI DELLA CONFIGURAZIONE [ TRY_FILES  ]
	//////////
	return (ret);
}

bool		VirtServ::saveFiles(std::string value, t_config & ret, t_connInfo & conn)
{
	std::string defaultFile;

	if (ret.client_max_body_size && std::strlen(conn.request.body.c_str()) > ret.client_max_body_size)
	{
		t_connInfo tmp;
		tmp.config = ret;
		tmp.fd = conn.fd;
		defaultAnswerError(413, tmp);
		return false;
	}
	while (value.find_first_of(" \t") != std::string::npos)
	{
		ret.files.push_back(value.substr(0, value.find_first_of(" \t")));
		value = value.substr(value.find_first_of(" \t"));
		value = value.substr(value.find_first_not_of(" \t"));
	}
	ret.files.push_back(value);
	return true;
}

int			VirtServ::handleClient(int fd)
{
	std::vector<t_connInfo>::iterator it = findFd(_connections.begin(), _connections.end(), fd);
	if (it == _connections.end())
		return (0);

	struct linger 	l;

	l.l_onoff  = 1;
	l.l_linger = 0;
	unsigned char buffer[1024] = {0};
	if (it->request.method == "")
	{	
		int dataRead;
		dataRead = recv(fd, buffer, sizeof buffer, 0);
		for (int i = 0; i < dataRead; i++)
			it->buffer.push_back(buffer[i]);
		if (it->buffer.find("\r\n\r\n") == it->buffer.npos) return 0;

		it->body = it->buffer.substr(it->buffer.find("\r\n\r\n") + 4, it->buffer.npos);
		it->buffer = it->buffer.substr(0, it->buffer.find("\r\n\r\n") + 4);

		if (!this->readRequest(*it, it->buffer))
		{
			defaultAnswerError(400, *it);
			_connections.erase(it);
			return (1);
		}
		it->location = searchLocationBlock(*it);
		if (!it->location)
		{
			defaultAnswerError(404, *it);
			return (1);
		}
		it->config = getConfig(*it);
		if (it->config.valid == false)
		{
			_connections.erase(it);
			return (1);
		}
		std::cout << it->buffer << std::endl;
		it->headers = it->buffer;
		if (it->body.size() > 0 && findKey(it->request.headers, "Transfer-Encoding")->second == "chunked") {
			it->chunk_size = strtoul(it->body.c_str(), NULL, 16);
			if (it->chunk_size > 0 && it->body.find("\r\n") == it->body.npos) {
				it->body = it->body.substr(it->body.find_first_of("\r\n"));
				char buffer[1] = {0};
				while (1) {
					recv(it->fd, buffer, sizeof buffer, 0);
					it->body.append(buffer);
					if (it->body.find("\r\n") != it->body.npos) {
						it->body.clear();
						break ;
					}
					memset(buffer, 0, sizeof buffer);
				}
			}
			else if (it->chunk_size > 0) {
				it->body = it->body.substr(it->body.find("\r\n") + 2);
				it->chunk_size = it->chunk_size - it->body.size();
			}
		}
        it->buffer.clear();
	}

	int (VirtServ::*execMethod[])(t_connInfo & info) = {&VirtServ::execGet, &VirtServ::execHead, &VirtServ::execPost, &VirtServ::execPut, &VirtServ::execDelete};
	std::string	method[] = {"GET", "HEAD", "POST", "PUT", "DELETE"};
	for (int i = 0; i < 5; i++) {
		if (method[i] == it->request.method) {
			if ((this->*execMethod[i])(*it) == 1) {
				delete it->location;
				setsockopt(it->fd, SOL_SOCKET, SO_LINGER, &l, sizeof(l));
				close(it->fd);
				_connections.erase(it);
				return 1;
				}
			}
		}
	return (0);
}

/*
	Parsing della richiesta letta dal client;
*/

int			VirtServ::readRequest(t_connInfo & conn, std::string req)
{
	std::string key;
	std::vector<std::pair<std::string, std::string> >::iterator header;
	std::string comp[] = {"GET", "POST", "DELETE", "PUT", "HEAD"};

	conn.request.line = req.substr(req.find_first_of("/"), req.npos);
	conn.request.line = conn.request.line.substr(0, conn.request.line.find_first_of("\r\n"));
	conn.path = conn.request.line.substr(0, conn.request.line.find(" "));

	if (conn.path.find("registered.html?") != conn.path.npos)
	{
		conn.request.arguments = conn.path.substr(conn.path.find("?") + 1);
		conn.path = conn.path.substr(0, conn.path.find("?"));
	}

	if (req.find_first_of(" \t") != std::string::npos)
		conn.request.method = req.substr(0, req.find_first_of(" \t"));
	if (req.find_first_of("/") != std::string::npos) {
		conn.request.path = req.substr(req.find_first_of("/"));
		conn.request.path = conn.request.path.substr(0, conn.request.path.find_first_of(" "));
	}

	int i;
	for (i = 0; i < 5; i++)
	{
		if (!conn.request.method.compare(comp[i]))
			break;
	}
	if (i == 5)
		return 0;
	req = req.substr(req.find_first_of("\n") + 1);

	while (req.find_first_not_of(" \t\n\r") != std::string::npos && std::strncmp(req.c_str(), "\n\n", 2))
	{
		key = req.substr(0, req.find_first_of(":"));
		req = req.substr(req.find_first_of(":") + 2);
		header = findKey(conn.request.headers, key);
		if (header != conn.request.headers.end())
			(*header).second = req.substr(0, req.find_first_of("\r\n"));
		if (req.find_first_of("\r") != std::string::npos && std::strlen(req.substr(req.find_first_of("\r")).c_str()) > 4 && !std::strncmp(req.substr(req.find_first_of("\r")).c_str(), "\r\n\r\n", 4))
			break;
		req = req.substr(req.find_first_of("\n") + 1);
	}

	if (req.find_first_of("\r") != std::string::npos && std::strlen(req.substr(req.find_first_of("\r")).c_str()) > 4 && !std::strncmp(req.substr(req.find_first_of("\r")).c_str(), "\r\n\r\n", 4))
	{
		if (header != conn.request.headers.end())
			(*header).second = req.substr(0, req.find_first_of("\r\n"));
		req = req.substr(req.find_first_of("\r") + 4);
		conn.request.body = req;
	}

	if (req.find_first_not_of("\r\n") != std::string::npos)
		conn.request.body = req.substr(req.find_first_not_of("\r\n"));

	if (findKey(conn.request.headers, "Expect") != conn.request.headers.end())
		return (1);
	std::vector<std::pair<std::string, std::string> >::iterator iter = conn.request.headers.begin();
	while (iter != conn.request.headers.end())
	{
		std::cout << (*iter).first << ": " << (*iter).second << std::endl;
		iter++;
	}
	std::cout << conn.request.body << std::endl;
	return (1);
}

void		VirtServ::checkAndRedirect(std::string value, t_connInfo conn)
{
	std::string			code;
	std::string			phrase;
	std::string			phrases[1] = {"301"};
	std::string			tmpString;
	std::stringstream	output;
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

	switch (i) {
		case 0: phrase = "Moved Permanently"; break;
		default: break;
		}

	value = value.substr(value.find_first_of(" \t"));
	value = value.substr(value.find_first_not_of(" \t"));
	if (std::strncmp("http://", value.c_str(), 7))
		die("Protocol HTTP is not specified in the given redirection address. Aborting");

	conn.response.line = "HTTP/1.1 " + code + " " + phrase;
	findKey(conn.response.headers, "Location")->second = value;

	output << conn.response.line << "\r" << std::endl;
	for (std::vector<std::pair<std::string, std::string> >::iterator iter = conn.response.headers.begin(); iter != conn.response.headers.end(); iter++)
	{
		if ((*iter).second.length())
			output << (*iter).first << ": " << (*iter).second << "\r" << std::endl;
	}
	output << "\r\n"
		   << conn.response.body;

	tmpString = output.str();

	send(conn.fd, tmpString.c_str(), tmpString.size(), 0);
}

/*
	Parsing del location block:> modifica dei metodi permessi.
*/

void		VirtServ::insertMethod(t_config &tmpConfig, std::string value)
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

void		VirtServ::tryFiles(t_connInfo conn)
{
	std::vector<std::string>::iterator iter = conn.config.files.begin();
	while (iter != conn.config.files.end())
	{
		if (tryGetResource(*iter, conn))
			return;
		iter++;
	}
}

/*
	Apre la cartella di riferimento: se l'autoindex è su off, cerca il file di default (tmpConfig.index [vector]), ritorna 404 in caso non sia presente.
									 se l'autoindex è su on, ritorna l'autoindex della cartella.
*/

DIR*		VirtServ::dirAnswer(std::string fullPath, struct dirent *dirent, t_connInfo conn)
{
	DIR *dir;

	if (*fullPath.rbegin() != '/')
		fullPath.append(("/"));

	std::string path = dirent ? fullPath + dirent->d_name + "/" : fullPath;

	dir = opendir(path.c_str());
	if (!conn.config.autoindex || conn.config.index.size())
	{
		struct dirent *tmp;
		tmp = readdir(dir);
		for (std::vector<std::string>::iterator it = conn.config.index.begin(); it != conn.config.index.end(); it++)
		{
			while (tmp)
			{
				if (!(std::strcmp(tmp->d_name, (*it).c_str())))
				{
					answer(path, tmp, conn.fd);
					return dir;
				}
				tmp = readdir(dir);
			}
			closedir(dir);
			dir = opendir(path.c_str());
			tmp = readdir(dir);
		}
		defaultAnswerError(404, conn);
		return dir;
	}
	else
		answerAutoindex(path, dir, conn.fd);
	return (dir);
}

/*
	Prende il valore Content-Length dalla richiesta e lo usa come benchmark per la lettura del file da uploadare.
	Apre il file (deve inserire corretttamente il nome)
	Riceve e contestualmente scrive il body ricevuto nel file
	Chiude il file e la connessione
*/

int			VirtServ::launchCGI(t_connInfo & conn)
{
	std::string	output;

	if (findKey(conn.request.headers, "Transfer-Encoding")->second == "chunked") {
		if (chunkEncoding(conn) == 1) {
			std::string code;
			std::string contentType;
			Cgi	cgi(conn, conn.config.port);
			output = cgi.executeCgi(conn.config.cgi_script, conn.path.c_str());
			if (output.find("Status: ") != output.npos) {
				code = output.substr(output.find("Status: ") + 8, output.npos);
				code = code.substr(0, code.find_first_of("\n"));
			}
			if (output.find("Content-Type: ") != output.npos) {
				contentType = output.substr(output.find("Content-Type ") + 17, output.npos);
				contentType = contentType.substr(0, contentType.find_first_of(";"));
			}
			output = output.substr(output.find("\r\n\r\n") + 4, output.npos);
			std::stringstream outputSize;
			outputSize << output.size();
			std::string answer = "HTTP/1.1 200 OK\r\nServer: webserv\r\n" + contentType;
			answer += "\r\nContent-Length: "; answer.append(outputSize.str());
			answer += "\r\nConnection: close\r\n\r\n";
			answer += output;
			send(conn.fd, answer.c_str(), answer.size(), 0);
			#ifdef linux
				usleep(2000);
			#endif
			conn.body.clear();
			return 1;
		}
	}
	return 0;
}

void		VirtServ::correctPath(std::string & filename, t_connInfo & conn)
{
	std::string	bench;

	for (std::vector<t_location>::iterator	iter = _config.locationRules.begin(); iter != _config.locationRules.end(); iter++)
	{
		if (!std::strncmp(filename.c_str(), iter->location.c_str(), iter->location.length()) && iter->location.length() > bench.length()) {
			bench = iter->location;
		}
	}
	if (bench != "")
	{
		filename = filename.substr(bench.length());
		if (filename.at(0) == '/' && filename.length() > 1) {
			filename = filename.substr(1);
		} else {
			filename = "";
		}
	}

	if (conn.request.arguments.size() > 0)
	{
		std::string	cookieKey;
		std::string	cookieValue;

		if (conn.request.arguments.find("=") != conn.request.arguments.npos && *(conn.request.arguments.end() - 1) != '=' && findKey(conn.request.headers, "Cookie")->second.find("name") == std::string::npos)
		{
			cookieKey = conn.request.arguments.substr(0, conn.request.arguments.find("="));
			conn.request.arguments = conn.request.arguments.substr(conn.request.arguments.find("=") + 1);
			cookieValue = conn.request.arguments;
			conn.cookie = std::make_pair(this->generateCookie(cookieValue), cookieValue);
			_cookies.insert(conn.cookie);
			print_table();
			conn.set_cookie = true;
		}
	}
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

bool		VirtServ::tryGetResource(std::string filename, t_connInfo conn)
{
	// std::cout << "------TRY GET RESOURCE------\n";
	std::string fullPath = conn.config.root;
	char rootPath[conn.config.root.size()];
	std::string	rootPath2;
	DIR *directory;
	struct dirent *dirent;



	conn.config.root.copy(rootPath, conn.config.root.size());
	rootPath[conn.config.root.size()] = '\0';
	rootPath2 = rootPath;
	if (conn.config.allowedMethods.size() && std::find(conn.config.allowedMethods.begin(), conn.config.allowedMethods.end(), conn.request.method) == conn.config.allowedMethods.end())
	{
		defaultAnswerError(405, conn);
		return (true);
	}
	if (!std::strncmp(conn.location->location.c_str(), filename.c_str(), std::strlen(conn.location->location.c_str())))
	{
		filename = filename.substr(std::strlen(conn.location->location.c_str()));
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
	correctPath(filename, conn);
	if (!(directory = opendir(rootPath2.c_str())))
	{
		if (errno == EACCES)
			defaultAnswerError(403, conn);
		if (errno == ENOENT || errno == ENOTDIR)
			defaultAnswerError(404, conn);
		return (true);
	}
	if (filename.length())
	{
		while ((dirent = readdir(directory)))
		{
			if (!filename.compare(dirent->d_name) || !(filename.compare("directory")))
			{
				if (!(filename.compare("directory")))
				{
					closedir(dirAnswer(rootPath2, NULL, conn));
					break;
				}
				if (dirent->d_type == DT_DIR)
				{
					closedir(dirAnswer(rootPath2, dirent, conn));
					break;
				}
				else
				{
					answer(rootPath2, dirent, conn);
					break;
				}
			}
		}
		if (!dirent)
			defaultAnswerError(404, conn);
		closedir(directory);
		return (true);
	}
	else if (conn.config.autoindex)
	{
		answerAutoindex(rootPath2, directory, conn);
		closedir(directory);
		return (true);
	}
	else if (!conn.config.autoindex)
	{
		closedir(directory);
		directory = dirAnswer(rootPath2, NULL, conn);
		closedir(directory);
		return true;
	}
	else
	{
		defaultAnswerError(404, conn);
		return (true);
	}
	closedir(directory);
	return (false);
}

/*
	Funzione di restituizione della pagina di errore: prima verifica la presenza di pagine custom di errore e, in caso, torna quelle;
	altrimenti opera un crafting manuale delle stesse, per poi tornarle.
*/

void		VirtServ::defaultAnswerError(int err, t_connInfo conn)
{
	std::string tmpString;
	std::stringstream convert;
	std::ifstream file;

	if (conn.config.errorPages.size() && err != 500)
	{
		convert << err;
		for (std::vector<std::string>::iterator it = conn.config.errorPages.begin(); it != conn.config.errorPages.end(); it++)
		{
			if (!std::strncmp(convert.str().c_str(), (*it).c_str(), 3))
			{
				if (*(_config.root.end() - 1) != '/')
					_config.root.push_back('/');
				file.open((_config.root + *it).c_str());
				if (!file)
				{
					file.close();
					defaultAnswerError(500, conn);
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

	conn.response.line = "HTTP/1.1 " + tmpString;

	if (file.is_open())
	{
		convert.clear();
		convert << file.rdbuf();
		file.close();
		conn.response.body = convert.str();
		conn.response.body.erase(0, 3);
		convert.str("");
		convert << conn.response.body.length();
		findKey(conn.response.headers, "Content-Length")->second = convert.str();
		tmpString.clear();
	}
	else if (err != 100)
	{
		conn.response.body = "<html>\n<head><title>" + tmpString + "</title></head>\n<body>\n<center><h1>" + tmpString + "</h1></center>\n<hr><center>webserv</center>\n</body>\n</html>\n";
		convert.str("");
		convert << conn.response.body.length();
		findKey(conn.response.headers, "Content-Length")->second = convert.str();
		tmpString.clear();
	}

	if (err == 405)
	{
		for (std::vector<std::string>::iterator iter = conn.config.allowedMethods.begin(); iter != conn.config.allowedMethods.end(); iter++)
		{
			if (findKey(conn.response.headers, "Allow")->second.length())
				findKey(conn.response.headers, "Allow")->second += " " + *iter;
			else
				findKey(conn.response.headers, "Allow")->second += *iter;
		}
	}

	tmpString = conn.response.line + "\r\n";
	std::vector<std::pair<std::string, std::string> >::iterator iter = conn.response.headers.begin();
	findKey(conn.response.headers, "Connection")->second = "close";

	while (iter != conn.response.headers.end())
	{
		if ((*iter).second.length())
			tmpString += (*iter).first + ": " + (*iter).second + "\r\n";
		iter++;
	}
	if (err != 100 && conn.request.method != "HEAD")
		tmpString += "\r\n" + conn.response.body + "\r\n";
	else
		tmpString += "\r\n";
	send(conn.fd, tmpString.c_str(), tmpString.size(), 0);
	std::cout << "SENT RESPONSE" << std::endl;
	std::cout << tmpString << std::endl;
}

/*
	Funzione di lettura e riempimento in struttura dei file letti all'interno di una directory;
*/

typedef struct dirent s_dirent;

std::vector<struct dirent *>	VirtServ::fill_dirent(DIR *directory, std::string path)
{
	std::vector<struct dirent *>	ret;
	struct dirent *tmp;
	
	(void)path;
	while ((tmp = readdir(directory)))
	{
		if (tmp->d_type == DT_DIR)
			ret.insert(ret.begin(), tmp);
		else
			ret.push_back(tmp);
	}
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

void		VirtServ::answerAutoindex(std::string fullPath, DIR *directory, t_connInfo conn)
{
	std::ostringstream convert;
	std::vector<struct dirent *> store;
	struct stat attr;
	std::string tmpString;
	std::stringstream output;
	std::string name;

	if ((*fullPath.end() - 1) != '/')
		fullPath.push_back('/');
	store = fill_dirent(directory, fullPath);
	conn.response.line = "HTTP/1.1 200 OK";
	conn.response.body = "<html>\n<head><title>Index of " + conn.request.line.substr(0, conn.request.line.find_first_of(" ")) + "</title></head>\n<body>\n<h1>Index of " + conn.request.line.substr(0, conn.request.line.find_first_of(" ")) + "</h1><hr><pre>";
	while (store.size() > 0)
	{
		name = std::string((store.front())->d_name);
		if (std::strncmp(".\0", name.c_str(), 2))
		{
			stat((fullPath + (store.front())->d_name).c_str(), &attr);
			convert << (size_t)attr.st_size;
			if (store.front()->d_type == DT_DIR)
				name += "/";
			conn.response.body += "<a href=\"" + name + "\">" + name + "</a>";
			if (std::strncmp("../\0", name.c_str(), 4))
			{
				// questa a volte manda errore
				// tmpString = std::string(ctime(&attr.st_mtime)).substr(0, std::string(ctime(&attr.st_mtime)).length() - 1);
				conn.response.body.append(52 - static_cast<int>(name.length()), ' ');
				conn.response.body += tmpString;
				conn.response.body.append(21 - convert.width(), ' ');
				conn.response.body += store.front()->d_type == DT_DIR ? "-" : convert.str();
			}
			conn.response.body += "\n";
			convert.str("");
		}
		store.erase(store.begin());
	}
	conn.response.body += "</pre><hr></body>\n</html>";
	convert << conn.response.body.length();
	tmpString = convert.str();
	output << conn.response.line << "\r\n";

	output << "Content-Length: " + tmpString << "\r\n";
	output << "Connection: close\r\nDate: " + getDateTime();
	output << "\r\nContent-Type: text/html\r\n";

	output << "\r\n"
		   << conn.response.body;
	tmpString = output.str();

	_storeReq.push_back(std::make_pair(conn.headers, tmpString));
	
	send(conn.fd, tmpString.c_str(), tmpString.size(), 0);

	static int print = 0;
	std::cout << print++ << std::endl;
}

/*
	Funzione di default per rispondere al client con file specifico, trovato nella root del virtual server;
*/

void		VirtServ::answer(std::string fullPath, struct dirent *dirent, t_connInfo conn)
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
	conn.response.line = "HTTP/1.1 200 OK";
	stream << resource.rdbuf();
	tmpBody = stream.str();
	stream.str("");
	std::cout << "QUI" << findKey(conn.request.headers, "Cookie")->second << std::endl;
	if (!std::strncmp(dirent->d_name, "registered.html", std::strlen(dirent->d_name) && findKey(conn.request.headers, "Cookie")->second.find("name=") != findKey(conn.request.headers, "Cookie")->second.npos) && conn.set_cookie == false && findKey(conn.request.headers, "Cookie")->second.find("name=") != findKey(conn.request.headers, "Cookie")->second.npos)
	{
		std::string	value;
		std::string tmpBody2;

		value = findKey(conn.request.headers, "Cookie")->second.substr(findKey(conn.request.headers, "Cookie")->second.find("name="));
		if (value.find_first_of(";") != value.npos)
			value = value.substr(value.find_first_of(";"));
		else if (value.find_first_of(" \t") != value.npos)
			value = value.substr(value.find_first_of(" \t"));
		value = value.substr(value.find("=") + 1);
		tmpBody2 = tmpBody.substr(0, tmpBody.find("greetings") + 11);
		tmpBody2.append(value);
		tmpBody2.append(tmpBody.substr(tmpBody.find("greetings") + 11));
		tmpBody = tmpBody2;
	}
	else if (!std::strncmp(dirent->d_name, "registered.html", std::strlen(dirent->d_name)) && conn.set_cookie == true)
	{
		std::string tmpBody2;

		tmpBody2 = tmpBody.substr(0, tmpBody.find("greetings") + 11);
		tmpBody2.append("Cookie is set. Refresh");
		tmpBody2.append(tmpBody.substr(tmpBody.find("greetings") + 11));
		tmpBody = tmpBody2;
	}
	else if (!std::strncmp(dirent->d_name, "registered.html", std::strlen(dirent->d_name)))
	{
		std::string tmpBody2;

		tmpBody2 = tmpBody.substr(0, tmpBody.find("greetings") + 11);
		tmpBody2.append("sign in at <a href=\"register.html\">register.html</a>");
		tmpBody2.append(tmpBody.substr(tmpBody.find("greetings") + 11));
		tmpBody = tmpBody2;
	}
	std::vector<std::pair<std::string, std::string> >::iterator iter2 = findKey(conn.response.headers, "Content-Length");
	if (tmpBody.length())
	{
		stream << tmpBody.length();
		stream >> tmpString;
		(*iter2).second = tmpString;
	}
	else
		(*iter2).second = "0";
	findKey(conn.response.headers, "Content-Type")->second = defineFileType(dirent->d_name);
	findKey(conn.response.headers, "Connection")->second = "close";
	if (conn.set_cookie)
		findKey(conn.response.headers, "Set-Cookie")->second = "name=" + conn.cookie.second;
	conn.response.body = tmpBody;

	responseStream << conn.response.line << "\r" << std::endl;
	std::vector<std::pair<std::string, std::string> >::iterator iter = conn.response.headers.begin();

	while (iter != conn.response.headers.end())
	{
		if ((*iter).second.length())
			responseStream << (*iter).first << ": " << (*iter).second << "\r" << std::endl;
		iter++;
	}

	if (conn.request.method != "HEAD")
		responseStream << "\r\n" << conn.response.body;
	else
		responseStream << "\r\n";

	responseString = responseStream.str();

	if (responseString.size() > IOV_MAX)
	{
		while (responseString.size() > IOV_MAX)
		{
			send(conn.fd, responseString.c_str(), IOV_MAX, 0);
			responseString = responseString.substr(IOV_MAX);
		}
		send(conn.fd, responseString.c_str(), responseString.size(), 0);
	}
	else
		send(conn.fd, responseString.c_str(), responseString.size(), 0);
	std::cout << "SENT RESPONSE" << std::endl;
	std::cout << responseString << std::endl;
}

bool		isRegex(std::string path, t_config _config)
{
	for (std::vector<t_location>::iterator iter = _config.locationRules.begin(); iter != _config.locationRules.end(); iter++)
	{
		if (iter->regex && !std::strncmp(iter->location.c_str(), &(path.c_str())[path.length() - iter->location.length()], iter->location.length()))
			return (true);
	}
	return (false);
}

t_location*	VirtServ::searchLocationBlock(t_connInfo & info)
{
	std::vector<t_location>::iterator iter = _config.locationRules.begin();
	t_location *ret = NULL;
	bool regex;

	regex = isRegex(info.request.path, _config);

	// Exact corrispondence
	while (iter != _config.locationRules.end() && !regex)
	{
		if (!std::strncmp(info.request.path.c_str(), (*iter).location.c_str(), (*iter).location.length()) && !(*iter).regex)
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
			if (!std::strncmp(info.request.path.c_str(), (*iter).location.c_str(), info.request.path.length()) && !(*iter).regex)
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
		iter = _config.locationRules.begin();
		while (iter != _config.locationRules.end())
		{
			if ((*iter).regex && !std::strncmp(iter->location.c_str(), &(info.request.path.c_str())[info.request.path.length() - iter->location.length()], iter->location.length()))
			{
				if (ret == NULL || !ret->regex || (*iter).location.length() > ret->location.length())
					ret = &(*iter);
			}
			iter++;
		}
	}

	if (!ret)
	{
		defaultAnswerError(404, info);
		return (NULL);
	}
	return (ret);
}

/*
	Funzione di sostituzione della variabile $uri con l'url richiesto;
*/

t_location* VirtServ::interpretLocationBlock(t_location *location, std::string path)
{
	t_location*	ret = new t_location;
	*ret = *location;
	std::string::iterator iter = ret->text.begin();

	if (path.find("?") != path.npos)
		path = path.substr(0, path.find("?"));

	while (ret->text.find("$uri") != std::string::npos)
	{
		ret->text.replace(iter + ret->text.find("$uri"), iter + ret->text.find("$uri") + 4, path);
		iter = ret->text.begin();
	}

	return (ret);
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
		switch (i) {
			case 0: return ("text/html");
			case 1: return ("text/css");
			case 2:	return ("text/csv");
			case 3:	return ("text/xml");
			default: break;
		}
	}

	for (i = 0; i < toCompareImage->size(); i++)
	{
		if (!fileExtension.compare(toCompareImage[i]))
			break;
	}

	if (i != toCompareImage->size())
	{
		switch (i) {
			case 0: return ("image/gif");
			case 1: //??????????????????????????
			case 2: return ("image/jpeg");
			case 3: return ("image/png");
			case 4: return ("image/tiff");
			case 5: return ("image/x-icon");
			default: break;
		}
	}

	for (i = 0; i < toCompareApp->size(); i++)
	{
		if (!fileExtension.compare(toCompareApp[i]))
			break;
	}

	if (i != toCompareApp->size())
	{
		switch (i) {
			case 0: return ("application/javascript");
			case 1: return ("application/zip");
			case 2: return ("application/pdf");
			default: break;
		}
	}

	return ("text/plain");
}

int 		VirtServ::execGet(t_connInfo & conn)
{
	std::vector<std::pair<std::string, std::string> >::iterator it;
	for (it = _storeReq.begin(); it != _storeReq.end(); it++) {
		if (it->first == conn.headers) {
			conn.request.method.clear();
			send(conn.fd, it->second.c_str(), it->second.size(), 0);
			return 1;
		}
	}
	
	tryFiles(conn); conn.request.method.clear(); return 1;
}

int			VirtServ::execHead(t_connInfo & conn)
{
	std::vector<std::pair<std::string, std::string> >::iterator it;
	for (it = _storeReq.begin(); it != _storeReq.end(); it++) {
		if (it->first == conn.headers) {
			conn.request.method.clear();
			send(conn.fd, it->second.c_str(), it->second.size(), 0);
			return 1;
		}
	}

	tryFiles(conn); conn.request.method.clear(); return 1;
}

int			VirtServ::execPut(t_connInfo & conn)
{	
	if (conn.config.allowedMethods.size() && std::find(conn.config.allowedMethods.begin(), conn.config.allowedMethods.end(), "POST") == conn.config.allowedMethods.end())
	{
		if (chunkEncodingCleaning(conn) == 1) {
			defaultAnswerError(405, conn);
			return 1;
		}
		return 0;
	}
	else if (findKey(conn.request.headers, "Content-Type")->second != "") {
		if (contentType(conn) == 1) {
			defaultAnswerError(201,conn);
			return 1;
		}
	}
	else if (findKey(conn.request.headers, "Transfer-Encoding")->second == "chunked") {
		if (chunkEncoding(conn) == 1) {
			std::string filename = conn.config.root;
			if (*(filename.end() - 1) != '/')
				filename.append("/");
			if (conn.config.root != _config.root)
			{
				std::string	path = conn.request.line.substr(conn.request.line.find_first_of(" \t"));
				path = path.substr(path.find_first_not_of(" \t"));
				path = path.substr(0, path.find_first_of(" ") - 1);
				path = path.substr(conn.location->location.length());
				if (path.at(0) == '/')
					path = path.substr(1);
				filename.append(path);
			}
			else
				filename.append(conn.request.line.substr(1, conn.request.line.find_first_of(" ") - 1));
			FILE* ofs = fopen(filename.c_str(), "wb+");
			for (size_t i = 0; i < conn.body.size(); i++)
				fwrite(&conn.body.at(i), 1, sizeof(char), ofs);
			fclose(ofs);
			conn.body.clear();
			defaultAnswerError(201,conn);
			return 1;
		}
	}
	return 0;
}

int			VirtServ::execDelete(t_connInfo & conn)
{
	if (conn.config.allowedMethods.size() && std::find(conn.config.allowedMethods.begin(), conn.config.allowedMethods.end(), "POST") == conn.config.allowedMethods.end())
	{
		if (chunkEncodingCleaning(conn) == 1) {
			defaultAnswerError(405, conn);
			return 1;
		}
		return 0;
	}
	else if (findKey(conn.request.headers, "Transfer-Encoding")->second == "chunked") {
		if (chunkEncoding(conn) == 1) {
			if (conn.body.size() > conn.config.client_max_body_size) {
				defaultAnswerError(413, conn);
				return 1;
			}
			std::string filename = conn.config.root;
			if (*(filename.end() - 1) != '/')
				filename.append("/");
			if (conn.config.root != _config.root)
			{
				std::string	path = conn.request.line.substr(conn.request.line.find_first_of(" \t"));
				path = path.substr(path.find_first_not_of(" \t"));
				path = path.substr(0, path.find_first_of(" ") - 1);
				path = path.substr(conn.location->location.length());
				if (path.at(0) == '/')
					path = path.substr(1);
				filename.append(path);
			}
			else
				filename.append(conn.request.line.substr(1, conn.request.line.find_first_of(" ") - 1));
			if (filename.at(0) == '/')
				filename = filename.substr(1);
			if (access(filename.c_str(), F_OK))
			{

			}
			else
			{
				remove(filename.c_str());
				defaultAnswerError(204, conn);
			}
			conn.body.clear();
			return 1;
		}
	}
	else
	{
		std::string filename = conn.config.root;
		if (*(filename.end() - 1) != '/')
			filename.append("/");
		if (conn.config.root != _config.root)
		{
			std::string	path = conn.request.line.substr(conn.request.line.find_first_of(" \t"));
			path = path.substr(path.find_first_not_of(" \t"));
			path = path.substr(0, path.find_first_of(" ") - 1);
			path = path.substr(conn.location->location.length());
			if (path.at(0) == '/')
				path = path.substr(1);
			filename.append(path);
		}
		else
			filename.append(conn.request.line.substr(1, conn.request.line.find_first_of(" ") - 1));
		if (conn.request.path.at(0) == '/')
			conn.request.path = conn.request.path.substr(1);
		if (access(filename.c_str(), F_OK))
		{
			defaultAnswerError(404, conn);
		}
		else
		{
			remove(filename.c_str());
			defaultAnswerError(204, conn);
		}
		return (1);
	}
	return (0);
}


int			VirtServ::execPost(t_connInfo & conn)
{
	std::cout << "------EXEC POST------\n";
	if (conn.config.cgi_script != "") {
		std::string filename = conn.request.line.substr(0, conn.request.line.find_first_of(" "));
		if (launchCGI(conn) == 1)
			return 1;
		return 0;
	}
	if (conn.config.allowedMethods.size() && std::find(conn.config.allowedMethods.begin(), conn.config.allowedMethods.end(), "POST") == conn.config.allowedMethods.end())
	{
		if (chunkEncodingCleaning(conn) == 1) {
			defaultAnswerError(405, conn);
			return 1;
		}
		return 0;
	}
	else if (findKey(conn.request.headers, "Transfer-Encoding")->second == "chunked") {
		if (chunkEncoding(conn) == 1) {
			if (conn.body.size() > conn.config.client_max_body_size) {
				defaultAnswerError(413, conn);
				return 1;
			}
			std::string filename = conn.config.root;
			if (*(filename.end() - 1) != '/')
				filename.append("/");
			if (conn.config.root != _config.root)
			{
				std::string	path = conn.request.line.substr(conn.request.line.find_first_of(" \t"));
				path = path.substr(path.find_first_not_of(" \t"));
				path = path.substr(0, path.find_first_of(" ") - 1);
				path = path.substr(conn.location->location.length());
				if (path.at(0) == '/')
					path = path.substr(1);
				filename.append(path);
			}
			else
				filename.append(conn.request.line.substr(1, conn.request.line.find_first_of(" ") - 1));
			FILE* ofs = fopen(filename.c_str(), "wb+");
			for (size_t i = 0; i < conn.body.size(); i++)
				fwrite(&conn.body.at(i), 1, sizeof(char), ofs);
			fclose(ofs);
			conn.body.clear();
			defaultAnswerError(201,conn);
			return 1;
		}
	}
	else if (findKey(conn.request.headers, "Content-Type")->second != "") {
		if (contentType(conn) == 1) {
			defaultAnswerError(201,conn);
			return 1;
		}
	}
	return 0;
}

bool		VirtServ::chunkEncoding(t_connInfo & conn)
{
	int dataRead = 0;
	int totalRead = 0;

	if (conn.chunk_size < 0) {
		unsigned char buffer = 0;
		dataRead = recv(conn.fd, &buffer, 1, 0);
		if (dataRead <= 0) {
			perror ("recv");
			return 1;
		}
		conn.buffer.push_back(buffer);
		if (conn.buffer.find("\r\n") != conn.buffer.npos) {
            conn.chunk_size = strtoul(conn.buffer.c_str(), NULL, 16);
			conn.buffer.clear();
		}
		return 0;
	}
	else if (conn.chunk_size > 0)
	{
		unsigned char buffer[conn.chunk_size];
		while (totalRead < conn.chunk_size) {
			if ((dataRead = recv(conn.fd, buffer + totalRead, conn.chunk_size - totalRead, MSG_DONTWAIT)) < 0)
				usleep(100);
			if (dataRead > 0)
				totalRead += dataRead;
		}
		for (int i = 0; i < totalRead; i++)
			conn.body.push_back(buffer[i]);
		dataRead = recv(conn.fd, buffer, 2, 0);
		conn.chunk_size = -1;
	}
	else if (conn.chunk_size == 0) {
		char chunk[4];
		recv(conn.fd, chunk, sizeof chunk, MSG_DONTWAIT);
		conn.chunk_size = -1;
		return 1;
	}
	return 0;
}

int			VirtServ::chunkEncodingCleaning(t_connInfo & conn)
{
	int dataRead = 0;
	int totalRead = 0;

	if (conn.chunk_size < 0) {
		char buffer[2048] = {0};
		dataRead = recv(conn.fd, buffer, 1, 0);
		if (dataRead <= 0) {
			perror ("recv");
			return 1;
		}
		conn.buffer.append(buffer);
		if (conn.buffer.find("\r\n") != conn.buffer.npos) {
            conn.chunk_size = strtoul(conn.buffer.c_str(), NULL, 16);
			conn.buffer.clear();
		}
		return 0;
	}
	else if (conn.chunk_size > 0)
	{
		unsigned char buffer[conn.chunk_size];
		buffer[0] = 0;
		while (totalRead < conn.chunk_size) {
			if ((dataRead = recv(conn.fd, buffer, conn.chunk_size - totalRead, MSG_DONTWAIT)) < 0)
				usleep(10000);
			if (dataRead > 0)
				totalRead += dataRead;
		}
		dataRead = recv(conn.fd, buffer, 2, 0);
		conn.chunk_size = -1;
	}
	else if (conn.chunk_size == 0) {
		char chunk[4];
		recv(conn.fd, chunk, sizeof chunk, MSG_DONTWAIT);
		conn.chunk_size = -1;
		return 1;
	}
	return 0;
}

bool		VirtServ::contentType(t_connInfo & conn)
{
	std::string _boundary = findKey(conn.request.headers, "Content-Type")->second;
	_boundary = _boundary.substr(_boundary.find_first_of("=") + 1, _boundary.find_last_of("\n") - 1);
	_boundary.append("--");
	_boundary.insert(0, "--");
	unsigned char read[4096] = {0};
	int dataRead = 0;
	dataRead = recv(conn.fd, read, sizeof read, MSG_DONTWAIT);
	for (int i = 0; i < dataRead; i++)
		conn.body.push_back(read[i]);
	if (conn.body.find(_boundary) == conn.body.npos)
		return 0;
	std::string filename;
	filename = conn.body.substr(conn.body.find("filename"), std::string::npos);
	filename = filename.substr(filename.find_first_of("\"") + 1, filename.find_first_of("\n"));
	filename = _config.root + "/put_test/" + filename.substr(0, filename.find_first_of("\""));
	FILE *ofs = fopen(filename.c_str(), "wb+");
	conn.body = conn.body.substr(conn.body.find("\r\n\r\n") + 4, conn.body.npos);
	conn.body = conn.body.substr(0, conn.body.find(_boundary) - 2);
	for (size_t i = 0; i < conn.body.length(); i++)
			fwrite(&conn.body.at(i), 1, sizeof(char), ofs);
	fclose(ofs);
	conn.body.clear();
	return 1;
}

std::string VirtServ::generateCookie(std::string name) {
	std::string ninjaName[28] = { "ka", "zu", "mi", "te", "ku", "lu","ji", 
								"ri", "ki", "zu", "me", "ta", "rin", "to", "mo", "no","no", 
								"ke", "shi", "ari", "chi", "do", "ru", "mei", "na", "fu", "zi" };
	
	std::string	ret;
	for (std::string::iterator it = name.begin(); it != name.end(); it++) {
		ret.append(ninjaName[tolower(*it) - 'a']);
	}
	return (ret);
}


void VirtServ::print_table() {
	std::cout << "\nHash Table\n----------------\n";
	for (std::map<std::string, std::string>::iterator it = _cookies.begin(); it != _cookies.end(); it++) {
		std::cout << it->first << ": " << it->second << std::endl;
	}
	std::cout << "\n----------------\n";
}
