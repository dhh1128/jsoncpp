// Derived from public-domain/MIT-licensed code at
// https://github.com/open-source-parsers/jsoncpp. Thanks, Baptiste Lepilleur!

#pragma once

#include <cstdlib>
#include <sstream>

#include "config.h"

// @todo <= add detail about condition in exception
#define JSON_ASSERT(condition)                             \
	{                                                      \
		if (!(condition)) {                                \
			json::throw_logic_error("assert json failed"); \
		}                                                  \
	}

#define JSON_FAIL_MESSAGE(message)          \
	{                                       \
		std::ostringstream oss;             \
		oss << message;                     \
		json::throw_logic_error(oss.str()); \
		abort();                            \
	}

#define JSON_ASSERT_MESSAGE(condition, message) \
	if (!(condition)) {                         \
		JSON_FAIL_MESSAGE(message);             \
	}

