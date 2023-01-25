#ifndef VIRTSERV_CLASS_HPP
# define VIRTSERV_CLASS_HPP

# include <webserv.hpp>

class VirtServ
{
	private:
		VirtServ();

		t_config				_config;
		struct sockaddr_in		_sin;
		int						_sockfd;
		int						_connfd;
		bool					startServer();
		bool					startListen();
		bool					stopServer();

	public:
		VirtServ(t_config config);
		~VirtServ();
};


#endif
