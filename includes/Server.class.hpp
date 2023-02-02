#ifndef SERVER_CLASS_HPP
# define SERVER_CLASS_HPP

# include <VirtServ.class.hpp>
# include <Parser.class.hpp>
# include <webserv.hpp>
# include <poll.h>

class Server
{
	private:
		Server();
		Server(Server const & src);
		Server&	operator=(Server const & rhs);

		std::vector<t_config>	_config;
		const char*				_filename;
		std::vector<VirtServ>	_virtServs;

		t_request			_request;

		struct pollfd		*_pfds;

	public:
		Server(const char* filename);
		~Server();

		bool	startListen();

		// Poll Functions
		void				acceptConnectionAddFd(int fd_count, int fd_size, int sockfd);
		void				handleClient(int i, int fd_count);
		void 				add_to_pfds(struct pollfd *pfds[], int newfd, int *fd_count, int *fd_size);
		void 				del_from_pfds(struct pollfd pfds[], int i, int *fd_count);


		void				cleanRequest();
		void				readRequest(std::string req);
		void				elaborateRequest(int dest_fd);
		
};

#endif
