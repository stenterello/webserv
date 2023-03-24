#ifndef SERVER_CLASS_HPP
# define SERVER_CLASS_HPP

class VirtServ;
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
		// Constructor / Destructor
		Server(const char* filename);
		~Server();

		// Gets
		struct pollfd*			getPollStruct();
		std::vector<VirtServ>	getVirtServ();
		const char*				getFilename() const;

		// Poll Functions
		void 				add_to_pfds(struct pollfd *pfds[], int newfd, int *fd_count, int *fd_size);
		void				set_fd(struct pollfd *pfds[], int newfd);
		void 				del_from_pfds(struct pollfd pfds[], int i, int *fd_count);
		void				rm_fd(struct pollfd *pfds[], int fd);
		int					getListener(int fd, int listeners[], int n);

		// Listening Loop
		bool				startListen();
		
};

#endif
