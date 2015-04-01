// Derived from public-domain/MIT-licensed code at
// https://github.com/open-source-parsers/jsoncpp. Thanks, Baptiste Lepilleur!

#ifndef _76c97a5fdc4e4f86a21a72493ef2ab89
#define _76c97a5fdc4e4f86a21a72493ef2ab89

#if !defined(JSON_IS_AMALGAMATION)
#include "value.h"
#endif // if !defined(JSON_IS_AMALGAMATION)
#include <vector>
#include <string>
#include <ostream>

// Disable warning C4251: <data member>: <type> needs to have dll-interface to
// be used by...
#if defined(JSONCPP_DISABLE_DLL_INTERFACE_WARNING)
#pragma warning(push)
#pragma warning(disable : 4251)
#endif // if defined(JSONCPP_DISABLE_DLL_INTERFACE_WARNING)

namespace json {

class value;

/**

Usage:
\code
  using namespace json;
  void writeToStdout(stream_writer::factory const& factory, value const& value) {
	std::unique_ptr<stream_writer> const writer(
	  factory.newStreamWriter());
	writer->write(value, &std::cout);
	std::cout << std::endl;  // add lf and flush
  }
\endcode
*/
class JSON_API stream_writer {
protected:
  std::ostream* sout_;  // not owned; will not delete
public:
  stream_writer();
  virtual ~stream_writer();
  /** Write value into document as configured in sub-class.
	  Do not take ownership of sout, but maintain a reference during function.
	  \pre sout != NULL
	  \return zero on success (For now, we always return zero, so check the stream instead.)
	  \throw std::exception possibly, depending on configuration
   */
  virtual int write(value const& root, std::ostream* sout) = 0;

  /** \brief A simple abstract factory.
   */
  class JSON_API factory {
  public:
	virtual ~factory();
	/** \brief Allocate a char_reader via operator new().
	 * \throw std::exception if something goes wrong (e.g. invalid settings)
	 */
	virtual stream_writer* newStreamWriter() const = 0;
  };  // factory
};  // stream_writer

/** \brief Write into stringstream, then return string, for convenience.
 * A stream_writer will be created from the factory, used, and then deleted.
 */
std::string JSON_API write_string(stream_writer::factory const& factory, value const& root);


/** \brief Build a stream_writer implementation.

Usage:
\code
  using namespace json;
  value value = ...;
  stream_writer_builder builder;
  builder["commentStyle"] = "None";
  builder["indentation"] = "   ";  // or whatever you like
  std::unique_ptr<json::stream_writer> writer(
	  builder.newStreamWriter());
  writer->write(value, &std::cout);
  std::cout << std::endl;  // add lf and flush
\endcode
*/
class JSON_API stream_writer_builder : public stream_writer::factory {
public:
  // Note: We use a json::value so that we can add data-members to this class
  // without a major version bump.
  /** Configuration of this builder.
	Available settings (case-sensitive):
	- "commentStyle": "None" or "All"
	- "indentation":  "<anything>"
	- "enableYAMLCompatibility": false or true
	  - slightly change the whitespace around colons
	- "dropNullPlaceholders": false or true
	  - Drop the "null" string from the writer's output for nullValues.
		Strictly speaking, this is not valid JSON. But when the output is being
		fed to a browser's Javascript, it makes for smaller output and the
		browser can handle the output just fine.

	You can examine 'settings_` yourself
	to see the defaults. You can also write and read them just like any
	JSON value.
	\sa setDefaults()
	*/
  json::value settings_;

  stream_writer_builder();
  virtual ~stream_writer_builder();

  /**
   * \throw std::exception if something goes wrong (e.g. invalid settings)
   */
  virtual stream_writer* newStreamWriter() const;

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
   * \snippet src/lib_json/json_writer.cpp StreamWriterBuilderDefaults
   */
  static void setDefaults(json::value* settings);
};

/** \brief Abstract class for writers.
 * \deprecated Use stream_writer. (And really, this is an implementation detail.)
 */
class JSON_API Writer {
public:
  virtual ~Writer();

  virtual std::string write(value const & root) = 0;
};

/** \brief Outputs a value in <a HREF="http://www.json.org">JSON</a> format
 *without formatting (not human friendly).
 *
 * The JSON document is written in a single line. It is not intended for 'human'
 *consumption,
 * but may be usefull to support feature such as RPC where bandwith is limited.
 * \sa reader, value
 * \deprecated Use stream_writer_builder.
 */
class JSON_API fast_writer : public Writer {

public:
  fast_writer();
  virtual ~fast_writer() {}

  void enableYAMLCompatibility();

  /** \brief Drop the "null" string from the writer's output for nullValues.
   * Strictly speaking, this is not valid JSON. But when the output is being
   * fed to a browser's Javascript, it makes for smaller output and the
   * browser can handle the output just fine.
   */
  void dropNullPlaceholders();

  void omitEndingLineFeed();

public: // overridden from Writer
  virtual std::string write(value const & root);

private:
  void writeValue(value const & value);

