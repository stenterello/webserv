#ifndef VIRTSERV_CLASS_HPP
# define VIRTSERV_CLASS_HPP

# include <Server.class.hpp>
# include <webserv.hpp>

class VirtServ
{
	private:
		VirtServ();

		Server*				_server;
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
		bool				stopServer();
		t_location*			searchLocationBlock(std::string method, std::string path);
		void				interpretLocationBlock(t_location* location);
		void				executeLocationRules(std::string text, int dest_fd);
		void				tryFiles(std::string value, t_config tmpConfig, int dest_fd);
		FILE*				tryGetResource(std::string filename, t_config tmpConfig, int dest_fd);
		void				answer(std::string fullPath, struct dirent* dirent, int dest_fd);
		void				answerAutoindex(std::string fullPath, DIR* directory, int dest_fd);
		void				sendResponse();

	public:
		VirtServ(t_config config, Server* server);
		~VirtServ();

		int					getSocket();
		int					getConnectionFd();
		void				cleanRequest();
		void				readRequest(std::string req);
		void				elaborateRequest(int dest_fd);

		int				acceptConnectionAddFd(int sockfd);
		int				handleClient(int fd);
		t_config			getConfig() { return _config; };
		int					getSockfd() { return _sockfd; };
		bool				sendAll(int socket, char *buf, size_t *len);
};

#endif
