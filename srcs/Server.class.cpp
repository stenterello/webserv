#include <webserv.hpp>
#include <Server.class.hpp>

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

Server::~Server() {}

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

void	Server::acceptConnectionAddFd(int fd_count, int fd_size, int sockfd)
{
	socklen_t				addrlen;
	int						newfd;
	struct sockaddr_storage	remoteaddr;

	// If listener is ready to read, handle new connection
	addrlen = sizeof remoteaddr;
	newfd = accept(sockfd,
		(struct sockaddr *)&remoteaddr,
		&addrlen);
	if (newfd == -1) {
		perror("accept");
	} else {
		add_to_pfds(&_pfds, newfd, &fd_count, &fd_size);
		printf("pollserver: new connection from %s on "
			"socket %d\n", "server ip address",
			newfd);
	}
	// In questo modo il newfd si perde dopo questa funzione, invece che essere salvato
	// Viene inviata la risposta a dest_fd, che è preso dalla lista pdfs, la quale contiene
	// i socket e non gli fd di connessione
}

void	Server::handleClient(int i, int fd_count)
{
	char					buf[256];

	std::cout << "VALUE OF I IS: " << i << std::endl;
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
		del_from_pfds(_pfds, i, &fd_count);
	} else {
		// We got some good data from a client
		for(int j = 0; j < fd_count; j++) {
			// Send to everyone!
			int dest_fd = _pfds[j].fd;
			// Except the listener and ourselves
			for(std::vector<VirtServ>::iterator it = _virtServs.begin(); it < _virtServs.end(); it++) {
				if (dest_fd != it->getSockfd() && dest_fd != sender_fd) {
					if (send(dest_fd, buf, nbytes, 0) == -1) {
						perror("send");
					}
				}
				else
				{
					it->cleanRequest();
					it->readRequest(buf);
					it->elaborateRequest(sender_fd);
				}
			}
		}
	}
	std::cout << "FINE HANDLE" << std::endl;
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
		int poll_count = poll(_pfds, fd_count, -1);
		if (poll_count == -1) {
			perror("poll");
			exit(1);
		}
		// Run through the existing connections looking for data to read
		for(std::vector<VirtServ>::iterator it = _virtServs.begin(); it != _virtServs.end(); it++) {
			for (int i = 0; i < fd_count; i++) {
				// Check if someone's ready to read
				if (_pfds[i].revents & POLLIN) // We got one!!
				{
					if (_pfds[i].fd == it->getSocket()) {
						acceptConnectionAddFd(fd_count, fd_size, it->getSocket());
							std::cout << "FINE CONNECTION" << std::endl;
					}
				} // END got ready-to-read from poll()
				else
					handleClient(i, fd_count); // END handle data from client
			}	
		} // END looping through file descriptors
	} // END for(;;)--and you thought it would never end!
	return (true);
}