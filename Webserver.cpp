#include "webserv.hpp"
#include <sys/poll.h>

//	Webserver::Webserver(void)
//	{
//		std::cout << "Webserver default construcrtor called\n";
//	}

Webserver::Webserver(std::vector<Server> &servers) : _configs(servers)
{
	std::cout << "Webserver constructor called\n";
}

Webserver::Webserver(const Webserver &other) : _configs(other._configs)
{
	if (this != &other)
		*this = other;
	std::cout << "Webserver copy constructor called\n";
}

Webserver	&Webserver::operator=(const Webserver &other)
{
	(void) other;
	//copy all one by one
	return (*this);
}

Webserver::~Webserver(void)
{
	std::cout << "Webserver destructor called\n";
}
		//void							_setupSockets();
		//void							_createSocket(const Server *config);
		//void							_accpetNewConnection(int server_fd);
		//void							_handleClientData(int client_fd);
		//void							_handleClientWrite(int client_fd);
		//void							_removeClient(int client_fd);
		//void							_checkTimeouts();
		//void							_cleanup();

void	Webserver::run(void)
{
	while (_running)
	{

		int	ret = poll(_poll_fds.data(), _poll_fds.size(), TIMEOUT_MS);
		if (ret < 0)
			break;
	}
}

void	Webserver::_setupSockets(void)
{
}
