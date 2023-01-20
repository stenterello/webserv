#include <webserv.hpp>

//////// Constructors & Destructor //////////////////////////////

Server::Server(const char* filename)
{
	openFile(filename);
}

Server::~Server() {}


//////// Private Functions /////////////////////////////////////

void	Server::openFile(const char* filename)
{
	std::ifstream	configFile(filename);

	if (configFile.is_open())
		defineConfig(configFile);
	else
		die("Error opening file. Aborting");

	configFile.close();
}

void	skipSpaces(std::string::iterator iter, std::string line)
{
	while (iter != line.end() && (*iter == ' ' || *iter == '\t'))
		iter++;
}

void	skipComment(std::string::iterator iter, std::string line)
{
	while (iter != line.end() && *iter != '\n')
		iter++;
}

void	Server::defineConfig(std::ifstream & configFile)
{
	std::string				line;
	std::stringstream		buffer;
	std::string::iterator	iter;

	buffer << configFile.rdbuf();
	line = buffer.str();

	iter = line.begin();
	if (iter == line.end())
		die("Configuration file: format error");
	skipSpaces(iter, line);
	if (iter != line.end() && *iter == '#')
		skipComment(iter, line);
	else if (iter != line.end() && !std::strncmp(&(*iter), "server", 6))
	{
		while (iter != line.end() && *iter != '{')
			iter++;
		if (iter == line.end())
			die("Configuration file: format error");
		saveServerConfig(iter, line);
	}
	else
		die("Configuration file: format error");
}

std::string::iterator	Server::saveServerConfig(std::string::iterator & iter, std::string line)
{
	
}
