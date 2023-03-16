#include <Parser.class.hpp>

//////// Constructors & Destructor //////////////////////////////

Parser::Parser(const char* filename, std::vector<t_config> & conf)
{
	openFile(filename, conf);
}

Parser::~Parser() {}


//////// Private Functions //////////////////////////////////////

void		Parser::openFile(const char* filename, std::vector<t_config> & conf)
{
	std::ifstream	configFile(filename);

	if (configFile.is_open())
		defineConfig(configFile, conf);
	else
		die("Error opening file. Aborting");

	configFile.close();
}

std::string	Parser::deleteComments(std::string text)
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

bool		Parser::curlyBrace(std::string text, size_t pos)
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

size_t		Parser::searchEndingCurlyBrace(std::string text, size_t pos)
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
		else if (text.at(pos) == '{')
			count++;
		pos++;
	}
	if (pos == len)
		return (std::string::npos);
	return (pos);
}

size_t		Parser::checkBlockStart(std::string text)
{
	size_t	i = 6 + text.find("server");
	while (text.at(i) == ' ' || text.at(i) == '\n' || text.at(i) == '\t')
		i++;
	if (text.at(i) != '{')
		die("Server block must start with 'server {'. Aborting");
	return (i + 1);
}

size_t		Parser::checkBlockEnd(std::string text)
{
	std::string::iterator	iter = text.begin();
	int						count = 0;

	while (iter != text.end())
	{
		if (*iter == '{')
			count++;
		else if (*iter == '}')
			count--;
		if (count == 0 && *iter == '}')
			return (std::distance(text.begin(), iter) - 1);
		iter++;
	}
	die("Server blocks must end with '}'. Aborting.");
	return (0);
}

void		Parser::divideAndCheck(std::string text, std::vector<std::string> & serverBlocks)
{
	size_t	start;
	size_t	end;

	while (text.find("server") != std::string::npos)
	{
		start = checkBlockStart(text);
		end = checkBlockEnd(text);
		serverBlocks.push_back(text.substr(start, end - start));
		text = text.substr(0, text.find("server")) + text.substr(text.find("}", end + 1) + 1);
	}

	if (text.find_first_not_of(" \t\n") != std::string::npos)
		die("All configuration must be inside server blocks. Aborting");
}

void		Parser::checkValidIP(std::string value)
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

