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
	// serv.getConnfd().clear();
	// if (serv.getConnectionFd())
	// 	close(serv.getConnectionFd());
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

// void	printMap(std::tr1::unordered_map<std::string, std::string> & map)
// {
// 	std::cout << "------PRINTING HEADER------" << std::endl;
// 	for (std::tr1::unordered_map<std::string, std::string>::iterator it = map.begin(); it != map.end(); it++) {
// 		std::cout << it->first << " " << it->second << std::endl;
// 	}
// 	std::cout << "------END PRINTING------" << std::endl;
// }
