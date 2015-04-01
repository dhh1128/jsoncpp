// Derived from public-domain/MIT-licensed code at
// https://github.com/open-source-parsers/jsoncpp. Thanks, Baptiste Lepilleur!

#include <json/assertions.h>
#include <json/reader.h>
#include <json/value.h>
#include "json_tool.h"
#include <utility>
#include <cstdio>
#include <cassert>
#include <cstring>
#include <istream>
#include <sstream>
#include <memory>
#include <set>

#if defined(_MSC_VER) && _MSC_VER < 1500 // VC++ 8.0 and below
#define snprintf _snprintf
#endif

#if defined(_MSC_VER) && _MSC_VER >= 1400 // VC++ 8.0
// Disable warning about strdup being deprecated.
#pragma warning(disable : 4996)
#endif

static int const stack_limit_g = 1000;
static int stack_depth_g = 0; // see read_value()

namespace json {

#if __cplusplus >= 201103L
typedef std::unique_ptr<char_reader> char_reader_ptr;
#else
typedef std::auto_ptr<char_reader> char_reader_ptr;
#endif

// Implementation of class features
// ////////////////////////////////

features::features()
    : allow_comments_(true)
    , strict_root_(false)
    , allow_dropped_null_placeholders_(false)
    , allow_numeric_keys_(false)
{
}

features features::all() { return features(); }

features features::strict_mode()
{
    features features;
    features.allow_comments_ = false;
    features.strict_root_ = true;
    features.allow_dropped_null_placeholders_ = false;
    features.allow_numeric_keys_ = false;
    return features;
}

// Implementation of class reader
// ////////////////////////////////

static bool contains_new_line(reader::location_t begin, reader::location_t end)
{
    for (; begin < end; ++begin)
        if (*begin == '\n' || *begin == '\r')
            return true;
    return false;
}

// Class reader
// //////////////////////////////////////////////////////////////////

reader::reader()
    : errors_()
    , document_()
    , begin_()
    , end_()
    , current_()
    , last_value_end_()
    , last_value_()
    , comments_before_()
    , features_(features::all())
    , collect_comments_()
{
}

reader::reader(features const& features)
    : errors_()
    , document_()
    , begin_()
    , end_()
    , current_()
    , last_value_end_()
    , last_value_()
    , comments_before_()
    , features_(features)
    , collect_comments_()
{
}

bool reader::parse(std::string const& document, value& root, bool collect_comments)
{
    document_ = document;
    const char* begin = document_.c_str();
    const char* end = begin + document_.length();
    return parse(begin, end, root, collect_comments);
}

bool reader::parse(std::istream& sin, value& root, bool collect_comments)
{
    // std::istream_iterator<char> begin(sin);
    // std::istream_iterator<char> end;
    // Those would allow streamed input from a file, if parse() were a
    // template function.

    // Since std::string is reference-counted, this at least does not
    // create an extra copy.
    std::string doc;
    std::getline(sin, doc, (char)EOF);
    return parse(doc, root, collect_comments);
}

bool reader::parse(const char* begin_doc,
    const char* end_doc,
    value& root,
    bool collect_comments)
{
    if (!features_.allow_comments_) {
        collect_comments = false;
    }

    begin_ = begin_doc;
    end_ = end_doc;
    collect_comments_ = collect_comments;
    current_ = begin_;
    last_value_end_ = 0;
    last_value_ = 0;
    comments_before_ = "";
    errors_.clear();
    while (!nodes_.empty())
        nodes_.pop();
    nodes_.push(&root);

    stack_depth_g = 0; // Yes, this is bad coding, but options are limited.
    bool successful = read_value();
    token token;
    skip_comment_tokens(token);
    if (collect_comments_ && !comments_before_.empty())
        root.set_comment(comments_before_, comment_after);
    if (features_.strict_root_) {
        if (!root.is_array() && !root.is_object()) {
            // Set error location to start of doc, ideally should be first token found
            // in doc
            token.type_ = tt_error;
            token.start_ = begin_doc;
            token.end_ = end_doc;
            add_error(
                "A valid JSON document must be either an array or an object value.",
                token);
            return false;
        }
    }
    return successful;
}

bool reader::read_value()
{
    // This is a non-reentrant way to support a stack limit. Terrible!
    // But this deprecated class has a security problem: Bad input can
    // cause a seg-fault. This seems like a fair, binary-compatible way
    // to prevent the problem.
    if (stack_depth_g >= stack_limit_g)
        throw_runtime_error("Exceeded stack_limit in read_value().");
    ++stack_depth_g;

    token token;
    skip_comment_tokens(token);
    bool successful = true;

    if (collect_comments_ && !comments_before_.empty()) {
        current_value().set_comment(comments_before_, comment_before);
        comments_before_ = "";
    }

    switch (token.type_) {
    case tt_object_begin:
        successful = read_object(token);
        current_value().set_offset_limit(current_ - begin_);
        break;
    case tt_array_begin:
        successful = read_array(token);
        current_value().set_offset_limit(current_ - begin_);
        break;
    case tt_number:
        successful = decode_number(token);
        break;
    case tt_string:
        successful = decode_string(token);
        break;
    case tt_true: {
        value v(true);
        current_value().swap_payload(v);
        current_value().set_offset_start(token.start_ - begin_);
        current_value().set_offset_limit(token.end_ - begin_);
    } break;
    case tt_false: {
        value v(false);
        current_value().swap_payload(v);
        current_value().set_offset_start(token.start_ - begin_);
        current_value().set_offset_limit(token.end_ - begin_);
    } break;
    case tt_null: {
        value v;
        current_value().swap_payload(v);
        current_value().set_offset_start(token.start_ - begin_);
        current_value().set_offset_limit(token.end_ - begin_);
    } break;
    case tt_array_separator:
    case tt_object_end:
    case tt_array_end:
        if (features_.allow_dropped_null_placeholders_) {
            // "Un-read" the current token and mark the current value as a null
            // token.
            current_--;
            value v;
            current_value().swap_payload(v);
            current_value().set_offset_start(current_ - begin_ - 1);
            current_value().set_offset_limit(current_ - begin_);
            break;
        } // Else, fall through...
    default:
        current_value().set_offset_start(token.start_ - begin_);
        current_value().set_offset_limit(token.end_ - begin_);
        return add_error("Syntax error: value, object or array expected.", token);
    }

    if (collect_comments_) {
        last_value_end_ = current_;
        last_value_ = &current_value();
    }

    --stack_depth_g;
    return successful;
}

void reader::skip_comment_tokens(token& token)
{
    if (features_.allow_comments_) {
        do {
            read_token(token);
        } while (token.type_ == tt_comment);
    }
    else {
        read_token(token);
    }
}

bool reader::read_token(token& token)
{
    skip_spaces();
    token.start_ = current_;
    char c = get_next_char();
    bool ok = true;
    switch (c) {
    case '{':
        token.type_ = tt_object_begin;
        break;
    case '}':
        token.type_ = tt_object_end;
        break;
    case '[':
        token.type_ = tt_array_begin;
        break;
    case ']':
        token.type_ = tt_array_end;
        break;
    case '"':
        token.type_ = tt_string;
        ok = read_string();
        break;
    case '/':
        token.type_ = tt_comment;
        ok = read_comment();
        break;
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
    case '-':
        token.type_ = tt_number;
        read_number();
        break;
    case 't':
        token.type_ = tt_true;
        ok = match("rue", 3);
        break;
    case 'f':
        token.type_ = tt_false;
        ok = match("alse", 4);
        break;
    case 'n':
        token.type_ = tt_null;
        ok = match("ull", 3);
        break;
    case ',':
        token.type_ = tt_array_separator;
        break;
    case ':':
        token.type_ = tt_member_separator;
        break;
    case 0:
        token.type_ = tt_end_of_stream;
        break;
    default:
        ok = false;
        break;
    }
    if (!ok)
        token.type_ = tt_error;
    token.end_ = current_;
    return true;
}

void reader::skip_spaces()
{
    while (current_ != end_) {
        char c = *current_;
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n')
            ++current_;
        else
            break;
    }
}

bool reader::match(location_t pattern, int pattern_length)
{
    if (end_ - current_ < pattern_length)
        return false;
    int index = pattern_length;
    while (index--)
        if (current_[index] != pattern[index])
            return false;
    current_ += pattern_length;
    return true;
}

bool reader::read_comment()
{
    location_t commentBegin = current_ - 1;
    char c = get_next_char();
    bool successful = false;
    if (c == '*')
        successful = read_c_style_comment();
    else if (c == '/')
        successful = read_cpp_style_comment();
    if (!successful)
        return false;

    if (collect_comments_) {
        comment_placement placement = comment_before;
        if (last_value_end_ && !contains_new_line(last_value_end_, commentBegin)) {
            if (c != '*' || !contains_new_line(commentBegin, current_))
                placement = comment_after_on_same_line;
        }

        add_comment(commentBegin, current_, placement);
    }
    return true;
}

static std::string normalize_eol(reader::location_t begin, reader::location_t end)
{
    std::string normalized;
    normalized.reserve(end - begin);
    reader::location_t current = begin;
    while (current != end) {
        char c = *current++;
        if (c == '\r') {
            if (current != end && *current == '\n')
                // convert dos EOL
                ++current;
            // convert Mac EOL
            normalized += '\n';
        }
        else {
            normalized += c;
        }
    }
    return normalized;
}

void reader::add_comment(location_t begin, location_t end, comment_placement placement)
{
    assert(collect_comments_);
    std::string const& normalized = normalize_eol(begin, end);
    if (placement == comment_after_on_same_line) {
        assert(last_value_ != 0);
        last_value_->set_comment(normalized, placement);
    }
    else {
        comments_before_ += normalized;
    }
}

bool reader::read_c_style_comment()
{
    while (current_ != end_) {
        char c = get_next_char();
        if (c == '*' && *current_ == '/')
            break;
    }
    return get_next_char() == '/';
}

bool reader::read_cpp_style_comment()
{
    while (current_ != end_) {
        char c = get_next_char();
        if (c == '\n')
            break;
        if (c == '\r') {
            // Consume DOS EOL. It will be normalized in add_comment.
            if (current_ != end_ && *current_ == '\n')
                get_next_char();
            // Break on Moc OS 9 EOL.
            break;
        }
    }
    return true;
}

void reader::read_number()
{
    const char* p = current_;
    char c = '0'; // stopgap for already consumed character
    // integral part
    while (c >= '0' && c <= '9')
        c = (current_ = p) < end_ ? *p++ : 0;
    // fractional part
    if (c == '.') {
        c = (current_ = p) < end_ ? *p++ : 0;
        while (c >= '0' && c <= '9')
            c = (current_ = p) < end_ ? *p++ : 0;
    }
    // exponential part
    if (c == 'e' || c == 'E') {
        c = (current_ = p) < end_ ? *p++ : 0;
        if (c == '+' || c == '-')
            c = (current_ = p) < end_ ? *p++ : 0;
        while (c >= '0' && c <= '9')
            c = (current_ = p) < end_ ? *p++ : 0;
    }
}

bool reader::read_string()
{
    char c = 0;
    while (current_ != end_) {
        c = get_next_char();
        if (c == '\\')
            get_next_char();
        else if (c == '"')
            break;
    }
    return c == '"';
}

bool reader::read_object(token& token_start)
{
    token token_name;
    std::string name;
    value init(vt_object);
    current_value().swap_payload(init);
    current_value().set_offset_start(token_start.start_ - begin_);
    while (read_token(token_name)) {
        bool initial_token_ok = true;
        while (token_name.type_ == tt_comment && initial_token_ok)
            initial_token_ok = read_token(token_name);
        if (!initial_token_ok)
            break;
        if (token_name.type_ == tt_object_end && name.empty()) // empty object
            return true;
        name = "";
        if (token_name.type_ == tt_string) {
            if (!decode_string(token_name, name))
                return recover_from_error(tt_object_end);
        }
        else if (token_name.type_ == tt_number && features_.allow_numeric_keys_) {
            value number_name;
            if (!decode_number(token_name, number_name))
                return recover_from_error(tt_object_end);
            name = number_name.as_string();
        }
        else {
            break;
        }

        token colon;
        if (!read_token(colon) || colon.type_ != tt_member_separator) {
            return add_error_and_recover(
                "Missing ':' after object member name", colon, tt_object_end);
        }
        value& value = current_value()[name];
        nodes_.push(&value);
        bool ok = read_value();
        nodes_.pop();
        if (!ok) // error already set
            return recover_from_error(tt_object_end);

        token comma;
        if (!read_token(comma) || (comma.type_ != tt_object_end && comma.type_ != tt_array_separator && comma.type_ != tt_comment)) {
            return add_error_and_recover(
                "Missing ',' or '}' in object declaration", comma, tt_object_end);
        }
        bool finalizeTokenOk = true;
        while (comma.type_ == tt_comment && finalizeTokenOk)
            finalizeTokenOk = read_token(comma);
        if (comma.type_ == tt_object_end)
            return true;
    }
    return add_error_and_recover(
        "Missing '}' or object member name", token_name, tt_object_end);
}

bool reader::read_array(token& token_start)
{
    value init(vt_array);
    current_value().swap_payload(init);
    current_value().set_offset_start(token_start.start_ - begin_);
    skip_spaces();
    if (*current_ == ']') // empty array
    {
        token endArray;
        read_token(endArray);
        return true;
    }
    int index = 0;
    for (;;) {
        value& value = current_value()[index++];
        nodes_.push(&value);
        bool ok = read_value();
        nodes_.pop();
        if (!ok) // error already set
            return recover_from_error(tt_array_end);

        token token;
        // Accept Comment after last item in the array.
        ok = read_token(token);
        while (token.type_ == tt_comment && ok) {
            ok = read_token(token);
        }
        bool badTokenType = (token.type_ != tt_array_separator && token.type_ != tt_array_end);
        if (!ok || badTokenType) {
            return add_error_and_recover(
                "Missing ',' or ']' in array declaration", token, tt_array_end);
        }
        if (token.type_ == tt_array_end)
            break;
    }
    return true;
}

bool reader::decode_number(token& token)
{
    value decoded;
    if (!decode_number(token, decoded))
        return false;
    current_value().swap_payload(decoded);
    current_value().set_offset_start(token.start_ - begin_);
    current_value().set_offset_limit(token.end_ - begin_);
    return true;
}

bool reader::decode_number(token& token, value& decoded)
{
    // Attempts to parse the number as an integer. If the number is
    // larger than the maximum supported value of an integer then
    // we decode the number as a double.
    location_t current = token.start_;
    bool is_negative = *current == '-';
    if (is_negative)
        ++current;
    // TODO: Help the compiler do the div and mod at compile time or get rid of them.
    value::largest_uint_t max_integer_value = is_negative ? value::largest_uint_t(-value::min_largest_int)
                                                          : value::max_largest_uint;
    value::largest_uint_t threshold = max_integer_value / 10;
    value::largest_uint_t value = 0;
    while (current < token.end_) {
        char c = *current++;
        if (c < '0' || c > '9')
            return decode_double(token, decoded);
        uint32_t digit(c - '0');
        if (value >= threshold) {
            // We've hit or exceeded the max value divided by 10 (rounded down). If
            // a) we've only just touched the limit, b) this is the last digit, and
            // c) it's small enough to fit in that rounding delta, we're okay.
            // Otherwise treat this number as a double to avoid overflow.
            if (value > threshold || current != token.end_ || digit > max_integer_value % 10) {
                return decode_double(token, decoded);
            }
        }
        value = value * 10 + digit;
    }
    if (is_negative)
        decoded = -value::largest_int_t(value);
    else if (value <= value::largest_uint_t(value::max_int))
        decoded = value::largest_int_t(value);
    else
        decoded = value;
    return true;
}

bool reader::decode_double(token& token)
{
    value decoded;
    if (!decode_double(token, decoded))
        return false;
    current_value().swap_payload(decoded);
    current_value().set_offset_start(token.start_ - begin_);
    current_value().set_offset_limit(token.end_ - begin_);
    return true;
}

bool reader::decode_double(token& token, value& decoded)
{
    double value = 0;
    const int bufferSize = 32;
    int count;
    int length = int(token.end_ - token.start_);

    // Sanity check to avoid buffer overflow exploits.
    if (length < 0) {
        return add_error("Unable to parse token length", token);
    }

    // Avoid using a string constant for the format control string given to
    // sscanf, as this can cause hard to debug crashes on OS X. See here for more
    // info:
    //
    //     http://developer.apple.com/library/mac/#DOCUMENTATION/DeveloperTools/gcc-4.0.1/gcc/Incompatibilities.html
    char format[] = "%lf";

    if (length <= bufferSize) {
        char buffer[bufferSize + 1];
        memcpy(buffer, token.start_, length);
        buffer[length] = 0;
        count = sscanf(buffer, format, &value);
    }
    else {
        std::string buffer(token.start_, token.end_);
        count = sscanf(buffer.c_str(), format, &value);
    }

    if (count != 1)
        return add_error("'" + std::string(token.start_, token.end_) + "' is not a number.",
            token);
    decoded = value;
    return true;
}

bool reader::decode_string(token& token)
{
    std::string decoded_string;
    if (!decode_string(token, decoded_string))
        return false;
    value decoded(decoded_string);
    current_value().swap_payload(decoded);
    current_value().set_offset_start(token.start_ - begin_);
    current_value().set_offset_limit(token.end_ - begin_);
    return true;
}

bool reader::decode_string(token& token, std::string& decoded)
{
    decoded.reserve(token.end_ - token.start_ - 2);
    location_t current = token.start_ + 1; // skip '"'
    location_t end = token.end_ - 1; // do not include '"'
    while (current != end) {
        char c = *current++;
        if (c == '"')
            break;
        else if (c == '\\') {
            if (current == end)
                return add_error("Empty escape sequence in string", token, current);
            char escape = *current++;
            switch (escape) {
            case '"':
                decoded += '"';
                break;
            case '/':
                decoded += '/';
                break;
            case '\\':
                decoded += '\\';
                break;
            case 'b':
                decoded += '\b';
                break;
            case 'f':
                decoded += '\f';
                break;
            case 'n':
                decoded += '\n';
                break;
            case 'r':
                decoded += '\r';
                break;
            case 't':
                decoded += '\t';
                break;
            case 'u': {
                unsigned int unicode;
                if (!decode_unicode_codepoint(token, current, end, unicode))
                    return false;
                decoded += codepoint_to_utf8(unicode);
            } break;
            default:
                return add_error("Bad escape sequence in string", token, current);
            }
        }
        else {
            decoded += c;
        }
    }
    return true;
}

bool reader::decode_unicode_codepoint(token& token,
    location_t& current,
    location_t end,
    unsigned int& unicode)
{

    if (!decode_unicode_escape_sequence(token, current, end, unicode))
        return false;
    if (unicode >= 0xD800 && unicode <= 0xDBFF) {
        // surrogate pairs
        if (end - current < 6)
            return add_error(
                "additional six characters expected to parse unicode surrogate pair.",
                token,
                current);
        unsigned int surrogatePair;
        if (*(current++) == '\\' && *(current++) == 'u') {
            if (decode_unicode_escape_sequence(token, current, end, surrogatePair)) {
                unicode = 0x10000 + ((unicode & 0x3FF) << 10) + (surrogatePair & 0x3FF);
            }
            else
                return false;
        }
        else
            return add_error("expecting another \\u token to begin the second half of "
                             "a unicode surrogate pair",
                token,
                current);
    }
    return true;
}

bool reader::decode_unicode_escape_sequence(token& token,
    location_t& current,
    location_t end,
    unsigned int& unicode)
{
    if (end - current < 4)
        return add_error(
            "Bad unicode escape sequence in string: four digits expected.",
            token,
            current);
    unicode = 0;
    for (int index = 0; index < 4; ++index) {
        char c = *current++;
        unicode *= 16;
        if (c >= '0' && c <= '9')
            unicode += c - '0';
        else if (c >= 'a' && c <= 'f')
            unicode += c - 'a' + 10;
        else if (c >= 'A' && c <= 'F')
            unicode += c - 'A' + 10;
        else
            return add_error(
                "Bad unicode escape sequence in string: hexadecimal digit expected.",
                token,
                current);
    }
    return true;
}

bool reader::add_error(std::string const& message, token& token, location_t extra)
{
    error_info info;
    info.token_ = token;
    info.message_ = message;
    info.extra_ = extra;
    errors_.push_back(info);
    return false;
}

bool reader::recover_from_error(token_type skip_until_token)
{
    int error_count = int(errors_.size());
    token skip;
    for (;;) {
        if (!read_token(skip))
            errors_.resize(error_count); // discard errors caused by recovery
        if (skip.type_ == skip_until_token || skip.type_ == tt_end_of_stream)
            break;
    }
    errors_.resize(error_count);
    return false;
}

bool reader::add_error_and_recover(std::string const& message,
    token& token,
    token_type skip_until_token)
{
    add_error(message, token);
    return recover_from_error(skip_until_token);
}

value& reader::current_value() { return *(nodes_.top()); }

char reader::get_next_char()
{
    if (current_ == end_)
        return 0;
    return *current_++;
}

void reader::get_location_line_and_column(location_t location,
    int& line,
    int& column) const
{
    location_t current = begin_;
    location_t last_line_start = current;
    line = 0;
    while (current < location && current != end_) {
        char c = *current++;
        if (c == '\r') {
            if (*current == '\n')
                ++current;
            last_line_start = current;
            ++line;
        }
        else if (c == '\n') {
            last_line_start = current;
            ++line;
        }
    }
    // column & line start at 1
    column = int(location - last_line_start) + 1;
    ++line;
}

std::string reader::get_location_line_and_column(location_t location) const
{
    int line, column;
    get_location_line_and_column(location, line, column);
    char buffer[18 + 16 + 16 + 1];
#if defined(_MSC_VER) && defined(__STDC_SECURE_LIB__)
#if defined(WINCE)
    _snprintf(buffer, sizeof(buffer), "Line %d, Column %d", line, column);
#else
    sprintf_s(buffer, sizeof(buffer), "Line %d, Column %d", line, column);
#endif
#else
    snprintf(buffer, sizeof(buffer), "Line %d, Column %d", line, column);
#endif
    return buffer;
}

std::string reader::get_formatted_messages() const
{
    std::string formattedMessage;
    for (errors::const_iterator itError = errors_.begin();
         itError != errors_.end();
         ++itError) {
        error_info const& error = *itError;
        formattedMessage += "* " + get_location_line_and_column(error.token_.start_) + "\n";
        formattedMessage += "  " + error.message_ + "\n";
        if (error.extra_)
            formattedMessage += "See " + get_location_line_and_column(error.extra_) + " for detail.\n";
    }
    return formattedMessage;
}

std::vector<reader::structured_error> reader::get_structured_errors() const
{
    std::vector<reader::structured_error> allErrors;
    for (errors::const_iterator itError = errors_.begin();
         itError != errors_.end();
         ++itError) {
        error_info const& error = *itError;
        reader::structured_error structured;
        structured.offset_start = error.token_.start_ - begin_;
        structured.offset_limit = error.token_.end_ - begin_;
        structured.message = error.message_;
        allErrors.push_back(structured);
    }
    return allErrors;
}

bool reader::push_error(value const& value, std::string const& message)
{
    size_t length = end_ - begin_;
    if (value.get_offset_start() > length
        || value.get_offset_limit() > length)
        return false;
    token token;
    token.type_ = tt_error;
    token.start_ = begin_ + value.get_offset_start();
    token.end_ = end_ + value.get_offset_limit();
    error_info info;
    info.token_ = token;
    info.message_ = message;
    info.extra_ = 0;
    errors_.push_back(info);
    return true;
}

bool reader::push_error(class value const& value, std::string const& message, class value const& extra)
{
    size_t length = end_ - begin_;
    if (value.get_offset_start() > length
        || value.get_offset_limit() > length
        || extra.get_offset_limit() > length)
        return false;
    token token;
    token.type_ = tt_error;
    token.start_ = begin_ + value.get_offset_start();
    token.end_ = begin_ + value.get_offset_limit();
    error_info info;
    info.token_ = token;
    info.message_ = message;
    info.extra_ = begin_ + extra.get_offset_start();
    errors_.push_back(info);
    return true;
}

bool reader::good() const
{
    return !errors_.size();
}

// exact copy of features
class our_features {
public:
    static our_features all();
    our_features();
    bool allow_comments_;
    bool strict_root_;
    bool allow_dropped_null_placeholders_;
    bool allow_numeric_keys_;
    bool allow_single_quotes_;
    bool fail_if_extra_;
    bool reject_dup_keys_;
    int stack_limit_;
}; // our_features

// exact copy of Implementation of class features
// ////////////////////////////////

our_features::our_features()
    : allow_comments_(true)
    , strict_root_(false)
    , allow_dropped_null_placeholders_(false)
    , allow_numeric_keys_(false)
    , allow_single_quotes_(false)
    , fail_if_extra_(false)
{
}

our_features our_features::all() { return our_features(); }

// Implementation of class reader
// ////////////////////////////////

// exact copy of reader, renamed to our_reader
class our_reader {
public:
    typedef const char* location_t;
    struct structured_error {
        size_t offset_start;
        size_t offset_limit;
        std::string message;
    };