  std::string document_;
  bool yamlCompatiblityEnabled_;
  bool dropNullPlaceholders_;
  bool omitEndingLineFeed_;
};

/** \brief Writes a value in <a HREF="http://www.json.org">JSON</a> format in a
 *human friendly way.
 *
 * The rules for line break and indent are as follow:
 * - Object value:
 *     - if empty then print {} without indent and line break
 *     - if not empty the print '{', line break & indent, print one value per
 *line
 *       and then unindent and line break and print '}'.
 * - Array value:
 *     - if empty then print [] without indent and line break
 *     - if the array contains no object value, empty array or some other value
 *types,
 *       and all the values fit on one lines, then print the array on a single
 *line.
 *     - otherwise, it the values do not fit on one line, or the array contains
 *       object or non empty array, then print one value per line.
 *
 * If the value have comments then they are outputed according to their
 *#comment_placement.
 *
 * \sa reader, value, value::set_comment()
 * \deprecated Use stream_writer_builder.
 */
class JSON_API styled_writer : public Writer {
public:
  styled_writer();
  virtual ~styled_writer() {}

public: // overridden from Writer
  /** \brief Serialize a value in <a HREF="http://www.json.org">JSON</a> format.
   * \param root value to serialize.
   * \return String containing the JSON document that represents the root value.
   */
  virtual std::string write(value const & root);

private:
  void writeValue(value const & value);
  void writeArrayValue(value const & value);
  bool isMultineArray(value const & value);
  void pushValue(std::string const & value);
  void writeIndent();
  void writeWithIndent(std::string const & value);
  void indent();
  void unindent();
  void writeCommentBeforeValue(value const & root);
  void writeCommentAfterValueOnSameLine(value const & root);
  bool hasCommentForValue(value const & value);
  static std::string normalize_eol(std::string const & text);

  typedef std::vector<std::string> ChildValues;

  ChildValues childValues_;
  std::string document_;
  std::string indentString_;
  int rightMargin_;
  int indentSize_;
  bool addChildValues_;
};

/** \brief Writes a value in <a HREF="http://www.json.org">JSON</a> format in a
 human friendly way,
	 to a stream rather than to a string.
 *
 * The rules for line break and indent are as follow:
 * - Object value:
 *     - if empty then print {} without indent and line break
 *     - if not empty the print '{', line break & indent, print one value per
 line
 *       and then unindent and line break and print '}'.
 * - Array value:
 *     - if empty then print [] without indent and line break
 *     - if the array contains no object value, empty array or some other value
 types,
 *       and all the values fit on one lines, then print the array on a single
 line.
 *     - otherwise, it the values do not fit on one line, or the array contains
 *       object or non empty array, then print one value per line.
 *
 * If the value have comments then they are outputed according to their
 #comment_placement.
 *
 * \param indentation Each level will be indented by this amount extra.
 * \sa reader, value, value::set_comment()
 * \deprecated Use stream_writer_builder.
 */
class JSON_API styled_stream_writer {
public:
  styled_stream_writer(std::string indentation = "\t");
  ~styled_stream_writer() {}

public:
  /** \brief Serialize a value in <a HREF="http://www.json.org">JSON</a> format.
   * \param out Stream to write to. (Can be ostringstream, e.g.)
   * \param root value to serialize.
   * \note There is no point in deriving from Writer, since write() should not
   * return a value.
   */
  void write(std::ostream& out, value const & root);

private:
  void writeValue(value const & value);
  void writeArrayValue(value const & value);
  bool isMultineArray(value const & value);
  void pushValue(std::string const & value);
  void writeIndent();
  void writeWithIndent(std::string const & value);
  void indent();
  void unindent();
  void writeCommentBeforeValue(value const & root);
  void writeCommentAfterValueOnSameLine(value const & root);
  bool hasCommentForValue(value const & value);
  static std::string normalize_eol(std::string const & text);

  typedef std::vector<std::string> ChildValues;

  ChildValues childValues_;
  std::ostream* document_;
  std::string indentString_;
  int rightMargin_;
  std::string indentation_;
  bool addChildValues_ : 1;
  bool indented_ : 1;
};

#if defined(JSON_HAS_INT64)
std::string JSON_API valueToString(int32_t value);
std::string JSON_API valueToString(uint32_t value);
#endif // if defined(JSON_HAS_INT64)
std::string JSON_API valueToString(largest_int_t value);
std::string JSON_API valueToString(largest_uint_t value);
std::string JSON_API valueToString(double value);
std::string JSON_API valueToString(bool value);
std::string JSON_API valueToQuotedString(const char* value);

/// \brief Output using the styled_stream_writer.
/// \see json::operator>>()
JSON_API std::ostream& operator<<(std::ostream&, value const & root);

} // namespace json

#if defined(JSONCPP_DISABLE_DLL_INTERFACE_WARNING)
#pragma warning(pop)
#endif // if defined(JSONCPP_DISABLE_DLL_INTERFACE_WARNING)

#endif // JSON_WRITER_H_INCLUDED
