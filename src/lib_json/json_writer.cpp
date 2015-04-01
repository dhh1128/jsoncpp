// Derived from public-domain/MIT-licensed code at
// https://github.com/open-source-parsers/jsoncpp. Thanks, Baptiste Lepilleur!

#include <json/writer.h>
#include "json_tool.h"
#include <iomanip>
#include <memory>
#include <sstream>
#include <utility>
#include <set>
#include <cassert>
#include <cstring>
#include <cstdio>

#if defined(_MSC_VER) && _MSC_VER >= 1200 && _MSC_VER < 1800 // Between VC++ 6.0 and VC++ 11.0
#include <float.h>
#define isfinite _finite
#elif defined(__sun) && defined(__SVR4) //Solaris
#include <ieeefp.h>
#define isfinite finite
#else
#include <cmath>
#define isfinite std::isfinite
#endif

#if defined(_MSC_VER) && _MSC_VER < 1500 // VC++ 8.0 and below
#define snprintf _snprintf
#elif __cplusplus >= 201103L
#define snprintf std::snprintf
#endif

#if defined(_MSC_VER) && _MSC_VER >= 1400 // VC++ 8.0
// Disable warning about strdup being deprecated.
#pragma warning(disable : 4996)
#endif

namespace json {

#if __cplusplus >= 201103L
typedef std::unique_ptr<stream_writer> StreamWriterPtr;
#else
typedef std::auto_ptr<stream_writer>   StreamWriterPtr;
#endif

static bool containsControlCharacter(const char* str) {
  while (*str) {
    if (isControlCharacter(*(str++)))
      return true;
  }
  return false;
}

static bool containsControlCharacter0(const char* str, unsigned len) {
  char const* end = str + len;
  while (end != str) {
    if (isControlCharacter(*str) || 0==*str)
      return true;
    ++str;
  }
  return false;
}

std::string valueToString(largest_int_t value) {
  UIntToStringBuffer buffer;
  char* current = buffer + sizeof(buffer);
  bool isNegative = value < 0;
  if (isNegative)
    value = -value;
  uintToString(largest_uint_t(value), current);
  if (isNegative)
    *--current = '-';
  assert(current >= buffer);
  return current;
}

std::string valueToString(largest_uint_t value) {
  UIntToStringBuffer buffer;
  char* current = buffer + sizeof(buffer);
  uintToString(value, current);
  assert(current >= buffer);
  return current;
}

#if defined(JSON_HAS_INT64)

std::string valueToString(int32_t value) {
  return valueToString(largest_int_t(value));
}

std::string valueToString(uint32_t value) {
  return valueToString(largest_uint_t(value));
}

#endif // # if defined(JSON_HAS_INT64)

std::string valueToString(double value) {
  // Allocate a buffer that is more than large enough to store the 16 digits of
  // precision requested below.
  char buffer[32];
  int len = -1;

// Print into the buffer. We need not request the alternative representation
// that always has a decimal point because JSON doesn't distingish the
// concepts of reals and integers.
#if defined(_MSC_VER) && defined(__STDC_SECURE_LIB__) // Use secure version with
                                                      // visual studio 2005 to
                                                      // avoid warning.
#if defined(WINCE)
  len = _snprintf(buffer, sizeof(buffer), "%.17g", value);
#else
  len = sprintf_s(buffer, sizeof(buffer), "%.17g", value);
#endif
#else
  if (isfinite(value)) {
    len = snprintf(buffer, sizeof(buffer), "%.17g", value);
  } else {
    // IEEE standard states that NaN values will not compare to themselves
    if (value != value) {
      len = snprintf(buffer, sizeof(buffer), "null");
    } else if (value < 0) {
      len = snprintf(buffer, sizeof(buffer), "-1e+9999");
    } else {
      len = snprintf(buffer, sizeof(buffer), "1e+9999");
    }
    // For those, we do not need to call fixNumLoc, but it is fast.
  }
#endif
  assert(len >= 0);
  fixNumericLocale(buffer, buffer + len);
  return buffer;
}

std::string valueToString(bool value) { return value ? "true" : "false"; }

std::string valueToQuotedString(const char* value) {
  if (value == NULL)
    return "";
  // Not sure how to handle unicode...
  if (strpbrk(value, "\"\\\b\f\n\r\t") == NULL &&
      !containsControlCharacter(value))
    return std::string("\"") + value + "\"";
  // We have to walk value and escape any special characters.
  // Appending to std::string is not efficient, but this should be rare.
  // (Note: forward slashes are *not* rare, but I am not escaping them.)
  std::string::size_type maxsize =
      strlen(value) * 2 + 3; // allescaped+quotes+NULL
  std::string result;
  result.reserve(maxsize); // to avoid lots of mallocs
  result += "\"";
  for (const char* c = value; *c != 0; ++c) {
    switch (*c) {
    case '\"':
      result += "\\\"";
      break;
    case '\\':
      result += "\\\\";
      break;
    case '\b':
      result += "\\b";
      break;
    case '\f':
      result += "\\f";
      break;
    case '\n':
      result += "\\n";
      break;
    case '\r':
      result += "\\r";
      break;
    case '\t':
      result += "\\t";
      break;
    // case '/':
    // Even though \/ is considered a legal escape in JSON, a bare
    // slash is also legal, so I see no reason to escape it.
    // (I hope I am not misunderstanding something.
    // blep notes: actually escaping \/ may be useful in javascript to avoid </
    // sequence.
    // Should add a flag to allow this compatibility mode and prevent this
    // sequence from occurring.
    default:
      if (isControlCharacter(*c)) {
        std::ostringstream oss;
        oss << "\\u" << std::hex << std::uppercase << std::setfill('0')
            << std::setw(4) << static_cast<int>(*c);
        result += oss.str();
      } else {
        result += *c;
      }
      break;
    }
  }
  result += "\"";
  return result;
}

// https://github.com/upcaste/upcaste/blob/master/src/upcore/src/cstring/strnpbrk.cpp
static char const* strnpbrk(char const* s, char const* accept, size_t n) {
  assert((s || !n) && accept);

  char const* const end = s + n;
  for (char const* cur = s; cur < end; ++cur) {
    int const c = *cur;
    for (char const* a = accept; *a; ++a) {
      if (*a == c) {
        return cur;
      }
    }
  }
  return NULL;
}
static std::string valueToQuotedStringN(const char* value, unsigned length) {
  if (value == NULL)
    return "";
  // Not sure how to handle unicode...
  if (strnpbrk(value, "\"\\\b\f\n\r\t", length) == NULL &&
      !containsControlCharacter0(value, length))
    return std::string("\"") + value + "\"";
  // We have to walk value and escape any special characters.
  // Appending to std::string is not efficient, but this should be rare.
  // (Note: forward slashes are *not* rare, but I am not escaping them.)
  std::string::size_type maxsize =
      length * 2 + 3; // allescaped+quotes+NULL
  std::string result;
  result.reserve(maxsize); // to avoid lots of mallocs
  result += "\"";
  char const* end = value + length;
  for (const char* c = value; c != end; ++c) {
    switch (*c) {
    case '\"':
      result += "\\\"";
      break;
    case '\\':
      result += "\\\\";
      break;
    case '\b':
      result += "\\b";
      break;
    case '\f':
      result += "\\f";
      break;
    case '\n':
      result += "\\n";
      break;
    case '\r':
      result += "\\r";
      break;
    case '\t':
      result += "\\t";
      break;
    // case '/':
    // Even though \/ is considered a legal escape in JSON, a bare
    // slash is also legal, so I see no reason to escape it.
    // (I hope I am not misunderstanding something.)
    // blep notes: actually escaping \/ may be useful in javascript to avoid </
    // sequence.
    // Should add a flag to allow this compatibility mode and prevent this
    // sequence from occurring.
    default:
      if ((isControlCharacter(*c)) || (*c == 0)) {
        std::ostringstream oss;
        oss << "\\u" << std::hex << std::uppercase << std::setfill('0')
            << std::setw(4) << static_cast<int>(*c);
        result += oss.str();
      } else {
        result += *c;
      }
      break;
    }
  }
  result += "\"";
  return result;
}

// Class Writer
// //////////////////////////////////////////////////////////////////
Writer::~Writer() {}

// Class fast_writer
// //////////////////////////////////////////////////////////////////

fast_writer::fast_writer()
    : yamlCompatiblityEnabled_(false), dropNullPlaceholders_(false),
      omitEndingLineFeed_(false) {}

void fast_writer::enableYAMLCompatibility() { yamlCompatiblityEnabled_ = true; }

void fast_writer::dropNullPlaceholders() { dropNullPlaceholders_ = true; }

void fast_writer::omitEndingLineFeed() { omitEndingLineFeed_ = true; }

std::string fast_writer::write(value const & root) {
  document_ = "";
  writeValue(root);
  if (!omitEndingLineFeed_)
    document_ += "\n";
  return document_;
}

void fast_writer::writeValue(value const & value) {
  switch (value.type()) {
  case vt_null:
    if (!dropNullPlaceholders_)
      document_ += "null";
    break;
  case vt_int:
    document_ += valueToString(value.as_largest_int());
    break;
  case vt_uint:
    document_ += valueToString(value.as_largest_uint());
    break;
  case vt_real:
    document_ += valueToString(value.as_double());
    break;
  case vt_string:
    document_ += valueToQuotedString(value.as_cstring());
    break;
  case vt_bool:
    document_ += valueToString(value.as_bool());
    break;
  case vt_array: {
    document_ += '[';
    int size = value.size();
    for (int index = 0; index < size; ++index) {
      if (index > 0)
        document_ += ',';
      writeValue(value[index]);
    }
    document_ += ']';
  } break;
  case vt_object: {
    value::Members members(value.get_member_names());
    document_ += '{';
    for (value::Members::iterator it = members.begin(); it != members.end();
         ++it) {
      std::string const & name = *it;
      if (it != members.begin())
        document_ += ',';
      document_ += valueToQuotedStringN(name.data(), name.length());
      document_ += yamlCompatiblityEnabled_ ? ": " : ":";
      writeValue(value[name]);
    }
    document_ += '}';
  } break;
  }
}

// Class styled_writer
// //////////////////////////////////////////////////////////////////

styled_writer::styled_writer()
    : rightMargin_(74), indentSize_(3), addChildValues_() {}

std::string styled_writer::write(value const & root) {
  document_ = "";
  addChildValues_ = false;
  indentString_ = "";
  writeCommentBeforeValue(root);
  writeValue(root);
  writeCommentAfterValueOnSameLine(root);
  document_ += "\n";
  return document_;
}

void styled_writer::writeValue(value const & value) {
  switch (value.type()) {
  case vt_null:
    pushValue("null");
    break;
  case vt_int:
    pushValue(valueToString(value.as_largest_int()));
    break;
  case vt_uint:
    pushValue(valueToString(value.as_largest_uint()));
    break;
  case vt_real:
    pushValue(valueToString(value.as_double()));
    break;
  case vt_string:
  {
    // Is NULL is possible for value.string_?
    char const* str;
    char const* end;
    bool ok = value.get_string(&str, &end);
    if (ok) pushValue(valueToQuotedStringN(str, static_cast<unsigned>(end-str)));
    else pushValue("");
    break;
  }
  case vt_bool:
    pushValue(valueToString(value.as_bool()));
    break;
  case vt_array:
    writeArrayValue(value);
    break;
  case vt_object: {
    value::Members members(value.get_member_names());
    if (members.empty())
      pushValue("{}");
    else {
      writeWithIndent("{");
      indent();
      value::Members::iterator it = members.begin();
      for (;;) {
        std::string const & name = *it;
        class value const & childValue = value[name];
        writeCommentBeforeValue(childValue);
        writeWithIndent(valueToQuotedString(name.c_str()));
        document_ += " : ";
        writeValue(childValue);
        if (++it == members.end()) {
          writeCommentAfterValueOnSameLine(childValue);
          break;
        }
        document_ += ',';
        writeCommentAfterValueOnSameLine(childValue);
      }
      unindent();
      writeWithIndent("}");
    }
  } break;
  }
}

void styled_writer::writeArrayValue(value const & value) {
  unsigned size = value.size();
  if (size == 0)
    pushValue("[]");
  else {
    bool isArrayMultiLine = isMultineArray(value);
    if (isArrayMultiLine) {
      writeWithIndent("[");
      indent();
      bool hasChildValue = !childValues_.empty();
      unsigned index = 0;
      for (;;) {
        class value const & childValue = value[index];
        writeCommentBeforeValue(childValue);
        if (hasChildValue)
          writeWithIndent(childValues_[index]);
        else {
          writeIndent();
          writeValue(childValue);
        }
        if (++index == size) {
          writeCommentAfterValueOnSameLine(childValue);
          break;
        }
        document_ += ',';
        writeCommentAfterValueOnSameLine(childValue);
      }
      unindent();
      writeWithIndent("]");
    } else // output on a single line
    {
      assert(childValues_.size() == size);
      document_ += "[ ";
      for (unsigned index = 0; index < size; ++index) {
        if (index > 0)
          document_ += ", ";
        document_ += childValues_[index];
      }
      document_ += " ]";
    }
  }
}

bool styled_writer::isMultineArray(value const & value) {
  int size = value.size();
  bool isMultiLine = size * 3 >= rightMargin_;
  childValues_.clear();
  for (int index = 0; index < size && !isMultiLine; ++index) {
    class value const & childValue = value[index];
    isMultiLine =
        isMultiLine || ((childValue.is_array() || childValue.is_object()) &&
                        childValue.size() > 0);
  }
  if (!isMultiLine) // check if line length > max line length
  {
    childValues_.reserve(size);
    addChildValues_ = true;
    int lineLength = 4 + (size - 1) * 2; // '[ ' + ', '*n + ' ]'
    for (int index = 0; index < size; ++index) {
      if (hasCommentForValue(value[index])) {
        isMultiLine = true;
      }
      writeValue(value[index]);
      lineLength += int(childValues_[index].length());
    }
    addChildValues_ = false;
    isMultiLine = isMultiLine || lineLength >= rightMargin_;
  }
  return isMultiLine;
}

void styled_writer::pushValue(std::string const & value) {
  if (addChildValues_)
    childValues_.push_back(value);
  else
    document_ += value;
}

void styled_writer::writeIndent() {
  if (!document_.empty()) {
    char last = document_[document_.length() - 1];
    if (last == ' ') // already indented
      return;
    if (last != '\n') // Comments may add new-line
      document_ += '\n';
  }
  document_ += indentString_;
}

void styled_writer::writeWithIndent(std::string const & value) {
  writeIndent();
  document_ += value;
}

void styled_writer::indent() { indentString_ += std::string(indentSize_, ' '); }

void styled_writer::unindent() {
  assert(int(indentString_.size()) >= indentSize_);
  indentString_.resize(indentString_.size() - indentSize_);
}

void styled_writer::writeCommentBeforeValue(value const & root) {
  if (!root.has_comment(comment_before))
    return;

  document_ += "\n";
  writeIndent();
  std::string const & comment = root.get_comment(comment_before);
  std::string::const_iterator iter = comment.begin();
  while (iter != comment.end()) {
    document_ += *iter;
    if (*iter == '\n' &&
       (iter != comment.end() && *(iter + 1) == '/'))
      writeIndent();
    ++iter;
  }

  // Comments are stripped of trailing newlines, so add one here
  document_ += "\n";
}

void styled_writer::writeCommentAfterValueOnSameLine(value const & root) {
  if (root.has_comment(comment_after_on_same_line))
    document_ += " " + root.get_comment(comment_after_on_same_line);

  if (root.has_comment(comment_after)) {
    document_ += "\n";
    document_ += root.get_comment(comment_after);
    document_ += "\n";
  }
}

bool styled_writer::hasCommentForValue(value const & value) {
  return value.has_comment(comment_before) ||
         value.has_comment(comment_after_on_same_line) ||
         value.has_comment(comment_after);
}

// Class styled_stream_writer
// //////////////////////////////////////////////////////////////////

styled_stream_writer::styled_stream_writer(std::string indentation)
    : document_(NULL), rightMargin_(74), indentation_(indentation),
      addChildValues_() {}

void styled_stream_writer::write(std::ostream& out, value const & root) {
  document_ = &out;
  addChildValues_ = false;
  indentString_ = "";
  indented_ = true;
  writeCommentBeforeValue(root);
  if (!indented_) writeIndent();
  indented_ = true;
  writeValue(root);
  writeCommentAfterValueOnSameLine(root);
  *document_ << "\n";
  document_ = NULL; // Forget the stream, for safety.
}

void styled_stream_writer::writeValue(value const & value) {
  switch (value.type()) {
  case vt_null:
    pushValue("null");
    break;
  case vt_int:
    pushValue(valueToString(value.as_largest_int()));
    break;
  case vt_uint:
    pushValue(valueToString(value.as_largest_uint()));
    break;
  case vt_real:
    pushValue(valueToString(value.as_double()));
    break;
  case vt_string:
    pushValue(valueToQuotedString(value.as_cstring()));
    break;
  case vt_bool:
    pushValue(valueToString(value.as_bool()));
    break;
  case vt_array:
    writeArrayValue(value);
    break;
  case vt_object: {
    value::Members members(value.get_member_names());
    if (members.empty())
      pushValue("{}");
    else {
      writeWithIndent("{");
      indent();
      value::Members::iterator it = members.begin();
      for (;;) {
        std::string const & name = *it;
        class value const & childValue = value[name];
        writeCommentBeforeValue(childValue);
        writeWithIndent(valueToQuotedString(name.c_str()));
        *document_ << " : ";
        writeValue(childValue);
        if (++it == members.end()) {
          writeCommentAfterValueOnSameLine(childValue);
          break;
        }
        *document_ << ",";
        writeCommentAfterValueOnSameLine(childValue);
      }
      unindent();
      writeWithIndent("}");
    }
  } break;
  }
}

void styled_stream_writer::writeArrayValue(value const & value) {
  unsigned size = value.size();
  if (size == 0)
    pushValue("[]");
  else {
    bool isArrayMultiLine = isMultineArray(value);
    if (isArrayMultiLine) {
      writeWithIndent("[");
      indent();
      bool hasChildValue = !childValues_.empty();
      unsigned index = 0;
      for (;;) {
        class value const & childValue = value[index];
        writeCommentBeforeValue(childValue);
        if (hasChildValue)
          writeWithIndent(childValues_[index]);
        else {
          if (!indented_) writeIndent();
          indented_ = true;
          writeValue(childValue);
          indented_ = false;
        }
        if (++index == size) {
          writeCommentAfterValueOnSameLine(childValue);
          break;
        }
        *document_ << ",";
        writeCommentAfterValueOnSameLine(childValue);
      }
      unindent();
      writeWithIndent("]");
    } else // output on a single line
    {
      assert(childValues_.size() == size);
      *document_ << "[ ";
      for (unsigned index = 0; index < size; ++index) {
        if (index > 0)
          *document_ << ", ";
        *document_ << childValues_[index];
      }
      *document_ << " ]";
    }
  }
}

bool styled_stream_writer::isMultineArray(value const & value) {
  int size = value.size();
  bool isMultiLine = size * 3 >= rightMargin_;
  childValues_.clear();
  for (int index = 0; index < size && !isMultiLine; ++index) {
    class value const & childValue = value[index];
    isMultiLine =
        isMultiLine || ((childValue.is_array() || childValue.is_object()) &&
                        childValue.size() > 0);
  }
  if (!isMultiLine) // check if line length > max line length
  {
    childValues_.reserve(size);
    addChildValues_ = true;
    int lineLength = 4 + (size - 1) * 2; // '[ ' + ', '*n + ' ]'
    for (int index = 0; index < size; ++index) {
      if (hasCommentForValue(value[index])) {
        isMultiLine = true;
      }
      writeValue(value[index]);
      lineLength += int(childValues_[index].length());
    }
    addChildValues_ = false;
    isMultiLine = isMultiLine || lineLength >= rightMargin_;
  }
  return isMultiLine;
}

void styled_stream_writer::pushValue(std::string const & value) {
  if (addChildValues_)
    childValues_.push_back(value);
  else
    *document_ << value;
}

void styled_stream_writer::writeIndent() {
  // blep intended this to look at the so-far-written string
  // to determine whether we are already indented, but
  // with a stream we cannot do that. So we rely on some saved state.
  // The caller checks indented_.
  *document_ << '\n' << indentString_;
}

void styled_stream_writer::writeWithIndent(std::string const & value) {
  if (!indented_) writeIndent();
  *document_ << value;
  indented_ = false;
}

void styled_stream_writer::indent() { indentString_ += indentation_; }

void styled_stream_writer::unindent() {
  assert(indentString_.size() >= indentation_.size());
  indentString_.resize(indentString_.size() - indentation_.size());
}

void styled_stream_writer::writeCommentBeforeValue(value const & root) {
  if (!root.has_comment(comment_before))
    return;

  if (!indented_) writeIndent();
  std::string const & comment = root.get_comment(comment_before);
  std::string::const_iterator iter = comment.begin();
  while (iter != comment.end()) {
    *document_ << *iter;
    if (*iter == '\n' &&
       (iter != comment.end() && *(iter + 1) == '/'))
      // writeIndent();  // would include newline
      *document_ << indentString_;
    ++iter;
  }
  indented_ = false;
}

void styled_stream_writer::writeCommentAfterValueOnSameLine(value const & root) {
  if (root.has_comment(comment_after_on_same_line))
    *document_ << ' ' << root.get_comment(comment_after_on_same_line);

  if (root.has_comment(comment_after)) {
    writeIndent();
    *document_ << root.get_comment(comment_after);
  }
  indented_ = false;
}

bool styled_stream_writer::hasCommentForValue(value const & value) {
  return value.has_comment(comment_before) ||
         value.has_comment(comment_after_on_same_line) ||
         value.has_comment(comment_after);
}

//////////////////////////
// built_styled_stream_writer

/// Scoped enums are not available until C++11.
struct CommentStyle {
  /// Decide whether to write comments.
  enum Enum {
    None,  ///< Drop all comments.
    Most,  ///< Recover odd behavior of previous versions (not implemented yet).
    All  ///< Keep all comments.
  };
};

struct built_styled_stream_writer : public stream_writer
{
  built_styled_stream_writer(
      std::string const& indentation,
      CommentStyle::Enum cs,
      std::string const& colonSymbol,
      std::string const& nullSymbol,
      std::string const& endingLineFeedSymbol);
  virtual int write(value const& root, std::ostream* sout);
private:
  void writeValue(value const& value);
  void writeArrayValue(value const& value);
  bool isMultineArray(value const& value);
  void pushValue(std::string const& value);
  void writeIndent();
  void writeWithIndent(std::string const& value);
  void indent();
  void unindent();
  void writeCommentBeforeValue(value const& root);
  void writeCommentAfterValueOnSameLine(value const& root);
  static bool hasCommentForValue(value const & value);

