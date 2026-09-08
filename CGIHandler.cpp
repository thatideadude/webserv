#include "webserv.hpp"

CGIHandler::CGIHandler(void) : _pid(-1), _input_fd(-1), _output_fd(-1), _start_time(0), _state(CGI_NOT_STARTED), _body_bytes_written(0), _status_code(200)
{
	std::cout << "CGIHandler default constructor called\n";
}

CGIHandler::CGIHandler(const CGIHandler &other)
{
	*this = other;
	std::cout << "CGIHandler copy constructor called\n";
}

CGIHandler	&CGIHandler::operator=(const CGIHandler &other)
{
	(void) other; // copy all variables one by one
	return (*this);
}

CGIHandler::~CGIHandler(void)
{
	_closeInput();
	_closeOutput();
	std::cout << "CGI Handler destructor called\n";
}

void	CGIHandler::_closeInput(void)
{
	if (_input_fd >= 0)
		close(_input_fd);
	_input_fd = -1;
}

void	CGIHandler::_closeOutput(void)
{
	if (_output_fd >= 0)
		close(_output_fd);
	_output_fd = -1;
}

bool	CGIHandler::start(const Request &request, const Location &location, const std::string &script_path, const std::string &uri,
		const std::string &server_name, int server_port, const std::string &interpreter)
{
	int	in_pipe[2];
	int	out_pipe[2];
 
	if (pipe(in_pipe) < 0 || pipe(out_pipe) < 0)
	{
		std::cerr << "CGI: pipe() failed: " << strerror(errno) << std::endl;
		return (false);
	}
 
	_request_body = request.getBody();
	_pid = fork();
	if (_pid < 0)
	{
		std::cerr << "CGI: fork() failed: " << strerror(errno) << std::endl;
		close(in_pipe[0]); close(in_pipe[1]);
		close(out_pipe[0]); close(out_pipe[1]);
		return (false);
	}
 
	if (_pid == 0)
	{
		// ---- child ----
		dup2(in_pipe[0], STDIN_FILENO);
		dup2(out_pipe[1], STDOUT_FILENO);
		close(in_pipe[0]); close(in_pipe[1]);
		close(out_pipe[0]); close(out_pipe[1]);
 
		char	**env = _buildEnv(request, location, script_path, uri, server_name, server_port);
		char	**argv = _buildArgv(interpreter, script_path);
 
		execve(argv[0], argv, env);
		// only reached if execve fails
		std::cerr << "CGI: execve failed: " << strerror(errno) << std::endl;
		_exit(1);
	}
 
	// ---- parent ----
	close(in_pipe[0]);
	close(out_pipe[1]);
	_input_fd = in_pipe[1];
	_output_fd = out_pipe[0];
 
	int	flags_in = fcntl(_input_fd, F_GETFL, 0);
	fcntl(_input_fd, F_SETFL, flags_in | O_NONBLOCK);
	int	flags_out = fcntl(_output_fd, F_GETFL, 0);
	fcntl(_output_fd, F_SETFL, flags_out | O_NONBLOCK);
 
	_start_time = time(NULL);
	if (_request_body.empty())
	{
		_closeInput();
		_state = CGI_READING_OUTPUT;
	}
	else
		_state = CGI_WRITING_BODY;
	return (true);
}

bool	CGIHandler::writeToInput(void)
{
	if (_state != CGI_WRITING_BODY || _input_fd < 0)
		return (true);
 
	size_t	remaining = _request_body.size() - _body_bytes_written;
	ssize_t	written = write(_input_fd, _request_body.c_str() + _body_bytes_written, remaining);
 
	if (written < 0)
	{
		if (errno == EAGAIN || errno == EWOULDBLOCK)
			return (false);
		_closeInput();
		_state = CGI_ERROR;
		return (true);
	}
	_body_bytes_written += written;
	if (_body_bytes_written >= _request_body.size())
	{
		_closeInput();
		_state = CGI_READING_OUTPUT;
		return (true);
	}
	return (false);
}

bool	CGIHandler::readFromOutput(void)
{
	char	buffer[4096];
	ssize_t	bytes = read(_output_fd, buffer, sizeof(buffer));
 
	if (bytes > 0)
	{
		_output_buffer.append(buffer, bytes);
		return (false);
	}
	if (bytes == 0)
	{
		_closeOutput();
		finalize();
		return (true);
	}
	if (errno == EAGAIN || errno == EWOULDBLOCK)
		return (false);
	_closeOutput();
	_state = CGI_ERROR;
	return (true);
}

bool	CGIHandler::finalize(void)
{
	int	status = 0;
 
	if (_pid > 0)
		waitpid(_pid, &status, 0);
	if (_state != CGI_ERROR)
	{
		_parseOutput();
		_state = CGI_DONE;
	}
	return (true);
}
 
void	CGIHandler::kill(void)
{
	if (_pid > 0)
	{
		::kill(_pid, SIGKILL);
		waitpid(_pid, NULL, 0);
	}
	_closeInput();
	_closeOutput();
	_state = CGI_TIMEOUT;
}

bool	CGIHandler::hasTimeOut(void)
{
	return (_state != CGI_DONE && _state != CGI_ERROR &&
		(time(NULL) - _start_time) > CGI_TIMEOUT_SECONDS);
}

