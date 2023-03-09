#include <Cgi.class.hpp>

Cgi::Cgi(t_connInfo & conn, unsigned short port)
{
    // https://www.rfc-editor.org/rfc/rfc3875#section-4.1

    std::stringstream ss;
    ss << port;
    _env.push_back(std::make_pair("AUTH_TYPE", ""));
    _env.push_back(std::make_pair("CONTENT_LENGTH", ""));
    _env.push_back(std::make_pair("CONTENT_TYPE", (*findKey(conn.request.headers, "Content-Type")).second));
    _env.push_back(std::make_pair("GATEWAY_INTERFACE", "CGI/1.1"));
    _env.push_back(std::make_pair("PATH_INFO", ""));
    _env.push_back(std::make_pair("PATH_TRANSLATED", ""));
    _env.push_back(std::make_pair("QUERY_STRING", ""));
    _env.push_back(std::make_pair("REMOTE_ADDR", ""));
    _env.push_back(std::make_pair("REMOTE_HOST", ""));
    _env.push_back(std::make_pair("REMOTE_IDENT", ""));
    _env.push_back(std::make_pair("REMOTE_USER", ""));
    _env.push_back(std::make_pair("REQUEST_METHOD", conn.request.method));
    _env.push_back(std::make_pair("SCRIPT_NAME", ""));
    _env.push_back(std::make_pair("SERVER_NAME", ""));
    _env.push_back(std::make_pair("SERVER_PORT", ss.str()));
    _env.push_back(std::make_pair("SERVER_PROTOCOL", "HTTP/1.1"));
    _env.push_back(std::make_pair("SERVER_SOFTWARE", ""));
    _env.push_back(std::make_pair("REDIRECT_STATUS", "200")); // ????

    // bisogna mettere Auth-Scheme nella request per l'autorizzazione?
    
	this->initEnv(conn);
}

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

// script should be 127.0.0.1:1025 ?????????????????
std::string		Cgi::executeCgi(const std::string & script)
{
    pid_t   pid;
    // int     std_cpy[2] = { dup(0), dup(1) };
    char    **env = this->getEnv();
    std::string _retBody;

    // use tmpFile() instead of pipe() to handle big amount of data
    FILE*   in = tmpfile();
	FILE*   out = tmpfile();
    int     fd_in = fileno(in);
    int     fd_out = fileno(out);

    pid = fork();

    if (pid == -1) {
        std::cout << "Fork failed" << std::endl;
        return ("Status: 500\r\n\r\n");
    }
    else if (!pid) {
		char * const *null = NULL;
		dup2(fd_in, 0);
		dup2(fd_out, 1);
		execve(script.c_str(), null, env);
		std::cout << "execve failed" << std::endl;
		write(1, "Status: 500\r\n\r\n", 15);
		exit (0);
    }
    else {
	waitpid(-1, NULL, 0);
	char    buffer[1024];

	while ((read(fd_out, buffer, sizeof buffer)) > 0)
	    _retBody.append(buffer);
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
