// Derived from public-domain/MIT-licensed code at
// https://github.com/open-source-parsers/jsoncpp. Thanks, Baptiste Lepilleur!

#ifndef _1b21596d9ecf48f28bb725321673eceb
#define _1b21596d9ecf48f28bb725321673eceb

#if !defined(JSON_IS_AMALGAMATION)
#include "config.h"
#endif // if !defined(JSON_IS_AMALGAMATION)

namespace json {

// writer.h
class fast_writer;
class styled_writer;

// reader.h
class reader;

// features.h
class features;

// value.h
typedef unsigned int array_index;
class static_string;
class path;
class path_argument;
class value;
class value_iterator_base;
class value_iterator;
class value_const_iterator;

} // end namespace

#endif // sentry
