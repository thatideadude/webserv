#include <iostream>
#include <string>
#include <vector>

std::vector<std::string>	string_split(std::string str)
{
	std::vector<std::string>	strings;
	int							i, j;
	const char					*s = str.c_str();

	i = 0;
	while (s[i])
	{
		while (s[i] && isspace(s[i]))
			++i;
		if (!s[i])
			break ;
		j = 0;
		while (s[i + j] && !isspace(s[i + j]))
			++j;
		strings.push_back(str.substr(i, j));
		i += j;
	}
	return (strings);
}
