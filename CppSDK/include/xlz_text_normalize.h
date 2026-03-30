#pragma once

#include <string>

namespace xlz
{
	inline std::string NormalizeNewlinesToLf(const char* s)
	{
		const char* p = s ? s : "";
		std::string out;
		out.reserve(std::char_traits<char>::length(p));
		for (size_t i = 0; p[i] != '\0'; ++i)
		{
			const char c = p[i];
			if (c == '\r')
			{
				if (p[i + 1] == '\n')
				{
					++i;
				}
				out.push_back('\n');
				continue;
			}
			if (c == '\n')
			{
				out.push_back('\n');
				continue;
			}
			out.push_back(c);
		}
		return out;
	}
}

