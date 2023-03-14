#include <Cgi.class.hpp>

Cgi::Cgi() {};

Cgi::Cgi(t_connInfo & conn, unsigned short port)
{
    // https://www.rfc-editor.org/rfc/rfc3875#section-4.1
    // https://www.ibm.com/docs/en/netcoolomnibus/8.1?topic=scripts-environment-variables-in-cgi-script

    this->_body = conn.body;
    // std::cout << this->_body << std::endl;

    std::stringstream ss;
    // std::stringstream content_lenght;
    ss << port;
    // content_lenght << this->_body.size();
    _env.push_back(std::make_pair("AUTH_TYPE", ""));
    _env.push_back(std::make_pair("CONTENT_LENGTH", ""));
    _env.push_back(std::make_pair("CONTENT_TYPE", "application/x-www-form-urlencoded"));
    _env.push_back(std::make_pair("GATEWAY_INTERFACE", "CGI/1.1"));
    _env.push_back(std::make_pair("PATH_INFO", "/YoupiBanane/youpi.bla"));
    _env.push_back(std::make_pair("PATH_TRANSLATED", "/YoupiBanane/youpi.bla"));
    _env.push_back(std::make_pair("QUERY_STRING", ""));
    _env.push_back(std::make_pair("REMOTE_ADDR", "localhost:12356"));
    _env.push_back(std::make_pair("REMOTE_HOST", ""));
    _env.push_back(std::make_pair("REMOTE_IDENT", ""));
    _env.push_back(std::make_pair("REMOTE_USER", ""));
    _env.push_back(std::make_pair("REQUEST_METHOD", conn.request.method));
    _env.push_back(std::make_pair("REQUEST_URI", "/YoupiBanane/youpi.bla"));
    _env.push_back(std::make_pair("SCRIPT_NAME", "cgi_tester"));
    _env.push_back(std::make_pair("SERVER_NAME", "http://localhost:12356"));
    _env.push_back(std::make_pair("SERVER_PORT", ss.str()));
    _env.push_back(std::make_pair("SERVER_PROTOCOL", "HTTP/1.1"));
    _env.push_back(std::make_pair("SERVER_SOFTWARE", "Webserv/1.0"));
    _env.push_back(std::make_pair("REDIRECT_STATUS", "200"));
    _env.push_back(std::make_pair("HTTP_SECRET_HEADER_FOR_TEST", "1"));
    
	// this->initEnv(conn);
}

Cgi::~Cgi() {};

char					**Cgi::getEnv() const {
	char	**env = new char*[this->_env.size() + 1];
	int	j = 0;
	for (std::vector<std::pair<std::string, std::string> >::const_iterator i = this->_env.begin(); i != this->_env.end(); i++) {
		std::string	element = i->first + "=" + i->second;
		env[j] = new char[element.size() + 1];
		env[j] = strcpy(env[j], (const char*)element.c_str());
		j++;
	}
	env[j] = NULL;
	return env;
}

std::string		Cgi::executeCgi(const std::string & script)
{
    pid_t   pid;
    // int     std_cpy[2] = { dup(0), dup(1) };
    char    **env = this->getEnv();
    std::string _retBody;

    // UNCOMMENT TO PRINT ENV
    // int i = 0;
    // while (env[i])
    //     printf("%s\n", env[i++]);
        
    // use tmpFile() instead of pipe() to handle big amount of data
    FILE*   in = tmpfile();
	FILE*   out = tmpfile();
    int     fd_in = fileno(in);
    int     fd_out = fileno(out);

    write(fd_in, _body.c_str(), _body.size());
    lseek(fd_in, 0, SEEK_SET);

    pid = fork();

    if (pid == -1) {
        std::cout << "Fork failed" << std::endl;
        return ("Status: 500\r\n\r\n");
    }
    else if (!pid) {
		char *const args[3] = {strdup(script.c_str()), strdup("youpi.bla"), NULL};
		// char *const *args = NULL;
        dup2(fd_in, 0);
		dup2(fd_out, 1);
		execve(script.c_str(), args, env);
		std::cout << "execve failed" << std::endl;
		write(1, "Status: 500\r\n\r\n", 15);
		exit (0);
    }
    else {
		waitpid(-1, NULL, 0);

        lseek(fd_out, 0, SEEK_SET);

		char    buffer[1024];
        size_t  dataRead = 0;
		while ((dataRead = read(fd_out, buffer, sizeof buffer)) > 0) {
			for (size_t i = 0; i < dataRead; i++)
                _retBody.push_back(buffer[i]);
            memset(buffer, 0, sizeof buffer);
        }
	}
	fclose(in);
	fclose(out);
	close(fd_in);
	close(fd_out);

    for (size_t i = 0; env[i]; i++)
		delete[] env[i];
	delete[] env;

    return (_retBody);
}
