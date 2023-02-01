#ifndef VIRTSERV_CLASS_HPP
# define VIRTSERV_CLASS_HPP

# include <webserv.hpp>

class VirtServ
{
	private:
		VirtServ();

		t_config			_config;
		struct sockaddr_in	_sin;
		struct sockaddr_in	_client;
		socklen_t			_size;
		int					_sockfd;
		int					_connfd;
		t_locationCases		_cases;
		t_request			_request;
		t_response			_response;
		bool				startServer();
		bool				startListen();
		bool				stopServer();
		void				cleanRequest();
		void				readRequest(std::string req);
		void				elaborateRequest();
		t_location*			searchLocationBlock(std::string method, std::string path);
		void				interpretLocationBlock(t_location* location);
		void				executeLocationRules(std::string text);
		void				tryFiles(std::string value, t_config tmpConfig);
		FILE*				tryGetResource(std::string filename, t_config tmpConfig);
		void				answer(std::string fullPath, struct dirent* dirent);
		void				answerAutoindex(std::string fullPath, DIR* directory);
		void				sendResponse();
		// Poll Functions
		void 				add_to_pfds(struct pollfd *pfds[], int newfd, int *fd_count, int *fd_size);
		void 				del_from_pfds(struct pollfd pfds[], int i, int *fd_count);

		struct pollfd		*_pfds;
		int					_fd_size;
		int					_fd_count;

	public:
		VirtServ(t_config config);
		~VirtServ();

		int					getSocket();
		int					getConnectionFd();


		// Select
		// bool				selectList();

		bool				sendAll(int socket, char *buf, size_t *len);
};

#endif