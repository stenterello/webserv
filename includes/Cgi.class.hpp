#ifndef CGI_CLASS_HPP
# define CGI_CLASS_HPP

# include <webserv.hpp>

class Cgi
{
	public:
        Cgi();
		Cgi(t_request & request, t_config & config, unsigned short port); // sets up env according to the request
		Cgi(Cgi const & src);
		virtual ~Cgi(void);

		Cgi  	        &operator=(Cgi const & src);
		std::string		executeCgi(const std::string & script);	// executes cgi and returns body
	private:
		Cgi(void);
		void								                initEnv(t_request &request, t_config &config);
		char**								                getEnv() const;
		std::vector<std::pair<std::string, std::string> >	_env; // env to pass to execve
		std::string							                _body;


};

#endif