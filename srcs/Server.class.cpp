#include <webserv.hpp>
#include <Server.class.hpp>
#include <stack>

//////// Constructors & Destructor //////////////////////////////

Server::Server(const char* filename) : _filename(filename == NULL ? "default.conf" : filename)
{
	_config.reserve(10);
	_virtServs.reserve(10);
	Parser(_filename, _config);
	std::vector<VirtServ>::iterator		iter = _virtServs.begin();
	std::vector<t_config>::iterator		iterConfig = _config.begin();

	while (iterConfig != _config.end())
	{
		_virtServs.push_back(VirtServ(*iterConfig));
		iter++;
		iterConfig++;
	}

	this->startListen();
}

Server::~Server()
{
	free(_pfds);
	_config.clear();
	_virtServs.clear();
}


////////// Getters & Setters /////////////////////////////////////

struct pollfd*			Server::getPollStruct() { return _pfds; }
std::vector<VirtServ>	Server::getVirtServ() { return _virtServs; }
const char*				Server::getFilename() const { return _filename; }


///////// Main functions /////////////////////////////////////////

void Server::add_to_pfds(struct pollfd *pfds[], int newfd, int *fd_count, int *fd_size)
{
	if (*fd_count == *fd_size) {
		*fd_size *= 2; // Double it
		*pfds = (struct pollfd *)realloc(*pfds, sizeof(**pfds) * (*fd_size));
	}
	(*pfds)[*fd_count].fd = newfd;
	(*pfds)[*fd_count].events = POLLIN | POLLPRI; // Check ready-to-read
	(*fd_count)++;
}

void Server::del_from_pfds(struct pollfd pfds[], int i, int *fd_count)
{
	pfds[i] = pfds[*fd_count-1];
	(*fd_count)--;
}

int	Server::getListener(int fd, int listeners[], int n)
{
	for (int j = 0; j < n; j++) {
		if (fd == listeners[j]) {
			return listeners[j];
		}
	}
	return -1;
}

bool    Server::startListen()
{
	std::stack<int>									vServSock;
	int												fd_size, fd_count = 0, nRead = 0, tmpListener, newFd;
	std::vector<std::pair<std::string, bool> >		requests (1000, std::make_pair("", false));
	unsigned char									buffer[512];
	socklen_t 										addrlen;
	struct sockaddr_storage 						remoteaddr;
	
	typedef std::vector<VirtServ>::iterator iterator;
	for(iterator it = _virtServs.begin(); it < _virtServs.end(); it++) {
		std::cout << it->getConfig().port << std::endl;
		vServSock.push(it->getSocket());
	}
	fd_size = vServSock.size();
	int		listeners[fd_size];
	int		nListeners = fd_size;
	this->_pfds = (struct pollfd*)malloc(sizeof(*_pfds) * fd_size);
	
	for(int i = 0; vServSock.size() > 0; i++) {
		listeners[i] = vServSock.top();
		_pfds[i].fd = vServSock.top();
		_pfds[i].events = POLLIN;
		vServSock.pop();
		fd_count++;
	}

	for(;;) {
		int poll_count = poll(_pfds, fd_count, -1);
		
		if (poll_count == -1) {
			perror("poll");
			exit(1);
		}
		for (int i = 0; i < fd_count; i++) {
			if (_pfds[i].revents & (POLLIN | POLLPRI | POLLRDNORM)) {
				if ((tmpListener = getListener(_pfds[i].fd, listeners, nListeners)) != -1) {
					addrlen = sizeof remoteaddr;
					newFd = accept(tmpListener, (struct sockaddr *)&remoteaddr, &addrlen);
					if (newFd == -1)
						perror("accept");
					else {
						for (std::vector<VirtServ>::iterator it = _virtServs.begin(); it != _virtServs.end(); it++) {
							if (tmpListener == it->getSocket()) {
								it->acceptConnectionAddFd(newFd);
							}
						}
						add_to_pfds(&_pfds, newFd, &fd_count, &fd_size);
					}
				} else {
					int clientFd = _pfds[i].fd;
					if (requests[clientFd].second == false) {
						nRead = recv(clientFd, buffer, sizeof buffer, 0);
						if (nRead <= 0) {
							if (nRead < 0)
								perror ("recv");
							if (nRead == 0)
								printf("pollserver: socket %d hung up\n", clientFd);
							close(clientFd);
							del_from_pfds(_pfds, i, &fd_count);
						}
						for (int j = 0; j < nRead; j++)
							requests[clientFd].first.push_back(buffer[j]);
						memset(buffer, 0, nRead);
						if (requests[clientFd].first.find("\r\n\r\n") != requests[clientFd].first.npos)
							requests[clientFd].second = true;
					}
					if (requests[clientFd].second == true){
						std::string tmp;
						tmp = requests[clientFd].first;
						tmp = tmp.substr(tmp.find("Host") + 6);
						tmp = tmp.substr(tmp.find(":") + 1, tmp.find("\r\n"));
						std::stringstream ss;
						unsigned short _port;
						ss << tmp;
						ss >> _port;
						for(iterator it = _virtServs.begin(); it < _virtServs.end(); it++) {
							if (it->getConfig().port == _port)
								if (it->handleClient(clientFd, requests[clientFd].first) == 1) {
									del_from_pfds(_pfds, i, &fd_count);
									requests[clientFd].first.clear();
									requests[clientFd].second = false;
								}
						}
					}
				}
			}
		}
	}
	return (true);
}
