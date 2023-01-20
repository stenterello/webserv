#include <webserv.hpp>

void	die(std::string const err)
{
	std::cerr << err << std::endl;
	exit(1);
}

void	usage()
{
	std::cerr << "Usage\n\t./webserv config_file" << std::endl;
	exit(1);
}
