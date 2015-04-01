// Derived from public-domain/MIT-licensed code at
// https://github.com/open-source-parsers/jsoncpp. Thanks, Baptiste Lepilleur!

#include <json/writer.h>
#include "tool.h"
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
typedef std::unique_ptr<stream_writer> StreamwriterPtr;
#else
typedef std::auto_ptr<stream_writer> StreamwriterPtr;
#endif

static bool contains_control_char(const char* str)
{
    while (*str) {
        if (is_control_char(*(str++)))
            return true;
    }
    return false;
}

static bool contains_control_char0(const char* str, unsigned len)
{
    char const* end = str + len;
    while (end != str) {
        if (is_control_char(*str) || 0 == *str)
            return true;
        ++str;
    }
    return false;
}

std::string value_to_string(largest_int_t value)
{
    uint_to_string_buffer buffer;
    char* current = buffer + sizeof(buffer);
    bool is_negative = value < 0;
    if (is_negative)
        value = -value;
    uint_to_string(largest_uint_t(value), current);
    if (is_negative)
        *--current = '-';
    assert(current >= buffer);
    return current;
}

std::string value_to_string(largest_uint_t value)
{
    uint_to_string_buffer buffer;
    char* current = buffer + sizeof(buffer);
    uint_to_string(value, current);
    assert(current >= buffer);
    return current;
}

#if defined(JSON_HAS_INT64)

std::string value_to_string(int32_t value)
{
    return value_to_string(largest_int_t(value));
}

std::string value_to_string(uint32_t value)
{
    return value_to_string(largest_uint_t(value));
}

#endif // # if defined(JSON_HAS_INT64)

std::string value_to_string(double value)
{
    // allocate a buffer that is more than large enough to store the 16 digits of
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
    }
    else {
        // IEEE standard states that NaN values will not compare to themselves
        if (value != value) {
            len = snprintf(buffer, sizeof(buffer), "null");
        }
        else if (value < 0) {
            len = snprintf(buffer, sizeof(buffer), "-1e+9999");
        }
        else {
            len = snprintf(buffer, sizeof(buffer), "1e+9999");
        }
        // For those, we do not need to call fixNumLoc, but it is fast.
    }
#endif
    assert(len >= 0);
    fix_numeric_locale(buffer, buffer + len);
    return buffer;
}

std::string value_to_string(bool value) { return value ? "true" : "false"; }

std::string value_to_quoted_string(const char* value)
{
    if (value == NULL)
        return "";
    // Not sure how to handle unicode...
    if (strpbrk(value, "\"\\\b\f\n\r\t") == NULL && !contains_control_char(value))
        return std::string("\"") + value + "\"";
    // We have to walk value and escape any special characters.
    // Appending to std::string is not efficient, but this should be rare.
    // (Note: forward slashes are *not* rare, but I am not escaping them.)
    std::string::size_type maxsize = strlen(value) * 2 + 3; // allescaped+quotes+NULL
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
            if (is_control_char(*c)) {
                std::ostringstream oss;
                oss << "\\u" << std::hex << std::uppercase << std::setfill('0')
                    << std::setw(4) << static_cast<int>(*c);
                result += oss.str();
            }
            else {
                result += *c;
            }
            break;
        }
    }
    result += "\"";
    return result;
}

// https://github.com/upcaste/upcaste/blob/master/src/upcore/src/cstring/strnpbrk.cpp
static char const* strnpbrk(char const* s, char const* accept, size_t n)
{
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
static std::string value_to_quoted_string_n(const char* value, unsigned length)
{
    if (value == NULL)
        return "";
    // Not sure how to handle unicode...
    if (strnpbrk(value, "\"\\\b\f\n\r\t", length) == NULL && !contains_control_char0(value, length))
        return std::string("\"") + value + "\"";
    // We have to walk value and escape any special characters.
    // Appending to std::string is not efficient, but this should be rare.
    // (Note: forward slashes are *not* rare, but I am not escaping them.)
    std::string::size_type maxsize = length * 2 + 3; // allescaped+quotes+NULL
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
            if ((is_control_char(*c)) || (*c == 0)) {
                std::ostringstream oss;
                oss << "\\u" << std::hex << std::uppercase << std::setfill('0')
                    << std::setw(4) << static_cast<int>(*c);
                result += oss.str();
            }
            else {
                result += *c;
            }
            break;
        }
    }
    result += "\"";
    return result;
}

