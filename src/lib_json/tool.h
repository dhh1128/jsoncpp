// Derived from public-domain/MIT-licensed code at
// https://github.com/open-source-parsers/jsoncpp. Thanks, Baptiste Lepilleur!

#pragma once

/* This header provides common string manipulation support, such as UTF-8,
 * portable conversion from/to string...
 *
 * It is an internal header that must not be exposed.
 */

namespace json {

/// Converts a unicode code-point to UTF-8.
static inline std::string codepoint_to_utf8(unsigned int cp)
{
	std::string result;

	// based on description from http://en.wikipedia.org/wiki/UTF-8

	if (cp <= 0x7f) {
		result.resize(1);
		result[0] = static_cast<char>(cp);
	}
	else if (cp <= 0x7FF) {
		result.resize(2);
		result[1] = static_cast<char>(0x80 | (0x3f & cp));
		result[0] = static_cast<char>(0xC0 | (0x1f & (cp >> 6)));
	}
	else if (cp <= 0xFFFF) {
		result.resize(3);
		result[2] = static_cast<char>(0x80 | (0x3f & cp));
		result[1] = 0x80 | static_cast<char>((0x3f & (cp >> 6)));
		result[0] = 0xE0 | static_cast<char>((0xf & (cp >> 12)));
	}
	else if (cp <= 0x10FFFF) {
		result.resize(4);
		result[3] = static_cast<char>(0x80 | (0x3f & cp));
		result[2] = static_cast<char>(0x80 | (0x3f & (cp >> 6)));
		result[1] = static_cast<char>(0x80 | (0x3f & (cp >> 12)));
		result[0] = static_cast<char>(0xF0 | (0x7 & (cp >> 18)));
	}

	return result;
}

/// Returns true if ch is a control character (in range [0,32[).
static inline bool is_control_char(char ch) { return ch > 0 && ch <= 0x1F; }

enum {
	/// Constant that specify the size of the buffer that must be passed to
	/// uint_to_string.
	uint_to_string_buffer_size = 3 * sizeof(largest_uint_t) + 1
};

// Defines a char buffer for use with uint_to_string().
typedef char uint_to_string_buffer[uint_to_string_buffer_size];

/** Converts an unsigned integer to string.
 * @param value Unsigned interger to convert to string
 * @param current Input/Output string buffer.
 *        Must have at least uint_to_string_buffer_size chars free.
 */
static inline void uint_to_string(largest_uint_t value, char*& current)
{
	*--current = 0;
	do {
		*--current = char(value % 10) + '0';
		value /= 10;
	} while (value != 0);
}

/** Change ',' to '.' everywhere in buffer.
 *
 * We had a sophisticated way, but it did not work in WinCE.
 * @see https://github.com/open-source-parsers/jsoncpp/pull/9
 */
static inline void fix_numeric_locale(char* begin, char* end)
{
	while (begin < end) {
		if (*begin == ',') {
			*begin = '.';
		}
		++begin;
	}
}

} // namespace json {

