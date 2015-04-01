// Derived from public-domain/MIT-licensed code at
// https://github.com/open-source-parsers/jsoncpp. Thanks, Baptiste Lepilleur!

#ifndef _c2afc98584374718aea026a78911def9
#define _c2afc98584374718aea026a78911def9

#if !defined(JSON_IS_AMALGAMATION)
#include "features.h"
#include "value.h"
#endif // if !defined(JSON_IS_AMALGAMATION)
#include <deque>
#include <iosfwd>
#include <stack>
#include <string>
#include <istream>

// Disable warning C4251: <data member>: <type> needs to have dll-interface to
// be used by...
#if defined(JSONCPP_DISABLE_DLL_INTERFACE_WARNING)
#pragma warning(push)
#pragma warning(disable : 4251)
#endif // if defined(JSONCPP_DISABLE_DLL_INTERFACE_WARNING)

namespace json {

/** \brief Unserialize a <a HREF="http://www.json.org">JSON</a> document into a
 *value.
 *
 * \deprecated Use char_reader and char_reader_builder.
 */
class JSON_API reader {
public:
  typedef const char* location_t;

  /** \brief An error tagged with where in the JSON text it was encountered.
   *
   * The offsets give the [start, limit) range of bytes within the text. Note
   * that this is bytes, not codepoints.
   *
   */
  struct structured_error {
	size_t offset_start;
	size_t offset_limit;
	std::string message;
  };

  /** \brief Constructs a reader allowing all features
   * for parsing.
   */
  reader();

  /** \brief Constructs a reader allowing the specified feature set
   * for parsing.
   */
  reader(const features& features);

  /** \brief Read a value from a <a HREF="http://www.json.org">JSON</a>
   * document.
   * \param document UTF-8 encoded string containing the document to read.
   * \param root [out] Contains the root value of the document if it was
   *             successfully parsed.
   * \param collect_comments \c true to collect comment and allow writing them
   * back during
   *                        serialization, \c false to discard comments.
   *                        This parameter is ignored if
   * features::allow_comments_
   *                        is \c false.
   * \return \c true if the document was successfully parsed, \c false if an
   * error occurred.
   */
  bool
  parse(std::string const & document, value& root, bool collect_comments = true);

  /** \brief Read a value from a <a HREF="http://www.json.org">JSON</a>
   document.
   * \param begin_doc Pointer on the beginning of the UTF-8 encoded string of the
   document to read.
   * \param end_doc Pointer on the end of the UTF-8 encoded string of the
   document to read.
   *               Must be >= begin_doc.
   * \param root [out] Contains the root value of the document if it was
   *             successfully parsed.
   * \param collect_comments \c true to collect comment and allow writing them
   back during
   *                        serialization, \c false to discard comments.
   *                        This parameter is ignored if
   features::allow_comments_
   *                        is \c false.
   * \return \c true if the document was successfully parsed, \c false if an
   error occurred.
   */
  bool parse(const char* begin_doc,
			 const char* end_doc,
			 value& root,
			 bool collect_comments = true);

  /// \brief Parse from input stream.
  /// \see json::operator>>(std::istream&, json::value&).
  bool parse(std::istream& is, value& root, bool collect_comments = true);

  /** \brief Returns a user friendly string that list errors in the parsed
   * document.
   * \return Formatted error message with the list of errors with their location
   * in
   *         the parsed document. An empty string is returned if no error
   * occurred
   *         during parsing.
   */
  std::string get_formatted_messages() const;

  /** \brief Returns a vector of structured erros encounted while parsing.
   * \return A (possibly empty) vector of structured_error objects. Currently
   *         only one error can be returned, but the caller should tolerate
   * multiple
   *         errors.  This can occur if the parser recovers from a non-fatal
   *         parse error and then encounters additional errors.
   */
  std::vector<structured_error> get_structured_errors() const;

