#ifndef SERVER_CLASS_HPP
# define SERVER_CLASS_HPP

# include <VirtServ.class.hpp>
# include <Parser.class.hpp>
# include <webserv.hpp>

class Server
{
	private:
		Server();
		Server(Server const & src);
		Server&	operator=(Server const & rhs);

		std::vector<t_config>	_config;
		const char*				_filename;
		std::vector<VirtServ>	_virtServs;

	public:
		Server(const char* filename);
		~Server();
};

#endif
