// Derived from public-domain/MIT-licensed code at
// https://github.com/open-source-parsers/jsoncpp. Thanks, Baptiste Lepilleur!

#ifndef _1feb388b3b7b4e3f92d1c3f399efebb8
#define _1feb388b3b7b4e3f92d1c3f399efebb8

#include <stdlib.h>
#include <sstream>

#if !defined(JSON_IS_AMALGAMATION)
#include "config.h"
#endif // if !defined(JSON_IS_AMALGAMATION)

/** It should not be possible for a maliciously designed file to
 *  cause an abort() or seg-fault, so these macros are used only
 *  for pre-condition violations and internal logic errors.
 */
#if JSON_USE_EXCEPTION

// @todo <= add detail about condition in exception
# define JSON_ASSERT(condition)                                                \
  {if (!(condition)) {json::throw_logic_error( "assert json failed" );}}

# define JSON_FAIL_MESSAGE(message)                                            \
  {                                                                            \
	std::ostringstream oss; oss << message;                                    \
	json::throw_logic_error(oss.str());                                          \
	abort();                                                                   \
  }

#else // JSON_USE_EXCEPTION

# define JSON_ASSERT(condition) assert(condition)

// The call to assert() will show the failure message in debug builds. In
// release builds we abort, for a core-dump or debugger.
# define JSON_FAIL_MESSAGE(message)                                            \
  {                                                                            \
	std::ostringstream oss; oss << message;                                    \
	assert(false && oss.str().c_str());                                        \
	abort();                                                                   \
  }


#endif

#define JSON_ASSERT_MESSAGE(condition, message)                                \
  if (!(condition)) {                                                          \
	JSON_FAIL_MESSAGE(message);                                                \
  }

#endif // sentry
