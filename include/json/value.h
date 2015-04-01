// Derived from public-domain/MIT-licensed code at
// https://github.com/open-source-parsers/jsoncpp. Thanks, Baptiste Lepilleur!

#ifndef _feb06e75ac584e1ea52502c12e8cad61
#define _feb06e75ac584e1ea52502c12e8cad61

#include "forwards.h"

#include <string>
#include <vector>
#include <exception>

#include <map>

// Disable warning C4251: <data member>: <type> needs to have dll-interface to
// be used by...
#if defined(JSONCPP_DISABLE_DLL_INTERFACE_WARNING)
#pragma warning(push)
#pragma warning(disable : 4251)
#endif // if defined(JSONCPP_DISABLE_DLL_INTERFACE_WARNING)

/** \brief JSON (JavaScript Object Notation).
 */
namespace json {

/** Base class for all exceptions we throw.
 *
 * We use nothing but these internally. Of course, STL can throw others.
 */
class JSON_API Exception;
/** Exceptions which the user cannot easily avoid.
 *
 * E.g. out-of-memory (when we use malloc), stack-overflow, malicious input
 *
 * \remark derived from json::Exception
 */
class JSON_API RuntimeError;
/** Exceptions thrown by JSON_ASSERT/JSON_FAIL macros.
 *
 * These are precondition-violations (user bugs) and internal errors (our bugs).
 *
 * \remark derived from json::Exception
 */
class JSON_API LogicError;

/// used internally
void throw_runtime_error(std::string const& msg);
/// used internally
void throw_logic_error(std::string const& msg);

/** \brief Type of the value held by a value object.
 */
enum value_type {
  vt_null = 0, ///< 'null' value
  vt_int,      ///< signed integer value
  vt_uint,     ///< unsigned integer value
  vt_real,     ///< double value
  vt_string,   ///< UTF-8 string value
  vt_bool,     ///< bool value
  vt_array,    ///< array value (ordered list)
  vt_object    ///< object value (collection of name/value pairs).
};

enum comment_placement {
  comment_before = 0,      ///< a comment placed on the line before a value
  comment_after_on_same_line, ///< a comment just after a value on the same line
  comment_after, ///< a comment on the line after a value (only make sense for
  /// root value)
  number_of_comment_placement
};

/** \brief Lightweight wrapper to tag static string.
 *
 * value constructor and vt_object member assignement takes advantage of the
 * static_string and avoid the cost of string duplication when storing the
 * string or the member name.
 *
 * Example of usage:
 * \code
 * json::value aValue( static_string("some text") );
 * json::value object;
 * static const static_string code("code");
 * object[code] = 1234;
 * \endcode
 */
class JSON_API static_string {
public:
  explicit static_string(const char* czstring) : c_str_(czstring) {}

  operator const char*() const { return c_str_; }

  const char* c_str() const { return c_str_; }

private:
  const char* c_str_;
};

/** \brief Represents a <a HREF="http://www.json.org">JSON</a> value.
 *
 * This class is a discriminated union wrapper that can represents a:
 * - signed integer [range: value::min_int - value::max_int]
 * - unsigned integer (range: 0 - value::max_uint)
 * - double
 * - UTF-8 string
 * - boolean
 * - 'null'
 * - an ordered list of value
 * - collection of name/value pairs (javascript object)
 *
 * The type of the held value is represented by a #value_type and
 * can be obtained using type().
 *
 * Values of an #vt_object or #vt_array can be accessed using operator[]()
 * methods.
 * Non-const methods will automatically create the a #vt_null element
 * if it does not exist.
 * The sequence of an #vt_array will be automatically resized and initialized
 * with #vt_null. resize() can be used to enlarge or truncate an #vt_array.
 *
 * The get() methods can be used to obtain default value in the case the
 * required element does not exist.
 *
 * It is possible to iterate over the list of a #vt_object values using
 * the get_member_names() method.
 *
 * \note #value string-length fit in size_t, but keys must be < 2^30.
 * (The reason is an implementation detail.) A #char_reader will raise an
 * exception if a bound is exceeded to avoid security holes in your app,
 * but the value API does *not* check bounds. That is the responsibility
 * of the caller.
 */
class JSON_API value {
  friend class value_iterator_base;
public:
  typedef std::vector<std::string> Members;
  typedef value_iterator iterator;
  typedef value_const_iterator const_iterator;
  typedef json::largest_int_t largest_int_t;
  typedef json::largest_uint_t largest_uint_t;
  typedef json::array_index array_index;

