// Derived from public-domain/MIT-licensed code at
// https://github.com/open-source-parsers/jsoncpp. Thanks, Baptiste Lepilleur!

#ifndef _7337e4f3cf6d40a7b164211ffa36a7c3
#define _7337e4f3cf6d40a7b164211ffa36a7c3

#include <cstdint>

// If non-zero, the library uses exceptions to report bad input instead of C
// assertion macros. The default is to use exceptions.
#ifndef JSON_USE_EXCEPTION
#define JSON_USE_EXCEPTION 1
#endif

/// If defined, indicates that the source file is amalgated
/// to prevent private header inclusion.
/// Remarks: it is automatically defined in the generated amalgated header.
// #define JSON_IS_AMALGAMATION

#if defined(JSON_DLL_BUILD)
	#if defined(_MSC_VER)
		#define JSON_API __declspec(dllexport)
		#define JSONCPP_DISABLE_DLL_INTERFACE_WARNING
	#endif // if defined(_MSC_VER)
#elif defined(JSON_DLL)
	#if defined(_MSC_VER)
		#define JSON_API __declspec(dllimport)
		#define JSONCPP_DISABLE_DLL_INTERFACE_WARNING
	#endif // if defined(_MSC_VER)
#endif // ifdef JSON_IN_CPPTL
#if !defined(JSON_API)
	#define JSON_API
#endif

// If JSON_NO_INT64 is defined, then json only support C++ "int" type for
// integer
// Storages, and 64 bits integer support is disabled.
// #define JSON_NO_INT64 1

#if defined(_MSC_VER) && _MSC_VER <= 1200 // MSVC 6
// Microsoft Visual Studio 6 only support conversion from __int64 to double
// (no conversion from unsigned __int64).
#define JSON_USE_INT64_DOUBLE_CONVERSION 1
// Disable warning 4786 for VS6 caused by STL (identifier was truncated to '255'
// characters in the debug information)
// All projects I've ever seen with VS6 were using this globally (not bothering
// with pragma push/pop).
#pragma warning(disable : 4786)
#endif // if defined(_MSC_VER)  &&  _MSC_VER < 1200 // MSVC 6

#if defined(_MSC_VER) && _MSC_VER >= 1500 // MSVC 2008
/// Indicates that the following function is deprecated.
#define JSONCPP_DEPRECATED(message) __declspec(deprecated(message))
#elif defined(__clang__) && defined(__has_feature)
#if __has_feature(attribute_deprecated_with_message)
#define JSONCPP_DEPRECATED(message)  __attribute__ ((deprecated(message)))
#endif
#elif defined(__GNUC__) &&  (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 5))
#define JSONCPP_DEPRECATED(message)  __attribute__ ((deprecated(message)))
#elif defined(__GNUC__) &&  (__GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 1))
#define JSONCPP_DEPRECATED(message)  __attribute__((__deprecated__))
#endif

#if !defined(JSONCPP_DEPRECATED)
#define JSONCPP_DEPRECATED(message)
#endif // if !defined(JSONCPP_DEPRECATED)

namespace json {
#if defined(JSON_NO_INT64)
typedef int32_t largest_int_t;
typedef uint32_t largest_uint_t;
#undef JSON_HAS_INT64
#else
typedef int64_t largest_int_t;
typedef uint64_t largest_uint_t;
#define JSON_HAS_INT64
#endif
} // end namespace json

#endif // sentry
