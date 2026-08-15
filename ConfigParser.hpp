#pragma once
#include "webserv.hpp"

struct	Location
{
	std::string					path;
	std::string					root;
	std::string					index;
	std::vector<std::string>	allowed_methods;
	std::string					cgi_path;
	std::string					upoload_store;
	bool						autoindex;
	std::string					return_redirect;
	std::map<int, std::string>	error_pages;
	size_t						client_max_body_size;
};

struct	Server
{
	int							port;
	std::string					host;
	std::vector<std::string>	server_names;
	std::map<int, std::string>	error_pages;
	size_t						client_max_body_size;
	std::vector<Location>		locations;
};

class	ConfigParser
{
	private:
		std::vector<Server>		m_servers;

	public:
		ConfigParser(void);
		ConfigParser(const std::string &filename);
		ConfigParser(const ConfigParser &other);
		ConfigParser &operator=(const ConfigParser &other);
		~ConfigParser(void);

		void	parseFile(const std::string &file);
		void	parseServer(Server &server, std::string &str);
		void	parseLocation(Location &location, const std::string &line);
		void	addServers(Server &server, char **str);
		int		isComment(std::string &str);
		int		compare(std::string &a, const char *needle);
		void    addServers(Server &server, const char *str);
		void 	addErrors(Server &server, const char *s);
		void	printConfig(void);
		void	printLocation(Location &location);
		std::vector<Server>	&getServers(void);
};