  static value const & null;  ///< We regret this reference to a global instance; prefer the simpler value().
  static value const & null_ref;  ///< just a kludge for binary-compatibility; same as null
  /// Minimum signed integer value that can be stored in a json::value.
  static const largest_int_t min_largest_int;
  /// Maximum signed integer value that can be stored in a json::value.
  static const largest_int_t max_largest_int;
  /// Maximum unsigned integer value that can be stored in a json::value.
  static const largest_uint_t max_largest_uint;

  /// Minimum signed int value that can be stored in a json::value.
  static const int32_t min_int;
  /// Maximum signed int value that can be stored in a json::value.
  static const int32_t max_int;
  /// Maximum unsigned int value that can be stored in a json::value.
  static const uint32_t max_uint;

#if defined(JSON_HAS_INT64)
  /// Minimum signed 64 bits int value that can be stored in a json::value.
  static const int64_t min_int64;
  /// Maximum signed 64 bits int value that can be stored in a json::value.
  static const int64_t max_int64;
  /// Maximum unsigned 64 bits int value that can be stored in a json::value.
  static const uint64_t max_uint64;
#endif // defined(JSON_HAS_INT64)

private:
#ifndef JSONCPP_DOC_EXCLUDE_IMPLEMENTATION
  class czstring {
  public:
    enum duplication_policy {
      no_duplication = 0,
      duplicate,
      duplicate_on_copy
    };
    czstring(array_index index);
    czstring(char const* str, unsigned length, duplication_policy allocate);
    czstring(czstring const& other);
    ~czstring();
    czstring& operator=(czstring other);
    bool operator<(czstring const& other) const;
    bool operator==(czstring const& other) const;
    array_index index() const;
    //const char* c_str() const; ///< \deprecated
    char const* data() const;
    unsigned length() const;
    bool is_static_string() const;

  private:
    void swap(czstring& other);

    struct string_storage {
      duplication_policy policy_: 2;
      unsigned length_: 30; // 1GB max
    };

    char const* cstr_;  // actually, a prefixed string, unless policy is noDup
    union {
      array_index index_;
      string_storage storage_;
    };
  };

public:
#ifndef JSON_USE_CPPTL_SMALLMAP
  typedef std::map<czstring, value> object_values;
#else
  typedef CppTL::SmallMap<czstring, value> object_values;
#endif // ifndef JSON_USE_CPPTL_SMALLMAP
#endif // ifndef JSONCPP_DOC_EXCLUDE_IMPLEMENTATION

public:
  /** \brief Create a default value of the given type.

    This is a very useful constructor.
    To create an empty array, pass vt_array.
    To create an empty object, pass vt_object.
    Another value can then be set to this one by assignment.
This is useful since clear() and resize() will not alter types.

    Examples:
\code
json::value null_value; // null
json::value arr_value(json::vt_array); // []
json::value obj_value(json::vt_object); // {}
\endcode
  */
  value(value_type type = vt_null);
  value(int32_t value);
  value(uint32_t value);
#if defined(JSON_HAS_INT64)
  value(int64_t value);
  value(uint64_t value);
#endif // if defined(JSON_HAS_INT64)
  value(double value);
  value(const char* value); ///< Copy til first 0. (NULL causes to seg-fault.)
  value(const char* beginValue, const char* endValue); ///< Copy all, incl zeroes.
  /** \brief Constructs a value from a static string.

   * Like other value string constructor but do not duplicate the string for
   * internal storage. The given string must remain alive after the call to this
   * constructor.
   * \note This works only for null-terminated strings. (We cannot change the
   *   size of this class, so we have nowhere to store the length,
   *   which might be computed later for various operations.)
   *
   * Example of usage:
   * \code
   * static static_string foo("some text");
   * json::value aValue(foo);
   * \endcode
   */
  value(const static_string& value);
  value(std::string const & value); ///< Copy data() til size(). Embedded zeroes too.
  value(bool value);
  /// Deep copy.
  value(value const & other);
  ~value();

  /// Deep copy, then swap(other).
  /// \note Over-write existing comments. To preserve comments, use #swap_payload().
  value& operator=(value other);
  /// Swap everything.
  void swap(value& other);
  /// Swap values but leave comments and source offsets in place.
  void swap_payload(value& other);

  value_type type() const;

  /// Compare payload only, not comments etc.
  bool operator<(value const & other) const;
  bool operator<=(value const & other) const;
  bool operator>=(value const & other) const;
  bool operator>(value const & other) const;
  bool operator==(value const & other) const;
  bool operator!=(value const & other) const;
  int compare(value const & other) const;

