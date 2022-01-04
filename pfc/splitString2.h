#pragma once
#include <list>
namespace pfc {
	std::list<pfc::string8> splitString2(const char* str, const char* delim);
	std::list<pfc::string8> splitStringByLines2(const char* str);
}