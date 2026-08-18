#pragma once
#include "webserv.hpp"

struct	Location;
class	Server;

class	Router
{
	public:
		Router(void);
		Router(const Router &other);
		Router &operator=(const Router &other);
		~Router(void);

		const Location	*findLocation(const Server &server, const std::string &uri);
		std::string		buildPath(const Location &location, const std::string &uri);
		bool			isMethodAllowed(const Location &location, const std::string &method);
		bool			isDirectory(const std::string &path);
		std::string		generateDirectoryListing(const std::string &path, const std::string &uri);
		std::string		getMimeType(const std::string &path);
	private:
		std::map<std::string, std::string>	_mime_types;
		void								_initMimeTypes(void);
};