  const char* as_cstring() const; ///< Embedded zeroes could cause you trouble!
  std::string as_string() const; ///< Embedded zeroes are possible.
  /** Get raw char* of string-value.
   *  \return false if !string. (Seg-fault if str or end are NULL.)
   */
  bool get_string(
      char const** str, char const** end) const;
  int32_t as_int() const;
  uint32_t as_uint() const;
#if defined(JSON_HAS_INT64)
  int64_t as_int64() const;
  uint64_t as_uint64() const;
#endif // if defined(JSON_HAS_INT64)
  largest_int_t as_largest_int() const;
  largest_uint_t as_largest_uint() const;
  float as_float() const;
  double as_double() const;
  bool as_bool() const;

  bool is_null() const;
  bool is_bool() const;
  bool is_int() const;
  bool is_int64() const;
  bool is_uint() const;
  bool isUInt64() const;
  bool isIntegral() const;
  bool isDouble() const;
  bool isNumeric() const;
  bool isString() const;
  bool is_array() const;
  bool is_object() const;

  bool is_convertible_to(value_type other) const;

  /// Number of values in array or object
  array_index size() const;

  /// \brief Return true if empty array, empty object, or null;
  /// otherwise, false.
  bool empty() const;

  /// Return is_null()
  bool operator!() const;

  /// Remove all object members and array elements.
  /// \pre type() is vt_array, vt_object, or vt_null
  /// \post type() is unchanged
  void clear();

  /// Resize the array to size elements.
  /// New elements are initialized to null.
  /// May only be called on vt_null or vt_array.
  /// \pre type() is vt_array or vt_null
  /// \post type() is vt_array
  void resize(array_index size);

  /// Access an array element (zero based index ).
  /// If the array contains less than index element, then null value are
  /// inserted
  /// in the array so that its size is index+1.
  /// (You may need to say 'value[0u]' to get your compiler to distinguish
  ///  this from the operator[] which takes a string.)
  value& operator[](array_index index);

  /// Access an array element (zero based index ).
  /// If the array contains less than index element, then null value are
  /// inserted
  /// in the array so that its size is index+1.
  /// (You may need to say 'value[0u]' to get your compiler to distinguish
  ///  this from the operator[] which takes a string.)
  value& operator[](int index);

  /// Access an array element (zero based index )
  /// (You may need to say 'value[0u]' to get your compiler to distinguish
  ///  this from the operator[] which takes a string.)
  value const & operator[](array_index index) const;

  /// Access an array element (zero based index )
  /// (You may need to say 'value[0u]' to get your compiler to distinguish
  ///  this from the operator[] which takes a string.)
  value const & operator[](int index) const;

  /// If the array contains at least index+1 elements, returns the element
  /// value,
  /// otherwise returns default_value.
  value get(array_index index, value const & default_value) const;
  /// Return true if index < size().
  bool is_valid_index(array_index index) const;
  /// \brief Append value to array at the end.
  ///
  /// Equivalent to jsonvalue[jsonvalue.size()] = value;
  value& append(value const & value);

  /// Access an object value by name, create a null member if it does not exist.
  /// \note Because of our implementation, keys are limited to 2^30 -1 chars.
  ///  Exceeding that will cause an exception.
  value& operator[](const char* key);
  /// Access an object value by name, returns null if there is no member with
  /// that name.
  value const & operator[](const char* key) const;
  /// Access an object value by name, create a null member if it does not exist.
  /// \param key may contain embedded nulls.
  value& operator[](std::string const & key);
  /// Access an object value by name, returns null if there is no member with
  /// that name.
  /// \param key may contain embedded nulls.
  value const & operator[](std::string const & key) const;
  /** \brief Access an object value by name, create a null member if it does not
   exist.

   * If the object has no entry for that name, then the member name used to store
   * the new entry is not duplicated.
   * Example of use:
   * \code
   * json::value object;
   * static const static_string code("code");
   * object[code] = 1234;
   * \endcode
   */
  value& operator[](const static_string& key);
  /// Return the member named key if it exist, default_value otherwise.
  /// \note deep copy
  value get(const char* key, value const & default_value) const;
  /// Return the member named key if it exist, default_value otherwise.
  /// \note deep copy
  /// \param key may contain embedded nulls.
  value get(const char* key, const char* end, value const & default_value) const;
  /// Return the member named key if it exist, default_value otherwise.
  /// \note deep copy
  /// \param key may contain embedded nulls.
  value get(std::string const & key, value const & default_value) const;
  /// Most general and efficient version of is_member()const, get()const,
  /// and operator[]const
  /// \note As stated elsewhere, behavior is undefined if (end-key) >= 2^30
  value const* find(char const* key, char const* end) const;
  /// Most general and efficient version of object-mutators.
  /// \note As stated elsewhere, behavior is undefined if (end-key) >= 2^30
  /// \return non-zero, but JSON_ASSERT if this is neither object nor vt_null.
  value const* demand(char const* key, char const* end);
  /// \brief Remove and return the named member.
  ///
  /// Do nothing if it did not exist.
  /// \return the removed value, or null.
  /// \pre type() is vt_object or vt_null
  /// \post type() is unchanged
  /// \deprecated
  value remove_member(const char* key);
  /// Same as remove_member(const char*)
  /// \param key may contain embedded nulls.
  /// \deprecated
  value remove_member(std::string const & key);
  /// Same as remove_member(const char* key, const char* end, value* removed),
  /// but 'key' is null-terminated.
  bool remove_member(const char* key, value* removed);
  /** \brief Remove the named map member.

      Update 'removed' iff removed.
      \param key may contain embedded nulls.
      \return true iff removed (no exceptions)
  */
  bool remove_member(std::string const& key, value* removed);
  /// Same as remove_member(std::string const& key, value* removed)
  bool remove_member(const char* key, const char* end, value* removed);
  /** \brief Remove the indexed array element.

      O(n) expensive operations.
      Update 'removed' iff removed.
      \return true iff removed (no exceptions)
  */
  bool remove_index(array_index i, value* removed);

