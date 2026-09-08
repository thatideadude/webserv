#include "webserv.hpp"

Router::Router(void)
{
	std::cout << "Router default constructor called\n";
	_initMimeTypes();
}

Router::Router(const Router &other)
{
	*this = other;
}

Router	&Router::operator=(const Router &other)
{
	_mime_types = other._mime_types;
	return (*this);
}

Router::~Router(void)
{
	std::cout << "Router destructor called\n";
}

const Location	*Router::findLocation(const Server &server, const std::string &uri)
{
	const Location	*best_match = NULL;
	size_t			best_match_len = 0;
	size_t			i = 0;

	std::cout << "Looking for location matching: '" << uri << "'" << std::endl;
	while (i < server.locations.size())
	{
		const Location		&loc = server.locations[i];
		const std::string	&path = loc.path;
		std::cout << "Checking: '" << path << "'" << std::endl;
		if (path == "/")
		{
			std::cout << "Root (will use as fallback if needed)" << std::endl;
			if (!best_match)
			{
				best_match = &loc;
				best_match_len = 1;
			}
			++i;
			continue ;
		}
		size_t	found = uri.find(path);
		std::cout << "uri.find(path) = " << found << std::endl;
		if (found == 0)
		{
			std::cout << "Match found\n";
			if (path.length() > best_match_len)
			{
				best_match = &loc;
				best_match_len = path.length();
				std::cout << "New best match: '" << path << "' (length: " << best_match_len << ")\n"; 
			}
		}
		else
			std::cout << "No match\n";
		++i;
	}
	std::cout << "Final match: '" << (best_match ? best_match->path : "NULL") << "'" << std::endl;
	return (best_match);
}

std::string	Router::buildPath(const Location &location, const std::string &uri)
{
	std::string	path = location.root;
	if (!path.empty() && path[path.size() - 1] != '/')
		path += '/';
	std::string loc_path = location.path;
	if (!loc_path.empty() && loc_path[0] == '/')
		loc_path = loc_path.substr(1);
	if (!loc_path.empty())
	{
		if (loc_path[loc_path.size() - 1] == '/')
			loc_path = loc_path.substr(0, loc_path.size() - 1);
		path += loc_path;
		path += "/";
	}
	std::string	file_part;
	if (uri.find(location.path) == 0)
		file_part = uri.substr(location.path.size());
	else
		file_part = uri;
	if (!file_part.empty())
		path += file_part;
	std::cout << "buildPath: '" << path << "'\n";
	return (path); 
}

bool	Router::isMethodAllowed(const Location &location, const std::string &method)
{
	size_t	i = 0;

	if (location.allowed_methods.empty())
		return (true);
	while (i < location.allowed_methods.size())
	{
		if (location.allowed_methods[i] == method)
			return (true);
		++i;
	}
	return (false);
}

std::string	Router::generateDirectoryListing(const std::string &path, const std::string &uri)
{
	DIR			*dir = opendir(path.c_str());
	std::string	html;

	if (!dir)
		return ("");
	html += "<html><head><title>Index of " + uri + "</title></head><body>";
	html += "<h1>Index of " + uri + "</h1><hr><ul>";

	struct dirent	*entry;
	while ((entry = readdir(dir)) != NULL)
	{
		std::string	name = entry->d_name;
		if (name == "." || name == "..")
			continue ;
		html += "<li><a href=\"" + uri;
		if (uri[uri.size() - 1] != '/')
			html += '/';
		html += name + "\">" + name + "</a></li>";
	}
	closedir(dir);
	return (html);
}

void	Router::_initMimeTypes(void)
{
	_mime_types[".html"] = "text/html";
	_mime_types[".htm"] = "text/html";
	_mime_types[".css"] = "text/css";
	_mime_types[".js"] = "application/javascript";
	_mime_types[".json"] = "application/json";
	_mime_types[".xml"] = "application/xml";
	_mime_types[".txt"] = "text/plain";
	_mime_types[".jpg"] = "image/jpeg";
	_mime_types[".jpeg"] = "image/jpeg";
	_mime_types[".png"] = "image/png";
	_mime_types[".gif"] = "image/gif";
	_mime_types[".svg"] = "image/svg+xml";
	_mime_types[".ico"] = "image/x-icon";
	_mime_types[".pdf"] = "application/pdf";
	_mime_types[".zip"] = "application/zip";
	_mime_types[".mp4"] = "video/mp4";
	_mime_types[".mp3"] = "audio/mpeg";
}

std::string	Router::getMimeType(const std::string &path)
{
	size_t	dot = path.rfind('.');
	if (dot == std::string::npos)
		return ("application/octet-stream");
	std::string	ext = path.substr(dot);
	std::map<std::string, std::string>::iterator	it = _mime_types.find(ext);
	if (it != _mime_types.end())
		return (it->second);
	return ("application/octet-stream");
}