// Class writer
// //////////////////////////////////////////////////////////////////
writer::~writer() {}

// Class fast_writer
// //////////////////////////////////////////////////////////////////

fast_writer::fast_writer()
    : yaml_compatibility_enabled_(false)
    , drop_null_placeholders_(false)
    , omit_ending_line_feed_(false)
{
}

void fast_writer::enable_yaml_compatibility() { yaml_compatibility_enabled_ = true; }

void fast_writer::drop_null_placeholders() { drop_null_placeholders_ = true; }

void fast_writer::omit_ending_line_feed() { omit_ending_line_feed_ = true; }

std::string fast_writer::write(value const& root)
{
    document_ = "";
    write_value(root);
    if (!omit_ending_line_feed_)
        document_ += "\n";
    return document_;
}

void fast_writer::write_value(value const& value)
{
    switch (value.type()) {
    case vt_null:
        if (!drop_null_placeholders_)
            document_ += "null";
        break;
    case vt_int:
        document_ += value_to_string(value.as_largest_int());
        break;
    case vt_uint:
        document_ += value_to_string(value.as_largest_uint());
        break;
    case vt_real:
        document_ += value_to_string(value.as_double());
        break;
    case vt_string:
        document_ += value_to_quoted_string(value.as_cstring());
        break;
    case vt_bool:
        document_ += value_to_string(value.as_bool());
        break;
    case vt_array: {
        document_ += '[';
        int size = value.size();
        for (int index = 0; index < size; ++index) {
            if (index > 0)
                document_ += ',';
            write_value(value[index]);
        }
        document_ += ']';
    } break;
    case vt_object: {
        value::members members(value.get_member_names());
        document_ += '{';
        for (value::members::iterator it = members.begin(); it != members.end();
             ++it) {
            std::string const& name = *it;
            if (it != members.begin())
                document_ += ',';
            document_ += value_to_quoted_string_n(name.data(), name.length());
            document_ += yaml_compatibility_enabled_ ? ": " : ":";
            write_value(value[name]);
        }
        document_ += '}';
    } break;
    }
}

// Class styled_writer
// //////////////////////////////////////////////////////////////////

styled_writer::styled_writer()
    : right_margin_(74)
    , indentSize_(3)
    , add_child_values_()
{
}

std::string styled_writer::write(value const& root)
{
    document_ = "";
    add_child_values_ = false;
    indentString_ = "";
    write_comment_before_value(root);
    write_value(root);
    write_comment_after_value_on_same_line(root);
    document_ += "\n";
    return document_;
}

void styled_writer::write_value(value const& value)
{
    switch (value.type()) {
    case vt_null:
        push_value("null");
        break;
    case vt_int:
        push_value(value_to_string(value.as_largest_int()));
        break;
    case vt_uint:
        push_value(value_to_string(value.as_largest_uint()));
        break;
    case vt_real:
        push_value(value_to_string(value.as_double()));
        break;
    case vt_string: {
        // Is NULL is possible for value.string_?
        char const* str;
        char const* end;
        bool ok = value.get_string(&str, &end);
        if (ok)
            push_value(value_to_quoted_string_n(str, static_cast<unsigned>(end - str)));
        else
            push_value("");
        break;
    }
    case vt_bool:
        push_value(value_to_string(value.as_bool()));
        break;
    case vt_array:
        write_array_value(value);
        break;
    case vt_object: {
        value::members members(value.get_member_names());
        if (members.empty())
            push_value("{}");
        else {
            write_with_indent("{");
            indent();
            value::members::iterator it = members.begin();
            for (;;) {
                std::string const& name = *it;
                class value const& child_value = value[name];
                write_comment_before_value(child_value);
                write_with_indent(value_to_quoted_string(name.c_str()));
                document_ += " : ";
                write_value(child_value);
                if (++it == members.end()) {
                    write_comment_after_value_on_same_line(child_value);
                    break;
                }
                document_ += ',';
                write_comment_after_value_on_same_line(child_value);
            }
            unindent();
            write_with_indent("}");
        }
    } break;
    }
}

