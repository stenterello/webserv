#include <webserv.hpp>

int	main(int argc, char** argv)
{
	if (argc != 2)
		usage();
	Server	webserv(argv[1]);
	return (0);
}