#ifndef VIRTSERV_CLASS_HPP
# define VIRTSERV_CLASS_HPP

# include <Server.class.hpp>
# include <Cgi.class.hpp>
# include <webserv.hpp>
# include <Cookies.class.hpp>

class VirtServ
{
	private:
		typedef std::vector<std::pair<std::string, std::string> >::iterator	iterator;
		VirtServ();

		t_config											_config;
		std::vector<std::pair<std::string, std::string> >	_storeReq; 
		struct sockaddr_in									_sin;
		struct sockaddr_in									_client;
		socklen_t											_size;
		int													_sockfd;
		std::vector<t_connInfo>								_connections;
		std::map<std::string, std::string>					_cookies;
		bool												startServer();
		bool												stopServer();
		t_location*											searchLocationBlock(t_connInfo & info);
		t_location*											interpretLocationBlock(t_location* location, std::string path);
		void												insertMethod(t_config & tmpConfig, std::string value);
		void												tryFiles(t_connInfo conn);
		bool												tryGetResource(std::string filename, t_connInfo conn);
		void												answer(std::string fullPath, struct dirent* dirent, t_connInfo conn);
		void												answerAutoindex(std::string fullPath, DIR* directory, t_connInfo conn);
		void												defaultAnswerError(int err, t_connInfo conn);
		std::vector<struct dirent *>						fill_dirent(DIR* directory, std::string path);
		DIR*												dirAnswer(std::string fullPath, struct dirent* dirent, t_connInfo conn);
		std::string											getDateTime();
		std::string											defineFileType(char* filename);
		void												checkAndRedirect(std::string value, t_connInfo conn);
		std::vector<t_connInfo>::iterator					findFd(std::vector<t_connInfo>::iterator begin, std::vector<t_connInfo>::iterator end, int fd);
		t_config											getConfig(t_connInfo & conn);
		bool												saveFiles(std::string, t_config & ret, t_connInfo & conn);
		void												correctPath(std::string & path, t_connInfo & conn);
		void												saveCookie(std::string arguments);


	public:
		// Structors
		VirtServ(t_config config);
		~VirtServ();

		// Gets
		int						getSocket();
		t_config				getConfig() const;
		std::vector<int>		getConnfd();
		
		// Communication Functions
		int						readRequest(t_connInfo & conn, std::string req);
		int						acceptConnectionAddFd(int sockfd);
		int						handleClient(int fd);

		int						execPost(t_connInfo & conn);
		int						execPut(t_connInfo & conn);
		int						execGet(t_connInfo & conn);
		int						execHead(t_connInfo & conn);
		int						execDelete(t_connInfo & conn);
		
		int 					keepConnectionAlive(int fd);
		int						launchCGI(t_connInfo & conn);

		// upload functions
		bool					contentType(t_connInfo & conn);
		bool					chunkEncoding(t_connInfo & conn);
		int						chunkEncodingCleaning(t_connInfo & conn);
};

#endif