void styled_writer::write_array_value(value const& value)
{
    unsigned size = value.size();
    if (size == 0)
        push_value("[]");
    else {
        bool isArrayMultiLine = is_multiline_array(value);
        if (isArrayMultiLine) {
            write_with_indent("[");
            indent();
            bool has_child_value = !child_values_.empty();
            unsigned index = 0;
            for (;;) {
                class value const& child_value = value[index];
                write_comment_before_value(child_value);
                if (has_child_value)
                    write_with_indent(child_values_[index]);
                else {
                    writeIndent();
                    write_value(child_value);
                }
                if (++index == size) {
                    write_comment_after_value_on_same_line(child_value);
                    break;
                }
                document_ += ',';
                write_comment_after_value_on_same_line(child_value);
            }
            unindent();
            write_with_indent("]");
        }
        else // output on a single line
        {
            assert(child_values_.size() == size);
            document_ += "[ ";
            for (unsigned index = 0; index < size; ++index) {
                if (index > 0)
                    document_ += ", ";
                document_ += child_values_[index];
            }
            document_ += " ]";
        }
    }
}

bool styled_writer::is_multiline_array(value const& value)
{
    int size = value.size();
    bool is_multiline = size * 3 >= right_margin_;
    child_values_.clear();
    for (int index = 0; index < size && !is_multiline; ++index) {
        class value const& child_value = value[index];
        is_multiline = is_multiline || ((child_value.is_array() || child_value.is_object()) && child_value.size() > 0);
    }
    if (!is_multiline) // check if line length > max line length
    {
        child_values_.reserve(size);
        add_child_values_ = true;
        int lineLength = 4 + (size - 1) * 2; // '[ ' + ', '*n + ' ]'
        for (int index = 0; index < size; ++index) {
            if (has_comment_for_value(value[index])) {
                is_multiline = true;
            }
            write_value(value[index]);
            lineLength += int(child_values_[index].length());
        }
        add_child_values_ = false;
        is_multiline = is_multiline || lineLength >= right_margin_;
    }
    return is_multiline;
}

void styled_writer::push_value(std::string const& value)
{
    if (add_child_values_)
        child_values_.push_back(value);
    else
        document_ += value;
}

void styled_writer::writeIndent()
{
    if (!document_.empty()) {
        char last = document_[document_.length() - 1];
        if (last == ' ') // already indented
            return;
        if (last != '\n') // Comments may add new-line
            document_ += '\n';
    }
    document_ += indentString_;
}

void styled_writer::write_with_indent(std::string const& value)
{
    writeIndent();
    document_ += value;
}

void styled_writer::indent() { indentString_ += std::string(indentSize_, ' '); }

void styled_writer::unindent()
{
    assert(int(indentString_.size()) >= indentSize_);
    indentString_.resize(indentString_.size() - indentSize_);
}

void styled_writer::write_comment_before_value(value const& root)
{
    if (!root.has_comment(comment_before))
        return;

    document_ += "\n";
    writeIndent();
    std::string const& comment = root.get_comment(comment_before);
    std::string::const_iterator iter = comment.begin();
    while (iter != comment.end()) {
        document_ += *iter;
        if (*iter == '\n' && (iter != comment.end() && *(iter + 1) == '/'))
            writeIndent();
        ++iter;
    }

    // Comments are stripped of trailing newlines, so add one here
    document_ += "\n";
}

void styled_writer::write_comment_after_value_on_same_line(value const& root)
{
    if (root.has_comment(comment_after_on_same_line))
        document_ += " " + root.get_comment(comment_after_on_same_line);

    if (root.has_comment(comment_after)) {
        document_ += "\n";
        document_ += root.get_comment(comment_after);
        document_ += "\n";
    }
}

bool styled_writer::has_comment_for_value(value const& value)
{
    return value.has_comment(comment_before) || value.has_comment(comment_after_on_same_line) || value.has_comment(comment_after);
}

// Class styled_stream_writer
// //////////////////////////////////////////////////////////////////

styled_stream_writer::styled_stream_writer(std::string indentation)
    : document_(NULL)
    , right_margin_(74)
    , indentation_(indentation)
    , add_child_values_()
{
}

