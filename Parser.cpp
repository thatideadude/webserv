#include "webserv.hpp"

Parser::Parser(void)
{
	std::cout << "Parser default constructor called\n";
}

Parser::Parser(const std::string file)
{
	std::cout << "Parser constructor called\n";
	try
	{
		_parse(file);
	}
	catch (std::exception &e)
	{
		std::cerr << e.what() << std::endl;
	}
	std::cout << "Parsing " << file << std::endl;
}

Parser::Parser(const Parser &other)
{
	*this = other;
}

Parser	&Parser::operator=(const Parser &other)
{
	std::cout << "Parser assignment operator called\n";
	_servers = other._servers;
	return (*this);
}

Parser::~Parser(void)
{
	std::cout << "Parser destructor called\n";
}

std::string	Parser::toString(int nbr)
{
	std::string	ret;
	int			neg = 0;

	if (nbr < 0)
	{
		neg = 1;
		nbr *= -1;
	}
	do
	{
		char	c = nbr % 10 + '0';
		char	str[2] = {c, 0};
		ret.insert(0, str);
		nbr /= 10;
	}
	while (nbr > 0);
	return (neg ? "-" + ret : ret);
}

char	Parser::toLower(char c)
{
	if (c >= 'A' && c <= 'Z')
		c -= 'A';
	return (c);
}

std::vector<std::string>	Parser::_split(std::string str)
{
	std::vector<std::string>	strings;
	size_t						i = 0;
	size_t						j = 0;
	const char					*s = str.c_str();

	while (s[i])
	{
		while (s[i] && std::isspace(s[i]))
			++i;
			
		j = 0;
		while (s[i + j] && !std::isspace(s[i + j]))
			++j;
		strings.push_back(str.substr(i, j));
		i += j;
	}
	return (strings);
}

void	Parser::_parse(std::string const &file)
{
	std::ifstream	stream(file.c_str());
	std::string		str;

	if (!stream.is_open())
		throw std::runtime_error("Could not open file " + file);
	while (std::getline(stream, str))
	{
		if (str.empty() || _isComment(str))
			continue ;
		if (_compare(str, "server") && str.find ('{') != std::string::npos)
		{
			Server	server;
			while (std::getline(stream, str))
			{
				if (str.empty() || _isComment(str))
					continue ;
				if (_compare(str, "}"))
					break ;
				if (_compare(str, "location"))
				{
					Location					location;
					std::vector<std::string>	tokens = _split(str);
					location.client_max_body_size = 0;
					if (tokens.size() >= 2)
						location.path = tokens[1];
					while (std::getline(stream, str))
					{
						if (str.empty() || _isComment(str))
							continue ;
						if (_compare(str, "}"))
							break ;
						_parseLocation(location, str);
					}
					server.locations.push_back(location);
				}
				else
					_parseServer(server, str);
			}
			_servers.push_back(server);
		}
	}
}

void	Parser::_parseServer(Server &server, const std::string &str)
{
	const char *s = str.c_str();
	if (_compare(str, "listen"))
	{
		size_t	pos = str.find_first_of("0123456789");
		if (pos != std::string::npos)
			server.port = std::atoi(s + pos);
	}
	if (_compare(str, "server_name"))
		_addServers(server, s);
	if (_compare(str, "error_page"))
		_addErrors(server, s);
	if (_compare(str, "client_max_body_size"))
		_addBodySize(server, str);
}

void	Parser::_parseLocation(Location &location, const std::string &line)
{
	std::vector<std::string>	tokens = _split(line);
	if (tokens.empty() || tokens.size() < 2)
		return;
	if (tokens[0] == "root")
		location.root = _stripSemicolon(tokens[1]);
	if (tokens[0] == "autoindex")
		location.autoindex = (_stripSemicolon(tokens[1]) == "on");
	if (tokens[0] == "allowed_methods")
	{
		size_t	i = 0;
		while (i < tokens.size())
			location.allowed_methods.push_back(_stripSemicolon(tokens[i++]));
	}
	if (tokens[0] == "index")
		location.index = _stripSemicolon(tokens.back());
	if (tokens[0] == "client_max_body_size")
		location.client_max_body_size = std::atoi((tokens.back()).c_str());
	if (tokens[0] == "cgi_pass" && tokens.size() > 2)
	{
		std::string	extension = tokens[1];
		std::string	interpreter = _stripSemicolon(tokens[2]);
		location.cgi_extensions[extension] = interpreter;
	}
	if (tokens[0] == "upload_store")
		location.upload_store = tokens[1].substr(0, tokens[1].length() - 1);
	if (tokens[0] == "return_redirect" && tokens.size() >= 3)
	{
		// Join all tokens after the first one (to handle URLs with spaces? though unlikely)
		std::string redirect_value;
		for (size_t i = 1; i < tokens.size(); ++i)
		{
			if (i > 1) redirect_value += " ";
			redirect_value += _stripSemicolon(tokens[i]);
		}
		location.return_redirect = redirect_value;
	}
}

