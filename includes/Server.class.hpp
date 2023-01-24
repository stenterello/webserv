#ifndef SERVER_CLASS_HPP
# define SERVER_CLASS_HPP

# include <Parser.class.hpp>
# include <webserv.hpp>

class Server
{
	private:
		Server(Server const & src);
		Server&	operator=(Server const & rhs);

		std::vector<t_config>	_config;
		const char*				_filename;

	public:
		Server();
		Server(const char* filename);
		~Server();
};

#endif