void		Parser::checkHostPort(std::string value, t_config & conf)
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


	if (value.find_first_of('.') != std::string::npos)// if a IP addr is present...
	{
		//Check that there are only 4 numbers and 3 dots
		checkValidIP(value);

		// insert them in string conf.host
		for (int i = 0; i < 4; i++)
		{
			ip = strtol(value.c_str(), NULL, 0);
			if (ip > 255 || ip < 0)
				die("IP must be of valid form. Aborting");
			conf.host += value.substr(0, value.find_first_of("."));
			if (i != 3)
				conf.host += ".";
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

	// check for the port
	port = strtol(value.c_str(), NULL, 0);
	if (port > 65535 || ip < 0)
		die("Port number must be unsigned short. Aborting");
	conf.port = (unsigned short) port;
}

void		Parser::checkServerName(std::string value, t_config & conf)
{
	std::string	tmp;
	size_t		idx;

	idx = value.find_first_of(" \t");
	if (idx == std::string::npos)
		tmp = value;
	else
	{
		while (idx != std::string::npos)
		{
			tmp = value.substr(0, idx);
			value = value.substr(idx);
			value = value.substr(value.find_first_not_of(" \t"));
			conf.server_name.push_back(tmp);
			idx = value.find_first_of(" \t");
		}
		tmp = value.substr(0, idx);
	}
	conf.server_name.push_back(tmp);

	std::vector<std::string>::iterator	iter = conf.server_name.begin();
	while (iter != conf.server_name.end())
	{
		if ((*iter).find_first_of(".") == std::string::npos)
			die("Each domain must have at least one dot. Aborting");
		tmp = *iter;
		while (tmp.find_first_of(".") != std::string::npos)
		{
			tmp = tmp.substr(tmp.find_first_of("."));
			if (tmp.at(1) == '\0' || tmp.at(1) == '.')
				die("Server name is of invalid format. Aborting");
			tmp = tmp.substr(1);
		}
		iter++;
	}
}

void		Parser::checkRoot(std::string value, t_config & conf)
{
	if (value.find(" \n\t") != std::string::npos)
		die("Root path can only be one. Aborting");
	conf.root = value;
}

void		Parser::checkAutoIndex(std::string value, t_config & conf)
{
	if (value.compare("on") && value.compare("off"))
		die("Valid values for autoindex are on|off. Aborting");
	if (!value.compare("on"))
		conf.autoindex = true;
	else
		conf.autoindex = false;
}

void		Parser::checkIndex(std::string value, t_config & conf)
{
	std::string	tmp;

	while (value.find_first_not_of(" \n \t") != std::string::npos)
	{
		tmp = value.substr(0, value.find_first_of(" \t"));
		conf.index.push_back(tmp);
		if (value.find_first_of(" \t") == std::string::npos)
			break ;
		value = value.substr(value.find_first_of(" \t"));
		value = value.substr(value.find_first_not_of(" \t"));
	}
}

void		Parser::checkErrorPages(std::string value, t_config & conf)
{
	std::string	tmp;

	while (value.find_first_not_of(" \n \t") != std::string::npos)
	{
		tmp = value.substr(0, value.find_first_of(" \t"));
		conf.errorPages.push_back(tmp);
		if (value.find_first_of(" \t") == std::string::npos)
			break ;
		value = value.substr(value.find_first_of(" \t"));
		value = value.substr(value.find_first_not_of(" \t"));
	}
}

void		Parser::checkClientBodyMaxSize(std::string value, t_config & conf)
{
	size_t	idx = value.find_first_not_of("0123456789");
	unsigned long	size;
	size = strtoul(value.c_str(), NULL, 0);
	if (idx != std::string::npos)
	{
		if (value.c_str()[idx + 1] != '\0')
			die("Write client_max_body_size in an acceptable way. Aborting");
		if (value.at(idx) == 'K')
			size *= 1024;
		else if (value.at(idx) == 'M')
			size *= (1024 * 1024);
		else if (value.at(idx) == 'B')
			;
		else
			die("Correct units for client_max_body_size are B (bit), K (kilobit), M (megabit). Aborting");
	}
	conf.client_max_body_size = size;
}

void		Parser::checkMethods(std::string value, t_config & conf)
{
	std::string	methods[5] = { "GET", "POST", "DELETE", "PUT", "HEAD" };
	std::string	tmp;
	int i;

	if (value.find_first_of(" \t\n") != std::string::npos)
	{
		while (value.length())
		{
			tmp = value.substr(0, value.find_first_of(" \t\n"));
			for (std::vector<std::string>::iterator	iter = conf.allowedMethods.begin(); iter != conf.allowedMethods.end(); iter++)
			{
				for (i = 0; i < 5; i++)
				{
					if (!(*iter).compare(methods[i]))
						break ;
				}
				if (i == 5)
					die("Method in configuration not recognized. Aborting");
			}
			conf.allowedMethods.push_back(tmp);
			if (value.find_first_of(" \t\n") == std::string::npos)
				break ;
			value = value.substr(value.find_first_of(" \t\n"));
			value = value.substr(value.find_first_not_of(" \t\n"));
		}
	}
	else
	{
		conf.allowedMethods.push_back(value);

		for (std::vector<std::string>::iterator	iter = conf.allowedMethods.begin(); iter != conf.allowedMethods.end(); iter++)
		{
			for (i = 0; i < 5; i++)
			{
				if (!(*iter).compare(methods[i]))
					break ;
			}
			if (i == 5)
				die("Method in configuration not recognized. Aborting");
		}
	}
}

void		Parser::fillConf(std::string key, std::string value, t_config & conf)
{
	int 		i;
	std::string	toCompare[9] = { "listen", "server_name", "root", "autoindex", "index", "error_page", "client_max_body_size", "allowed_methods", "location" };

	for (i = 0; i < 9; i++)
		if (!toCompare[i].compare(key))
			break ;
	if (i == 9)
		die("Directive not recognized: " + key + ". Aborting.");

	switch (i)
	{
		case 0: // listen
			checkHostPort(value, conf); break ;
		case 1: // server_name
			checkServerName(value, conf); break ;
		case 2: // root
			checkRoot(value, conf); break ;
		case 3: // auto_index
			checkAutoIndex(value, conf); break ;
		case 4: // index
			checkIndex(value, conf); break ;
		case 5: // error_page
			checkErrorPages(value, conf); break ;
		case 6: // client_max_body_size
			checkClientBodyMaxSize(value, conf); break ;
		case 7:
			checkMethods(value, conf); break ;
		case 8:
			break ;
		default:
			die("Unrecognized rule. Aborting");
	}
}

bool		Parser::parseLocation(std::string & line, t_config & conf)
{
	t_location	tmp;
	size_t		start;
	size_t		end;

	// check if there's a regex or exact path case
	line = line.substr(8).substr(line.find_first_not_of(" \t"));
	line = line.substr(line.find_first_not_of(" \t"));
	tmp.regex = false;
	tmp.exact_path = false;
	if (line.at(0) == '~')
		tmp.regex = true;
	else if (line.at(0) == '=')
		tmp.exact_path = true;

	// find the location path
	start = line.find_first_not_of(" \t~=");
	line = line.substr(start);
	end = line.find_first_of(" \t\n{");
	tmp.location = line.substr(0, end);
	if (tmp.location.empty())
		die("There's no path defined in location. Aborting");

	// insert the remaining part inside tmp.text

	line = line.substr(line.find_first_of(" \t\n{"));
	start = line.find_first_not_of(" \t\n{");
	end = line.find_last_not_of(" \t\n}");
	tmp.text = line.substr(start, end - start + 1);

	conf.locationRules.push_back(tmp);
	return (false);
}

void		Parser::prepareRule(std::string & line, t_config & conf)
{
	std::string	key;
	std::string	value;

	// (Check whitespace between key and value)
	if (line.find_first_of(" \t") == std::string::npos)
		die("Rules key and values must be separated by a whitespace. Aborting");

	// (Divide into key and value)
	key = line.substr(0, line.find_first_of(" \t"));
	if (!key.compare("location"))
	{
		parseLocation(line, conf);
	}
	else
	{
		value = line.substr(key.length());
		value = value.substr(value.find_first_not_of(" \t"));
	}
	fillConf(key, value, conf);
}

t_config		Parser::elaborateServerBlock(std::string serverBlock)
{
	ssize_t		start;
	ssize_t		end;
	std::string	tmpString;
	t_config	ret;

	end = -1;
	do {
		if (end >= static_cast<ssize_t>(serverBlock.length()))
			break ;
		serverBlock = serverBlock.substr(end + 1);
		if (serverBlock.empty())
			break ;
		start = serverBlock.find_first_not_of(" \n\t");
		if (std::strncmp(serverBlock.substr(start).c_str(), "location", 8))
			end = serverBlock.find(";");
		else
			end = serverBlock.find("}") + 1;
		tmpString = serverBlock.substr(start, end - start);
		if (std::strncmp(tmpString.c_str(), "location", 8) && tmpString.find("\n") != std::string::npos)
			die("Rules must end with semicolon. Aborting");
		prepareRule(tmpString, ret);
	} while (1);

	if (!serverBlock.empty())
		serverBlock = serverBlock.replace(serverBlock.begin() + start, serverBlock.begin() + start + tmpString.length(), "");
	// if (serverBlock.find_first_not_of(" \n\t") != std::string::npos)
	// 	die("Anything outside server block is not accepted. Aborting");

	if (ret.port == 0 || ret.root == "")
		die("Server must listen to a port and be rooted in a path. Aborting");
		
	return (ret);
}

void		Parser::defineConfig(std::ifstream & configFile, std::vector<t_config> & conf)
{
	std::string					text;
	std::stringstream			buffer;
	std::vector<std::string>	serverBlocks;

	// Save file in one string
	buffer << configFile.rdbuf();
	text = buffer.str();

	// Delete all comments
	text = deleteComments(text);

	// std::cout << "Test output [ after deleting comments ]" << std::endl;
	// std::cout << text << std::endl;

	// Divide text in location blocks string and check that nothing is outside
	divideAndCheck(text, serverBlocks);

	// Elaborate each server block
	std::vector<std::string>::iterator	iter = serverBlocks.begin();

	while (iter != serverBlocks.end())
	{
		conf.push_back(elaborateServerBlock(*iter));
		iter++;
	}
}