  /** \brief Add a semantic error message.
   * \param value JSON value location associated with the error
   * \param message The error message.
   * \return \c true if the error was successfully added, \c false if the
   * value offset exceeds the document size.
   */
  bool push_error(value const & value, std::string const & message);

  /** \brief Add a semantic error message with extra context.
   * \param value JSON value location associated with the error
   * \param message The error message.
   * \param extra Additional JSON value location to contextualize the error
   * \return \c true if the error was successfully added, \c false if either
   * value offset exceeds the document size.
   */
  bool push_error(value const & v, std::string const & message, value const & extra);

  /** \brief Return whether there are any errors.
   * \return \c true if there are no errors to report \c false if
   * errors have occurred.
   */
  bool good() const;

private:
  enum token_type {
	tt_end_of_stream = 0,
	tt_object_begin,
	tt_object_end,
	tt_array_begin,
	tt_array_end,
	tt_string,
	tt_number,
	tt_true,
	tt_false,
	tt_null,
	tt_array_separator,
	tt_member_separator,
	tt_comment,
	tt_error
  };

  class token {
  public:
	token_type type_;
	location_t start_;
	location_t end_;
  };

  class ErrorInfo {
  public:
	token token_;
	std::string message_;
	location_t extra_;
  };

  typedef std::deque<ErrorInfo> errors;

  bool read_token(token& token);
  void skip_spaces();
  bool match(location_t pattern, int pattern_length);
  bool read_comment();
  bool read_c_style_comment();
  bool read_cpp_style_comment();
  bool read_string();
  void read_number();
  bool read_value();
  bool read_object(token& token);
  bool read_array(token& token);
  bool decode_number(token& token);
  bool decode_number(token& token, value& decoded);
  bool decode_string(token& token);
  bool decode_string(token& token, std::string& decoded);
  bool decode_double(token& token);
  bool decode_double(token& token, value& decoded);
  bool decode_unicode_codepoint(token& token,
							  location_t& current,
							  location_t end,
							  unsigned int& unicode);
  bool decode_unicode_escape_sequence(token& token,
								   location_t& current,
								   location_t end,
								   unsigned int& unicode);
  bool add_error(std::string const & message, token& token, location_t extra = 0);
  bool recover_from_error(token_type skip_until_token);
  bool add_error_and_recover(std::string const & message,
						  token& token,
						  token_type skip_until_token);
  void skip_until_space();
  value& current_value();
  char get_next_char();
  void get_location_line_and_column(location_t, int& line, int& column) const;
  std::string get_location_line_and_column(location_t) const;
  void add_comment(location_t begin, location_t end, comment_placement placement);
  void skip_comment_tokens(token& token);

  typedef std::stack<value*> nodes;
  nodes nodes_;
  errors errors_;
  std::string document_;
  location_t begin_;
  location_t end_;
  location_t current_;
  location_t last_value_end_;
  value* last_value_;
  std::string comments_before_;
  features features_;
  bool collect_comments_;
};  // reader

/** Interface for reading JSON from a char array.
 */
class JSON_API char_reader {
public:
  virtual ~char_reader() {}
  /** \brief Read a value from a <a HREF="http://www.json.org">JSON</a>
   document.
   * The document must be a UTF-8 encoded string containing the document to read.
   *
   * \param begin_doc Pointer on the beginning of the UTF-8 encoded string of the
   document to read.
   * \param end_doc Pointer on the end of the UTF-8 encoded string of the
   document to read.
   *        Must be >= begin_doc.
   * \param root [out] Contains the root value of the document if it was
   *             successfully parsed.
   * \param errs [out] Formatted error messages (if not NULL)
   *        a user friendly string that lists errors in the parsed
   * document.
   * \return \c true if the document was successfully parsed, \c false if an
   error occurred.
   */
  virtual bool parse(
	  char const* begin_doc, char const* end_doc,
	  value* root, std::string* errs) = 0;