  /// Return true if the object has a member named key.
  /// \note 'key' must be null-terminated.
  bool is_member(const char* key) const;
  /// Return true if the object has a member named key.
  /// \param key may contain embedded nulls.
  bool is_member(std::string const & key) const;
  /// Same as is_member(std::string const& key)const
  bool is_member(const char* key, const char* end) const;

  /// \brief Return a list of the member names.
  ///
  /// If null, return an empty list.
  /// \pre type() is vt_object or vt_null
  /// \post if type() was vt_null, it remains vt_null
  Members get_member_names() const;

  /// \deprecated Always pass len.
  void set_comment(const char* comment, comment_placement placement);
  /// Comments must be //... or /* ... */
  void set_comment(const char* comment, size_t len, comment_placement placement);
  /// Comments must be //... or /* ... */
  void set_comment(std::string const & comment, comment_placement placement);
  bool has_comment(comment_placement placement) const;
  /// Include delimiters and embedded newlines.
  std::string get_comment(comment_placement placement) const;

  std::string toStyledString() const;

  const_iterator begin() const;
  const_iterator end() const;

  iterator begin();
  iterator end();

  // Accessors for the [start, limit) range of bytes within the JSON text from
  // which this value was parsed, if any.
  void set_offset_start(size_t start);
  void set_offset_limit(size_t limit);
  size_t get_offset_start() const;
  size_t get_offset_limit() const;

private:
  void init_basic(value_type type, bool allocated = false);

  value& resolve_reference(const char* key);
  value& resolve_reference(const char* key, const char* end);

  struct comment_info {
    comment_info();
    ~comment_info();

    void set_comment(const char* text, size_t len);

    char* comment_;
  };

  // struct MemberNamesTransform
  //{
  //   typedef const char *result_type;
  //   const char *operator()( const czstring &name ) const
  //   {
  //      return name.c_str();
  //   }
  //};

  union ValueHolder {
    largest_int_t int_;
    largest_uint_t uint_;
    double real_;
    bool bool_;
    char* string_;  // actually ptr to unsigned, followed by str, unless !allocated_
    object_values* map_;
  } value_;
  value_type type_ : 8;
  unsigned int allocated_ : 1; // Notes: if declared as bool, bitfield is useless.
                               // If not allocated_, string_ must be null-terminated.
  comment_info* comments_;

  // [start, limit) byte offsets in the source JSON text from which this value
  // was extracted.
  size_t start_;
  size_t limit_;
};

/** \brief Experimental and untested: represents an element of the "path" to
 * access a node.
 */
class JSON_API path_argument {
public:
  friend class path;

  path_argument();
  path_argument(array_index index);
  path_argument(const char* key);
  path_argument(std::string const & key);

private:
  enum Kind {
    kindNone = 0,
    kindIndex,
    kindKey
  };
  std::string key_;
  array_index index_;
  Kind kind_;
};

/** \brief Experimental and untested: represents a "path" to access a node.
 *
 * Syntax:
 * - "." => root node
 * - ".[n]" => elements at index 'n' of root node (an array value)
 * - ".name" => member named 'name' of root node (an object value)
 * - ".name1.name2.name3"
 * - ".[0][1][2].name1[3]"
 * - ".%" => member name is provided as parameter
 * - ".[%]" => index is provied as parameter
 */
class JSON_API path {
public:
  path(std::string const & path,
       const path_argument& a1 = path_argument(),
       const path_argument& a2 = path_argument(),
       const path_argument& a3 = path_argument(),
       const path_argument& a4 = path_argument(),
       const path_argument& a5 = path_argument());

