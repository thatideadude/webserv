#include "webserv.hpp"

std::vector<std::string> string_split(std::string str);

int		main(int argc, char **argv)
{
	if (argc != 2)
		return (1);
	ConfigParser	serverInfo(argv[1]);
	Webserver		webserver(serverInfo.getServers());
	serverInfo.printConfig();
}