int	CGIHandler::getInputFd(void) const
{
	return (_input_fd);
}

int	CGIHandler::getOutputFd(void) const
{
	return (_output_fd);
}

CGIState	CGIHandler::getState(void) const
{
	return (_state);
}

const std::string	&CGIHandler::getRawOutput(void) const
{
	return (_output_buffer);
}

const std::string	CGIHandler::getParsedBody(void) const
{
	return (_parsed_body);
}

std::map<std::string, std::string>	CGIHandler::getParsedHeaders(void) const
{
	return (_parsed_headers);
}

int	CGIHandler::getStatusCode(void) const
{
	return (_status_code);
}

void	CGIHandler::_parseOutput(void)
{
	size_t	header_end = _output_buffer.find("\r\n\r\n");
	size_t	sep_len = 4;
 
	if (header_end == std::string::npos)
	{
		header_end = _output_buffer.find("\n\n");
		sep_len = 2;
	}
	if (header_end == std::string::npos)
	{
		_parsed_body = _output_buffer;
		_status_code = 200;
		return ;
	}
 
	std::string	header_part = _output_buffer.substr(0, header_end);
	_parsed_body = _output_buffer.substr(header_end + sep_len);
 
	std::istringstream	stream(header_part);
	std::string			line;
 
	_status_code = 200;
	while (std::getline(stream, line))
	{
		if (!line.empty() && line[line.size() - 1] == '\r')
			line.erase(line.size() - 1);
		if (line.empty())
			continue ;
		size_t	colon = line.find(':');
		if (colon == std::string::npos)
			continue ;
		std::string	key = line.substr(0, colon);
		std::string	value = line.substr(colon + 1);
		size_t		start = value.find_first_not_of(" \t");
		if (start != std::string::npos)
			value = value.substr(start);
 
		if (key == "Status")
			_status_code = std::atoi(value.c_str());
		else
			_parsed_headers[key] = value;
	}
}

char	**CGIHandler::_buildArgv(const std::string &interpreter, const std::string &script_path)
{
	char	**argv;
 
	if (!interpreter.empty())
	{
		argv = new char*[3];
		argv[0] = _makeString(interpreter.c_str());
		argv[1] = _makeString(script_path.c_str());
		argv[2] = NULL;
	}
	else
	{
		argv = new char*[2];
		argv[0] = _makeString(script_path.c_str());
		argv[1] = NULL;
	}
	return (argv);
}

char	**CGIHandler::_buildEnv(const Request &request, const Location &location,
	const std::string &script_path, const std::string &uri,
	const std::string &server_name, int server_port)
{
	std::vector<std::string>	env_vec;
	std::string					query_string;
	size_t						qmark = uri.find('?');
 
	if (qmark != std::string::npos)
		query_string = uri.substr(qmark + 1);
 
	env_vec.push_back("REQUEST_METHOD=" + request.getMethod());
	env_vec.push_back("QUERY_STRING=" + query_string);
	env_vec.push_back("CONTENT_LENGTH=" + Parser::toString(request.getBody().size()));
	env_vec.push_back("CONTENT_TYPE=" + request.getHeader("Content-Type"));
	env_vec.push_back("SCRIPT_NAME=" + (qmark != std::string::npos ? uri.substr(0, qmark) : uri));
	env_vec.push_back("SCRIPT_FILENAME=" + script_path);
	env_vec.push_back("PATH_INFO=");
	env_vec.push_back("SERVER_PROTOCOL=HTTP/1.1");
	env_vec.push_back("SERVER_NAME=" + server_name);
	env_vec.push_back("SERVER_PORT=" + Parser::toString(server_port));
	env_vec.push_back("SERVER_SOFTWARE=webserv/1.0");
	env_vec.push_back("GATEWAY_INTERFACE=CGI/1.1");
	env_vec.push_back("REDIRECT_STATUS=200");
	env_vec.push_back("REMOTE_ADDR=127.0.0.1");
	(void)location;
 
	const std::map<std::string, std::string>	&headers = request.getHeaders();
	std::map<std::string, std::string>::const_iterator it = headers.begin();
	while (it != headers.end())
	{
		std::string	key = "HTTP_" + it->first;
		size_t		i = 0;
		while (i < key.size())
		{
			if (key[i] == '-')
				key[i] = '_';
			else
				key[i] = std::toupper(key[i]);
			++i;
		}
		env_vec.push_back(key + "=" + it->second);
		++it;
	}
 
	char	**env = new char*[env_vec.size() + 1];
	size_t	i = 0;
	while (i < env_vec.size())
	{
		env[i] = _makeString(env_vec[i].c_str());
		++i;
	}
	env[i] = NULL;
	return (env);
}

void	CGIHandler::_freeArray(char **arr)
{
	if (!arr)
		return ;
	size_t	i = 0;
	while (arr[i])
		free(arr[i++]);
	delete[] arr;
}

char	*CGIHandler::_makeString(const std::string str)
{
	char					*ret = new char[str.length() + 1];
	size_t					i = 0;
	while (i < str.length())
	{
		ret[i] = str.at(i);
		++i;
	}
	ret[i] = 0;
	return (ret);
}
