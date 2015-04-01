// Derived from public-domain/MIT-licensed code at
// https://github.com/open-source-parsers/jsoncpp. Thanks, Baptiste Lepilleur!

#ifndef _b6d1716efdfd4e8bb9fff5b55b9b64bd
#define _b6d1716efdfd4e8bb9fff5b55b9b64bd

#include "forwards.h"

namespace json {

/** \brief Configuration passed to reader and writer.
 * This configuration object can be used to force the reader or Writer
 * to behave in a standard conforming way.
 */
class JSON_API features {
public:
    /** \brief A configuration that allows all features and assumes all strings
   * are UTF-8.
   * - C & C++ comments are allowed
   * - Root object can be any JSON value
   * - Assumes value strings are encoded in UTF-8
   */
    static features all();

    /** \brief A configuration that is strictly compatible with the JSON
   * specification.
   * - Comments are forbidden.
   * - Root object must be either an array or an object value.
   * - Assumes value strings are encoded in UTF-8
   */
    static features strict_mode();

    /** \brief Initialize the configuration like JsonConfig::allFeatures;
   */
    features();

    /// \c true if comments are allowed. Default: \c true.
    bool allow_comments_;

    /// \c true if root must be either an array or an object value. Default: \c
    /// false.
    bool strict_root_;

    /// \c true if dropped null placeholders are allowed. Default: \c false.
    bool allow_dropped_null_placeholders_;

    /// \c true if numeric object key are allowed. Default: \c false.
    bool allow_numeric_keys_;
};

} // namespace json

#endif // CPPTL_JSON_FEATURES_H_INCLUDED