void styled_stream_writer::write(std::ostream& out, value const& root)
{
    document_ = &out;
    add_child_values_ = false;
    indentString_ = "";
    indented_ = true;
    write_comment_before_value(root);
    if (!indented_)
        writeIndent();
    indented_ = true;
    write_value(root);
    write_comment_after_value_on_same_line(root);
    *document_ << "\n";
    document_ = NULL; // Forget the stream, for safety.
}

void styled_stream_writer::write_value(value const& value)
{
    switch (value.type()) {
    case vt_null:
        push_value("null");
        break;
    case vt_int:
        push_value(value_to_string(value.as_largest_int()));
        break;
    case vt_uint:
        push_value(value_to_string(value.as_largest_uint()));
        break;
    case vt_real:
        push_value(value_to_string(value.as_double()));
        break;
    case vt_string:
        push_value(value_to_quoted_string(value.as_cstring()));
        break;
    case vt_bool:
        push_value(value_to_string(value.as_bool()));
        break;
    case vt_array:
        write_array_value(value);
        break;
    case vt_object: {
        value::members members(value.get_member_names());
        if (members.empty())
            push_value("{}");
        else {
            write_with_indent("{");
            indent();
            value::members::iterator it = members.begin();
            for (;;) {
                std::string const& name = *it;
                class value const& child_value = value[name];
                write_comment_before_value(child_value);
                write_with_indent(value_to_quoted_string(name.c_str()));
                *document_ << " : ";
                write_value(child_value);
                if (++it == members.end()) {
                    write_comment_after_value_on_same_line(child_value);
                    break;
                }
                *document_ << ",";
                write_comment_after_value_on_same_line(child_value);
            }
            unindent();
            write_with_indent("}");
        }
    } break;
    }
}

void styled_stream_writer::write_array_value(value const& value)
{
    unsigned size = value.size();
    if (size == 0)
        push_value("[]");
    else {
        bool isArrayMultiLine = is_multiline_array(value);
        if (isArrayMultiLine) {
            write_with_indent("[");
            indent();
            bool has_child_value = !child_values_.empty();
            unsigned index = 0;
            for (;;) {
                class value const& child_value = value[index];
                write_comment_before_value(child_value);
                if (has_child_value)
                    write_with_indent(child_values_[index]);
                else {
                    if (!indented_)
                        writeIndent();
                    indented_ = true;
                    write_value(child_value);
                    indented_ = false;
                }
                if (++index == size) {
                    write_comment_after_value_on_same_line(child_value);
                    break;
                }
                *document_ << ",";
                write_comment_after_value_on_same_line(child_value);
            }
            unindent();
            write_with_indent("]");
        }
        else // output on a single line
        {
            assert(child_values_.size() == size);
            *document_ << "[ ";
            for (unsigned index = 0; index < size; ++index) {
                if (index > 0)
                    *document_ << ", ";
                *document_ << child_values_[index];
            }
            *document_ << " ]";
        }
    }
}

bool styled_stream_writer::is_multiline_array(value const& value)
{
    int size = value.size();
    bool is_multiline = size * 3 >= right_margin_;
    child_values_.clear();
    for (int index = 0; index < size && !is_multiline; ++index) {
        class value const& child_value = value[index];
        is_multiline = is_multiline || ((child_value.is_array() || child_value.is_object()) && child_value.size() > 0);
    }
    if (!is_multiline) // check if line length > max line length
    {
        child_values_.reserve(size);
        add_child_values_ = true;
        int lineLength = 4 + (size - 1) * 2; // '[ ' + ', '*n + ' ]'
        for (int index = 0; index < size; ++index) {
            if (has_comment_for_value(value[index])) {
                is_multiline = true;
            }
            write_value(value[index]);
            lineLength += int(child_values_[index].length());
        }
        add_child_values_ = false;
        is_multiline = is_multiline || lineLength >= right_margin_;
    }
    return is_multiline;
}

void styled_stream_writer::push_value(std::string const& value)
{
    if (add_child_values_)
        child_values_.push_back(value);
    else
        *document_ << value;
}

void styled_stream_writer::writeIndent()
{
    // blep intended this to look at the so-far-written string
    // to determine whether we are already indented, but
    // with a stream we cannot do that. So we rely on some saved state.
    // The caller checks indented_.
    *document_ << '\n' << indentString_;
}

