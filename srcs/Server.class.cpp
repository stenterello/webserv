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

std::string	Server::deleteComments(std::string text)
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

bool	curlyBrace(std::string text, size_t pos)
{
	std::string::iterator	iter = text.begin();

	for (size_t i = 0; i < pos + 5; i++)
		iter++;
	
	while (*iter && *iter != '{')
	{
		if (*iter != ' ' && *iter != '\t')
			return (false);
		iter++;
	}
	return (true);
}

void	divideAndCheck(std::string text, std::vector<std::string> serverBlocks)
{
	size_t	start;
	size_t	end;

	while (text.find("server") != std::string::npos)
	{
		start = text.find("server");
		if (!curlyBrace(text, start))
			die("Each server block must be inside curly braces. Aborting");
		end = text.find('}', start);
		if (end == std::string::npos)
			die("Each server block must end, at a certain point. With a '}'. Aborting");
		serverBlocks.push_back(text.substr(start, end));
		text = text.substr(0, start) + text.substr(end);
	}

	if (text.find_first_not_of(" \t") != std::string::npos)
		die("All configuration must be inside server blocks. Aborting");
}

void	Server::defineConfig(std::ifstream & configFile)
{
	std::string					text;
	std::stringstream			buffer;
	std::string::iterator		iter;
	std::vector<std::string>	serverBlocks;

	// Save file in one string
	buffer << configFile.rdbuf();
	text = buffer.str();

	// Delete all comments
	iter = text.begin();
	text = deleteComments(text);
	
	// Divide text in location blocks string and check that nothing is outside
	divideAndCheck(text, serverBlocks);
}
