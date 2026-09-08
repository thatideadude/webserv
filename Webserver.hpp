#pragma once

#define TIMEOUT_MS 100
	
class	Router;
class	CGIHandler;
struct	Server;
struct	Location;
struct	pollfd;

class	Webserver
{
	public:
		Webserver(void);
		Webserver(std::vector<Server> &configs);
		Webserver(const Webserver &other);
		Webserver	&operator=(const Webserver &other);
		~Webserver(void);

		void						run(void);
		void						stop(void);
	private:
		std::vector<Server>			_configs;
		std::map<int, Server>		_server_sockets;
		std::vector<struct pollfd>	_poll_fds;
		std::map<int, Client*>		_clients;
		std::map<int, int>			_cgi_fd_to_client;
		bool						_running;
		Router						_router;

		void						_setupSockets();
		void						_createSockets(const Server &config);
		void						_acceptNewConnection(int server_fd);
		void						_handleClientData(int client_fd);
		void						_handleClientWrite(int client_fd);
		void						_removeClient(int client_fd);
		void						_checkTimeouts(void);
		void						_cleanup(void);
		void						_updatePollEvents(int fd, short events);
		void						_processRequest(int client_fd);
		void						_sendErrorResponse(Client *client, int status, const std::string &status_next);
		std::string					_buildResponse(int status, const std::string &status_text,
									const std::string &content_type, const std::string &body);
		Server						*_findServer(const std::string &host_header, int listen_port);
		void						_handleGetRequest(Client *client, Location *location,
									const std::string &fs_path, const std::string &uri);
		void						_handlePostRequest(Client *client, Location *location);
		void						_handleDeleteRequest(Client *client, const std::string &fs_path);
		void						_handleMultipartUpload(Client *client, const Location &location,
									const std::string &content_type, const std::string &body);
		void						_handleFileUpload(Client *client, const Location &location,
									const std::string &filename, const std::string &content);
		void						_handleHeadRequest(Client *client, const Location &location, const std::string &script_path);

		void						_serveFile(Client *client, const std::string &path);
		void						_handleCGIRequest(Client *client, const Location &location,
										const std::string &script_path, const std::string &uri, Server *server,
										const std::string &interpreter);
		bool						_isFdTracked(int fd) const;
		void						_handleFdError(int fd);
		void						_handleCGIRead(int fd);
		void						_handleCGIWrite(int fd);
		void						_finishCGI(int client_fd);
		void						_removeCGIFd(int fd);
};
