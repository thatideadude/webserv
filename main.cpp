#include "webserv.hpp"

int main(void)
{
	ConfigParser config("minimal.conf");
	Webserver	server(config.getServers());
	while(1)
		server.run();
}