    our_reader(our_features const& features);
    bool parse(const char* begin_doc,
        const char* end_doc,
        value& root,
        bool collect_comments = true);
    std::string get_formatted_messages() const;
    std::vector<structured_error> get_structured_errors() const;
    bool push_error(value const&, std::string const& message);
    bool push_error(value const&, std::string const& message, class value const& extra);
    bool good() const;

private:
    our_reader(our_reader const&); // no impl
    void operator=(our_reader const&); // no impl

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

    class error_info {
    public:
        token token_;
        std::string message_;
        location_t extra_;
    };

    typedef std::deque<error_info> errors;

    bool read_token(token& token);
    void skip_spaces();
    bool match(location_t pattern, int pattern_length);
    bool read_comment();
    bool read_c_style_comment();
    bool read_cpp_style_comment();
    bool read_string();
    bool read_string_single_quote();
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
    bool add_error(std::string const& message, token& token, location_t extra = 0);
    bool recover_from_error(token_type skip_until_token);
    bool add_error_and_recover(std::string const& message,
        token& token,
        token_type skip_until_token);
    void skip_until_space();
    value& current_value();
    char get_next_char();
    void get_location_line_and_column(location_t location, int& line, int& column) const;
    std::string get_location_line_and_column(location_t location) const;
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
    int stack_depth_;