void styled_stream_writer::write_with_indent(std::string const& value)
{
    if (!indented_)
        writeIndent();
    *document_ << value;
    indented_ = false;
}

void styled_stream_writer::indent() { indentString_ += indentation_; }

void styled_stream_writer::unindent()
{
    assert(indentString_.size() >= indentation_.size());
    indentString_.resize(indentString_.size() - indentation_.size());
}

void styled_stream_writer::write_comment_before_value(value const& root)
{
    if (!root.has_comment(comment_before))
        return;

    if (!indented_)
        writeIndent();
    std::string const& comment = root.get_comment(comment_before);
    std::string::const_iterator iter = comment.begin();
    while (iter != comment.end()) {
        *document_ << *iter;
        if (*iter == '\n' && (iter != comment.end() && *(iter + 1) == '/'))
            // writeIndent();  // would include newline
            *document_ << indentString_;
        ++iter;
    }
    indented_ = false;
}

void styled_stream_writer::write_comment_after_value_on_same_line(value const& root)
{
    if (root.has_comment(comment_after_on_same_line))
        *document_ << ' ' << root.get_comment(comment_after_on_same_line);

    if (root.has_comment(comment_after)) {
        writeIndent();
        *document_ << root.get_comment(comment_after);
    }
    indented_ = false;
}

bool styled_stream_writer::has_comment_for_value(value const& value)
{
    return value.has_comment(comment_before) || value.has_comment(comment_after_on_same_line) || value.has_comment(comment_after);
}

//////////////////////////
// built_styled_stream_writer

/// Decide whether to write comments.
enum class comment_style {
    none, ///< Drop all comments.
    most, ///< Recover odd behavior of previous versions (not implemented yet).
    all ///< Keep all comments.
};

struct built_styled_stream_writer : public stream_writer {
    built_styled_stream_writer(
        std::string const& indentation,
        comment_style cs,
        std::string const& colon_symbol,
        std::string const& null_symbol,
        std::string const& ending_linefeed_symbol);
    virtual int write(value const& root, std::ostream* sout);

private:
    void write_value(value const& value);
    void write_array_value(value const& value);
    bool is_multiline_array(value const& value);
    void push_value(std::string const& value);
    void writeIndent();
    void write_with_indent(std::string const& value);
    void indent();
    void unindent();
    void write_comment_before_value(value const& root);
    void write_comment_after_value_on_same_line(value const& root);
    static bool has_comment_for_value(value const& value);

    typedef std::vector<std::string> ChildValues;

    ChildValues child_values_;
    std::string indentString_;
    int right_margin_;
    std::string indentation_;
    comment_style cs_;
    std::string colon_symbol_;
    std::string null_symbol_;
    std::string ending_linefeed_symbol_;
    bool add_child_values_ : 1;
    bool indented_ : 1;
};
built_styled_stream_writer::built_styled_stream_writer(
    std::string const& indentation,
    comment_style cs,
    std::string const& colon_symbol,
    std::string const& null_symbol,
    std::string const& ending_linefeed_symbol)
    : right_margin_(74)
    , indentation_(indentation)
    , cs_(cs)
    , colon_symbol_(colon_symbol)
    , null_symbol_(null_symbol)
    , ending_linefeed_symbol_(ending_linefeed_symbol)
    , add_child_values_(false)
    , indented_(false)
{
}
int built_styled_stream_writer::write(value const& root, std::ostream* sout)
{
    sout_ = sout;
    add_child_values_ = false;
    indented_ = true;
    indentString_ = "";
    write_comment_before_value(root);
    if (!indented_)
        writeIndent();
    indented_ = true;
    write_value(root);
    write_comment_after_value_on_same_line(root);
    *sout_ << ending_linefeed_symbol_;
    sout_ = NULL;
    return 0;
}
void built_styled_stream_writer::write_value(value const& value)
{
    switch (value.type()) {
    case vt_null:
        push_value(null_symbol_);
        break;
    case vt_int:
        push_value(value_to_string(value.as_largest_int()));
        break;
    case vt_uint:
        push_value(value_to_string(value.as_largest_uint()));
        break;
    case vt_real:
        push_value(value_to_string(value.as_double()));
        break;
    case vt_string: {
        // Is NULL is possible for value.string_?
        char const* str;
        char const* end;
        bool ok = value.get_string(&str, &end);
        if (ok)
            push_value(value_to_quoted_string_n(str, static_cast<unsigned>(end - str)));
        else
            push_value("");
        break;
    }
    case vt_bool:
        push_value(value_to_string(value.as_bool()));
        break;
    case vt_array:
        write_array_value(value);
        break;
    case vt_object: {
        value::members members(value.get_member_names());
        if (members.empty())
            push_value("{}");
        else {
            write_with_indent("{");
            indent();
            value::members::iterator it = members.begin();
            for (;;) {
                std::string const& name = *it;
                class value const& child_value = value[name];
                write_comment_before_value(child_value);
                write_with_indent(value_to_quoted_string_n(name.data(), name.length()));
                *sout_ << colon_symbol_;
                write_value(child_value);
                if (++it == members.end()) {
                    write_comment_after_value_on_same_line(child_value);
                    break;
                }
                *sout_ << ",";
                write_comment_after_value_on_same_line(child_value);
            }
            unindent();
            write_with_indent("}");
        }
    } break;
    }
}

