#ifndef VIRTSERV_CLASS_HPP
# define VIRTSERV_CLASS_HPP

# include <Server.class.hpp>
# include <webserv.hpp>

class VirtServ
{
	private:
		typedef std::vector<std::pair<std::string, std::string> >::iterator	iterator;
		VirtServ();

		t_config				_config;
		struct sockaddr_in		_sin;
		struct sockaddr_in		_client;
		socklen_t				_size;
		int						_sockfd;
		std::vector<t_connInfo>	_connections;
		t_request				_request;
		t_response				_response;
		bool					startServer();
		bool					stopServer();
		t_location*				searchLocationBlock(std::string method, std::string path, int dest_fd);
		t_location*				interpretLocationBlock(t_location* location, std::string path);
		t_config				executeLocationRules(std::string locationName, std::string text, int dest_fd);
		void					insertMethod(t_config & tmpConfig, std::string value);
		void					tryFiles(std::string value, t_connInfo conn);
		bool					tryGetResource(std::string filename, t_connInfo conn);
		void					answer(std::string fullPath, struct dirent* dirent, int dest_fd);
		void					answerAutoindex(std::string fullPath, DIR* directory, int dest_fd);
		void					defaultAnswerError(int err, t_connInfo conn);
		struct dirent**			fill_dirent(DIR* directory, std::string path);
		DIR*					dirAnswer(std::string fullPath, struct dirent* dirent, t_connInfo conn);
		std::string				getDateTime();
		iterator				findKey(std::vector<std::pair<std::string, std::string> > & vector, std::string key);
		std::string				defineFileType(char* filename);
		void					checkAndRedirect(std::string value, int dest_fd);
		std::vector<t_connInfo>::iterator	findFd(std::vector<t_connInfo>::iterator begin, std::vector<t_connInfo>::iterator end, int fd);
		t_config				getConfig(t_location* loc, int connfd, std::string path);
		bool					saveFiles(std::string, t_config & ret, int connfd);


	public:
		// Structors
		VirtServ(t_config config);
		~VirtServ();

		// Gets
		int						getSocket();
		t_config				getConfig() const;
		std::vector<int>		getConnfd();
		
		// Communication Functions
		void					cleanRequest();
		void					cleanResponse();
		int						readRequest(std::string req);
		t_config				elaborateRequest(int dest_fd);
		int						acceptConnectionAddFd(int sockfd);
		int						handleClient(int fd);

		int						execPost(t_connInfo conn);
		int						execPut(t_connInfo conn);
		
		int 					keepConnectionAlive(int fd);
		int						launchCGI();
		// upload functions
		FILE*					contentType(int fd);
		FILE*					chunkEncoding(int fd);
		bool					chunkEncodingCleaning(int fd);
		
		char*					recv_timeout(t_connInfo & info); // recv with gettimeofday prototype
};

#endif
