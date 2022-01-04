#include "pfc-lite.h"
#include "pfc.h"
#include "splitString.h"
#include "splitString2.h"

namespace {
	class wrapper_t {
	public:
		std::list<pfc::string8> * ret;

		inline void operator+=(pfc::string_part_ref const & p) {
			ret->emplace_back(p);
		}
	};
}

namespace pfc {
	
	std::list<pfc::string8> splitString2(const char* str, const char* delim) {
		std::list<pfc::string8> ret;
		wrapper_t w; w.ret = &ret;
		pfc::splitStringBySubstring(w, str, delim);
		return ret;
	}
	std::list<pfc::string8> splitStringByLines2(const char* str) {
		std::list<pfc::string8> ret;
		wrapper_t w; w.ret = &ret;
		pfc::splitStringByLines(w, str);
		return ret;
	}
}