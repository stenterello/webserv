#ifndef VIRTSERV_CLASS_HPP
# define VIRTSERV_CLASS_HPP

# include <Server.class.hpp>
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
		std::vector<int>	_connfd;
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
		// Structors
		VirtServ(t_config config);
		VirtServ(const VirtServ & cpy, unsigned short port);
		~VirtServ();

		// Gets
		int					getSocket();
		t_config			getConfig() const;
		std::vector<int>	getConnfd();
		
		// Communication Functions
		void				cleanRequest();
		void				readRequest(std::string req);
		void				elaborateRequest(int dest_fd);
		int					acceptConnectionAddFd(int sockfd);
		int					handleClient(int fd);
		bool				sendAll(int socket, char *buf, size_t *len);
};

#endif
