#include "webserv.hpp"
#include <csignal>

volatile sig_atomic_t	g_shutdown = 0;
static void handleSignal(int sig)
{
	(void) sig;
	g_shutdown = 1;
}
int	main(int argc, char **argv)
{
	if (argc != 2)
	{
		std::cout << "Usage: \"./webserv + config_file.conf\"\n";
		return (1);
	}
	signal(SIGINT, handleSignal);
	signal(SIGTERM, handleSignal);
	signal(SIGPIPE, SIG_IGN);

	Parser	parser(argv[1]);
	parser.printConfig();

	Webserver	server(parser.getServers());
	server.run();
	std::cout << "Shutting down gracefully." << std::endl;
	return (0);
}
