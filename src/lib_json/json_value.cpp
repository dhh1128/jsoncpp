// Derived from public-domain/MIT-licensed code at
// https://github.com/open-source-parsers/jsoncpp. Thanks, Baptiste Lepilleur!

#include <json/assertions.h>
#include <json/value.h>
#include <json/writer.h>
#include <math.h>
#include <sstream>
#include <utility>
#include <cstring>
#include <cassert>
#include <cstddef> // size_t
#include <algorithm> // min()

#define JSON_ASSERT_UNREACHABLE assert(false)

namespace json {

// This is a walkaround to avoid the static initialization of value::null.
// kNull must be word-aligned to avoid crashing on ARM.  We use an alignment of
// 8 (instead of 4) as a bit of future-proofing.
#if defined(__ARMEL__)
#define ALIGNAS(byte_alignment) __attribute__((aligned(byte_alignment)))
#else
#define ALIGNAS(byte_alignment)
#endif
static const unsigned char ALIGNAS(8) kNull[sizeof(value)] = { 0 };
const unsigned char& kNullRef = kNull[0];
value const & value::null = reinterpret_cast<value const &>(kNullRef);
value const & value::null_ref = null;

const int32_t value::min_int = int32_t(~(uint32_t(-1) / 2));
const int32_t value::max_int = int32_t(uint32_t(-1) / 2);
const uint32_t value::max_uint = uint32_t(-1);
#if defined(JSON_HAS_INT64)
const int64_t value::min_int64 = int64_t(~(uint64_t(-1) / 2));
const int64_t value::max_int64 = int64_t(uint64_t(-1) / 2);
const uint64_t value::max_uint64 = uint64_t(-1);
// The constant is hard-coded because some compiler have trouble
// converting value::max_uint64 to a double correctly (AIX/xlC).
// Assumes that uint64_t is a 64 bits integer.
static const double maxUInt64AsDouble = 18446744073709551615.0;
#endif // defined(JSON_HAS_INT64)
const largest_int_t value::min_largest_int = largest_int_t(~(largest_uint_t(-1) / 2));
const largest_int_t value::max_largest_int = largest_int_t(largest_uint_t(-1) / 2);
const largest_uint_t value::max_largest_uint = largest_uint_t(-1);

#if !defined(JSON_USE_INT64_DOUBLE_CONVERSION)
template <typename T, typename U>
static inline bool in_range(double d, T min, U max) {
  return d >= min && d <= max;
}
#else  // if !defined(JSON_USE_INT64_DOUBLE_CONVERSION)
static inline double integerToDouble(json::uint64_t value) {
  return static_cast<double>(int64_t(value / 2)) * 2.0 + int64_t(value & 1);
}

template <typename T> static inline double integerToDouble(T value) {
  return static_cast<double>(value);
}

template <typename T, typename U>
static inline bool in_range(double d, T min, U max) {
  return d >= integerToDouble(min) && d <= integerToDouble(max);
}
#endif // if !defined(JSON_USE_INT64_DOUBLE_CONVERSION)

/** Duplicates the specified string value.
 * @param value Pointer to the string to duplicate. Must be zero-terminated if
 *              length is "unknown".
 * @param length Length of the value. if equals to unknown, then it will be
 *               computed using strlen(value).
 * @return Pointer on the duplicate instance of string.
 */
static inline char* duplicateStringValue(const char* value,
                                         size_t length) {
  // Avoid an integer overflow in the call to malloc below by limiting length
  // to a sane value.
  if (length >= (size_t)value::max_int)
    length = value::max_int - 1;

  char* newString = static_cast<char*>(malloc(length + 1));
  if (newString == NULL) {
    throw_runtime_error(
        "in json::value::duplicateStringValue(): "
        "Failed to allocate string value buffer");
  }
  memcpy(newString, value, length);
  newString[length] = 0;
  return newString;
}

/* Record the length as a prefix.
 */
static inline char* duplicateAndPrefixStringValue(
    const char* value,
    unsigned int length)
{
  // Avoid an integer overflow in the call to malloc below by limiting length
  // to a sane value.
  JSON_ASSERT_MESSAGE(length <= (unsigned)value::max_int - sizeof(unsigned) - 1U,
                      "in json::value::duplicateAndPrefixStringValue(): "
                      "length too big for prefixing");
  unsigned actualLength = length + sizeof(unsigned) + 1U;
  char* newString = static_cast<char*>(malloc(actualLength));
  if (newString == 0) {
    throw_runtime_error(
        "in json::value::duplicateAndPrefixStringValue(): "
        "Failed to allocate string value buffer");
  }
  *reinterpret_cast<unsigned*>(newString) = length;
  memcpy(newString + sizeof(unsigned), value, length);
  newString[actualLength - 1U] = 0; // to avoid buffer over-run accidents by users later
  return newString;
}
inline static void decodePrefixedString(
    bool isPrefixed, char const* prefixed,
    unsigned* length, char const** value)
{
  if (!isPrefixed) {
    *length = strlen(prefixed);
    *value = prefixed;
  } else {
    *length = *reinterpret_cast<unsigned const*>(prefixed);
    *value = prefixed + sizeof(unsigned);
  }
}
/** Free the string duplicated by duplicateStringValue()/duplicateAndPrefixStringValue().
 */
static inline void releaseStringValue(char* value) { free(value); }

} // namespace json

// //////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////
// ValueInternals...
// //////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////

#include "json_valueiterator.inl"