  class factory {
  public:
	virtual ~factory() {}
	/** \brief Allocate a char_reader via operator new().
	 * \throw std::exception if something goes wrong (e.g. invalid settings)
	 */
	virtual char_reader* newCharReader() const = 0;
  };  // factory
};  // char_reader

/** \brief Build a char_reader implementation.

Usage:
\code
  using namespace json;
  char_reader_builder builder;
  builder["collect_comments"] = false;
  value value;
  std::string errs;
  bool ok = parseFromStream(builder, std::cin, &value, &errs);
\endcode
*/
class JSON_API char_reader_builder : public char_reader::factory {
public:
  // Note: We use a json::value so that we can add data-members to this class
  // without a major version bump.
  /** Configuration of this builder.
	These are case-sensitive.
	Available settings (case-sensitive):
	- `"collect_comments": false or true`
	  - true to collect comment and allow writing them
		back during serialization, false to discard comments.
		This parameter is ignored if allowComments is false.
	- `"allowComments": false or true`
	  - true if comments are allowed.
	- `"strictRoot": false or true`
	  - true if root must be either an array or an object value
	- `"allowDroppedNullPlaceholders": false or true`
	  - true if dropped null placeholders are allowed. (See stream_writer_builder.)
	- `"allowNumericKeys": false or true`
	  - true if numeric object keys are allowed.
	- `"allowSingleQuotes": false or true`
	  - true if '' are allowed for strings (both keys and values)
	- `"stackLimit": integer`
	  - Exceeding stackLimit (recursive depth of `read_value()`) will
		cause an exception.
	  - This is a security issue (seg-faults caused by deeply nested JSON),
		so the default is low.
	- `"failIfExtra": false or true`
	  - If true, `parse()` returns false when extra non-whitespace trails
		the JSON value in the input string.
	- `"rejectDupKeys": false or true`
	  - If true, `parse()` returns false when a key is duplicated within an object.

	You can examine 'settings_` yourself
	to see the defaults. You can also write and read them just like any
	JSON value.
	\sa setDefaults()
	*/
  json::value settings_;

  char_reader_builder();
  virtual ~char_reader_builder();

  virtual char_reader* newCharReader() const;

  /** \return true if 'settings' are legal and consistent;
   *   otherwise, indicate bad settings via 'invalid'.
   */
  bool validate(json::value* invalid) const;

  /** A simple way to update a specific setting.
   */
  value& operator[](std::string key);

  /** Called by ctor, but you can use this to reset settings_.
   * \pre 'settings' != NULL (but json::null is fine)
   * \remark Defaults:
   * \snippet src/lib_json/json_reader.cpp CharReaderBuilderStrictMode
   */
  static void setDefaults(json::value* settings);
  /** Same as old features::strict_mode().
   * \pre 'settings' != NULL (but json::null is fine)
   * \remark Defaults:
   * \snippet src/lib_json/json_reader.cpp CharReaderBuilderDefaults
   */
  static void strict_mode(json::value* settings);
};

/** Consume entire stream and use its begin/end.
  * Someday we might have a real StreamReader, but for now this
  * is convenient.
  */
bool JSON_API parseFromStream(
	char_reader::factory const&,
	std::istream&,
	value* root, std::string* errs);

/** \brief Read from 'sin' into 'root'.

 Always keep comments from the input JSON.

 This can be used to read a file into a particular sub-object.
 For example:
 \code
 json::value root;
 cin >> root["dir"]["file"];
 cout << root;
 \endcode
 Result:
 \verbatim
 {
 "dir": {
	 "file": {
	 // The input stream JSON would be nested here.
	 }
 }
 }
 \endverbatim
 \throw std::exception on parse error.
 \see json::operator<<()
*/
JSON_API std::istream& operator>>(std::istream&, value&);

} // namespace json

#if defined(JSONCPP_DISABLE_DLL_INTERFACE_WARNING)
#pragma warning(pop)
#endif // if defined(JSONCPP_DISABLE_DLL_INTERFACE_WARNING)

#endif // CPPTL_JSON_READER_H_INCLUDED