void built_styled_stream_writer::write_array_value(value const& value)
{
    unsigned size = value.size();
    if (size == 0)
        push_value("[]");
    else {
        bool is_multiline = (cs_ == comment_style::all) || is_multiline_array(value);
        if (is_multiline) {
            write_with_indent("[");
            indent();
            bool has_child_value = !child_values_.empty();
            unsigned index = 0;
            for (;;) {
                class value const& child_value = value[index];
                write_comment_before_value(child_value);
                if (has_child_value)
                    write_with_indent(child_values_[index]);
                else {
                    if (!indented_)
                        writeIndent();
                    indented_ = true;
                    write_value(child_value);
                    indented_ = false;
                }
                if (++index == size) {
                    write_comment_after_value_on_same_line(child_value);
                    break;
                }
                *sout_ << ",";
                write_comment_after_value_on_same_line(child_value);
            }
            unindent();
            write_with_indent("]");
        }
        else // output on a single line
        {
            assert(child_values_.size() == size);
            *sout_ << "[";
            if (!indentation_.empty())
                *sout_ << " ";
            for (unsigned index = 0; index < size; ++index) {
                if (index > 0)
                    *sout_ << ", ";
                *sout_ << child_values_[index];
            }
            if (!indentation_.empty())
                *sout_ << " ";
            *sout_ << "]";
        }
    }
}

bool built_styled_stream_writer::is_multiline_array(value const& value)
{
    int size = value.size();
    bool is_multiline = size * 3 >= right_margin_;
    child_values_.clear();
    for (int index = 0; index < size && !is_multiline; ++index) {
        class value const& child_value = value[index];
        is_multiline = is_multiline || ((child_value.is_array() || child_value.is_object()) && child_value.size() > 0);
    }
    if (!is_multiline) // check if line length > max line length
    {
        child_values_.reserve(size);
        add_child_values_ = true;
        int lineLength = 4 + (size - 1) * 2; // '[ ' + ', '*n + ' ]'
        for (int index = 0; index < size; ++index) {
            if (has_comment_for_value(value[index])) {
                is_multiline = true;
            }
            write_value(value[index]);
            lineLength += int(child_values_[index].length());
        }
        add_child_values_ = false;
        is_multiline = is_multiline || lineLength >= right_margin_;
    }
    return is_multiline;
}

void built_styled_stream_writer::push_value(std::string const& value)
{
    if (add_child_values_)
        child_values_.push_back(value);
    else
        *sout_ << value;
}

void built_styled_stream_writer::writeIndent()
{
    // blep intended this to look at the so-far-written string
    // to determine whether we are already indented, but
    // with a stream we cannot do that. So we rely on some saved state.
    // The caller checks indented_.

    if (!indentation_.empty()) {
        // In this case, drop newlines too.
        *sout_ << '\n' << indentString_;
    }
}

void built_styled_stream_writer::write_with_indent(std::string const& value)
{
    if (!indented_)
        writeIndent();
    *sout_ << value;
    indented_ = false;
}

