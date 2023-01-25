#ifndef SERVER_CLASS_HPP
# define SERVER_CLASS_HPP

# include <Parser.class.hpp>
# include <webserv.hpp>
# include <sys/types.h>
# include <sys/socket.h>
# include <netdb.h>
# include <netinet/in.h>
# include <arpa/inet.h>

class Server
{
	private:
		Server();
		Server(Server const & src);
		Server&	operator=(Server const & rhs);

		std::vector<t_config>	_config;
		const char*				_filename;
		std::vector<VirtServ*>	_virtServs;
		int						_sockfd;
		struct addrinfo			*_addrStruct;

		bool	startServer();
		void	closeServer();
		bool	listenServer();

	public:
		Server(const char* filename);
		~Server();
};

#endif