bool	Parser::_isComment(const std::string &line) const
{
	size_t	first = line.find_first_not_of(" \t\r\n");
	if (first != std::string::npos && line[first] == '#')
		return (true);
	return (false);
}

bool	Parser::_compare(const std::string &a, const std::string &b) const
{
	size_t	start = a.find_first_not_of(" \t\r\n");
	if (start == std::string::npos)
		return (false);
	std::string	trimmed = a.substr(start);
	return (trimmed.rfind(b.c_str(), 0) == 0);
}

void	Parser::_addServers(Server &server, const std::string &line)
{
	std::vector<std::string>	vector = _split(line);
	if (vector.empty())
		return ;
	size_t	i = 0;
	while (++i < vector.size())
		server.server_names.push_back(_stripSemicolon(vector[i]));
}

void	Parser::_addErrors(Server &server, const std::string &line)
{
	std::vector<std::string>	vector = _split(line);
	if (vector.empty())
		return ;
	std::string	last = vector.back();
	if (!last.empty() && last[last.length() - 1] == ';')
		last.erase(last.length() - 1);
	size_t	i = 1;
	while (i < vector.size())
	{
		if (!vector[i].empty())
		{
			std::pair<int, std::string>	pair;
			pair.first = std::atoi(vector[i].c_str());
			pair.second = last;
			server.error_pages.insert(pair);
		}
		++i;
	}
}

void	Parser::_addBodySize(Server &server, const std::string &line)
{
	std::vector<std::string>	vector = _split(line);
	if (vector. empty())
		return ;
	std::string last = vector.back();
	server.client_max_body_size = std::atoi(last.c_str());
}

std::vector<Server>	&Parser::getServers(void)
{
	return (_servers);
}

std::string	Parser::_stripSemicolon(std::string str)
{
	if (!str.empty() && str[str.size() - 1] == ';')
		str.erase(str.size() - 1, 1);
	return (str);
};

void Parser::printConfig(void)
{
	if (_servers.empty())
		return;

	for (size_t i = 0; i < _servers.size(); ++i)
	{
		std::cout << "--- Server #" << i << " ---" << std::endl;
		std::cout << "port = " << _servers[i].port << std::endl;
		std::cout << "host = " << _servers[i].host << std::endl;

		for (size_t j = 0; j < _servers[i].server_names.size(); ++j)
			std::cout << "#" << j << " server_name = " << _servers[i].server_names[j] << std::endl;

		std::map<int, std::string>::const_iterator it;
		for (it = _servers[i].error_pages.begin(); it != _servers[i].error_pages.end(); ++it)
			std::cout << "error " << it->first << " = " << it->second << std::endl;

		std::cout << "client_max_body_size = " << _servers[i].client_max_body_size << std::endl;

		for (size_t j = 0; j < _servers[i].locations.size(); ++j)
			printLocation(_servers[i].locations[j]);
	}
}

void Parser::printLocation(Location &location)
{
	std::cout << "  path = " << location.path << std::endl;
	std::cout << "  root = " << location.root << std::endl;
	std::cout << "  index = " << location.index << std::endl;

	for (size_t j = 0; j < location.allowed_methods.size(); ++j)
	{
		std::cout << "  #" << j << " allowed_method = " << location.allowed_methods[j] << std::endl;
	}

	std::map<std::string, std::string>::const_iterator cgi_it;
	for (cgi_it = location.cgi_extensions.begin(); cgi_it != location.cgi_extensions.end(); ++cgi_it)
		std::cout << "  cgi_pass " << cgi_it->first << " = " << cgi_it->second << std::endl;

	std::cout << "  upload_store = " << location.upload_store << std::endl;
	std::cout << "  autoindex = " << location.autoindex << std::endl;
	std::cout << "  return_redirect = " << location.return_redirect << std::endl;

	std::map<int, std::string>::const_iterator it;
	for (it = location.error_pages.begin(); it != location.error_pages.end(); ++it)
	{
		std::cout << "  error " << it->first << " = " << it->second << std::endl;
	}

	std::cout << "  client_max_body_size = " << location.client_max_body_size << std::endl;
}
