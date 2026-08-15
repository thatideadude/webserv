#include "ConfigParser.hpp"
#include <cstring>
#include <cstdlib>
#include <cctype>

std::string stripSemicolon(std::string str)
{
    if (!str.empty() && str.back() == ';')
        str.pop_back();
    return str;
}

ConfigParser::ConfigParser(void)
{
	std::cout << "ConfigParser default constructor called\n";
}

ConfigParser::ConfigParser(const std::string &filename)
{
	std::cout << "ConfigParser constructor called with filename " << filename << std::endl;
	parseFile(filename);
}

ConfigParser::ConfigParser(const ConfigParser &other)
{
	if (this != &other)
		*this = other;
	std::cout << "ConfigParser copy constructor called\n";
}

ConfigParser &ConfigParser::operator=(const ConfigParser &other)
{
	if (this != &other)
		m_servers = other.m_servers;
	return (*this);
}

ConfigParser::~ConfigParser(void)
{
	std::cout << "ConfigParser destructor called\n";
}

int ConfigParser::isComment(std::string &str)
{
	size_t first = str.find_first_not_of(" \t\r\n");
	if (first != std::string::npos && str[first] == '#')
		return 1;
	return 0;
}

int ConfigParser::compare(std::string &a, const char *needle)
{
	size_t start = a.find_first_not_of(" \t\r\n");
	if (start == std::string::npos)
		return 0;
	
	std::string trimmed = a.substr(start);
	return (trimmed.rfind(needle, 0) == 0); 
}

void ConfigParser::addServers(Server &server, const char *s)
{
	int i = 0;
	while (s[i] && isspace(s[i]))
		++i;
	while (s[i] && !isspace(s[i]))
		++i;
	while (s[i])
	{
		int j = 0;
		char charname[1024];
		while (s[i] && isspace(s[i]))
			++i;
		while (s[i] && !isspace(s[i]) && s[i] != ';')
			charname[j++] = s[i++];
		charname[j] = 0;
		if (j > 0)
		{
			std::string name(charname);
			server.server_names.push_back(name);
		}
		if (s[i] == ';')
			break;
	}
}

void ConfigParser::addErrors(Server &server, const char *s)
{
	std::vector<std::string> vector = string_split(std::string(s));
	if (vector.empty())
		return;

	std::string last_element = vector.back();
	if (!last_element.empty() && last_element[last_element.length() - 1] == ';')
		last_element.erase(last_element.length() - 1);

	for (size_t i = 1; i < vector.size() - 1; ++i)
	{
		if (!vector[i].empty())
		{
			std::pair<int, std::string> pair;
			pair.first = std::atoi(vector[i].c_str());
			pair.second = last_element;
			server.error_pages.insert(pair);
		}
	}
}

static void addBodySize(Server &server, std::string str)
{
	std::vector<std::string> vector = string_split(str);
	if (vector.empty())
		return;

	std::string last_element = vector.back();
	server.client_max_body_size = std::atoi(last_element.c_str());
}

void ConfigParser::parseServer(Server &server, std::string &str)
{
	if (compare(str, "listen"))
	{
		size_t pos = str.find_first_of("0123456789");
		if (pos != std::string::npos)
			server.port = std::atoi(str.c_str() + pos);
	}
	else if (compare(str, "server_name"))
		addServers(server, str.c_str());
	else if (compare(str, "error_page"))
		addErrors(server, str.c_str());
	else if (compare(str, "client_max_body_size"))
		addBodySize(server, str);
}


void ConfigParser::parseLocation(Location &location, const std::string &line)
{
    std::vector<std::string> tokens = string_split(line);
    if (tokens.empty())
        return;

    std::string directive = tokens[0];

    if (directive == "root" && tokens.size() > 1)
    {
        location.root = stripSemicolon(tokens[1]);
    }
    else if (directive == "autoindex" && tokens.size() > 1)
    {
        location.autoindex = (stripSemicolon(tokens[1]) == "on");
    }
    else if (directive == "allowed_methods")
    {
        for (size_t i = 1; i < tokens.size(); ++i)
            location.allowed_methods.push_back(stripSemicolon(tokens[i]));
    }
    else if (directive == "index")
           location.index = stripSemicolon(tokens.back());
    else if (directive == "client_max_body_size" && tokens.size() > 1)
    {
			location.client_max_body_size = std::atoi((tokens.back()).c_str());
    }
    else if (directive == "cgi_path" && tokens.size() > 1)
    {
        location.cgi_path = stripSemicolon(tokens[1]);
    }
}

void ConfigParser::printLocation(Location &location)
{
	std::cout << "  path = " << location.path << std::endl;
	std::cout << "  root = " << location.root << std::endl;
	std::cout << "  index = " << location.index << std::endl;

	for (size_t j = 0; j < location.allowed_methods.size(); ++j)
	{
		std::cout << "  #" << j << " allowed_method = " << location.allowed_methods[j] << std::endl;
	}

	std::cout << "  cgi_path = " << location.cgi_path << std::endl;
	std::cout << "  upload_store = " << location.upoload_store << std::endl;
	std::cout << "  autoindex = " << location.autoindex << std::endl;
	std::cout << "  return_redirect = " << location.return_redirect << std::endl;

	std::map<int, std::string>::const_iterator it;
	for (it = location.error_pages.begin(); it != location.error_pages.end(); ++it)
	{
		std::cout << "  error " << it->first << " = " << it->second << std::endl;
	}

	std::cout << "  client_max_body_size = " << location.client_max_body_size << std::endl;
}

void ConfigParser::printConfig(void)
{
	if (m_servers.empty())
		return;

	for (size_t i = 0; i < m_servers.size(); ++i)
	{
		std::cout << "--- Server #" << i << " ---" << std::endl;
		std::cout << "port = " << m_servers[i].port << std::endl;
		std::cout << "host = " << m_servers[i].host << std::endl;

		for (size_t j = 0; j < m_servers[i].server_names.size(); ++j)
			std::cout << "#" << j << " server_name = " << m_servers[i].server_names[j] << std::endl;

		std::map<int, std::string>::const_iterator it;
		for (it = m_servers[i].error_pages.begin(); it != m_servers[i].error_pages.end(); ++it)
			std::cout << "error " << it->first << " = " << it->second << std::endl;

		std::cout << "client_max_body_size = " << m_servers[i].client_max_body_size << std::endl;

		for (size_t j = 0; j < m_servers[i].locations.size(); ++j)
			printLocation(m_servers[i].locations[j]);
	}
}

void ConfigParser::parseFile(std::string const &file)
{
	std::ifstream stream(file.c_str());
	std::string str;

	if (!stream.is_open())
	{
		std::cerr << "Error: Could not open file " << file << std::endl;
		return;
	}

	while (std::getline(stream, str))
	{
		if (str.empty() || isComment(str))
			continue;

		if (compare(str, "server") && str.find('{') != std::string::npos)
		{
			Server server;

			while (std::getline(stream, str))
			{
				if (str.empty() || isComment(str))
					continue;

				if (compare(str, "}"))
					break;

				if (compare(str, "location"))
				{
					Location location;

					std::vector<std::string> tokens = string_split(str);
					if (tokens.size() >= 2)
						location.path = tokens[1];

					while (std::getline(stream, str))
					{
						if (str.empty() || isComment(str))
							continue;
						if (compare(str, "}"))
							break; 
						
						parseLocation(location, str);
					}
					server.locations.push_back(location);
				}
				else
				{
					parseServer(server, str);
				}
			}
			m_servers.push_back(server);
		}
	}
}

std::vector<Server>	&ConfigParser::getServers(void)
{
	return (m_servers);
}

