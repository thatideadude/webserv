#pragma once
#include "webserv.hpp"

# define TIMEOUT_MS 100

class	Router;
class	Client;
struct	Server;
struct	pollfd;

class	Webserver
{
	private:
		std::vector<Server>				_configs;
		std::map<int, Server> 			_server_sockets;
	   	std::vector<struct pollfd> 		_poll_fds;
		std::map<int, Client*> 			_clients;
		bool							_running;
		Router							_router;
	
		void							_setupSockets();
		void							_createSocket(const Server &config);
		void							_acceptNewConnection(int server_fd);
		void							_handleClientData(int client_fd);
		void							_handleClientWrite(int client_fd);
		void							_removeClient(int client_fd);
		void							_checkTimeouts();
		void							_cleanup();
		void							_updatePollEvents(int fd, short events);
 		void							_processRequest(int client_fd);
		void							_sendErrorResponse(Client *client, int status, const std::string &status_next);
		std::string						_buildResponse(int status, const std::string &status_text,
										const std::string &content_type, const std::string &body);
		void							_serveFile(Client *client, const std::string &path);
		void							_handleGetRequest(Client *client, Location *location,
										const std::string &fs_path, const std::string &uri)	;
		void							_handlePostRequest(Client *client, Location *location);
		void							_handleDeleteRequest(Client *client, const std::string &fs_path);
		void							_handleMultipartUpload(Client *client, const Location &location,
										const std::string &content_type, const std::string &body);
		void							_handleFileUpload(Client *client, const Location &location,
										const std::string &filename, const std::string &content);
		void							_handleHeadRequest(Client *client, const Location &location,
										const std::string &fs_path);
		Server							*_findServer(const std::string &host_header);
	public:
		Webserver(void);
		Webserver(std::vector<Server> &configs);
		Webserver(const Webserver &other);
		Webserver &operator=(const Webserver &other);
		~Webserver(void);
		void run(void);
		void stop(void);
};
