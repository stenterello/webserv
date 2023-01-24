#ifndef PARSER_CLASS_HPP
# define PARSER_CLASS_HPP

# include <webserv.hpp>

class Parser
{
	private:
		t_cases		_cases;

		void		openFile(const char* filename, std::vector<t_config> & conf);
		void		defineConfig(std::ifstream & configFile, std::vector<t_config> & conf);
		std::string	deleteComments(std::string text);
		bool		curlyBrace(std::string text, size_t pos);
		size_t		searchEndingCurlyBrace(std::string text, size_t pos);
		void		divideAndCheck(std::string text, std::vector<std::string> & serverBlocks);
		void		elaborateServerBlock(std::string serverBlock, t_config & conf);
		bool		parseLocation(std::string & line, t_config & conf);
		bool		prepareRule(std::string & text, t_config & conf);
		bool		fillConf(std::string key, std::string value, t_config & conf);
		void		checkHostPort(std::string value, t_config & conf);
		void		checkValidIP(std::string value);
		void		checkServerName(std::string value, t_config & conf);
		void		checkRoot(std::string value, t_config & conf);
		void		checkAutoIndex(std::string value, t_config & conf);
		void		checkIndex(std::string value, t_config & conf);
		void		checkErrorPages(std::string value, t_config & conf);
		void		checkClientBodyMaxSize(std::string value, t_config & conf);
		bool		configComplete(t_config & conf);
		size_t		checkBlockStart(std::string text);
		size_t		checkBlockEnd(std::string text);

	public:
		Parser(const char* filename, std::vector<t_config> & conf);
		~Parser();

};

#endif