  value const & resolve(value const & root) const;
  value resolve(value const & root, value const & default_value) const;
  /// Creates the "path" to access the specified node and returns a reference on
  /// the node.
  value& make(value& root) const;

private:
  typedef std::vector<const path_argument*> InArgs;
  typedef std::vector<path_argument> Args;

  void makePath(std::string const & path, const InArgs& in);
  void addPathInArg(std::string const & path,
                    const InArgs& in,
                    InArgs::const_iterator& itInArg,
                    path_argument::Kind kind);
  void invalidPath(std::string const & path, int location);

  Args args_;
};

/** \brief base class for value iterators.
 *
 */
class JSON_API value_iterator_base {
public:
  typedef std::bidirectional_iterator_tag iterator_category;
  typedef unsigned int size_t;
  typedef int difference_type;
  typedef value_iterator_base self_type;

  value_iterator_base();
  explicit value_iterator_base(const value::object_values::iterator& current);

  bool operator==(const self_type& other) const { return is_equal(other); }

  bool operator!=(const self_type& other) const { return !is_equal(other); }

  difference_type operator-(const self_type& other) const {
    return other.compute_distance(*this);
  }

  /// Return either the index or the member name of the referenced value as a
  /// value.
  value key() const;

  /// Return the index of the referenced value, or -1 if it is not an vt_array.
  uint32_t index() const;

  /// Return the member name of the referenced value, or "" if it is not an
  /// vt_object.
  /// \note Avoid `c_str()` on result, as embedded zeroes are possible.
  std::string name() const;

  /// Return the member name of the referenced value, or NULL if it is not an
  /// vt_object. Because end is passed as an OUT param, embedded nulls are supported.
  char const* member_name(char const** end) const;

protected:
  value& deref() const;

  void increment();

  void decrement();

  difference_type compute_distance(const self_type& other) const;

  bool is_equal(const self_type& other) const;

  void copy(const self_type& other);

private:
  value::object_values::iterator current_;
  // Indicates that iterator is for a null value.
  bool is_null_;
};

/** \brief const iterator for object and array value.
 *
 */
class JSON_API value_const_iterator : public value_iterator_base {
  friend class value;

public:
  typedef const value value_type;
  //typedef unsigned int size_t;
  //typedef int difference_type;
  typedef value const & reference;
  typedef const value* pointer;
  typedef value_const_iterator self_type;

  value_const_iterator();

private:
/*! \internal Use by value to create an iterator.
 */
  explicit value_const_iterator(const value::object_values::iterator& current);
public:
  self_type& operator=(const value_iterator_base& other);

  self_type operator++(int) {
    self_type temp(*this);
    ++*this;
    return temp;
  }

  self_type operator--(int) {
    self_type temp(*this);
    --*this;
    return temp;
  }

  self_type& operator--() {
    decrement();
    return *this;
  }

  self_type& operator++() {
    increment();
    return *this;
  }

  reference operator*() const { return deref(); }

  pointer operator->() const { return &deref(); }
};

/** \brief Iterator for object and array value.
 */
class JSON_API value_iterator : public value_iterator_base {
  friend class value;

public:
  typedef value value_type;
  typedef unsigned int size_t;
  typedef int difference_type;
  typedef value& reference;
  typedef value* pointer;
  typedef value_iterator self_type;

  value_iterator();
  value_iterator(const value_const_iterator& other);
  value_iterator(const value_iterator& other);

private:
/*! \internal Use by value to create an iterator.
 */
  explicit value_iterator(const value::object_values::iterator& current);
public:
  self_type& operator=(const self_type& other);

  self_type operator++(int) {
    self_type temp(*this);
    ++*this;
    return temp;
  }

  self_type operator--(int) {
    self_type temp(*this);
    --*this;
    return temp;
  }

  self_type& operator--() {
    decrement();
    return *this;
  }

  self_type& operator++() {
    increment();
    return *this;
  }

  reference operator*() const { return deref(); }

  pointer operator->() const { return &deref(); }
};

} // namespace json


namespace std {
/// Specialize std::swap() for json::value.
template<>
inline void swap(json::value& a, json::value& b) { a.swap(b); }
}


#if defined(JSONCPP_DISABLE_DLL_INTERFACE_WARNING)
#pragma warning(pop)
#endif // if defined(JSONCPP_DISABLE_DLL_INTERFACE_WARNING)

#endif // sentry