namespace json {

class JSON_API Exception : public std::exception {
public:
  Exception(std::string const& msg);
  virtual ~Exception() throw();
  virtual char const* what() const throw();
protected:
  std::string const msg_;
};
class JSON_API RuntimeError : public Exception {
public:
  RuntimeError(std::string const& msg);
};
class JSON_API LogicError : public Exception {
public:
  LogicError(std::string const& msg);
};

Exception::Exception(std::string const& msg)
  : msg_(msg)
{}
Exception::~Exception() throw()
{}
char const* Exception::what() const throw()
{
  return msg_.c_str();
}
RuntimeError::RuntimeError(std::string const& msg)
  : Exception(msg)
{}
LogicError::LogicError(std::string const& msg)
  : Exception(msg)
{}
void throw_runtime_error(std::string const& msg)
{
  throw RuntimeError(msg);
}
void throw_logic_error(std::string const& msg)
{
  throw LogicError(msg);
}

// //////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////
// class value::comment_info
// //////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////

value::comment_info::comment_info() : comment_(0) {}

value::comment_info::~comment_info() {
  if (comment_)
    releaseStringValue(comment_);
}

void value::comment_info::set_comment(const char* text, size_t len) {
  if (comment_) {
    releaseStringValue(comment_);
    comment_ = 0;
  }
  JSON_ASSERT(text != 0);
  JSON_ASSERT_MESSAGE(
      text[0] == '\0' || text[0] == '/',
      "in json::value::set_comment(): Comments must start with /");
  // It seems that /**/ style comments are acceptable as well.
  comment_ = duplicateStringValue(text, len);
}

// //////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////
// class value::czstring
// //////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////

// Notes: policy_ indicates if the string was allocated when
// a string is stored.

value::czstring::czstring(array_index index) : cstr_(0), index_(index) {}

value::czstring::czstring(char const* str, unsigned length, duplication_policy allocate)
    : cstr_(str)
{
  // allocate != duplicate
  storage_.policy_ = allocate;
  storage_.length_ = length;
}

value::czstring::czstring(const czstring& other)
    : cstr_(other.storage_.policy_ != no_duplication && other.cstr_ != 0
                ? duplicateStringValue(other.cstr_, other.storage_.length_)
                : other.cstr_)
{
  storage_.policy_ = (other.cstr_
                 ? (other.storage_.policy_ == no_duplication
                     ? no_duplication : duplicate)
                 : other.storage_.policy_);
  storage_.length_ = other.storage_.length_;
}

value::czstring::~czstring() {
  if (cstr_ && storage_.policy_ == duplicate)
    releaseStringValue(const_cast<char*>(cstr_));
}

void value::czstring::swap(czstring& other) {
  std::swap(cstr_, other.cstr_);
  std::swap(index_, other.index_);
}

value::czstring& value::czstring::operator=(czstring other) {
  swap(other);
  return *this;
}

bool value::czstring::operator<(const czstring& other) const {
  if (!cstr_) return index_ < other.index_;
  //return strcmp(cstr_, other.cstr_) < 0;
  // Assume both are strings.
  unsigned this_len = this->storage_.length_;
  unsigned other_len = other.storage_.length_;
  unsigned min_len = std::min(this_len, other_len);
  int comp = memcmp(this->cstr_, other.cstr_, min_len);
  if (comp < 0) return true;
  if (comp > 0) return false;
  return (this_len < other_len);
}

bool value::czstring::operator==(const czstring& other) const {
  if (!cstr_) return index_ == other.index_;
  //return strcmp(cstr_, other.cstr_) == 0;
  // Assume both are strings.
  unsigned this_len = this->storage_.length_;
  unsigned other_len = other.storage_.length_;
  if (this_len != other_len) return false;
  int comp = memcmp(this->cstr_, other.cstr_, this_len);
  return comp == 0;
}

array_index value::czstring::index() const { return index_; }

//const char* value::czstring::c_str() const { return cstr_; }
const char* value::czstring::data() const { return cstr_; }
unsigned value::czstring::length() const { return storage_.length_; }
bool value::czstring::is_static_string() const { return storage_.policy_ == no_duplication; }

// //////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////
// class value::value
// //////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////

/*! \internal Default constructor initialization must be equivalent to:
 * memset( this, 0, sizeof(value) )
 * This optimization is used in ValueInternalMap fast allocator.
 */
value::value(value_type type) {
  init_basic(type);
  switch (type) {
  case vt_null:
    break;
  case vt_int:
  case vt_uint:
    value_.int_ = 0;
    break;
  case vt_real:
    value_.real_ = 0.0;
    break;
  case vt_string:
    value_.string_ = 0;
    break;
  case vt_array:
  case vt_object:
    value_.map_ = new object_values();
    break;
  case vt_bool:
    value_.bool_ = false;
    break;
  default:
    JSON_ASSERT_UNREACHABLE;
  }
}

value::value(int32_t value) {
  init_basic(vt_int);
  value_.int_ = value;
}

value::value(uint32_t value) {
  init_basic(vt_uint);
  value_.uint_ = value;
}
#if defined(JSON_HAS_INT64)
value::value(int64_t value) {
  init_basic(vt_int);
  value_.int_ = value;
}
value::value(uint64_t value) {
  init_basic(vt_uint);
  value_.uint_ = value;
}
#endif // defined(JSON_HAS_INT64)

value::value(double value) {
  init_basic(vt_real);
  value_.real_ = value;
}

value::value(const char* value) {
  init_basic(vt_string, true);
  value_.string_ = duplicateAndPrefixStringValue(value, static_cast<unsigned>(strlen(value)));
}

value::value(const char* beginValue, const char* endValue) {
  init_basic(vt_string, true);
  value_.string_ =
      duplicateAndPrefixStringValue(beginValue, static_cast<unsigned>(endValue - beginValue));
}

value::value(std::string const & value) {
  init_basic(vt_string, true);
  value_.string_ =
      duplicateAndPrefixStringValue(value.data(), static_cast<unsigned>(value.length()));
}

value::value(const static_string& value) {
  init_basic(vt_string);
  value_.string_ = const_cast<char*>(value.c_str());
}

value::value(bool value) {
  init_basic(vt_bool);
  value_.bool_ = value;
}

value::value(value const& other)
    : type_(other.type_), allocated_(false)
      ,
      comments_(0), start_(other.start_), limit_(other.limit_)
{
  switch (type_) {
  case vt_null:
  case vt_int:
  case vt_uint:
  case vt_real:
  case vt_bool:
    value_ = other.value_;
    break;
  case vt_string:
    if (other.value_.string_ && other.allocated_) {
      unsigned len;
      char const* str;
      decodePrefixedString(other.allocated_, other.value_.string_,
          &len, &str);
      value_.string_ = duplicateAndPrefixStringValue(str, len);
      allocated_ = true;
    } else {
      value_.string_ = other.value_.string_;
      allocated_ = false;
    }
    break;
  case vt_array:
  case vt_object:
    value_.map_ = new object_values(*other.value_.map_);
    break;
  default:
    JSON_ASSERT_UNREACHABLE;
  }
  if (other.comments_) {
    comments_ = new comment_info[number_of_comment_placement];
    for (int comment = 0; comment < number_of_comment_placement; ++comment) {
      const comment_info& otherComment = other.comments_[comment];
      if (otherComment.comment_)
        comments_[comment].set_comment(
            otherComment.comment_, strlen(otherComment.comment_));
    }
  }
}

value::~value() {
  switch (type_) {
  case vt_null:
  case vt_int:
  case vt_uint:
  case vt_real:
  case vt_bool:
    break;
  case vt_string:
    if (allocated_)
      releaseStringValue(value_.string_);
    break;
  case vt_array:
  case vt_object:
    delete value_.map_;
    break;
  default:
    JSON_ASSERT_UNREACHABLE;
  }

  if (comments_)
    delete[] comments_;
}

value& value::operator=(value other) {
  swap(other);
  return *this;
}

void value::swap_payload(value& other) {
  value_type temp = type_;
  type_ = other.type_;
  other.type_ = temp;
  std::swap(value_, other.value_);
  int temp2 = allocated_;
  allocated_ = other.allocated_;
  other.allocated_ = temp2;
}

void value::swap(value& other) {
  swap_payload(other);
  std::swap(comments_, other.comments_);
  std::swap(start_, other.start_);
  std::swap(limit_, other.limit_);
}

value_type value::type() const { return type_; }

int value::compare(value const & other) const {
  if (*this < other)
    return -1;
  if (*this > other)
    return 1;
  return 0;
}

bool value::operator<(value const & other) const {
  int typeDelta = type_ - other.type_;
  if (typeDelta)
    return typeDelta < 0 ? true : false;
  switch (type_) {
  case vt_null:
    return false;
  case vt_int:
    return value_.int_ < other.value_.int_;
  case vt_uint:
    return value_.uint_ < other.value_.uint_;
  case vt_real:
    return value_.real_ < other.value_.real_;
  case vt_bool:
    return value_.bool_ < other.value_.bool_;
  case vt_string:
  {
    if ((value_.string_ == 0) || (other.value_.string_ == 0)) {
      if (other.value_.string_) return true;
      else return false;
    }
    unsigned this_len;
    unsigned other_len;
    char const* this_str;
    char const* other_str;
    decodePrefixedString(this->allocated_, this->value_.string_, &this_len, &this_str);
    decodePrefixedString(other.allocated_, other.value_.string_, &other_len, &other_str);
    unsigned min_len = std::min(this_len, other_len);
    int comp = memcmp(this_str, other_str, min_len);
    if (comp < 0) return true;
    if (comp > 0) return false;
    return (this_len < other_len);
  }
  case vt_array:
  case vt_object: {
    int delta = int(value_.map_->size() - other.value_.map_->size());
    if (delta)
      return delta < 0;
    return (*value_.map_) < (*other.value_.map_);
  }
  default:
    JSON_ASSERT_UNREACHABLE;
  }
  return false; // unreachable
}

bool value::operator<=(value const & other) const { return !(other < *this); }

bool value::operator>=(value const & other) const { return !(*this < other); }

bool value::operator>(value const & other) const { return other < *this; }

bool value::operator==(value const & other) const {
  // if ( type_ != other.type_ )
  // GCC 2.95.3 says:
  // attempt to take address of bit-field structure member `json::value::type_'
  // Beats me, but a temp solves the problem.
  int temp = other.type_;
  if (type_ != temp)
    return false;
  switch (type_) {
  case vt_null:
    return true;
  case vt_int:
    return value_.int_ == other.value_.int_;
  case vt_uint:
    return value_.uint_ == other.value_.uint_;
  case vt_real:
    return value_.real_ == other.value_.real_;
  case vt_bool:
    return value_.bool_ == other.value_.bool_;
  case vt_string:
  {
    if ((value_.string_ == 0) || (other.value_.string_ == 0)) {
      return (value_.string_ == other.value_.string_);
    }
    unsigned this_len;
    unsigned other_len;
    char const* this_str;
    char const* other_str;
    decodePrefixedString(this->allocated_, this->value_.string_, &this_len, &this_str);
    decodePrefixedString(other.allocated_, other.value_.string_, &other_len, &other_str);
    if (this_len != other_len) return false;
    int comp = memcmp(this_str, other_str, this_len);
    return comp == 0;
  }
  case vt_array:
  case vt_object:
    return value_.map_->size() == other.value_.map_->size() &&
           (*value_.map_) == (*other.value_.map_);
  default:
    JSON_ASSERT_UNREACHABLE;
  }
  return false; // unreachable
}

bool value::operator!=(value const & other) const { return !(*this == other); }

const char* value::as_cstring() const {
  JSON_ASSERT_MESSAGE(type_ == vt_string,
                      "in json::value::as_cstring(): requires vt_string");
  if (value_.string_ == 0) return 0;
  unsigned this_len;
  char const* this_str;
  decodePrefixedString(this->allocated_, this->value_.string_, &this_len, &this_str);
  return this_str;
}

bool value::get_string(char const** str, char const** end) const {
  if (type_ != vt_string) return false;
  if (value_.string_ == 0) return false;
  unsigned length;
  decodePrefixedString(this->allocated_, this->value_.string_, &length, str);
  *end = *str + length;
  return true;
}

std::string value::as_string() const {
  switch (type_) {
  case vt_null:
    return "";
  case vt_string:
  {
    if (value_.string_ == 0) return "";
    unsigned this_len;
    char const* this_str;
    decodePrefixedString(this->allocated_, this->value_.string_, &this_len, &this_str);
    return std::string(this_str, this_len);
  }
  case vt_bool:
    return value_.bool_ ? "true" : "false";
  case vt_int:
    return valueToString(value_.int_);
  case vt_uint:
    return valueToString(value_.uint_);
  case vt_real:
    return valueToString(value_.real_);
  default:
    JSON_FAIL_MESSAGE("Type is not convertible to string");
  }
}

int32_t value::as_int() const {
  switch (type_) {
  case vt_int:
    JSON_ASSERT_MESSAGE(is_int(), "largest_int_t out of int32_t range");
    return int32_t(value_.int_);
  case vt_uint:
    JSON_ASSERT_MESSAGE(is_int(), "largest_uint_t out of int32_t range");
    return int32_t(value_.uint_);
  case vt_real:
    JSON_ASSERT_MESSAGE(in_range(value_.real_, min_int, max_int),
                        "double out of int32_t range");
    return int32_t(value_.real_);
  case vt_null:
    return 0;
  case vt_bool:
    return value_.bool_ ? 1 : 0;
  default:
    break;
  }
  JSON_FAIL_MESSAGE("value is not convertible to int32_t.");
}

uint32_t value::as_uint() const {
  switch (type_) {
  case vt_int:
    JSON_ASSERT_MESSAGE(is_uint(), "largest_int_t out of uint32_t range");
    return uint32_t(value_.int_);
  case vt_uint:
    JSON_ASSERT_MESSAGE(is_uint(), "largest_uint_t out of uint32_t range");
    return uint32_t(value_.uint_);
  case vt_real:
    JSON_ASSERT_MESSAGE(in_range(value_.real_, 0, max_uint),
                        "double out of uint32_t range");
    return uint32_t(value_.real_);
  case vt_null:
    return 0;
  case vt_bool:
    return value_.bool_ ? 1 : 0;
  default:
    break;
  }
  JSON_FAIL_MESSAGE("value is not convertible to uint32_t.");
}

#if defined(JSON_HAS_INT64)

int64_t value::as_int64() const {
  switch (type_) {
  case vt_int:
    return int64_t(value_.int_);
  case vt_uint:
    JSON_ASSERT_MESSAGE(is_int64(), "largest_uint_t out of int64_t range");
    return int64_t(value_.uint_);
  case vt_real:
    JSON_ASSERT_MESSAGE(in_range(value_.real_, min_int64, max_int64),
                        "double out of int64_t range");
    return int64_t(value_.real_);
  case vt_null:
    return 0;
  case vt_bool:
    return value_.bool_ ? 1 : 0;
  default:
    break;
  }
  JSON_FAIL_MESSAGE("value is not convertible to int64_t.");
}

uint64_t value::as_uint64() const {
  switch (type_) {
  case vt_int:
    JSON_ASSERT_MESSAGE(isUInt64(), "largest_int_t out of uint64_t range");
    return uint64_t(value_.int_);
  case vt_uint:
    return uint64_t(value_.uint_);
  case vt_real:
    JSON_ASSERT_MESSAGE(in_range(value_.real_, 0, max_uint64),
                        "double out of uint64_t range");
    return uint64_t(value_.real_);
  case vt_null:
    return 0;
  case vt_bool:
    return value_.bool_ ? 1 : 0;
  default:
    break;
  }
  JSON_FAIL_MESSAGE("value is not convertible to uint64_t.");
}
#endif // if defined(JSON_HAS_INT64)

largest_int_t value::as_largest_int() const {
#if defined(JSON_NO_INT64)
  return as_int();
#else
  return as_int64();
#endif
}

largest_uint_t value::as_largest_uint() const {
#if defined(JSON_NO_INT64)
  return as_uint();
#else
  return as_uint64();
#endif
}

double value::as_double() const {
  switch (type_) {
  case vt_int:
    return static_cast<double>(value_.int_);
  case vt_uint:
#if !defined(JSON_USE_INT64_DOUBLE_CONVERSION)
    return static_cast<double>(value_.uint_);
#else  // if !defined(JSON_USE_INT64_DOUBLE_CONVERSION)
    return integerToDouble(value_.uint_);
#endif // if !defined(JSON_USE_INT64_DOUBLE_CONVERSION)
  case vt_real:
    return value_.real_;
  case vt_null:
    return 0.0;
  case vt_bool:
    return value_.bool_ ? 1.0 : 0.0;
  default:
    break;
  }
  JSON_FAIL_MESSAGE("value is not convertible to double.");
}

float value::as_float() const {
  switch (type_) {
  case vt_int:
    return static_cast<float>(value_.int_);
  case vt_uint:
#if !defined(JSON_USE_INT64_DOUBLE_CONVERSION)
    return static_cast<float>(value_.uint_);
#else  // if !defined(JSON_USE_INT64_DOUBLE_CONVERSION)
    return integerToDouble(value_.uint_);
#endif // if !defined(JSON_USE_INT64_DOUBLE_CONVERSION)
  case vt_real:
    return static_cast<float>(value_.real_);
  case vt_null:
    return 0.0;
  case vt_bool:
    return value_.bool_ ? 1.0f : 0.0f;
  default:
    break;
  }
  JSON_FAIL_MESSAGE("value is not convertible to float.");
}

bool value::as_bool() const {
  switch (type_) {
  case vt_bool:
    return value_.bool_;
  case vt_null:
    return false;
  case vt_int:
    return value_.int_ ? true : false;
  case vt_uint:
    return value_.uint_ ? true : false;
  case vt_real:
    return value_.real_ ? true : false;
  default:
    break;
  }
  JSON_FAIL_MESSAGE("value is not convertible to bool.");
}

bool value::is_convertible_to(value_type other) const {
  switch (other) {
  case vt_null:
    return (isNumeric() && as_double() == 0.0) ||
           (type_ == vt_bool && value_.bool_ == false) ||
           (type_ == vt_string && as_string() == "") ||
           (type_ == vt_array && value_.map_->size() == 0) ||
           (type_ == vt_object && value_.map_->size() == 0) ||
           type_ == vt_null;
  case vt_int:
    return is_int() ||
           (type_ == vt_real && in_range(value_.real_, min_int, max_int)) ||
           type_ == vt_bool || type_ == vt_null;
  case vt_uint:
    return is_uint() ||
           (type_ == vt_real && in_range(value_.real_, 0, max_uint)) ||
           type_ == vt_bool || type_ == vt_null;
  case vt_real:
    return isNumeric() || type_ == vt_bool || type_ == vt_null;
  case vt_bool:
    return isNumeric() || type_ == vt_bool || type_ == vt_null;
  case vt_string:
    return isNumeric() || type_ == vt_bool || type_ == vt_string ||
           type_ == vt_null;
  case vt_array:
    return type_ == vt_array || type_ == vt_null;
  case vt_object:
    return type_ == vt_object || type_ == vt_null;
  }
  JSON_ASSERT_UNREACHABLE;
  return false;
}

/// Number of values in array or object
array_index value::size() const {
  switch (type_) {
  case vt_null:
  case vt_int:
  case vt_uint:
  case vt_real:
  case vt_bool:
  case vt_string:
    return 0;
  case vt_array: // size of the array is highest index + 1
    if (!value_.map_->empty()) {
      object_values::const_iterator itLast = value_.map_->end();
      --itLast;
      return (*itLast).first.index() + 1;
    }
    return 0;
  case vt_object:
    return array_index(value_.map_->size());
  }
  JSON_ASSERT_UNREACHABLE;
  return 0; // unreachable;
}

bool value::empty() const {
  if (is_null() || is_array() || is_object())
    return size() == 0u;
  else
    return false;
}

bool value::operator!() const { return is_null(); }

void value::clear() {
  JSON_ASSERT_MESSAGE(type_ == vt_null || type_ == vt_array ||
                          type_ == vt_object,
                      "in json::value::clear(): requires complex value");
  start_ = 0;
  limit_ = 0;
  switch (type_) {
  case vt_array:
  case vt_object:
    value_.map_->clear();
    break;
  default:
    break;
  }
}

void value::resize(array_index newSize) {
  JSON_ASSERT_MESSAGE(type_ == vt_null || type_ == vt_array,
                      "in json::value::resize(): requires vt_array");
  if (type_ == vt_null)
    *this = value(vt_array);
  array_index oldSize = size();
  if (newSize == 0)
    clear();
  else if (newSize > oldSize)
    (*this)[newSize - 1];
  else {
    for (array_index index = newSize; index < oldSize; ++index) {
      value_.map_->erase(index);
    }
    assert(size() == newSize);
  }
}

value& value::operator[](array_index index) {
  JSON_ASSERT_MESSAGE(
      type_ == vt_null || type_ == vt_array,
      "in json::value::operator[](array_index): requires vt_array");
  if (type_ == vt_null)
    *this = value(vt_array);
  czstring key(index);
  object_values::iterator it = value_.map_->lower_bound(key);
  if (it != value_.map_->end() && (*it).first == key)
    return (*it).second;

  object_values::value_type default_value(key, null_ref);
  it = value_.map_->insert(it, default_value);
  return (*it).second;
}

value& value::operator[](int index) {
  JSON_ASSERT_MESSAGE(
      index >= 0,
      "in json::value::operator[](int index): index cannot be negative");
  return (*this)[array_index(index)];
}

value const & value::operator[](array_index index) const {
  JSON_ASSERT_MESSAGE(
      type_ == vt_null || type_ == vt_array,
      "in json::value::operator[](array_index)const: requires vt_array");
  if (type_ == vt_null)
    return null_ref;
  czstring key(index);
  object_values::const_iterator it = value_.map_->find(key);
  if (it == value_.map_->end())
    return null_ref;
  return (*it).second;
}

value const & value::operator[](int index) const {
  JSON_ASSERT_MESSAGE(
      index >= 0,
      "in json::value::operator[](int index) const: index cannot be negative");
  return (*this)[array_index(index)];
}

void value::init_basic(value_type type, bool allocated) {
  type_ = type;
  allocated_ = allocated;
  comments_ = 0;
  start_ = 0;
  limit_ = 0;
}

// Access an object value by name, create a null member if it does not exist.
// @pre Type of '*this' is object or null.
// @param key is null-terminated.
value& value::resolve_reference(const char* key) {
  JSON_ASSERT_MESSAGE(
      type_ == vt_null || type_ == vt_object,
      "in json::value::resolve_reference(): requires vt_object");
  if (type_ == vt_null)
    *this = value(vt_object);
  czstring actual_key(
      key, static_cast<unsigned>(strlen(key)), czstring::no_duplication); // NOTE!
  object_values::iterator it = value_.map_->lower_bound(actual_key);
  if (it != value_.map_->end() && (*it).first == actual_key)
    return (*it).second;

  object_values::value_type default_value(actual_key, null_ref);
  it = value_.map_->insert(it, default_value);
  value& value = (*it).second;
  return value;
}

// @param key is not null-terminated.
value& value::resolve_reference(char const* key, char const* end)
{
  JSON_ASSERT_MESSAGE(
      type_ == vt_null || type_ == vt_object,
      "in json::value::resolve_reference(key, end): requires vt_object");
  if (type_ == vt_null)
    *this = value(vt_object);
  czstring actual_key(
      key, static_cast<unsigned>(end-key), czstring::duplicate_on_copy);
  object_values::iterator it = value_.map_->lower_bound(actual_key);
  if (it != value_.map_->end() && (*it).first == actual_key)
    return (*it).second;

  object_values::value_type default_value(actual_key, null_ref);
  it = value_.map_->insert(it, default_value);
  value& value = (*it).second;
  return value;
}

value value::get(array_index index, value const & default_value) const {
  const value* value = &((*this)[index]);
  return value == &null_ref ? default_value : *value;
}

bool value::is_valid_index(array_index index) const { return index < size(); }

value const* value::find(char const* key, char const* end) const
{
  JSON_ASSERT_MESSAGE(
      type_ == vt_null || type_ == vt_object,
      "in json::value::find(key, end, found): requires vt_object or vt_null");
  if (type_ == vt_null) return NULL;
  czstring actual_key(key, static_cast<unsigned>(end-key), czstring::no_duplication);
  object_values::const_iterator it = value_.map_->find(actual_key);
  if (it == value_.map_->end()) return NULL;
  return &(*it).second;
}
value const & value::operator[](const char* key) const
{
  value const* found = find(key, key + strlen(key));
  if (!found) return null_ref;
  return *found;
}
value const& value::operator[](std::string const& key) const
{
  value const* found = find(key.data(), key.data() + key.length());
  if (!found) return null_ref;
  return *found;
}

value& value::operator[](const char* key) {
  return resolve_reference(key, key + strlen(key));
}

value& value::operator[](std::string const & key) {
  return resolve_reference(key.data(), key.data() + key.length());
}

value& value::operator[](const static_string& key) {
  return resolve_reference(key.c_str());
}

value& value::append(value const & value) { return (*this)[size()] = value; }

value value::get(char const* key, char const* end, value const& default_value) const
{
  value const* found = find(key, end);
  return !found ? default_value : *found;
}
value value::get(char const* key, value const& default_value) const
{
  return get(key, key + strlen(key), default_value);
}
value value::get(std::string const& key, value const& default_value) const
{
  return get(key.data(), key.data() + key.length(), default_value);
}


bool value::remove_member(const char* key, const char* end, value* removed)
{
  if (type_ != vt_object) {
    return false;
  }
  czstring actual_key(key, static_cast<unsigned>(end-key), czstring::no_duplication);
  object_values::iterator it = value_.map_->find(actual_key);
  if (it == value_.map_->end())
    return false;
  *removed = it->second;
  value_.map_->erase(it);
  return true;
}
bool value::remove_member(const char* key, value* removed)
{
  return remove_member(key, key + strlen(key), removed);
}
bool value::remove_member(std::string const& key, value* removed)
{
  return remove_member(key.data(), key.data() + key.length(), removed);
}
value value::remove_member(const char* key)
{
  JSON_ASSERT_MESSAGE(type_ == vt_null || type_ == vt_object,
                      "in json::value::remove_member(): requires vt_object");
  if (type_ == vt_null)
    return null_ref;

  value removed;  // null
  remove_member(key, key + strlen(key), &removed);
  return removed; // still null if remove_member() did nothing
}
value value::remove_member(std::string const & key)
{
  return remove_member(key.c_str());
}

bool value::remove_index(array_index index, value* removed) {
  if (type_ != vt_array) {
    return false;
  }
  czstring key(index);
  object_values::iterator it = value_.map_->find(key);
  if (it == value_.map_->end()) {
    return false;
  }
  *removed = it->second;
  array_index oldSize = size();
  // shift left all items left, into the place of the "removed"
  for (array_index i = index; i < (oldSize - 1); ++i){
    czstring key(i);
    (*value_.map_)[key] = (*this)[i + 1];
  }
  // erase the last one ("leftover")
  czstring keyLast(oldSize - 1);
  object_values::iterator itLast = value_.map_->find(keyLast);
  value_.map_->erase(itLast);
  return true;
}

bool value::is_member(char const* key, char const* end) const
{
  value const* value = find(key, end);
  return NULL != value;
}
bool value::is_member(char const* key) const
{
  return is_member(key, key + strlen(key));
}
bool value::is_member(std::string const& key) const
{
  return is_member(key.data(), key.data() + key.length());
}

value::Members value::get_member_names() const {
  JSON_ASSERT_MESSAGE(
      type_ == vt_null || type_ == vt_object,
      "in json::value::get_member_names(), value must be vt_object");
  if (type_ == vt_null)
    return value::Members();
  Members members;
  members.reserve(value_.map_->size());
  object_values::const_iterator it = value_.map_->begin();
  object_values::const_iterator itEnd = value_.map_->end();
  for (; it != itEnd; ++it) {
    members.push_back(std::string((*it).first.data(),
                                  (*it).first.length()));
  }
  return members;
}

static bool is_integral(double d) {
  double integral_part;
  return modf(d, &integral_part) == 0.0;
}

bool value::is_null() const { return type_ == vt_null; }

bool value::is_bool() const { return type_ == vt_bool; }

bool value::is_int() const {
  switch (type_) {
  case vt_int:
    return value_.int_ >= min_int && value_.int_ <= max_int;
  case vt_uint:
    return value_.uint_ <= uint32_t(max_int);
  case vt_real:
    return value_.real_ >= min_int && value_.real_ <= max_int &&
           is_integral(value_.real_);
  default:
    break;
  }
  return false;
}

bool value::is_uint() const {
  switch (type_) {
  case vt_int:
    return value_.int_ >= 0 && largest_uint_t(value_.int_) <= largest_uint_t(max_uint);
  case vt_uint:
    return value_.uint_ <= max_uint;
  case vt_real:
    return value_.real_ >= 0 && value_.real_ <= max_uint &&
           is_integral(value_.real_);
  default:
    break;
  }
  return false;
}

bool value::is_int64() const {
#if defined(JSON_HAS_INT64)
  switch (type_) {
  case vt_int:
    return true;
  case vt_uint:
    return value_.uint_ <= uint64_t(max_int64);
  case vt_real:
    // Note that max_int64 (= 2^63 - 1) is not exactly representable as a
    // double, so double(max_int64) will be rounded up to 2^63. Therefore we
    // require the value to be strictly less than the limit.
    return value_.real_ >= double(min_int64) &&
           value_.real_ < double(max_int64) && is_integral(value_.real_);
  default:
    break;
  }
#endif // JSON_HAS_INT64
  return false;
}

bool value::isUInt64() const {
#if defined(JSON_HAS_INT64)
  switch (type_) {
  case vt_int:
    return value_.int_ >= 0;
  case vt_uint:
    return true;
  case vt_real:
    // Note that max_uint64 (= 2^64 - 1) is not exactly representable as a
    // double, so double(max_uint64) will be rounded up to 2^64. Therefore we
    // require the value to be strictly less than the limit.
    return value_.real_ >= 0 && value_.real_ < maxUInt64AsDouble &&
           is_integral(value_.real_);
  default:
    break;
  }
#endif // JSON_HAS_INT64
  return false;
}

bool value::isIntegral() const {
#if defined(JSON_HAS_INT64)
  return is_int64() || isUInt64();
#else
  return is_int() || is_uint();
#endif
}

bool value::isDouble() const { return type_ == vt_real || isIntegral(); }

bool value::isNumeric() const { return isIntegral() || isDouble(); }

bool value::isString() const { return type_ == vt_string; }

bool value::is_array() const { return type_ == vt_array; }

bool value::is_object() const { return type_ == vt_object; }

void value::set_comment(const char* comment, size_t len, comment_placement placement) {
  if (!comments_)
    comments_ = new comment_info[number_of_comment_placement];
  if ((len > 0) && (comment[len-1] == '\n')) {
    // Always discard trailing newline, to aid indentation.
    len -= 1;
  }
  comments_[placement].set_comment(comment, len);
}

void value::set_comment(const char* comment, comment_placement placement) {
  set_comment(comment, strlen(comment), placement);
}

void value::set_comment(std::string const & comment, comment_placement placement) {
  set_comment(comment.c_str(), comment.length(), placement);
}

bool value::has_comment(comment_placement placement) const {
  return comments_ != 0 && comments_[placement].comment_ != 0;
}

std::string value::get_comment(comment_placement placement) const {
  if (has_comment(placement))
    return comments_[placement].comment_;
  return "";
}

void value::set_offset_start(size_t start) { start_ = start; }

void value::set_offset_limit(size_t limit) { limit_ = limit; }

size_t value::get_offset_start() const { return start_; }

size_t value::get_offset_limit() const { return limit_; }

std::string value::toStyledString() const {
  styled_writer writer;
  return writer.write(*this);
}

value::const_iterator value::begin() const {
  switch (type_) {
  case vt_array:
  case vt_object:
    if (value_.map_)
      return const_iterator(value_.map_->begin());
    break;
  default:
    break;
  }
  return const_iterator();
}

value::const_iterator value::end() const {
  switch (type_) {
  case vt_array:
  case vt_object:
    if (value_.map_)
      return const_iterator(value_.map_->end());
    break;
  default:
    break;
  }
  return const_iterator();
}

value::iterator value::begin() {
  switch (type_) {
  case vt_array:
  case vt_object:
    if (value_.map_)
      return iterator(value_.map_->begin());
    break;
  default:
    break;
  }
  return iterator();
}

value::iterator value::end() {
  switch (type_) {
  case vt_array:
  case vt_object:
    if (value_.map_)
      return iterator(value_.map_->end());
    break;
  default:
    break;
  }
  return iterator();
}

// class path_argument
// //////////////////////////////////////////////////////////////////

path_argument::path_argument() : key_(), index_(), kind_(kindNone) {}

path_argument::path_argument(array_index index)
    : key_(), index_(index), kind_(kindIndex) {}

path_argument::path_argument(const char* key)
    : key_(key), index_(), kind_(kindKey) {}

path_argument::path_argument(std::string const & key)
    : key_(key.c_str()), index_(), kind_(kindKey) {}

// class path
// //////////////////////////////////////////////////////////////////

path::path(std::string const & path,
           const path_argument& a1,
           const path_argument& a2,
           const path_argument& a3,
           const path_argument& a4,
           const path_argument& a5) {
  InArgs in;
  in.push_back(&a1);
  in.push_back(&a2);
  in.push_back(&a3);
  in.push_back(&a4);
  in.push_back(&a5);
  makePath(path, in);
}

void path::makePath(std::string const & path, const InArgs& in) {
  const char* current = path.c_str();
  const char* end = current + path.length();
  InArgs::const_iterator itInArg = in.begin();
  while (current != end) {
    if (*current == '[') {
      ++current;
      if (*current == '%')
        addPathInArg(path, in, itInArg, path_argument::kindIndex);
      else {
        array_index index = 0;
        for (; current != end && *current >= '0' && *current <= '9'; ++current)
          index = index * 10 + array_index(*current - '0');
        args_.push_back(index);
      }
      if (current == end || *current++ != ']')
        invalidPath(path, int(current - path.c_str()));
    } else if (*current == '%') {
      addPathInArg(path, in, itInArg, path_argument::kindKey);
      ++current;
    } else if (*current == '.') {
      ++current;
    } else {
      const char* beginName = current;
      while (current != end && !strchr("[.", *current))
        ++current;
      args_.push_back(std::string(beginName, current));
    }
  }
}

void path::addPathInArg(std::string const & /*path*/,
                        const InArgs& in,
                        InArgs::const_iterator& itInArg,
                        path_argument::Kind kind) {
  if (itInArg == in.end()) {
    // Error: missing argument %d
  } else if ((*itInArg)->kind_ != kind) {
    // Error: bad argument type
  } else {
    args_.push_back(**itInArg);
  }
}

void path::invalidPath(std::string const & /*path*/, int /*location*/) {
  // Error: invalid path.
}

value const & path::resolve(value const & root) const {
  const value* node = &root;
  for (Args::const_iterator it = args_.begin(); it != args_.end(); ++it) {
    const path_argument& arg = *it;
    if (arg.kind_ == path_argument::kindIndex) {
      if (!node->is_array() || !node->is_valid_index(arg.index_)) {
        // Error: unable to resolve path (array value expected at position...
      }
      node = &((*node)[arg.index_]);
    } else if (arg.kind_ == path_argument::kindKey) {
      if (!node->is_object()) {
        // Error: unable to resolve path (object value expected at position...)
      }
      node = &((*node)[arg.key_]);
      if (node == &value::null_ref) {
        // Error: unable to resolve path (object has no member named '' at
        // position...)
      }
    }
  }
  return *node;
}

value path::resolve(value const & root, value const & default_value) const {
  const value* node = &root;
  for (Args::const_iterator it = args_.begin(); it != args_.end(); ++it) {
    const path_argument& arg = *it;
    if (arg.kind_ == path_argument::kindIndex) {
      if (!node->is_array() || !node->is_valid_index(arg.index_))
        return default_value;
      node = &((*node)[arg.index_]);
    } else if (arg.kind_ == path_argument::kindKey) {
      if (!node->is_object())
        return default_value;
      node = &((*node)[arg.key_]);
      if (node == &value::null_ref)
        return default_value;
    }
  }
  return *node;
}

value& path::make(value& root) const {
  value* node = &root;
  for (Args::const_iterator it = args_.begin(); it != args_.end(); ++it) {
    const path_argument& arg = *it;
    if (arg.kind_ == path_argument::kindIndex) {
      if (!node->is_array()) {
        // Error: node is not an array at position ...
      }
      node = &((*node)[arg.index_]);
    } else if (arg.kind_ == path_argument::kindKey) {
      if (!node->is_object()) {
        // Error: node is not an object at position...
      }
      node = &((*node)[arg.key_]);
    }
  }
  return *node;
}

} // namespace json
