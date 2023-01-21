#include <webserv.hpp>

//////// Constructors & Destructor //////////////////////////////

Server::Server() : _filename("default.conf")
{
	openFile(_filename);
}

Server::Server(const char* filename) : _filename(filename)
{
	openFile(filename);
}

Server::~Server() {}


//////// Private Functions /////////////////////////////////////

void		Server::openFile(const char* filename)
{
	std::ifstream	configFile(filename);

	if (configFile.is_open())
		defineConfig(configFile);
	else
		die("Error opening file. Aborting");

	configFile.close();
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

bool		Server::curlyBrace(std::string text, size_t pos)
{
	std::string::iterator	iter = text.begin();

	for (size_t i = 0; i < pos + 6; i++)
		iter++;
	
	while (*iter && *iter != '{')
	{
		if (*iter != ' ' && *iter != '\t' && *iter != '\n')
			return (false);
		iter++;
	}
	return (true);
}

size_t		Server::searchEndingCurlyBrace(std::string text, size_t pos)
{
	size_t	len = text.length();
	int		count = 0;

	while (pos < len)
	{
		if (!std::strncmp(&text.c_str()[pos], "location", 8))
		{
			pos += 8;
			while (pos < len && text.at(pos) != '{')
				pos++;
			if (pos == len)
				die("Rules must end with a closing curly brace");
			if (text.at(pos) != '{')
				die("'location' rule must precede an opening curly brace");
			count++;
			pos++;
		}
		if (text.at(pos) == '}' && !count)
			return (pos);
		else if (text.at(pos) == '}')
			count--;
		pos++;
	}
	if (pos == len)
		return (std::string::npos);
	return (pos);
}

void		Server::divideAndCheck(std::string text, std::vector<std::string> & serverBlocks)
{
	size_t	start;
	size_t	end;

	while (text.find("server") != std::string::npos)
	{
		start = text.find("server");
		if (!curlyBrace(text, start))
			die("Each server block must be inside curly braces. Aborting");
		end = searchEndingCurlyBrace(text, start + 6);
		if (end == std::string::npos)
			die("Each server block must end, at a certain point. With a '}'. Aborting");
		serverBlocks.push_back(text.substr(start, end));
		text = text.substr(0, start) + text.substr(end + 1);
	}

	if (text.find_first_not_of(" \t\n") != std::string::npos)
		die("All configuration must be inside server blocks. Aborting");
}

bool		Server::takeRule(std::string & text, t_config & conf)
{
	std::string	line;
	size_t		start;
	size_t		end;
	std::string	key;
	std::string	value;

	// Take line and erase it from text
	end = text.find(';');
	if (end == std::string::npos)
		die("Rules must end with semicolon. Aborting");
	line = text.substr(0, end);
	text = text.substr(end + 1);
	std::cout << "LINE : " << line << std::endl;
	std::cout << "TEXT : " << text << std::endl;
	if (line.find('\n') != std::string::npos)
		die("Rules must can't be splitted on more lines. Aborting");
	
	// Split line and insert in conf
	

	return (false);
}

void		Server::elaborateServerBlock(std::string serverBlock)
{
	t_config	tmp;
	size_t		start;
	size_t		end;
	std::string	tmpString;

	// Clean string from 'server {' line and ending curly brace
	start = serverBlock.find('{');
	while (serverBlock.at(start) == '{' || serverBlock.at(start) == ' '
			|| serverBlock.at(start) == '\t' || serverBlock.at(start) == '\n')
		start++;
	end = serverBlock.find('}');
	while (end != std::string::npos && (serverBlock.at(end) == '{'
			|| serverBlock.at(end) == ' ' || serverBlock.at(end) == '\t'
			|| serverBlock.at(end) == '\n'))
		end--;

	// Parse each line while there is one and no error encountered
	tmpString = serverBlock.substr(start, end);
	while (takeRule(tmpString, tmp))
		;
}

void		Server::defineConfig(std::ifstream & configFile)
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

	// std::cout << "Test output [ after deleting comments ]" << std::endl;
	// std::cout << text << std::endl;
	
	// Divide text in location blocks string and check that nothing is outside
	divideAndCheck(text, serverBlocks);

	// Elaborate each server block
	std::vector<std::string>::iterator	iter2 = serverBlocks.begin();

	while (iter2 != serverBlocks.end())
	{
		elaborateServerBlock(*iter2);
		iter2++;
	}
}
