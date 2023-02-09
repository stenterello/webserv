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
		t_request			_request;
		t_response			_response;
		bool				startServer();
		bool				stopServer();
		t_location*			searchLocationBlock(std::string method, std::string path, int dest_fd);
		void				interpretLocationBlock(t_location* location);
		void				executeLocationRules(std::string text, int dest_fd);
		void				tryFiles(std::string value, t_config tmpConfig, int dest_fd);
		bool				tryGetResource(std::string filename, t_config tmpConfig, int dest_fd);
		void				answer(std::string fullPath, struct dirent* dirent, int dest_fd);
		void				answerAutoindex(std::string fullPath, DIR* directory, int dest_fd);
		void				defaultAnswerError(int err, int dest_fd, t_config tmpConfig);
		struct dirent**		fill_dirent(DIR* directory);
		void				sendResponse();
		void				dirAnswer(std::string fullPath, struct dirent* dirent, int dest_fd, t_config tmpConfig);

	public:
		// Structors
		VirtServ(t_config config);
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