void built_styled_stream_writer::indent() { indentString_ += indentation_; }

void built_styled_stream_writer::unindent()
{
    assert(indentString_.size() >= indentation_.size());
    indentString_.resize(indentString_.size() - indentation_.size());
}

void built_styled_stream_writer::write_comment_before_value(value const& root)
{
    if (cs_ == comment_style::none)
        return;
    if (!root.has_comment(comment_before))
        return;

    if (!indented_)
        writeIndent();
    std::string const& comment = root.get_comment(comment_before);
    std::string::const_iterator iter = comment.begin();
    while (iter != comment.end()) {
        *sout_ << *iter;
        if (*iter == '\n' && (iter != comment.end() && *(iter + 1) == '/'))
            // writeIndent();  // would write extra newline
            *sout_ << indentString_;
        ++iter;
    }
    indented_ = false;
}

void built_styled_stream_writer::write_comment_after_value_on_same_line(value const& root)
{
    if (cs_ == comment_style::none)
        return;
    if (root.has_comment(comment_after_on_same_line))
        *sout_ << " " + root.get_comment(comment_after_on_same_line);

    if (root.has_comment(comment_after)) {
        writeIndent();
        *sout_ << root.get_comment(comment_after);
    }
}

// static
bool built_styled_stream_writer::has_comment_for_value(value const& value)
{
    return value.has_comment(comment_before) || value.has_comment(comment_after_on_same_line) || value.has_comment(comment_after);
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
{
}

stream_writer_builder::stream_writer_builder()
{
    set_defaults(&settings_);
}

stream_writer_builder::~stream_writer_builder()
{
}

stream_writer* stream_writer_builder::new_stream_writer() const
{
    std::string indentation = settings_["indentation"].as_string();
    std::string cs_str = settings_["comment_style"].as_string();
    bool eyc = settings_["enable_yaml_compatibility"].as_bool();
    bool dnp = settings_["drop_null_placeholders"].as_bool();
    comment_style cs = comment_style::all;
    if (cs_str == "all") {
        cs = comment_style::all;
    }
    else if (cs_str == "none") {
        cs = comment_style::none;
    }
    else {
        throw_runtime_error("comment_style must be 'all' or 'none'");
    }
    std::string colon_symbol = " : ";
    if (eyc) {
        colon_symbol = ": ";
    }
    else if (indentation.empty()) {
        colon_symbol = ":";
    }
    std::string null_symbol = "null";
    if (dnp) {
        null_symbol = "";
    }
    std::string ending_linefeed_symbol = "";
    return new built_styled_stream_writer(
        indentation, cs,
        colon_symbol, null_symbol, ending_linefeed_symbol);
}
static void get_valid_writer_keys(std::set<std::string>* valid_keys)
{
    valid_keys->clear();
    valid_keys->insert("indentation");
    valid_keys->insert("comment_style");
    valid_keys->insert("enable_yaml_compatibility");
    valid_keys->insert("drop_null_placeholders");
}
bool stream_writer_builder::validate(json::value* invalid) const
{
    json::value my_invalid;
    if (!invalid)
        invalid = &my_invalid; // so we do not need to test for NULL
    json::value& inv = *invalid;
    std::set<std::string> valid_keys;
    get_valid_writer_keys(&valid_keys);
    value::members keys = settings_.get_member_names();
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
void stream_writer_builder::set_defaults(json::value* settings)
{
    //! [StreamwriterBuilderDefaults]
    (*settings)["comment_style"] = "all";
    (*settings)["indentation"] = "\t";
    (*settings)["enable_yaml_compatibility"] = false;
    (*settings)["drop_null_placeholders"] = false;
    //! [StreamwriterBuilderDefaults]
}

std::string write_string(stream_writer::factory const& builder, value const& root)
{
    std::ostringstream sout;
    StreamwriterPtr const writer(builder.new_stream_writer());
    writer->write(root, &sout);
    return sout.str();
}

std::ostream& operator<<(std::ostream& sout, value const& root)
{
    stream_writer_builder builder;
    StreamwriterPtr const writer(builder.new_stream_writer());
    writer->write(root, &sout);
    return sout;
}

} // namespace json
