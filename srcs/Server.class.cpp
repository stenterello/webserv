#include <webserv.hpp>
#include <Server.class.hpp>
#include <stack>

//////// Constructors & Destructor //////////////////////////////

VirtServ*	portIsOpen(std::vector<VirtServ> & servers, t_config config)
{
	int	i;

	for (std::vector<VirtServ>::iterator iter = servers.begin(); iter != servers.end(); iter++)
	{
		i = 0;
		while (i < iter->getConfigSize())
		{
			if (iter->getConfig(i++)->port == config.port)
				return (&(*iter));
		}
	}
	return (NULL);
}

Server::Server(const char* filename) : _filename(filename == NULL ? "default.conf" : filename)
{
	VirtServ*	virtServ;
	_config.reserve(30);
	_virtServs.reserve(30);
	Parser(_filename, _config);
	std::vector<t_config>::iterator		iterConfig = _config.begin();

	while (iterConfig != _config.end())
	{
		virtServ = portIsOpen(_virtServs, *iterConfig);
		if (virtServ)
			virtServ->addConfig(*iterConfig);
		else
			_virtServs.push_back(VirtServ(*iterConfig));
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

bool    Server::startListen()
{
	std::stack<int>	vServSock;
	int				fd_count = 0;
	int				fd_size;

	for(std::vector<VirtServ>::iterator it = _virtServs.begin(); it < _virtServs.end(); it++) {
		vServSock.push(it->getSocket());
	}
	fd_size = vServSock.size();
	this->_pfds = (struct pollfd*)malloc(sizeof(*_pfds) * fd_size);
	
	for(int i = 0; vServSock.size() > 0; i++) {
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
				for (std::vector<VirtServ>::iterator it = _virtServs.begin(); it != _virtServs.end(); it++) {
					if (_pfds[i].fd == it->getSocket()) {
						int tmpfd = it->acceptConnectionAddFd(it->getSocket());
						if (tmpfd != -1) {
							this->add_to_pfds(&_pfds, tmpfd, &fd_count, &fd_size);
						}
					} else {
						if (it->handleClient(_pfds[i].fd) == 1) {
							del_from_pfds(_pfds, i, &fd_count);
						}
					}
				}
			}
		}
	}
	return (true);
}
