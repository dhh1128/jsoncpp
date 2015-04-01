// Derived from public-domain/MIT-licensed code at
// https://github.com/open-source-parsers/jsoncpp. Thanks, Baptiste Lepilleur!

// included by json_value.cpp

namespace json {

// //////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////
// class value_iterator_base
// //////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////

value_iterator_base::value_iterator_base()
    : current_(), is_null_(true) {
}

value_iterator_base::value_iterator_base(
    value::object_values::iterator const & current)
    : current_(current), is_null_(false) {}

value& value_iterator_base::deref() const {
  return current_->second;
}

void value_iterator_base::increment() {
  ++current_;
}

void value_iterator_base::decrement() {
  --current_;
}

value_iterator_base::difference_type
value_iterator_base::compute_distance(self_type const & other) const {
  // Iterator for null value are initialized using the default
  // constructor, which initialize current_ to the default
  // std::map::iterator. As begin() and end() are two instance
  // of the default std::map::iterator, they can not be compared.
  // To allow this, we handle this comparison specifically.
  if (is_null_ && other.is_null_) {
    return 0;
  }

  // Usage of std::distance is not portable (does not compile with Sun Studio 12
  // RogueWave STL, which is the one used by default).
  // Using a portable hand-made version for non random iterator instead:
  //   return difference_type( std::distance( current_, other.current_ ) );
  difference_type my_distance = 0;
  for (value::object_values::iterator it = current_; it != other.current_;
       ++it) {
    ++my_distance;
  }
  return my_distance;
}

bool value_iterator_base::is_equal(self_type const & other) const {
  if (is_null_) {
    return other.is_null_;
  }
  return current_ == other.current_;
}

void value_iterator_base::copy(self_type const & other) {
  current_ = other.current_;
  is_null_ = other.is_null_;
}

value value_iterator_base::key() const {
  const value::czstring czstring = (*current_).first;
  if (czstring.data()) {
    if (czstring.is_static_string())
      return value(static_string(czstring.data()));
    return value(czstring.data(), czstring.data() + czstring.length());
  }
  return value(czstring.index());
}

uint32_t value_iterator_base::index() const {
  const value::czstring czstring = (*current_).first;
  if (!czstring.data())
    return czstring.index();
  return uint32_t(-1);
}

std::string value_iterator_base::name() const {
  char const* key;
  char const* end;
  key = member_name(&end);
  if (!key) return std::string();
  return std::string(key, end);
}

char const* value_iterator_base::member_name(char const** end) const {
  const char* name = (*current_).first.data();
  if (!name) {
    *end = NULL;
    return NULL;
  }
  *end = name + (*current_).first.length();
  return name;
}

// //////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////
// class value_const_iterator
// //////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////

value_const_iterator::value_const_iterator() {}

value_const_iterator::value_const_iterator(
    value::object_values::iterator const & current)
    : value_iterator_base(current) {}

value_const_iterator& value_const_iterator::
operator=(value_iterator_base const & other) {
  copy(other);
  return *this;
}

// //////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////
// class value_iterator
// //////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////

value_iterator::value_iterator() {}

value_iterator::value_iterator(value::object_values::iterator const & current)
    : value_iterator_base(current) {}

value_iterator::value_iterator(value_const_iterator const & other)
    : value_iterator_base(other) {}

value_iterator::value_iterator(value_iterator const & other)
    : value_iterator_base(other) {}

value_iterator& value_iterator::operator=(self_type const & other) {
  copy(other);
  return *this;
}

} // namespace json
