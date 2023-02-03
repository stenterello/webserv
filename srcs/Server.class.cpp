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
		_virtServs.push_back(VirtServ(*iterConfig, this));
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


///////// Main functions /////////////////////////////////////////

// Add a new file descriptor to the set
void Server::add_to_pfds(struct pollfd *pfds[], int newfd, int *fd_count, int *fd_size)
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
void Server::del_from_pfds(struct pollfd pfds[], int i, int *fd_count)
{
	// Copy the one from the end over this one
	pfds[i] = pfds[*fd_count-1];
	(*fd_count)--;
}

bool    Server::startListen()
{
	std::stack<int>	vServSock;
	int				fd_count = 0; // For the listener
	int				fd_size;

	for(std::vector<VirtServ>::iterator it = _virtServs.begin(); it < _virtServs.end(); it++) {
		std::cout << it->getConfig().port << std::endl;
		vServSock.push(it->getSockfd());
	}
	fd_size = vServSock.size();
	this->_pfds = (struct pollfd*)malloc(sizeof(*_pfds) * fd_size);
	
	 // Add the listener to set
	for(int i = 0; vServSock.size() > 0; i++) {
		_pfds[i].fd = vServSock.top();
		_pfds[i].events = POLLIN;
		vServSock.pop();
		fd_count++;
	}

	for(;;) {
		std::cout << "START LOOP" << std::endl;  
		int poll_count = poll(_pfds, fd_count, 30000);
		if (poll_count == -1) {
			perror("poll");
			exit(1);
		}
		if (poll_count == 0) {
			perror("Timeout");
			exit(1);
		}
		// Run through the existing connections looking for data to read
		for (int i = 0; i < fd_count; i++) {
			for (std::vector<VirtServ>::iterator it = _virtServs.begin(); it != _virtServs.end(); it++) {
				if (_pfds[i].revents & POLLIN) {
					if (_pfds[i].fd == it->getSocket()) {
						int tmpfd = it->acceptConnectionAddFd(it->getSocket());
						if (tmpfd != -1)
							this->add_to_pfds(&_pfds, tmpfd, &fd_count, &fd_size);
					}
				} else {
					if (it->handleClient(_pfds[i].fd) == 0) {
						del_from_pfds(_pfds, i, &fd_count);
					}
				} // END looping through file descriptors
			} // END for(;;)--and you thought it would never end!
		}
	}
	return (true);
}