  typedef std::vector<std::string> ChildValues;

  ChildValues childValues_;
  std::string indentString_;
  int rightMargin_;
  std::string indentation_;
  CommentStyle::Enum cs_;
  std::string colonSymbol_;
  std::string nullSymbol_;
  std::string endingLineFeedSymbol_;
  bool addChildValues_ : 1;
  bool indented_ : 1;
};
built_styled_stream_writer::built_styled_stream_writer(
      std::string const& indentation,
      CommentStyle::Enum cs,
      std::string const& colonSymbol,
      std::string const& nullSymbol,
      std::string const& endingLineFeedSymbol)
  : rightMargin_(74)
  , indentation_(indentation)
  , cs_(cs)
  , colonSymbol_(colonSymbol)
  , nullSymbol_(nullSymbol)
  , endingLineFeedSymbol_(endingLineFeedSymbol)
  , addChildValues_(false)
  , indented_(false)
{
}
int built_styled_stream_writer::write(value const& root, std::ostream* sout)
{
  sout_ = sout;
  addChildValues_ = false;
  indented_ = true;
  indentString_ = "";
  writeCommentBeforeValue(root);
  if (!indented_) writeIndent();
  indented_ = true;
  writeValue(root);
  writeCommentAfterValueOnSameLine(root);
  *sout_ << endingLineFeedSymbol_;
  sout_ = NULL;
  return 0;
}
void built_styled_stream_writer::writeValue(value const& value) {
  switch (value.type()) {
  case vt_null:
    pushValue(nullSymbol_);
    break;
  case vt_int:
    pushValue(valueToString(value.as_largest_int()));
    break;
  case vt_uint:
    pushValue(valueToString(value.as_largest_uint()));
    break;
  case vt_real:
    pushValue(valueToString(value.as_double()));
    break;
  case vt_string:
  {
    // Is NULL is possible for value.string_?
    char const* str;
    char const* end;
    bool ok = value.get_string(&str, &end);
    if (ok) pushValue(valueToQuotedStringN(str, static_cast<unsigned>(end-str)));
    else pushValue("");
    break;
  }
  case vt_bool:
    pushValue(valueToString(value.as_bool()));
    break;
  case vt_array:
    writeArrayValue(value);
    break;
  case vt_object: {
    value::Members members(value.get_member_names());
    if (members.empty())
      pushValue("{}");
    else {
      writeWithIndent("{");
      indent();
      value::Members::iterator it = members.begin();
      for (;;) {
        std::string const& name = *it;
        class value const& childValue = value[name];
        writeCommentBeforeValue(childValue);
        writeWithIndent(valueToQuotedStringN(name.data(), name.length()));
        *sout_ << colonSymbol_;
        writeValue(childValue);
        if (++it == members.end()) {
          writeCommentAfterValueOnSameLine(childValue);
          break;
        }
        *sout_ << ",";
        writeCommentAfterValueOnSameLine(childValue);
      }
      unindent();
      writeWithIndent("}");
    }
  } break;
  }
}

void built_styled_stream_writer::writeArrayValue(value const& value) {
  unsigned size = value.size();
  if (size == 0)
    pushValue("[]");
  else {
    bool isMultiLine = (cs_ == CommentStyle::All) || isMultineArray(value);
    if (isMultiLine) {
      writeWithIndent("[");
      indent();
      bool hasChildValue = !childValues_.empty();
      unsigned index = 0;
      for (;;) {
        class value const& childValue = value[index];
        writeCommentBeforeValue(childValue);
        if (hasChildValue)
          writeWithIndent(childValues_[index]);
        else {
          if (!indented_) writeIndent();
          indented_ = true;
          writeValue(childValue);
          indented_ = false;
        }
        if (++index == size) {
          writeCommentAfterValueOnSameLine(childValue);
          break;
        }
        *sout_ << ",";
        writeCommentAfterValueOnSameLine(childValue);
      }
      unindent();
      writeWithIndent("]");
    } else // output on a single line
    {
      assert(childValues_.size() == size);
      *sout_ << "[";
      if (!indentation_.empty()) *sout_ << " ";
      for (unsigned index = 0; index < size; ++index) {
        if (index > 0)
          *sout_ << ", ";
        *sout_ << childValues_[index];
      }
      if (!indentation_.empty()) *sout_ << " ";
      *sout_ << "]";
    }
  }
}

bool built_styled_stream_writer::isMultineArray(value const& value) {
  int size = value.size();
  bool isMultiLine = size * 3 >= rightMargin_;
  childValues_.clear();
  for (int index = 0; index < size && !isMultiLine; ++index) {
    class value const& childValue = value[index];
    isMultiLine =
        isMultiLine || ((childValue.is_array() || childValue.is_object()) &&
                        childValue.size() > 0);
  }
  if (!isMultiLine) // check if line length > max line length
  {
    childValues_.reserve(size);
    addChildValues_ = true;
    int lineLength = 4 + (size - 1) * 2; // '[ ' + ', '*n + ' ]'
    for (int index = 0; index < size; ++index) {
      if (hasCommentForValue(value[index])) {
        isMultiLine = true;
      }
      writeValue(value[index]);
      lineLength += int(childValues_[index].length());
    }
    addChildValues_ = false;
    isMultiLine = isMultiLine || lineLength >= rightMargin_;
  }
  return isMultiLine;
}

void built_styled_stream_writer::pushValue(std::string const& value) {
  if (addChildValues_)
    childValues_.push_back(value);
  else
    *sout_ << value;
}

void built_styled_stream_writer::writeIndent() {
  // blep intended this to look at the so-far-written string
  // to determine whether we are already indented, but
  // with a stream we cannot do that. So we rely on some saved state.
  // The caller checks indented_.

  if (!indentation_.empty()) {
    // In this case, drop newlines too.
    *sout_ << '\n' << indentString_;
  }
}

void built_styled_stream_writer::writeWithIndent(std::string const& value) {
  if (!indented_) writeIndent();
  *sout_ << value;
  indented_ = false;
}

void built_styled_stream_writer::indent() { indentString_ += indentation_; }

void built_styled_stream_writer::unindent() {
  assert(indentString_.size() >= indentation_.size());
  indentString_.resize(indentString_.size() - indentation_.size());
}

void built_styled_stream_writer::writeCommentBeforeValue(value const& root) {
  if (cs_ == CommentStyle::None) return;
  if (!root.has_comment(comment_before))
    return;

  if (!indented_) writeIndent();
  std::string const & comment = root.get_comment(comment_before);
  std::string::const_iterator iter = comment.begin();
  while (iter != comment.end()) {
    *sout_ << *iter;
    if (*iter == '\n' &&
       (iter != comment.end() && *(iter + 1) == '/'))
      // writeIndent();  // would write extra newline
      *sout_ << indentString_;
    ++iter;
  }
  indented_ = false;
}

void built_styled_stream_writer::writeCommentAfterValueOnSameLine(value const& root) {
  if (cs_ == CommentStyle::None) return;
  if (root.has_comment(comment_after_on_same_line))
    *sout_ << " " + root.get_comment(comment_after_on_same_line);

  if (root.has_comment(comment_after)) {
    writeIndent();
    *sout_ << root.get_comment(comment_after);
  }
}

// static
bool built_styled_stream_writer::hasCommentForValue(value const & value) {
  return value.has_comment(comment_before) ||
         value.has_comment(comment_after_on_same_line) ||
         value.has_comment(comment_after);
}

///////////////
// stream_writer

stream_writer::stream_writer()
    : sout_(NULL)
{
}
stream_writer::~stream_writer()
{
}
stream_writer::factory::~factory()
{}
stream_writer_builder::stream_writer_builder()
{
  setDefaults(&settings_);
}
stream_writer_builder::~stream_writer_builder()
{}
stream_writer* stream_writer_builder::newStreamWriter() const
{
  std::string indentation = settings_["indentation"].as_string();
  std::string cs_str = settings_["commentStyle"].as_string();
  bool eyc = settings_["enableYAMLCompatibility"].as_bool();
  bool dnp = settings_["dropNullPlaceholders"].as_bool();
  CommentStyle::Enum cs = CommentStyle::All;
  if (cs_str == "All") {
    cs = CommentStyle::All;
  } else if (cs_str == "None") {
    cs = CommentStyle::None;
  } else {
    throw_runtime_error("commentStyle must be 'All' or 'None'");
  }
  std::string colonSymbol = " : ";
  if (eyc) {
    colonSymbol = ": ";
  } else if (indentation.empty()) {
    colonSymbol = ":";
  }
  std::string nullSymbol = "null";
  if (dnp) {
    nullSymbol = "";
  }
  std::string endingLineFeedSymbol = "";
  return new built_styled_stream_writer(
      indentation, cs,
      colonSymbol, nullSymbol, endingLineFeedSymbol);
}
static void getValidWriterKeys(std::set<std::string>* valid_keys)
{
  valid_keys->clear();
  valid_keys->insert("indentation");
  valid_keys->insert("commentStyle");
  valid_keys->insert("enableYAMLCompatibility");
  valid_keys->insert("dropNullPlaceholders");
}
bool stream_writer_builder::validate(json::value* invalid) const
{
  json::value my_invalid;
  if (!invalid) invalid = &my_invalid;  // so we do not need to test for NULL
  json::value& inv = *invalid;
  std::set<std::string> valid_keys;
  getValidWriterKeys(&valid_keys);
  value::Members keys = settings_.get_member_names();
  size_t n = keys.size();
  for (size_t i = 0; i < n; ++i) {
    std::string const& key = keys[i];
    if (valid_keys.find(key) == valid_keys.end()) {
      inv[key] = settings_[key];
    }
  }
  return 0u == inv.size();
}
value& stream_writer_builder::operator[](std::string key)
{
  return settings_[key];
}
// static
void stream_writer_builder::setDefaults(json::value* settings)
{
  //! [StreamWriterBuilderDefaults]
  (*settings)["commentStyle"] = "All";
  (*settings)["indentation"] = "\t";
  (*settings)["enableYAMLCompatibility"] = false;
  (*settings)["dropNullPlaceholders"] = false;
  //! [StreamWriterBuilderDefaults]
}

std::string write_string(stream_writer::factory const& builder, value const& root) {
  std::ostringstream sout;
  StreamWriterPtr const writer(builder.newStreamWriter());
  writer->write(root, &sout);
  return sout.str();
}

std::ostream& operator<<(std::ostream& sout, value const& root) {
  stream_writer_builder builder;
  StreamWriterPtr const writer(builder.newStreamWriter());
  writer->write(root, &sout);
  return sout;
}

} // namespace json
