#pragma once
#include "webserv.hpp"

# define TIMEOUT_MS 100
class Client;
struct Server;
struct pollfd;

class	Webserver
{
	private:
		std::vector<Server>				_configs;
		std::map<int, Server> 			_server_sockets;
	   	std::vector<struct pollfd> 		_poll_fds;
		//std::map<int, Client*> 			_clients;
		bool							_running;
	
		void							_setupSockets();
		void							_createSocket(const Server *config);
		void							_accpetNewConnection(int server_fd);
		void							_handleClientData(int client_fd);
		void							_handleClientWrite(int client_fd);
		void							_removeClient(int client_fd);
		void							_checkTimeouts();
		void							_cleanup();
	public:
		Webserver(void);
		Webserver(std::vector<Server> &configs);
		Webserver(const Webserver &other);
		Webserver &operator=(const Webserver &other);
		~Webserver(void);
		void run(void);
		void stop(void);
};
