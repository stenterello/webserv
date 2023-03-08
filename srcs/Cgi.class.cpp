#include <Cgi.class.hpp>

Cgi::Cgi(t_request & request, t_config & config, unsigned short port)
{
    // https://www.rfc-editor.org/rfc/rfc3875#section-4.1

    std::stringstream ss;
    ss << port;
    _env.push_back(std::make_pair("AUTH_TYPE", ""));
    _env.push_back(std::make_pair("CONTENT_LENGTH", ""));
    _env.push_back(std::make_pair("CONTENT_TYPE", (*findKey(request.headers, "Content-Type")).second));
    _env.push_back(std::make_pair("GATEWAY_INTERFACE", "CGI/1.1"));
    _env.push_back(std::make_pair("PATH_INFO", ""));
    _env.push_back(std::make_pair("PATH_TRANSLATED", ""));
    _env.push_back(std::make_pair("QUERY_STRING", ""));
    _env.push_back(std::make_pair("REMOTE_ADDR", ""));
    _env.push_back(std::make_pair("REMOTE_HOST", ""));
    _env.push_back(std::make_pair("REMOTE_IDENT", ""));
    _env.push_back(std::make_pair("REMOTE_USER", ""));
    _env.push_back(std::make_pair("REQUEST_METHOD", request.method));
    _env.push_back(std::make_pair("SCRIPT_NAME", ""));
    _env.push_back(std::make_pair("SERVER_NAME", ""));
    _env.push_back(std::make_pair("SERVER_PORT", ss.str()));
    _env.push_back(std::make_pair("SERVER_PROTOCOL", "HTTP/1.1"));
    _env.push_back(std::make_pair("SERVER_SOFTWARE", ""));
    _env.push_back(std::make_pair("REDIRECT_STATUS", "200")); // ????

    // bisogna mettere Auth-Scheme nella request per l'autorizzazione?
    
	this->initEnv(request, config);
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