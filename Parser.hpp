#pragma once

struct	Location
{
	std::string							path;
	std::string							root;
	std::string							index;
	std::vector<std::string>			allowed_methods;
	std::map<std::string, std::string>	cgi_extensions;
	std::string							upload_store;
	bool								autoindex;
	std::string							return_redirect;
	std::map<int, std::string>			error_pages;
	size_t								client_max_body_size;
};

struct	Server
{
	int									port;
	std::string							host;
	std::vector<std::string>			server_names;
	std::map<int, std::string>			error_pages;
	size_t								client_max_body_size;
	std::vector<Location>				locations;
};

class	Parser
{
	public:
		Parser(void);
		Parser(const std::string);
		Parser(const Parser &other);
		Parser &operator=(const Parser &other);
		~Parser(void);
		void							printConfig(void);
		void							printLocation(Location &location);
		std::vector<Server>				&getServers(void);
		static std::string				toString(int nbr);
		static char						toLower(char c);

	private:
		std::vector<Server>				_servers;

		std::vector<std::string>		_split(std::string str);
		void							_parse(std::string const &file);
		void							_parseServer(Server &server, const std::string &str);
		void							_parseLocation(Location &location, const std::string &line);
		bool							_isComment(const std::string &line) const;
		bool							_compare(const std::string &a, const std::string &b) const;
		void							_addServers(Server &server, const std::string &line);
		void							_addErrors(Server &server, const std::string &line);
		void							_addBodySize(Server &server, const std::string &str);
		std::string						_stripSemicolon(std::string str);
};
