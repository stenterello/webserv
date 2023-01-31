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
		void				sendResponse();

	public:
		VirtServ(t_config config);
		~VirtServ();

		int					getSocket();
		int					getConnectionFd();
};


#endif
