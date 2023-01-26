#ifndef VIRTSERV_CLASS_HPP
# define VIRTSERV_CLASS_HPP

# include <webserv.hpp>

class VirtServ
{
	private:
		VirtServ();

		t_config							_config;
		struct sockaddr_in					_sin;
		struct sockaddr_in					_client;
		socklen_t							_size;
		int									_sockfd;
		int									_connfd;
		std::map<std::string, std::string>	_request;
		bool								startServer();
		bool								startListen();
		bool								stopServer();
		void								cleanRequest();
		void								readRequest(std::string req);

	public:
		VirtServ(t_config config);
		~VirtServ();
};


#endif
