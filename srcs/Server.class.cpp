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

void	Server::defineConfig(std::ifstream & configFile)
{
	std::string	line;
	bool		serverMatched = false;
	bool		inBlock = false;
	t_config	n_conf;

	while (configFile.good())
	{
		configFile >> line;
		parseLine(line, serverMatched, inBlock, n_conf);
	}
}

std::string	trim(std::string line)
{
	const std::string	whitespace = " \t";
	size_t				start;
	size_t				end;

	start = line.find_first_not_of(whitespace);
	if (start == std::string::npos)
		return ("");
	end = line.find_last_not_of(whitespace);
	return (line.substr(start, end - start + 1));
}

void	Server::findServerBlock(std::string line, bool serverMatched, bool inBlock)
{
	char	*ptr;

	if (line.at(0) == '#') // Comment handling
		return ;
	else if (!isalpha(line.at(0))) // First character is not alpha
		die("\"" + line + "\" is not a valid line in configuration file. Aborting");
	else if (line.compare("server") && !inBlock) // First word outside block is not server
		die("Configuration must start with a server block ('server'). Aborting");
	else if (!line.compare("server") && !serverMatched && !inBlock) // server is mentioned, entering block
	{
		serverMatched = true;
		// Search for '{'
		strncpy(ptr, line.c_str(), 100);
		ptr += strlen("server");
		while (*ptr && (*ptr == ' ' || *ptr == '\t'))
			ptr++;
		if (*ptr && *ptr != '{')
			die("Server blocks must start with '{'. Aborting");
		else if (*ptr && *ptr == '{')
			inBlock = true;
	}
	else if (!line.compare("server") && inBlock) // server rule is nested inside another server block
		die("Server block can't be nested. Aborting");
}

void	Server::findOpeningOfBlock(std::string line, bool inBlock)
{
	char	*ptr;

	strncpy(ptr, line.c_str(), 100);
	if (ptr[0] != '{')
		die("Server blocks must start with '{'. Aborting");
	while (*ptr && (*ptr == ' ' || *ptr == '\t'))
		ptr++;
	if (*ptr)
		die("After '{' there must be a return character. Aborting");
	else
		inBlock = true;
}

void	Server::searchRule(std::string line, t_config n_conf)
{
	std::string::difference_type	count = std::count(line.begin(), line.end(), '{');
	if (count)
		die();
}

void	Server::parseLine(std::string line, bool serverMatched, bool inBlock, t_config n_conf)
{
	// Delete whitespaces at start and end
	line = trim(line);
	if (line == "")
		return ;

	if (!serverMatched)
		findServerBlock(line, serverMatched, inBlock);
	else if (serverMatched && !inBlock)
		findOpeningOfBlock(line, inBlock);
	if (inBlock)
		searchRule(line, n_conf);
}