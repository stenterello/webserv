#include <webserv.hpp>
#include <VirtServ.class.hpp>

void	die(std::string const err)
{
	std::cerr << err << std::endl;
	exit(1);
}

void	die(std::string const err, VirtServ & serv)
{
	std::cerr << err << std::endl;
	close(serv.getSocket());
	exit(1);
}

void	usage()
{
	std::cerr << "Usage\n\t./webserv config_file" << std::endl;
	exit(1);
}

bool	bool_error(std::string error)
{
	std::cout << error;
	return false;
}
