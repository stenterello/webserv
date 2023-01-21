#include <webserv.hpp>

//////// Constructors & Destructor //////////////////////////////

Server::Server(const char* filename) : _filename(filename)
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

std::string	deleteComments(std::string text)
{
	std::string	temp;
	size_t		pos;
	size_t		nl;

	temp = text;
	while (text.find('#') != std::string::npos)
	{
		pos = text.find('#');
		nl = text.find('\n', pos);
		if (nl == std::string::npos)
			return (text.substr(0, pos));
		else
		{
			temp = text.substr(0, pos) + text.substr(nl);
			text = temp;
		}
	}
	return (temp);
}

void	Server::defineConfig(std::ifstream & configFile)
{
	std::string				text;
	std::stringstream		buffer;
	std::string::iterator	iter;

	buffer << configFile.rdbuf();
	text = buffer.str();

	iter = text.begin();
	text = deleteComments(text);
	std::cout << text << std::endl;
}
