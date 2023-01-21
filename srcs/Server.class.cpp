#include <webserv.hpp>

//////// Constructors & Destructor //////////////////////////////

Server::Server() : _filename("default.conf")
{
	openFile(_filename);
}

Server::Server(const char* filename) : _filename(filename == NULL ? "default.conf" : filename)
{
	openFile(_filename);
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

void		Server::checkValidIP(std::string value)
{
	std::string::iterator	iter = value.begin();
	int						dots = 0;
	int						numbers;

	if (!std::isdigit(*iter))
		die("IP must be of valid form. Aborting");
	numbers = 1;
	while (*iter && std::isdigit(*iter))
		iter++;
	while (iter != value.end())
	{
		if (*iter && std::isdigit(*iter))
		{
			numbers++;
			while (*iter && std::isdigit(*iter))
				iter++;
		}
		if (*iter && *iter == '.')
			dots++;
		if (*iter && *iter == ':')
			break ;
		iter++;
	}
	if (dots == 3 && numbers == 4)
		return ;
	die("IP must be of valid form. Aborting");
}

void		Server::checkHostPort(std::string value, t_config & conf)
{
	std::string::iterator	iter = value.begin();
	long					port;
	long					ip;
	
	if (value.find_first_of(" \t") != std::string::npos)
		die("There can't be spaces in host:port declaration. Aborting");
	while (iter != value.end())
	{
		if (!std::isdigit(*iter) && *iter != '.' && *iter != ':')
			die("Listen directive must contains only accepted client address (IPv4) and port numbers. Aborting");
		iter++;
	}
	if (value.find_first_of('.') != std::string::npos)
	{
		checkValidIP(value);
		for (int i = 0; i < 4; i++)
		{
			ip = strtoul(value.c_str(), NULL, 0);
			if (ip > 255 || ip < 0)
				die("IP must be of valid form. Aborting");
			conf.host[i] = (unsigned char)ip;
			value = value.substr(value.find_first_of('.') + 1);
		}
		iter = value.begin();
		while (*iter && std::isdigit(*iter))
			iter++;
		if (*iter != ':')
			die("IP must be of valid form. Aborting");
		if (iter++ == value.end())
			die("'listen' directive must specify a port. Aborting");
		if (!std::isdigit(*iter))
			die("IP must be of valid form. Aborting");
		value = value.substr(value.find_first_of(':') + 1);
	}
	port = strtoul(value.c_str(), NULL, 0);
	if (port > 65535 || ip < 0)
		die("Port number must be unsigned short. Aborting");
	conf.port = (unsigned short) port;
}

void		Server::fillConf(std::string key, std::string value, t_config & conf)
{
	int i;

	for (i = 0; i < 7; i++)
		if (!_cases.c[i].compare(key))
			break ;

	switch (i)
	{
		case 0: // listen
			checkHostPort(value, conf); break ;
		case 1: // server_name
			break ;
		case 2: // root
			break ;
		case 3: // auto_index
			break ;
		case 4: // index
			break ;
		case 5: // error_pages 
			break ;
		case 6: // client_body_max_size
			break ;
	}

	std::cout << "Debug host:port info" << std::endl;
	std::cout << conf.port << std::endl;
	std::cout << static_cast<int>(conf.host[0]) << "." << static_cast<int>(conf.host[1]) << "." << static_cast<int>(conf.host[2]) << "." << static_cast<int>(conf.host[3]) << std::endl << std::endl;
}

bool		Server::prepareRule(std::string & text, t_config & conf)
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
	if (line.find('\n') != std::string::npos)
		die("Rules must can't be splitted on more lines. Aborting");
	
	// Split line and insert in conf
	// (Trim)
	start = line.find_first_not_of(" \t\n");
	end = line.find_last_not_of(" \t\n");
	line = line.substr(start, end + 1);

	// (Check whitespace between key and value)
	if (line.find_first_of(" \t") == std::string::npos)
		die("Rules key and values must be separated by a whitespace. Aborting");

	// (Divide into key and value)
	key = line.substr(0, line.find_first_of(" \t"));
	value = line.substr(key.length());
	value = value.substr(value.find_first_not_of(" \t"));

	std::cout << std::endl << "Debug key value splitting" << std::endl;
	std::cout << "|" << key << "|" << std::endl;
	std::cout << "|" << value << "|" << std::endl << std::endl;

	fillConf(key, value, conf);
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
	while (prepareRule(tmpString, tmp))
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