    our_features const features_;
    bool collect_comments_;
}; // our_reader

// complete copy of Read impl, for our_reader

our_reader::our_reader(our_features const& features)
    : errors_()
    , document_()
    , begin_()
    , end_()
    , current_()
    , last_value_end_()
    , last_value_()
    , comments_before_()
    , features_(features)
    , collect_comments_()
{
}

bool our_reader::parse(const char* begin_doc,
    const char* end_doc,
    value& root,
    bool collect_comments)
{
    if (!features_.allow_comments_) {
        collect_comments = false;
    }

    begin_ = begin_doc;
    end_ = end_doc;
    collect_comments_ = collect_comments;
    current_ = begin_;
    last_value_end_ = 0;
    last_value_ = 0;
    comments_before_ = "";
    errors_.clear();
    while (!nodes_.empty())
        nodes_.pop();
    nodes_.push(&root);

    stack_depth_ = 0;
    bool successful = read_value();
    token token;
    skip_comment_tokens(token);
    if (features_.fail_if_extra_) {
        if (token.type_ != tt_error && token.type_ != tt_end_of_stream) {
            add_error("Extra non-whitespace after JSON value.", token);
            return false;
        }
    }
    if (collect_comments_ && !comments_before_.empty())
        root.set_comment(comments_before_, comment_after);
    if (features_.strict_root_) {
        if (!root.is_array() && !root.is_object()) {
            // Set error location to start of doc, ideally should be first token found
            // in doc
            token.type_ = tt_error;
            token.start_ = begin_doc;
            token.end_ = end_doc;
            add_error(
                "A valid JSON document must be either an array or an object value.",
                token);
            return false;
        }
    }
    return successful;
}

bool our_reader::read_value()
{
    if (stack_depth_ >= features_.stack_limit_)
        throw_runtime_error("Exceeded stack_limit in read_value().");
    ++stack_depth_;
    token token;
    skip_comment_tokens(token);
    bool successful = true;

    if (collect_comments_ && !comments_before_.empty()) {
        current_value().set_comment(comments_before_, comment_before);
        comments_before_ = "";
    }

    switch (token.type_) {
    case tt_object_begin:
        successful = read_object(token);
        current_value().set_offset_limit(current_ - begin_);
        break;
    case tt_array_begin:
        successful = read_array(token);
        current_value().set_offset_limit(current_ - begin_);
        break;
    case tt_number:
        successful = decode_number(token);
        break;
    case tt_string:
        successful = decode_string(token);
        break;
    case tt_true: {
        value v(true);
        current_value().swap_payload(v);
        current_value().set_offset_start(token.start_ - begin_);
        current_value().set_offset_limit(token.end_ - begin_);
    } break;
    case tt_false: {
        value v(false);
        current_value().swap_payload(v);
        current_value().set_offset_start(token.start_ - begin_);
        current_value().set_offset_limit(token.end_ - begin_);
    } break;
    case tt_null: {
        value v;
        current_value().swap_payload(v);
        current_value().set_offset_start(token.start_ - begin_);
        current_value().set_offset_limit(token.end_ - begin_);
    } break;
    case tt_array_separator:
    case tt_object_end:
    case tt_array_end:
        if (features_.allow_dropped_null_placeholders_) {
            // "Un-read" the current token and mark the current value as a null
            // token.
            current_--;
            value v;
            current_value().swap_payload(v);
            current_value().set_offset_start(current_ - begin_ - 1);
            current_value().set_offset_limit(current_ - begin_);
            break;
        } // else, fall through ...
    default:
        current_value().set_offset_start(token.start_ - begin_);
        current_value().set_offset_limit(token.end_ - begin_);
        return add_error("Syntax error: value, object or array expected.", token);
    }

    if (collect_comments_) {
        last_value_end_ = current_;
        last_value_ = &current_value();
    }

    --stack_depth_;
    return successful;
}

void our_reader::skip_comment_tokens(token& token)
{
    if (features_.allow_comments_) {
        do {
            read_token(token);
        } while (token.type_ == tt_comment);
    }
    else {
        read_token(token);
    }
}

bool our_reader::read_token(token& token)
{
    skip_spaces();
    token.start_ = current_;
    char c = get_next_char();
    bool ok = true;
    switch (c) {
    case '{':
        token.type_ = tt_object_begin;
        break;
    case '}':
        token.type_ = tt_object_end;
        break;
    case '[':
        token.type_ = tt_array_begin;
        break;
    case ']':
        token.type_ = tt_array_end;
        break;
    case '"':
        token.type_ = tt_string;
        ok = read_string();
        break;
    case '\'':
        if (features_.allow_single_quotes_) {
            token.type_ = tt_string;
            ok = read_string_single_quote();
            break;
        } // else continue
    case '/':
        token.type_ = tt_comment;
        ok = read_comment();
        break;
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
    case '-':
        token.type_ = tt_number;
        read_number();
        break;
    case 't':
        token.type_ = tt_true;
        ok = match("rue", 3);
        break;
    case 'f':
        token.type_ = tt_false;
        ok = match("alse", 4);
        break;
    case 'n':
        token.type_ = tt_null;
        ok = match("ull", 3);
        break;
    case ',':
        token.type_ = tt_array_separator;
        break;
    case ':':
        token.type_ = tt_member_separator;
        break;
    case 0:
        token.type_ = tt_end_of_stream;
        break;
    default:
        ok = false;
        break;
    }
    if (!ok)
        token.type_ = tt_error;
    token.end_ = current_;
    return true;
}

void our_reader::skip_spaces()
{
    while (current_ != end_) {
        char c = *current_;
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n')
            ++current_;
        else
            break;
    }
}

bool our_reader::match(location_t pattern, int pattern_length)
{
    if (end_ - current_ < pattern_length)
        return false;
    int index = pattern_length;
    while (index--)
        if (current_[index] != pattern[index])
            return false;
    current_ += pattern_length;
    return true;
}

bool our_reader::read_comment()
{
    location_t commentBegin = current_ - 1;
    char c = get_next_char();
    bool successful = false;
    if (c == '*')
        successful = read_c_style_comment();
    else if (c == '/')
        successful = read_cpp_style_comment();
    if (!successful)
        return false;

    if (collect_comments_) {
        comment_placement placement = comment_before;
        if (last_value_end_ && !contains_new_line(last_value_end_, commentBegin)) {
            if (c != '*' || !contains_new_line(commentBegin, current_))
                placement = comment_after_on_same_line;
        }

        add_comment(commentBegin, current_, placement);
    }
    return true;
}

void our_reader::add_comment(location_t begin, location_t end, comment_placement placement)
{
    assert(collect_comments_);
    std::string const& normalized = normalize_eol(begin, end);
    if (placement == comment_after_on_same_line) {
        assert(last_value_ != 0);
        last_value_->set_comment(normalized, placement);
    }
    else {
        comments_before_ += normalized;
    }
}

bool our_reader::read_c_style_comment()
{
    while (current_ != end_) {
        char c = get_next_char();
        if (c == '*' && *current_ == '/')
            break;
    }
    return get_next_char() == '/';
}

bool our_reader::read_cpp_style_comment()
{
    while (current_ != end_) {
        char c = get_next_char();
        if (c == '\n')
            break;
        if (c == '\r') {
            // Consume DOS EOL. It will be normalized in add_comment.
            if (current_ != end_ && *current_ == '\n')
                get_next_char();
            // Break on Moc OS 9 EOL.
            break;
        }
    }
    return true;
}

void our_reader::read_number()
{
    const char* p = current_;
    char c = '0'; // stopgap for already consumed character
    // integral part
    while (c >= '0' && c <= '9')
        c = (current_ = p) < end_ ? *p++ : 0;
    // fractional part
    if (c == '.') {
        c = (current_ = p) < end_ ? *p++ : 0;
        while (c >= '0' && c <= '9')
            c = (current_ = p) < end_ ? *p++ : 0;
    }
    // exponential part
    if (c == 'e' || c == 'E') {
        c = (current_ = p) < end_ ? *p++ : 0;
        if (c == '+' || c == '-')
            c = (current_ = p) < end_ ? *p++ : 0;
        while (c >= '0' && c <= '9')
            c = (current_ = p) < end_ ? *p++ : 0;
    }
}
bool our_reader::read_string()
{
    char c = 0;
    while (current_ != end_) {
        c = get_next_char();
        if (c == '\\')
            get_next_char();
        else if (c == '"')
            break;
    }
    return c == '"';
}

bool our_reader::read_string_single_quote()
{
    char c = 0;
    while (current_ != end_) {
        c = get_next_char();
        if (c == '\\')
            get_next_char();
        else if (c == '\'')
            break;
    }
    return c == '\'';
}

bool our_reader::read_object(token& token_start)
{
    token token_name;
    std::string name;
    value init(vt_object);
    current_value().swap_payload(init);
    current_value().set_offset_start(token_start.start_ - begin_);
    while (read_token(token_name)) {
        bool initial_token_ok = true;
        while (token_name.type_ == tt_comment && initial_token_ok)
            initial_token_ok = read_token(token_name);
        if (!initial_token_ok)
            break;
        if (token_name.type_ == tt_object_end && name.empty()) // empty object
            return true;
        name = "";
        if (token_name.type_ == tt_string) {
            if (!decode_string(token_name, name))
                return recover_from_error(tt_object_end);
        }
        else if (token_name.type_ == tt_number && features_.allow_numeric_keys_) {
            value number_name;
            if (!decode_number(token_name, number_name))
                return recover_from_error(tt_object_end);
            name = number_name.as_string();
        }
        else {
            break;
        }

        token colon;
        if (!read_token(colon) || colon.type_ != tt_member_separator) {
            return add_error_and_recover(
                "Missing ':' after object member name", colon, tt_object_end);
        }
        if (name.length() >= (1U << 30))
            throw_runtime_error("keylength >= 2^30");
        if (features_.reject_dup_keys_ && current_value().is_member(name)) {
            std::string msg = "Duplicate key: '" + name + "'";
            return add_error_and_recover(
                msg, token_name, tt_object_end);
        }
        value& value = current_value()[name];
        nodes_.push(&value);
        bool ok = read_value();
        nodes_.pop();
        if (!ok) // error already set
            return recover_from_error(tt_object_end);

        token comma;
        if (!read_token(comma) || (comma.type_ != tt_object_end && comma.type_ != tt_array_separator && comma.type_ != tt_comment)) {
            return add_error_and_recover(
                "Missing ',' or '}' in object declaration", comma, tt_object_end);
        }
        bool finalizeTokenOk = true;
        while (comma.type_ == tt_comment && finalizeTokenOk)
            finalizeTokenOk = read_token(comma);
        if (comma.type_ == tt_object_end)
            return true;
    }
    return add_error_and_recover(
        "Missing '}' or object member name", token_name, tt_object_end);
}

bool our_reader::read_array(token& token_start)
{
    value init(vt_array);
    current_value().swap_payload(init);
    current_value().set_offset_start(token_start.start_ - begin_);
    skip_spaces();
    if (*current_ == ']') // empty array
    {
        token endArray;
        read_token(endArray);
        return true;
    }
    int index = 0;
    for (;;) {
        value& value = current_value()[index++];
        nodes_.push(&value);
        bool ok = read_value();
        nodes_.pop();
        if (!ok) // error already set
            return recover_from_error(tt_array_end);

        token token;
        // Accept Comment after last item in the array.
        ok = read_token(token);
        while (token.type_ == tt_comment && ok) {
            ok = read_token(token);
        }
        bool badTokenType = (token.type_ != tt_array_separator && token.type_ != tt_array_end);
        if (!ok || badTokenType) {
            return add_error_and_recover(
                "Missing ',' or ']' in array declaration", token, tt_array_end);
        }
        if (token.type_ == tt_array_end)
            break;
    }
    return true;
}

bool our_reader::decode_number(token& token)
{
    value decoded;
    if (!decode_number(token, decoded))
        return false;
    current_value().swap_payload(decoded);
    current_value().set_offset_start(token.start_ - begin_);
    current_value().set_offset_limit(token.end_ - begin_);
    return true;
}

bool our_reader::decode_number(token& token, value& decoded)
{
    // Attempts to parse the number as an integer. If the number is
    // larger than the maximum supported value of an integer then
    // we decode the number as a double.
    location_t current = token.start_;
    bool is_negative = *current == '-';
    if (is_negative)
        ++current;
    // TODO: Help the compiler do the div and mod at compile time or get rid of them.
    value::largest_uint_t max_integer_value = is_negative ? value::largest_uint_t(-value::min_largest_int)
                                                          : value::max_largest_uint;
    value::largest_uint_t threshold = max_integer_value / 10;
    value::largest_uint_t value = 0;
    while (current < token.end_) {
        char c = *current++;
        if (c < '0' || c > '9')
            return decode_double(token, decoded);
        uint32_t digit(c - '0');
        if (value >= threshold) {
            // We've hit or exceeded the max value divided by 10 (rounded down). If
            // a) we've only just touched the limit, b) this is the last digit, and
            // c) it's small enough to fit in that rounding delta, we're okay.
            // Otherwise treat this number as a double to avoid overflow.
            if (value > threshold || current != token.end_ || digit > max_integer_value % 10) {
                return decode_double(token, decoded);
            }
        }
        value = value * 10 + digit;
    }
    if (is_negative)
        decoded = -value::largest_int_t(value);
    else if (value <= value::largest_uint_t(value::max_int))
        decoded = value::largest_int_t(value);
    else
        decoded = value;
    return true;
}

bool our_reader::decode_double(token& token)
{
    value decoded;
    if (!decode_double(token, decoded))
        return false;
    current_value().swap_payload(decoded);
    current_value().set_offset_start(token.start_ - begin_);
    current_value().set_offset_limit(token.end_ - begin_);
    return true;
}

bool our_reader::decode_double(token& token, value& decoded)
{
    double value = 0;
    const int bufferSize = 32;
    int count;
    int length = int(token.end_ - token.start_);

    // Sanity check to avoid buffer overflow exploits.
    if (length < 0) {
        return add_error("Unable to parse token length", token);
    }

    // Avoid using a string constant for the format control string given to
    // sscanf, as this can cause hard to debug crashes on OS X. See here for more
    // info:
    //
    //     http://developer.apple.com/library/mac/#DOCUMENTATION/DeveloperTools/gcc-4.0.1/gcc/Incompatibilities.html
    char format[] = "%lf";

    if (length <= bufferSize) {
        char buffer[bufferSize + 1];
        memcpy(buffer, token.start_, length);
        buffer[length] = 0;
        count = sscanf(buffer, format, &value);
    }
    else {
        std::string buffer(token.start_, token.end_);
        count = sscanf(buffer.c_str(), format, &value);
    }

    if (count != 1)
        return add_error("'" + std::string(token.start_, token.end_) + "' is not a number.",
            token);
    decoded = value;
    return true;
}

bool our_reader::decode_string(token& token)
{
    std::string decoded_string;
    if (!decode_string(token, decoded_string))
        return false;
    value decoded(decoded_string);
    current_value().swap_payload(decoded);
    current_value().set_offset_start(token.start_ - begin_);
    current_value().set_offset_limit(token.end_ - begin_);
    return true;
}

bool our_reader::decode_string(token& token, std::string& decoded)
{
    decoded.reserve(token.end_ - token.start_ - 2);
    location_t current = token.start_ + 1; // skip '"'
    location_t end = token.end_ - 1; // do not include '"'
    while (current != end) {
        char c = *current++;
        if (c == '"')
            break;
        else if (c == '\\') {
            if (current == end)
                return add_error("Empty escape sequence in string", token, current);
            char escape = *current++;
            switch (escape) {
            case '"':
                decoded += '"';
                break;
            case '/':
                decoded += '/';
                break;
            case '\\':
                decoded += '\\';
                break;
            case 'b':
                decoded += '\b';
                break;
            case 'f':
                decoded += '\f';
                break;
            case 'n':
                decoded += '\n';
                break;
            case 'r':
                decoded += '\r';
                break;
            case 't':
                decoded += '\t';
                break;
            case 'u': {
                unsigned int unicode;
                if (!decode_unicode_codepoint(token, current, end, unicode))
                    return false;
                decoded += codepoint_to_utf8(unicode);
            } break;
            default:
                return add_error("Bad escape sequence in string", token, current);
            }
        }
        else {
            decoded += c;
        }
    }
    return true;
}

bool our_reader::decode_unicode_codepoint(token& token,
    location_t& current,
    location_t end,
    unsigned int& unicode)
{

    if (!decode_unicode_escape_sequence(token, current, end, unicode))
        return false;
    if (unicode >= 0xD800 && unicode <= 0xDBFF) {
        // surrogate pairs
        if (end - current < 6)
            return add_error(
                "additional six characters expected to parse unicode surrogate pair.",
                token,
                current);
        unsigned int surrogatePair;
        if (*(current++) == '\\' && *(current++) == 'u') {
            if (decode_unicode_escape_sequence(token, current, end, surrogatePair)) {
                unicode = 0x10000 + ((unicode & 0x3FF) << 10) + (surrogatePair & 0x3FF);
            }
            else
                return false;
        }
        else
            return add_error("expecting another \\u token to begin the second half of "
                             "a unicode surrogate pair",
                token,
                current);
    }
    return true;
}

bool our_reader::decode_unicode_escape_sequence(token& token,
    location_t& current,
    location_t end,
    unsigned int& unicode)
{
    if (end - current < 4)
        return add_error(
            "Bad unicode escape sequence in string: four digits expected.",
            token,
            current);
    unicode = 0;
    for (int index = 0; index < 4; ++index) {
        char c = *current++;
        unicode *= 16;
        if (c >= '0' && c <= '9')
            unicode += c - '0';
        else if (c >= 'a' && c <= 'f')
            unicode += c - 'a' + 10;
        else if (c >= 'A' && c <= 'F')
            unicode += c - 'A' + 10;
        else
            return add_error(
                "Bad unicode escape sequence in string: hexadecimal digit expected.",
                token,
                current);
    }
    return true;
}

bool our_reader::add_error(std::string const& message, token& token, location_t extra)
{
    error_info info;
    info.token_ = token;
    info.message_ = message;
    info.extra_ = extra;
    errors_.push_back(info);
    return false;
}

bool our_reader::recover_from_error(token_type skip_until_token)
{
    int error_count = int(errors_.size());
    token skip;
    for (;;) {
        if (!read_token(skip))
            errors_.resize(error_count); // discard errors caused by recovery
        if (skip.type_ == skip_until_token || skip.type_ == tt_end_of_stream)
            break;
    }
    errors_.resize(error_count);
    return false;
}

bool our_reader::add_error_and_recover(std::string const& message,
    token& token,
    token_type skip_until_token)
{
    add_error(message, token);
    return recover_from_error(skip_until_token);
}

value& our_reader::current_value() { return *(nodes_.top()); }

char our_reader::get_next_char()
{
    if (current_ == end_)
        return 0;
    return *current_++;
}

void our_reader::get_location_line_and_column(location_t location,
    int& line,
    int& column) const
{
    location_t current = begin_;
    location_t last_line_start = current;
    line = 0;
    while (current < location && current != end_) {
        char c = *current++;
        if (c == '\r') {
            if (*current == '\n')
                ++current;
            last_line_start = current;
            ++line;
        }
        else if (c == '\n') {
            last_line_start = current;
            ++line;
        }
    }
    // column & line start at 1
    column = int(location - last_line_start) + 1;
    ++line;
}

std::string our_reader::get_location_line_and_column(location_t location) const
{
    int line, column;
    get_location_line_and_column(location, line, column);
    char buffer[18 + 16 + 16 + 1];
#if defined(_MSC_VER) && defined(__STDC_SECURE_LIB__)
#if defined(WINCE)
    _snprintf(buffer, sizeof(buffer), "Line %d, Column %d", line, column);
#else
    sprintf_s(buffer, sizeof(buffer), "Line %d, Column %d", line, column);
#endif
#else
    snprintf(buffer, sizeof(buffer), "Line %d, Column %d", line, column);
#endif
    return buffer;
}

std::string our_reader::get_formatted_messages() const
{
    std::string formattedMessage;
    for (errors::const_iterator itError = errors_.begin();
         itError != errors_.end();
         ++itError) {
        error_info const& error = *itError;
        formattedMessage += "* " + get_location_line_and_column(error.token_.start_) + "\n";
        formattedMessage += "  " + error.message_ + "\n";
        if (error.extra_)
            formattedMessage += "See " + get_location_line_and_column(error.extra_) + " for detail.\n";
    }
    return formattedMessage;
}

std::vector<our_reader::structured_error> our_reader::get_structured_errors() const
{
    std::vector<our_reader::structured_error> allErrors;
    for (errors::const_iterator itError = errors_.begin();
         itError != errors_.end();
         ++itError) {
        error_info const& error = *itError;
        our_reader::structured_error structured;
        structured.offset_start = error.token_.start_ - begin_;
        structured.offset_limit = error.token_.end_ - begin_;
        structured.message = error.message_;
        allErrors.push_back(structured);
    }
    return allErrors;
}

bool our_reader::push_error(value const& value, std::string const& message)
{
    size_t length = end_ - begin_;
    if (value.get_offset_start() > length
        || value.get_offset_limit() > length)
        return false;
    token token;
    token.type_ = tt_error;
    token.start_ = begin_ + value.get_offset_start();
    token.end_ = end_ + value.get_offset_limit();
    error_info info;
    info.token_ = token;
    info.message_ = message;
    info.extra_ = 0;
    errors_.push_back(info);
    return true;
}

bool our_reader::push_error(class value const& value, std::string const& message, class value const& extra)
{
    size_t length = end_ - begin_;
    if (value.get_offset_start() > length
        || value.get_offset_limit() > length
        || extra.get_offset_limit() > length)
        return false;
    token token;
    token.type_ = tt_error;
    token.start_ = begin_ + value.get_offset_start();
    token.end_ = begin_ + value.get_offset_limit();
    error_info info;
    info.token_ = token;
    info.message_ = message;
    info.extra_ = begin_ + extra.get_offset_start();
    errors_.push_back(info);
    return true;
}

bool our_reader::good() const
{
    return !errors_.size();
}

class our_char_reader : public char_reader {
    bool const collect_comments_;
    our_reader reader_;

public:
    our_char_reader(
        bool collect_comments,
        our_features const& features)
        : collect_comments_(collect_comments)
        , reader_(features)
    {
    }
    virtual bool parse(
        char const* begin_doc, char const* end_doc,
        value* root, std::string* errs)
    {
        bool ok = reader_.parse(begin_doc, end_doc, *root, collect_comments_);
        if (errs) {
            *errs = reader_.get_formatted_messages();
        }
        return ok;
    }
};

char_reader_builder::char_reader_builder()
{
    set_defaults(&settings_);
}
char_reader_builder::~char_reader_builder()
{
}
char_reader* char_reader_builder::new_char_reader() const
{
    bool collect_comments = settings_["collect_comments"].as_bool();
    our_features features = our_features::all();
    features.allow_comments_ = settings_["allow_comments"].as_bool();
    features.strict_root_ = settings_["strict_root"].as_bool();
    features.allow_dropped_null_placeholders_ = settings_["allow_dropped_null_placeholders"].as_bool();
    features.allow_numeric_keys_ = settings_["allow_numeric_keys"].as_bool();
    features.allow_single_quotes_ = settings_["allow_single_quotes"].as_bool();
    features.stack_limit_ = settings_["stack_limit"].as_int();
    features.fail_if_extra_ = settings_["fail_if_extra"].as_bool();
    features.reject_dup_keys_ = settings_["reject_dup_keys"].as_bool();
    return new our_char_reader(collect_comments, features);
}
static void get_valid_reader_keys(std::set<std::string>* valid_keys)
{
    valid_keys->clear();
    valid_keys->insert("collect_comments");
    valid_keys->insert("allow_comments");
    valid_keys->insert("strict_root");
    valid_keys->insert("allow_dropped_null_placeholders");
    valid_keys->insert("allow_numeric_keys");
    valid_keys->insert("allow_single_quotes");
    valid_keys->insert("stack_limit");
    valid_keys->insert("fail_if_extra");
    valid_keys->insert("reject_dup_keys");
}
bool char_reader_builder::validate(json::value* invalid) const
{
    json::value my_invalid;
    if (!invalid)
        invalid = &my_invalid; // so we do not need to test for NULL
    json::value& inv = *invalid;
    std::set<std::string> valid_keys;
    get_valid_reader_keys(&valid_keys);
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
value& char_reader_builder::operator[](std::string key)
{
    return settings_[key];
}
// static
void char_reader_builder::strict_mode(json::value* settings)
{
    //! [CharReaderBuilderStrictMode]
    (*settings)["allow_comments"] = false;
    (*settings)["strict_root"] = true;
    (*settings)["allow_dropped_null_placeholders"] = false;
    (*settings)["allow_numeric_keys"] = false;
    (*settings)["allow_single_quotes"] = false;
    (*settings)["fail_if_extra"] = true;
    (*settings)["reject_dup_keys"] = true;
    //! [CharReaderBuilderStrictMode]
}
// static
void char_reader_builder::set_defaults(json::value* settings)
{
    //! [CharReaderBuilderDefaults]
    (*settings)["collect_comments"] = true;
    (*settings)["allow_comments"] = true;
    (*settings)["strict_root"] = false;
    (*settings)["allow_dropped_null_placeholders"] = false;
    (*settings)["allow_numeric_keys"] = false;
    (*settings)["allow_single_quotes"] = false;
    (*settings)["stack_limit"] = 1000;
    (*settings)["fail_if_extra"] = false;
    (*settings)["reject_dup_keys"] = false;
    //! [CharReaderBuilderDefaults]
}

//////////////////////////////////
// global functions

bool parse_from_stream(
    char_reader::factory const& fact, std::istream& sin,
    value* root, std::string* errs)
{
    std::ostringstream ssin;
    ssin << sin.rdbuf();
    std::string doc = ssin.str();
    char const* begin = doc.data();
    char const* end = begin + doc.size();
    // Note that we do not actually need a null-terminator.
    char_reader_ptr const reader(fact.new_char_reader());
    return reader->parse(begin, end, root, errs);
}

std::istream& operator>>(std::istream& sin, value& root)
{
    char_reader_builder b;
    std::string errs;
    bool ok = parse_from_stream(b, sin, &root, &errs);
    if (!ok) {
        fprintf(stderr,
            "Error from reader: %s",
            errs.c_str());

        throw_runtime_error("reader error");
    }
    return sin;
}

} // namespace json
