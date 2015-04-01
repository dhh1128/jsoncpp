// Derived from public-domain/MIT-licensed code at
// https://github.com/open-source-parsers/jsoncpp. Thanks, Baptiste Lepilleur!

#include "jsontest.h"
#include <json/config.h>
#include <json/json.h>
#include <cstring>

// Make numeric limits more convenient to talk about.
// Assumes int type in 32 bits.
#define kint32max json::value::max_int
#define kint32min json::value::min_int
#define kuint32max json::value::max_uint
#define kint64max json::value::max_int64
#define kint64min json::value::min_int64
#define kuint64max json::value::max_uint64

//static const double kdint64max = double(kint64max);
//static const float kfint64max = float(kint64max);
static const float kfint32max = float(kint32max);
static const float kfuint32max = float(kuint32max);

// //////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////
// json Library test cases
// //////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////

#if !defined(JSON_USE_INT64_DOUBLE_CONVERSION)
static inline double uint64_to_double(uint64_t value) {
  return static_cast<double>(value);
}
#else  // if !defined(JSON_USE_INT64_DOUBLE_CONVERSION)
static inline double uint64_to_double(uint64_t value) {
  return static_cast<double>(int64_t(value / 2)) * 2.0 +
		 int64_t(value & 1);
}
#endif // if !defined(JSON_USE_INT64_DOUBLE_CONVERSION)

struct ValueTest : JsonTest::TestCase {
  json::value null_;
  json::value emptyArray_;
  json::value emptyObject_;
  json::value integer_;
  json::value unsignedInteger_;
  json::value smallUnsignedInteger_;
  json::value real_;
  json::value float_;
  json::value array1_;
  json::value object1_;
  json::value emptyString_;
  json::value string1_;
  json::value string_;
  json::value true_;
  json::value false_;

  ValueTest()
	  : emptyArray_(json::vt_array), emptyObject_(json::vt_object),
		integer_(123456789), unsignedInteger_(34567890u),
		smallUnsignedInteger_(uint32_t(json::value::max_int)),
		real_(1234.56789), float_(0.00390625f), emptyString_(""), string1_("a"),
		string_("sometext with space"), true_(true), false_(false) {
	array1_.append(1234);
	object1_["id"] = 1234;
  }

  struct IsCheck {
	/// Initialize all checks to \c false by default.
	IsCheck();

	bool isObject_;
	bool isArray_;
	bool isBool_;
	bool isString_;
	bool isNull_;

	bool isInt_;
	bool isInt64_;
	bool isUInt_;
	bool isUInt64_;
	bool isIntegral_;
	bool isDouble_;
	bool isNumeric_;
  };

  void checkConstMemberCount(const json::value& value,
							 unsigned int expectedCount);

  void checkMemberCount(json::value& value, unsigned int expectedCount);

  void checkIs(const json::value& value, const IsCheck& check);

  void checkIsLess(const json::value& x, const json::value& y);

  void checkIsEqual(const json::value& x, const json::value& y);

  /// Normalize the representation of floating-point number by stripped leading
  /// 0 in exponent.
  static std::string normalize_floating_point_str(std::string const & s);
};

std::string ValueTest::normalize_floating_point_str(std::string const & s) {
  std::string::size_type index = s.find_last_of("eE");
  if (index != std::string::npos) {
	std::string::size_type hasSign =
		(s[index + 1] == '+' || s[index + 1] == '-') ? 1 : 0;
	std::string::size_type exponent_start_index = index + 1 + hasSign;
	std::string normalized = s.substr(0, exponent_start_index);
	std::string::size_type index_digit =
		s.find_first_not_of('0', exponent_start_index);
	std::string exponent = "0";
	if (index_digit !=
		std::string::npos) // There is an exponent different from 0
	{
	  exponent = s.substr(index_digit);
	}
	return normalized + exponent;
  }
  return s;
}

JSONTEST_FIXTURE(ValueTest, checkNormalizeFloatingPointStr) {
  JSONTEST_ASSERT_STRING_EQUAL("0.0", normalize_floating_point_str("0.0"));
  JSONTEST_ASSERT_STRING_EQUAL("0e0", normalize_floating_point_str("0e0"));
  JSONTEST_ASSERT_STRING_EQUAL("1234.0", normalize_floating_point_str("1234.0"));
  JSONTEST_ASSERT_STRING_EQUAL("1234.0e0",
							   normalize_floating_point_str("1234.0e0"));
  JSONTEST_ASSERT_STRING_EQUAL("1234.0e+0",
							   normalize_floating_point_str("1234.0e+0"));
  JSONTEST_ASSERT_STRING_EQUAL("1234e-1", normalize_floating_point_str("1234e-1"));
  JSONTEST_ASSERT_STRING_EQUAL("1234e10", normalize_floating_point_str("1234e10"));
  JSONTEST_ASSERT_STRING_EQUAL("1234e10",
							   normalize_floating_point_str("1234e010"));
  JSONTEST_ASSERT_STRING_EQUAL("1234e+10",
							   normalize_floating_point_str("1234e+010"));
  JSONTEST_ASSERT_STRING_EQUAL("1234e-10",
							   normalize_floating_point_str("1234e-010"));
  JSONTEST_ASSERT_STRING_EQUAL("1234e+100",
							   normalize_floating_point_str("1234e+100"));
  JSONTEST_ASSERT_STRING_EQUAL("1234e-100",
							   normalize_floating_point_str("1234e-100"));
  JSONTEST_ASSERT_STRING_EQUAL("1234e+1",
							   normalize_floating_point_str("1234e+001"));
}

JSONTEST_FIXTURE(ValueTest, memberCount) {
  JSONTEST_ASSERT_PRED(checkMemberCount(emptyArray_, 0));
  JSONTEST_ASSERT_PRED(checkMemberCount(emptyObject_, 0));
  JSONTEST_ASSERT_PRED(checkMemberCount(array1_, 1));
  JSONTEST_ASSERT_PRED(checkMemberCount(object1_, 1));
  JSONTEST_ASSERT_PRED(checkMemberCount(null_, 0));
  JSONTEST_ASSERT_PRED(checkMemberCount(integer_, 0));
  JSONTEST_ASSERT_PRED(checkMemberCount(unsignedInteger_, 0));
  JSONTEST_ASSERT_PRED(checkMemberCount(smallUnsignedInteger_, 0));
  JSONTEST_ASSERT_PRED(checkMemberCount(real_, 0));
  JSONTEST_ASSERT_PRED(checkMemberCount(emptyString_, 0));
  JSONTEST_ASSERT_PRED(checkMemberCount(string_, 0));
  JSONTEST_ASSERT_PRED(checkMemberCount(true_, 0));
}

JSONTEST_FIXTURE(ValueTest, objects) {
  // Types
  IsCheck checks;
  checks.isObject_ = true;
  JSONTEST_ASSERT_PRED(checkIs(emptyObject_, checks));
  JSONTEST_ASSERT_PRED(checkIs(object1_, checks));

  JSONTEST_ASSERT_EQUAL(json::vt_object, emptyObject_.type());

  // Empty object okay
  JSONTEST_ASSERT(emptyObject_.is_convertible_to(json::vt_null));

  // Non-empty object not okay
  JSONTEST_ASSERT(!object1_.is_convertible_to(json::vt_null));

  // Always okay
  JSONTEST_ASSERT(emptyObject_.is_convertible_to(json::vt_object));

  // Never okay
  JSONTEST_ASSERT(!emptyObject_.is_convertible_to(json::vt_array));
  JSONTEST_ASSERT(!emptyObject_.is_convertible_to(json::vt_int));
  JSONTEST_ASSERT(!emptyObject_.is_convertible_to(json::vt_uint));
  JSONTEST_ASSERT(!emptyObject_.is_convertible_to(json::vt_real));
  JSONTEST_ASSERT(!emptyObject_.is_convertible_to(json::vt_bool));
  JSONTEST_ASSERT(!emptyObject_.is_convertible_to(json::vt_string));

  // Access through const reference
  const json::value& constObject = object1_;

  JSONTEST_ASSERT_EQUAL(json::value(1234), constObject["id"]);
  JSONTEST_ASSERT_EQUAL(json::value(), constObject["unknown id"]);

  // Access through non-const reference
  JSONTEST_ASSERT_EQUAL(json::value(1234), object1_["id"]);
  JSONTEST_ASSERT_EQUAL(json::value(), object1_["unknown id"]);

  object1_["some other id"] = "foo";
  JSONTEST_ASSERT_EQUAL(json::value("foo"), object1_["some other id"]);
  JSONTEST_ASSERT_EQUAL(json::value("foo"), object1_["some other id"]);

  // Remove.
  json::value got;
  bool did;
  did = object1_.remove_member("some other id", &got);
  JSONTEST_ASSERT_EQUAL(json::value("foo"), got);
  JSONTEST_ASSERT_EQUAL(true, did);
  got = json::value("bar");
  did = object1_.remove_member("some other id", &got);
  JSONTEST_ASSERT_EQUAL(json::value("bar"), got);
  JSONTEST_ASSERT_EQUAL(false, did);
}

JSONTEST_FIXTURE(ValueTest, arrays) {
  const unsigned int index0 = 0;

  // Types
  IsCheck checks;
  checks.isArray_ = true;
  JSONTEST_ASSERT_PRED(checkIs(emptyArray_, checks));
  JSONTEST_ASSERT_PRED(checkIs(array1_, checks));

  JSONTEST_ASSERT_EQUAL(json::vt_array, array1_.type());

  // Empty array okay
  JSONTEST_ASSERT(emptyArray_.is_convertible_to(json::vt_null));

  // Non-empty array not okay
  JSONTEST_ASSERT(!array1_.is_convertible_to(json::vt_null));

  // Always okay
  JSONTEST_ASSERT(emptyArray_.is_convertible_to(json::vt_array));

  // Never okay
  JSONTEST_ASSERT(!emptyArray_.is_convertible_to(json::vt_object));
  JSONTEST_ASSERT(!emptyArray_.is_convertible_to(json::vt_int));
  JSONTEST_ASSERT(!emptyArray_.is_convertible_to(json::vt_uint));
  JSONTEST_ASSERT(!emptyArray_.is_convertible_to(json::vt_real));
  JSONTEST_ASSERT(!emptyArray_.is_convertible_to(json::vt_bool));
  JSONTEST_ASSERT(!emptyArray_.is_convertible_to(json::vt_string));

  // Access through const reference
  const json::value& constArray = array1_;
  JSONTEST_ASSERT_EQUAL(json::value(1234), constArray[index0]);
  JSONTEST_ASSERT_EQUAL(json::value(1234), constArray[0]);

  // Access through non-const reference
  JSONTEST_ASSERT_EQUAL(json::value(1234), array1_[index0]);
  JSONTEST_ASSERT_EQUAL(json::value(1234), array1_[0]);

  array1_[2] = json::value(17);
  JSONTEST_ASSERT_EQUAL(json::value(), array1_[1]);
  JSONTEST_ASSERT_EQUAL(json::value(17), array1_[2]);
  json::value got;
  JSONTEST_ASSERT_EQUAL(true, array1_.remove_index(2, &got));
  JSONTEST_ASSERT_EQUAL(json::value(17), got);
  JSONTEST_ASSERT_EQUAL(false, array1_.remove_index(2, &got)); // gone now
}

JSONTEST_FIXTURE(ValueTest, null) {
  JSONTEST_ASSERT_EQUAL(json::vt_null, null_.type());

  IsCheck checks;
  checks.isNull_ = true;
  JSONTEST_ASSERT_PRED(checkIs(null_, checks));

  JSONTEST_ASSERT(null_.is_convertible_to(json::vt_null));
  JSONTEST_ASSERT(null_.is_convertible_to(json::vt_int));
  JSONTEST_ASSERT(null_.is_convertible_to(json::vt_uint));
  JSONTEST_ASSERT(null_.is_convertible_to(json::vt_real));
  JSONTEST_ASSERT(null_.is_convertible_to(json::vt_bool));
  JSONTEST_ASSERT(null_.is_convertible_to(json::vt_string));
  JSONTEST_ASSERT(null_.is_convertible_to(json::vt_array));
  JSONTEST_ASSERT(null_.is_convertible_to(json::vt_object));

  JSONTEST_ASSERT_EQUAL(int32_t(0), null_.as_int());
  JSONTEST_ASSERT_EQUAL(json::largest_int_t(0), null_.as_largest_int());
  JSONTEST_ASSERT_EQUAL(uint32_t(0), null_.as_uint());
  JSONTEST_ASSERT_EQUAL(json::largest_uint_t(0), null_.as_largest_uint());
  JSONTEST_ASSERT_EQUAL(0.0, null_.as_double());
  JSONTEST_ASSERT_EQUAL(0.0, null_.as_float());
  JSONTEST_ASSERT_STRING_EQUAL("", null_.as_string());

  JSONTEST_ASSERT_EQUAL(json::value::null, null_);
}

JSONTEST_FIXTURE(ValueTest, strings) {
  JSONTEST_ASSERT_EQUAL(json::vt_string, string1_.type());

  IsCheck checks;
  checks.isString_ = true;
  JSONTEST_ASSERT_PRED(checkIs(emptyString_, checks));
  JSONTEST_ASSERT_PRED(checkIs(string_, checks));
  JSONTEST_ASSERT_PRED(checkIs(string1_, checks));

  // Empty string okay
  JSONTEST_ASSERT(emptyString_.is_convertible_to(json::vt_null));

  // Non-empty string not okay
  JSONTEST_ASSERT(!string1_.is_convertible_to(json::vt_null));

  // Always okay
  JSONTEST_ASSERT(string1_.is_convertible_to(json::vt_string));

  // Never okay
  JSONTEST_ASSERT(!string1_.is_convertible_to(json::vt_object));
  JSONTEST_ASSERT(!string1_.is_convertible_to(json::vt_array));
  JSONTEST_ASSERT(!string1_.is_convertible_to(json::vt_int));
  JSONTEST_ASSERT(!string1_.is_convertible_to(json::vt_uint));
  JSONTEST_ASSERT(!string1_.is_convertible_to(json::vt_real));

  JSONTEST_ASSERT_STRING_EQUAL("a", string1_.as_string());
  JSONTEST_ASSERT_STRING_EQUAL("a", string1_.as_cstring());
}

JSONTEST_FIXTURE(ValueTest, bools) {
  JSONTEST_ASSERT_EQUAL(json::vt_bool, false_.type());

  IsCheck checks;
  checks.isBool_ = true;
  JSONTEST_ASSERT_PRED(checkIs(false_, checks));
  JSONTEST_ASSERT_PRED(checkIs(true_, checks));

  // False okay
  JSONTEST_ASSERT(false_.is_convertible_to(json::vt_null));

  // True not okay
  JSONTEST_ASSERT(!true_.is_convertible_to(json::vt_null));

  // Always okay
  JSONTEST_ASSERT(true_.is_convertible_to(json::vt_int));
  JSONTEST_ASSERT(true_.is_convertible_to(json::vt_uint));
  JSONTEST_ASSERT(true_.is_convertible_to(json::vt_real));
  JSONTEST_ASSERT(true_.is_convertible_to(json::vt_bool));
  JSONTEST_ASSERT(true_.is_convertible_to(json::vt_string));

  // Never okay
  JSONTEST_ASSERT(!true_.is_convertible_to(json::vt_array));
  JSONTEST_ASSERT(!true_.is_convertible_to(json::vt_object));

  JSONTEST_ASSERT_EQUAL(true, true_.as_bool());
  JSONTEST_ASSERT_EQUAL(1, true_.as_int());
  JSONTEST_ASSERT_EQUAL(1, true_.as_largest_int());
  JSONTEST_ASSERT_EQUAL(1, true_.as_uint());
  JSONTEST_ASSERT_EQUAL(1, true_.as_largest_uint());
  JSONTEST_ASSERT_EQUAL(1.0, true_.as_double());
  JSONTEST_ASSERT_EQUAL(1.0, true_.as_float());

  JSONTEST_ASSERT_EQUAL(false, false_.as_bool());
  JSONTEST_ASSERT_EQUAL(0, false_.as_int());
  JSONTEST_ASSERT_EQUAL(0, false_.as_largest_int());
  JSONTEST_ASSERT_EQUAL(0, false_.as_uint());
  JSONTEST_ASSERT_EQUAL(0, false_.as_largest_uint());
  JSONTEST_ASSERT_EQUAL(0.0, false_.as_double());
  JSONTEST_ASSERT_EQUAL(0.0, false_.as_float());
}

JSONTEST_FIXTURE(ValueTest, integers) {
  IsCheck checks;
  json::value val;

  // Conversions that don't depend on the value.
  JSONTEST_ASSERT(json::value(17).is_convertible_to(json::vt_real));
  JSONTEST_ASSERT(json::value(17).is_convertible_to(json::vt_string));
  JSONTEST_ASSERT(json::value(17).is_convertible_to(json::vt_bool));
  JSONTEST_ASSERT(!json::value(17).is_convertible_to(json::vt_array));
  JSONTEST_ASSERT(!json::value(17).is_convertible_to(json::vt_object));

  JSONTEST_ASSERT(json::value(17U).is_convertible_to(json::vt_real));
  JSONTEST_ASSERT(json::value(17U).is_convertible_to(json::vt_string));
  JSONTEST_ASSERT(json::value(17U).is_convertible_to(json::vt_bool));
  JSONTEST_ASSERT(!json::value(17U).is_convertible_to(json::vt_array));
  JSONTEST_ASSERT(!json::value(17U).is_convertible_to(json::vt_object));

  JSONTEST_ASSERT(json::value(17.0).is_convertible_to(json::vt_real));
  JSONTEST_ASSERT(json::value(17.0).is_convertible_to(json::vt_string));
  JSONTEST_ASSERT(json::value(17.0).is_convertible_to(json::vt_bool));
  JSONTEST_ASSERT(!json::value(17.0).is_convertible_to(json::vt_array));
  JSONTEST_ASSERT(!json::value(17.0).is_convertible_to(json::vt_object));

  // Default int
  val = json::value(json::vt_int);

  JSONTEST_ASSERT_EQUAL(json::vt_int, val.type());

  checks = IsCheck();
  checks.isInt_ = true;
  checks.isInt64_ = true;
  checks.isUInt_ = true;
  checks.isUInt64_ = true;
  checks.isIntegral_ = true;
  checks.isDouble_ = true;
  checks.isNumeric_ = true;
  JSONTEST_ASSERT_PRED(checkIs(val, checks));

  JSONTEST_ASSERT(val.is_convertible_to(json::vt_null));
  JSONTEST_ASSERT(val.is_convertible_to(json::vt_int));
  JSONTEST_ASSERT(val.is_convertible_to(json::vt_uint));

  JSONTEST_ASSERT_EQUAL(0, val.as_int());
  JSONTEST_ASSERT_EQUAL(0, val.as_largest_int());
  JSONTEST_ASSERT_EQUAL(0, val.as_uint());
  JSONTEST_ASSERT_EQUAL(0, val.as_largest_uint());
  JSONTEST_ASSERT_EQUAL(0.0, val.as_double());
  JSONTEST_ASSERT_EQUAL(0.0, val.as_float());
  JSONTEST_ASSERT_EQUAL(false, val.as_bool());
  JSONTEST_ASSERT_STRING_EQUAL("0", val.as_string());

  // Default uint
  val = json::value(json::vt_uint);

  JSONTEST_ASSERT_EQUAL(json::vt_uint, val.type());

  checks = IsCheck();
  checks.isInt_ = true;
  checks.isInt64_ = true;
  checks.isUInt_ = true;
  checks.isUInt64_ = true;
  checks.isIntegral_ = true;
  checks.isDouble_ = true;
  checks.isNumeric_ = true;
  JSONTEST_ASSERT_PRED(checkIs(val, checks));

  JSONTEST_ASSERT(val.is_convertible_to(json::vt_null));
  JSONTEST_ASSERT(val.is_convertible_to(json::vt_int));
  JSONTEST_ASSERT(val.is_convertible_to(json::vt_uint));

  JSONTEST_ASSERT_EQUAL(0, val.as_int());
  JSONTEST_ASSERT_EQUAL(0, val.as_largest_int());
  JSONTEST_ASSERT_EQUAL(0, val.as_uint());
  JSONTEST_ASSERT_EQUAL(0, val.as_largest_uint());
  JSONTEST_ASSERT_EQUAL(0.0, val.as_double());
  JSONTEST_ASSERT_EQUAL(0.0, val.as_float());
  JSONTEST_ASSERT_EQUAL(false, val.as_bool());
  JSONTEST_ASSERT_STRING_EQUAL("0", val.as_string());

  // Default real
  val = json::value(json::vt_real);

  JSONTEST_ASSERT_EQUAL(json::vt_real, val.type());

  JSONTEST_ASSERT(val.is_convertible_to(json::vt_null));
  JSONTEST_ASSERT(val.is_convertible_to(json::vt_int));
  JSONTEST_ASSERT(val.is_convertible_to(json::vt_uint));

  checks = IsCheck();
  checks.isInt_ = true;
  checks.isInt64_ = true;
  checks.isUInt_ = true;
  checks.isUInt64_ = true;
  checks.isIntegral_ = true;
  checks.isDouble_ = true;
  checks.isNumeric_ = true;
  JSONTEST_ASSERT_PRED(checkIs(val, checks));

  JSONTEST_ASSERT_EQUAL(0, val.as_int());
  JSONTEST_ASSERT_EQUAL(0, val.as_largest_int());
  JSONTEST_ASSERT_EQUAL(0, val.as_uint());
  JSONTEST_ASSERT_EQUAL(0, val.as_largest_uint());
  JSONTEST_ASSERT_EQUAL(0.0, val.as_double());
  JSONTEST_ASSERT_EQUAL(0.0, val.as_float());
  JSONTEST_ASSERT_EQUAL(false, val.as_bool());
  JSONTEST_ASSERT_STRING_EQUAL("0", val.as_string());

  // Zero (signed constructor arg)
  val = json::value(0);

  JSONTEST_ASSERT_EQUAL(json::vt_int, val.type());

  checks = IsCheck();
  checks.isInt_ = true;
  checks.isInt64_ = true;
  checks.isUInt_ = true;
  checks.isUInt64_ = true;
  checks.isIntegral_ = true;
  checks.isDouble_ = true;
  checks.isNumeric_ = true;
  JSONTEST_ASSERT_PRED(checkIs(val, checks));

  JSONTEST_ASSERT(val.is_convertible_to(json::vt_null));
  JSONTEST_ASSERT(val.is_convertible_to(json::vt_int));
  JSONTEST_ASSERT(val.is_convertible_to(json::vt_uint));

  JSONTEST_ASSERT_EQUAL(0, val.as_int());
  JSONTEST_ASSERT_EQUAL(0, val.as_largest_int());
  JSONTEST_ASSERT_EQUAL(0, val.as_uint());
  JSONTEST_ASSERT_EQUAL(0, val.as_largest_uint());
  JSONTEST_ASSERT_EQUAL(0.0, val.as_double());
  JSONTEST_ASSERT_EQUAL(0.0, val.as_float());
  JSONTEST_ASSERT_EQUAL(false, val.as_bool());
  JSONTEST_ASSERT_STRING_EQUAL("0", val.as_string());

  // Zero (unsigned constructor arg)
  val = json::value(0u);

  JSONTEST_ASSERT_EQUAL(json::vt_uint, val.type());

  checks = IsCheck();
  checks.isInt_ = true;
  checks.isInt64_ = true;
  checks.isUInt_ = true;
  checks.isUInt64_ = true;
  checks.isIntegral_ = true;
  checks.isDouble_ = true;
  checks.isNumeric_ = true;
  JSONTEST_ASSERT_PRED(checkIs(val, checks));

  JSONTEST_ASSERT(val.is_convertible_to(json::vt_null));
  JSONTEST_ASSERT(val.is_convertible_to(json::vt_int));
  JSONTEST_ASSERT(val.is_convertible_to(json::vt_uint));

  JSONTEST_ASSERT_EQUAL(0, val.as_int());
  JSONTEST_ASSERT_EQUAL(0, val.as_largest_int());
  JSONTEST_ASSERT_EQUAL(0, val.as_uint());
  JSONTEST_ASSERT_EQUAL(0, val.as_largest_uint());
  JSONTEST_ASSERT_EQUAL(0.0, val.as_double());
  JSONTEST_ASSERT_EQUAL(0.0, val.as_float());
  JSONTEST_ASSERT_EQUAL(false, val.as_bool());
  JSONTEST_ASSERT_STRING_EQUAL("0", val.as_string());

  // Zero (floating-point constructor arg)
  val = json::value(0.0);

  JSONTEST_ASSERT_EQUAL(json::vt_real, val.type());

  checks = IsCheck();
  checks.isInt_ = true;
  checks.isInt64_ = true;
  checks.isUInt_ = true;
  checks.isUInt64_ = true;
  checks.isIntegral_ = true;
  checks.isDouble_ = true;
  checks.isNumeric_ = true;
  JSONTEST_ASSERT_PRED(checkIs(val, checks));

  JSONTEST_ASSERT(val.is_convertible_to(json::vt_null));
  JSONTEST_ASSERT(val.is_convertible_to(json::vt_int));
  JSONTEST_ASSERT(val.is_convertible_to(json::vt_uint));

  JSONTEST_ASSERT_EQUAL(0, val.as_int());
  JSONTEST_ASSERT_EQUAL(0, val.as_largest_int());
  JSONTEST_ASSERT_EQUAL(0, val.as_uint());
  JSONTEST_ASSERT_EQUAL(0, val.as_largest_uint());
  JSONTEST_ASSERT_EQUAL(0.0, val.as_double());
  JSONTEST_ASSERT_EQUAL(0.0, val.as_float());
  JSONTEST_ASSERT_EQUAL(false, val.as_bool());
  JSONTEST_ASSERT_STRING_EQUAL("0", val.as_string());

  // 2^20 (signed constructor arg)
  val = json::value(1 << 20);

  JSONTEST_ASSERT_EQUAL(json::vt_int, val.type());
  checks = IsCheck();
  checks.isInt_ = true;
  checks.isInt64_ = true;
  checks.isUInt_ = true;
  checks.isUInt64_ = true;
  checks.isIntegral_ = true;
  checks.isDouble_ = true;
  checks.isNumeric_ = true;
  JSONTEST_ASSERT_PRED(checkIs(val, checks));

  JSONTEST_ASSERT(!val.is_convertible_to(json::vt_null));
  JSONTEST_ASSERT(val.is_convertible_to(json::vt_int));
  JSONTEST_ASSERT(val.is_convertible_to(json::vt_uint));

  JSONTEST_ASSERT_EQUAL((1 << 20), val.as_int());
  JSONTEST_ASSERT_EQUAL((1 << 20), val.as_largest_int());
  JSONTEST_ASSERT_EQUAL((1 << 20), val.as_uint());
  JSONTEST_ASSERT_EQUAL((1 << 20), val.as_largest_uint());
  JSONTEST_ASSERT_EQUAL((1 << 20), val.as_double());
  JSONTEST_ASSERT_EQUAL((1 << 20), val.as_float());
  JSONTEST_ASSERT_EQUAL(true, val.as_bool());
  JSONTEST_ASSERT_STRING_EQUAL("1048576", val.as_string());

  // 2^20 (unsigned constructor arg)
  val = json::value(uint32_t(1 << 20));

  JSONTEST_ASSERT_EQUAL(json::vt_uint, val.type());

  checks = IsCheck();
  checks.isInt_ = true;
  checks.isInt64_ = true;
  checks.isUInt_ = true;
  checks.isUInt64_ = true;
  checks.isIntegral_ = true;
  checks.isDouble_ = true;
  checks.isNumeric_ = true;
  JSONTEST_ASSERT_PRED(checkIs(val, checks));

  JSONTEST_ASSERT(!val.is_convertible_to(json::vt_null));
  JSONTEST_ASSERT(val.is_convertible_to(json::vt_int));
  JSONTEST_ASSERT(val.is_convertible_to(json::vt_uint));

  JSONTEST_ASSERT_EQUAL((1 << 20), val.as_int());
  JSONTEST_ASSERT_EQUAL((1 << 20), val.as_largest_int());
  JSONTEST_ASSERT_EQUAL((1 << 20), val.as_uint());
  JSONTEST_ASSERT_EQUAL((1 << 20), val.as_largest_uint());
  JSONTEST_ASSERT_EQUAL((1 << 20), val.as_double());
  JSONTEST_ASSERT_EQUAL((1 << 20), val.as_float());
  JSONTEST_ASSERT_EQUAL(true, val.as_bool());
  JSONTEST_ASSERT_STRING_EQUAL("1048576", val.as_string());

  // 2^20 (floating-point constructor arg)
  val = json::value((1 << 20) / 1.0);

  JSONTEST_ASSERT_EQUAL(json::vt_real, val.type());

  checks = IsCheck();
  checks.isInt_ = true;
  checks.isInt64_ = true;
  checks.isUInt_ = true;
  checks.isUInt64_ = true;
  checks.isIntegral_ = true;
  checks.isDouble_ = true;
  checks.isNumeric_ = true;
  JSONTEST_ASSERT_PRED(checkIs(val, checks));

  JSONTEST_ASSERT(!val.is_convertible_to(json::vt_null));
  JSONTEST_ASSERT(val.is_convertible_to(json::vt_int));
  JSONTEST_ASSERT(val.is_convertible_to(json::vt_uint));

  JSONTEST_ASSERT_EQUAL((1 << 20), val.as_int());
  JSONTEST_ASSERT_EQUAL((1 << 20), val.as_largest_int());
  JSONTEST_ASSERT_EQUAL((1 << 20), val.as_uint());
  JSONTEST_ASSERT_EQUAL((1 << 20), val.as_largest_uint());
  JSONTEST_ASSERT_EQUAL((1 << 20), val.as_double());
  JSONTEST_ASSERT_EQUAL((1 << 20), val.as_float());
  JSONTEST_ASSERT_EQUAL(true, val.as_bool());
  JSONTEST_ASSERT_STRING_EQUAL("1048576",
							   normalize_floating_point_str(val.as_string()));

  // -2^20
  val = json::value(-(1 << 20));

  JSONTEST_ASSERT_EQUAL(json::vt_int, val.type());

  checks = IsCheck();
  checks.isInt_ = true;
  checks.isInt64_ = true;
  checks.isIntegral_ = true;
  checks.isDouble_ = true;
  checks.isNumeric_ = true;
  JSONTEST_ASSERT_PRED(checkIs(val, checks));

  JSONTEST_ASSERT(!val.is_convertible_to(json::vt_null));
  JSONTEST_ASSERT(val.is_convertible_to(json::vt_int));
  JSONTEST_ASSERT(!val.is_convertible_to(json::vt_uint));

  JSONTEST_ASSERT_EQUAL(-(1 << 20), val.as_int());
  JSONTEST_ASSERT_EQUAL(-(1 << 20), val.as_largest_int());
  JSONTEST_ASSERT_EQUAL(-(1 << 20), val.as_double());
  JSONTEST_ASSERT_EQUAL(-(1 << 20), val.as_float());
  JSONTEST_ASSERT_EQUAL(true, val.as_bool());
  JSONTEST_ASSERT_STRING_EQUAL("-1048576", val.as_string());

  // int32 max
  val = json::value(kint32max);

  JSONTEST_ASSERT_EQUAL(json::vt_int, val.type());

  checks = IsCheck();
  checks.isInt_ = true;
  checks.isInt64_ = true;
  checks.isUInt_ = true;
  checks.isUInt64_ = true;
  checks.isIntegral_ = true;
  checks.isDouble_ = true;
  checks.isNumeric_ = true;
  JSONTEST_ASSERT_PRED(checkIs(val, checks));

  JSONTEST_ASSERT(!val.is_convertible_to(json::vt_null));
  JSONTEST_ASSERT(val.is_convertible_to(json::vt_int));
  JSONTEST_ASSERT(val.is_convertible_to(json::vt_uint));

  JSONTEST_ASSERT_EQUAL(kint32max, val.as_int());
  JSONTEST_ASSERT_EQUAL(kint32max, val.as_largest_int());
  JSONTEST_ASSERT_EQUAL(kint32max, val.as_uint());
  JSONTEST_ASSERT_EQUAL(kint32max, val.as_largest_uint());
  JSONTEST_ASSERT_EQUAL(kint32max, val.as_double());
  JSONTEST_ASSERT_EQUAL(kfint32max, val.as_float());
  JSONTEST_ASSERT_EQUAL(true, val.as_bool());
  JSONTEST_ASSERT_STRING_EQUAL("2147483647", val.as_string());

  // int32 min
  val = json::value(kint32min);

  JSONTEST_ASSERT_EQUAL(json::vt_int, val.type());

  checks = IsCheck();
  checks.isInt_ = true;
  checks.isInt64_ = true;
  checks.isIntegral_ = true;
  checks.isDouble_ = true;
  checks.isNumeric_ = true;
  JSONTEST_ASSERT_PRED(checkIs(val, checks));

  JSONTEST_ASSERT(!val.is_convertible_to(json::vt_null));
  JSONTEST_ASSERT(val.is_convertible_to(json::vt_int));
  JSONTEST_ASSERT(!val.is_convertible_to(json::vt_uint));

  JSONTEST_ASSERT_EQUAL(kint32min, val.as_int());
  JSONTEST_ASSERT_EQUAL(kint32min, val.as_largest_int());
  JSONTEST_ASSERT_EQUAL(kint32min, val.as_double());
  JSONTEST_ASSERT_EQUAL(kint32min, val.as_float());
  JSONTEST_ASSERT_EQUAL(true, val.as_bool());
  JSONTEST_ASSERT_STRING_EQUAL("-2147483648", val.as_string());

  // uint32 max
  val = json::value(kuint32max);

  JSONTEST_ASSERT_EQUAL(json::vt_uint, val.type());

  checks = IsCheck();
  checks.isInt64_ = true;
  checks.isUInt_ = true;
  checks.isUInt64_ = true;
  checks.isIntegral_ = true;
  checks.isDouble_ = true;
  checks.isNumeric_ = true;
  JSONTEST_ASSERT_PRED(checkIs(val, checks));

  JSONTEST_ASSERT(!val.is_convertible_to(json::vt_null));
  JSONTEST_ASSERT(!val.is_convertible_to(json::vt_int));
  JSONTEST_ASSERT(val.is_convertible_to(json::vt_uint));

#ifndef JSON_NO_INT64
  JSONTEST_ASSERT_EQUAL(kuint32max, val.as_largest_int());
#endif
  JSONTEST_ASSERT_EQUAL(kuint32max, val.as_uint());
  JSONTEST_ASSERT_EQUAL(kuint32max, val.as_largest_uint());
  JSONTEST_ASSERT_EQUAL(kuint32max, val.as_double());
  JSONTEST_ASSERT_EQUAL(kfuint32max, val.as_float());
  JSONTEST_ASSERT_EQUAL(true, val.as_bool());
  JSONTEST_ASSERT_STRING_EQUAL("4294967295", val.as_string());

#ifdef JSON_NO_INT64
  // int64 max
  val = json::value(double(kint64max));

  JSONTEST_ASSERT_EQUAL(json::vt_real, val.type());

  checks = IsCheck();
  checks.isDouble_ = true;
  checks.isNumeric_ = true;
  JSONTEST_ASSERT_PRED(checkIs(val, checks));

  JSONTEST_ASSERT(!val.is_convertible_to(json::vt_null));
  JSONTEST_ASSERT(!val.is_convertible_to(json::vt_int));
  JSONTEST_ASSERT(!val.is_convertible_to(json::vt_uint));

  JSONTEST_ASSERT_EQUAL(double(kint64max), val.as_double());
  JSONTEST_ASSERT_EQUAL(float(kint64max), val.as_float());
  JSONTEST_ASSERT_EQUAL(true, val.as_bool());
  JSONTEST_ASSERT_STRING_EQUAL("9.22337e+18", val.as_string());

  // int64 min
  val = json::value(double(kint64min));

  JSONTEST_ASSERT_EQUAL(json::vt_real, val.type());

  checks = IsCheck();
  checks.isDouble_ = true;
  checks.isNumeric_ = true;
  JSONTEST_ASSERT_PRED(checkIs(val, checks));

  JSONTEST_ASSERT(!val.is_convertible_to(json::vt_null));
  JSONTEST_ASSERT(!val.is_convertible_to(json::vt_int));
  JSONTEST_ASSERT(!val.is_convertible_to(json::vt_uint));

  JSONTEST_ASSERT_EQUAL(double(kint64min), val.as_double());
  JSONTEST_ASSERT_EQUAL(float(kint64min), val.as_float());
  JSONTEST_ASSERT_EQUAL(true, val.as_bool());
  JSONTEST_ASSERT_STRING_EQUAL("-9.22337e+18", val.as_string());

  // uint64 max
  val = json::value(double(kuint64max));

  JSONTEST_ASSERT_EQUAL(json::vt_real, val.type());

  checks = IsCheck();
  checks.isDouble_ = true;
  checks.isNumeric_ = true;
  JSONTEST_ASSERT_PRED(checkIs(val, checks));

  JSONTEST_ASSERT(!val.is_convertible_to(json::vt_null));
  JSONTEST_ASSERT(!val.is_convertible_to(json::vt_int));
  JSONTEST_ASSERT(!val.is_convertible_to(json::vt_uint));

  JSONTEST_ASSERT_EQUAL(double(kuint64max), val.as_double());
  JSONTEST_ASSERT_EQUAL(float(kuint64max), val.as_float());
  JSONTEST_ASSERT_EQUAL(true, val.as_bool());
  JSONTEST_ASSERT_STRING_EQUAL("1.84467e+19", val.as_string());
#else // ifdef JSON_NO_INT64
  // 2^40 (signed constructor arg)
  val = json::value(int64_t(1) << 40);

  JSONTEST_ASSERT_EQUAL(json::vt_int, val.type());

  checks = IsCheck();
  checks.isInt64_ = true;
  checks.isUInt64_ = true;
  checks.isIntegral_ = true;
  checks.isDouble_ = true;
  checks.isNumeric_ = true;
  JSONTEST_ASSERT_PRED(checkIs(val, checks));

  JSONTEST_ASSERT(!val.is_convertible_to(json::vt_null));
  JSONTEST_ASSERT(!val.is_convertible_to(json::vt_int));
  JSONTEST_ASSERT(!val.is_convertible_to(json::vt_uint));

  JSONTEST_ASSERT_EQUAL((int64_t(1) << 40), val.as_int64());
  JSONTEST_ASSERT_EQUAL((int64_t(1) << 40), val.as_largest_int());
  JSONTEST_ASSERT_EQUAL((int64_t(1) << 40), val.as_uint64());
  JSONTEST_ASSERT_EQUAL((int64_t(1) << 40), val.as_largest_uint());
  JSONTEST_ASSERT_EQUAL((int64_t(1) << 40), val.as_double());
  JSONTEST_ASSERT_EQUAL((int64_t(1) << 40), val.as_float());
  JSONTEST_ASSERT_EQUAL(true, val.as_bool());
  JSONTEST_ASSERT_STRING_EQUAL("1099511627776", val.as_string());

  // 2^40 (unsigned constructor arg)
  val = json::value(uint64_t(1) << 40);

  JSONTEST_ASSERT_EQUAL(json::vt_uint, val.type());

  checks = IsCheck();
  checks.isInt64_ = true;
  checks.isUInt64_ = true;
  checks.isIntegral_ = true;
  checks.isDouble_ = true;
  checks.isNumeric_ = true;
  JSONTEST_ASSERT_PRED(checkIs(val, checks));

  JSONTEST_ASSERT(!val.is_convertible_to(json::vt_null));
  JSONTEST_ASSERT(!val.is_convertible_to(json::vt_int));
  JSONTEST_ASSERT(!val.is_convertible_to(json::vt_uint));

  JSONTEST_ASSERT_EQUAL((int64_t(1) << 40), val.as_int64());
  JSONTEST_ASSERT_EQUAL((int64_t(1) << 40), val.as_largest_int());
  JSONTEST_ASSERT_EQUAL((int64_t(1) << 40), val.as_uint64());
  JSONTEST_ASSERT_EQUAL((int64_t(1) << 40), val.as_largest_uint());
  JSONTEST_ASSERT_EQUAL((int64_t(1) << 40), val.as_double());
  JSONTEST_ASSERT_EQUAL((int64_t(1) << 40), val.as_float());
  JSONTEST_ASSERT_EQUAL(true, val.as_bool());
  JSONTEST_ASSERT_STRING_EQUAL("1099511627776", val.as_string());

  // 2^40 (floating-point constructor arg)
  val = json::value((int64_t(1) << 40) / 1.0);

  JSONTEST_ASSERT_EQUAL(json::vt_real, val.type());

  checks = IsCheck();
  checks.isInt64_ = true;
  checks.isUInt64_ = true;
  checks.isIntegral_ = true;
  checks.isDouble_ = true;
  checks.isNumeric_ = true;
  JSONTEST_ASSERT_PRED(checkIs(val, checks));

  JSONTEST_ASSERT(!val.is_convertible_to(json::vt_null));
  JSONTEST_ASSERT(!val.is_convertible_to(json::vt_int));
  JSONTEST_ASSERT(!val.is_convertible_to(json::vt_uint));

  JSONTEST_ASSERT_EQUAL((int64_t(1) << 40), val.as_int64());
  JSONTEST_ASSERT_EQUAL((int64_t(1) << 40), val.as_largest_int());
  JSONTEST_ASSERT_EQUAL((int64_t(1) << 40), val.as_uint64());
  JSONTEST_ASSERT_EQUAL((int64_t(1) << 40), val.as_largest_uint());
  JSONTEST_ASSERT_EQUAL((int64_t(1) << 40), val.as_double());
  JSONTEST_ASSERT_EQUAL((int64_t(1) << 40), val.as_float());
  JSONTEST_ASSERT_EQUAL(true, val.as_bool());
  JSONTEST_ASSERT_STRING_EQUAL("1099511627776",
							   normalize_floating_point_str(val.as_string()));

  // -2^40
  val = json::value(-(int64_t(1) << 40));

  JSONTEST_ASSERT_EQUAL(json::vt_int, val.type());

  checks = IsCheck();
  checks.isInt64_ = true;
  checks.isIntegral_ = true;
  checks.isDouble_ = true;
  checks.isNumeric_ = true;
  JSONTEST_ASSERT_PRED(checkIs(val, checks));

  JSONTEST_ASSERT(!val.is_convertible_to(json::vt_null));
  JSONTEST_ASSERT(!val.is_convertible_to(json::vt_int));
  JSONTEST_ASSERT(!val.is_convertible_to(json::vt_uint));

  JSONTEST_ASSERT_EQUAL(-(int64_t(1) << 40), val.as_int64());
  JSONTEST_ASSERT_EQUAL(-(int64_t(1) << 40), val.as_largest_int());
  JSONTEST_ASSERT_EQUAL(-(int64_t(1) << 40), val.as_double());
  JSONTEST_ASSERT_EQUAL(-(int64_t(1) << 40), val.as_float());
  JSONTEST_ASSERT_EQUAL(true, val.as_bool());
  JSONTEST_ASSERT_STRING_EQUAL("-1099511627776", val.as_string());

  // int64 max
  val = json::value(int64_t(kint64max));

  JSONTEST_ASSERT_EQUAL(json::vt_int, val.type());

  checks = IsCheck();
  checks.isInt64_ = true;
  checks.isUInt64_ = true;
  checks.isIntegral_ = true;
  checks.isDouble_ = true;
  checks.isNumeric_ = true;
  JSONTEST_ASSERT_PRED(checkIs(val, checks));

  JSONTEST_ASSERT(!val.is_convertible_to(json::vt_null));
  JSONTEST_ASSERT(!val.is_convertible_to(json::vt_int));
  JSONTEST_ASSERT(!val.is_convertible_to(json::vt_uint));

  JSONTEST_ASSERT_EQUAL(kint64max, val.as_int64());
  JSONTEST_ASSERT_EQUAL(kint64max, val.as_largest_int());
  JSONTEST_ASSERT_EQUAL(kint64max, val.as_uint64());
  JSONTEST_ASSERT_EQUAL(kint64max, val.as_largest_uint());
  JSONTEST_ASSERT_EQUAL(double(kint64max), val.as_double());
  JSONTEST_ASSERT_EQUAL(float(kint64max), val.as_float());
  JSONTEST_ASSERT_EQUAL(true, val.as_bool());
  JSONTEST_ASSERT_STRING_EQUAL("9223372036854775807", val.as_string());

  // int64 max (floating point constructor). Note that kint64max is not exactly
  // representable as a double, and will be rounded up to be higher.
  val = json::value(double(kint64max));

  JSONTEST_ASSERT_EQUAL(json::vt_real, val.type());

  checks = IsCheck();
  checks.isUInt64_ = true;
  checks.isIntegral_ = true;
  checks.isDouble_ = true;
  checks.isNumeric_ = true;
  JSONTEST_ASSERT_PRED(checkIs(val, checks));

  JSONTEST_ASSERT(!val.is_convertible_to(json::vt_null));
  JSONTEST_ASSERT(!val.is_convertible_to(json::vt_int));
  JSONTEST_ASSERT(!val.is_convertible_to(json::vt_uint));

  JSONTEST_ASSERT_EQUAL(uint64_t(1) << 63, val.as_uint64());
  JSONTEST_ASSERT_EQUAL(uint64_t(1) << 63, val.as_largest_uint());
  JSONTEST_ASSERT_EQUAL(uint64_to_double(uint64_t(1) << 63), val.as_double());
  JSONTEST_ASSERT_EQUAL(float(uint64_to_double(uint64_t(1) << 63)),
						val.as_float());
  JSONTEST_ASSERT_EQUAL(true, val.as_bool());
  JSONTEST_ASSERT_STRING_EQUAL("9.2233720368547758e+18",
							   normalize_floating_point_str(val.as_string()));

  // int64 min
  val = json::value(int64_t(kint64min));

  JSONTEST_ASSERT_EQUAL(json::vt_int, val.type());

  checks = IsCheck();
  checks.isInt64_ = true;
  checks.isIntegral_ = true;
  checks.isDouble_ = true;
  checks.isNumeric_ = true;
  JSONTEST_ASSERT_PRED(checkIs(val, checks));

  JSONTEST_ASSERT(!val.is_convertible_to(json::vt_null));
  JSONTEST_ASSERT(!val.is_convertible_to(json::vt_int));
  JSONTEST_ASSERT(!val.is_convertible_to(json::vt_uint));

  JSONTEST_ASSERT_EQUAL(kint64min, val.as_int64());
  JSONTEST_ASSERT_EQUAL(kint64min, val.as_largest_int());
  JSONTEST_ASSERT_EQUAL(double(kint64min), val.as_double());
  JSONTEST_ASSERT_EQUAL(float(kint64min), val.as_float());
  JSONTEST_ASSERT_EQUAL(true, val.as_bool());
  JSONTEST_ASSERT_STRING_EQUAL("-9223372036854775808", val.as_string());

  // int64 min (floating point constructor). Note that kint64min *is* exactly
  // representable as a double.
  val = json::value(double(kint64min));

  JSONTEST_ASSERT_EQUAL(json::vt_real, val.type());

  checks = IsCheck();
  checks.isInt64_ = true;
  checks.isIntegral_ = true;
  checks.isDouble_ = true;
  checks.isNumeric_ = true;
  JSONTEST_ASSERT_PRED(checkIs(val, checks));

  JSONTEST_ASSERT(!val.is_convertible_to(json::vt_null));
  JSONTEST_ASSERT(!val.is_convertible_to(json::vt_int));
  JSONTEST_ASSERT(!val.is_convertible_to(json::vt_uint));

  JSONTEST_ASSERT_EQUAL(kint64min, val.as_int64());
  JSONTEST_ASSERT_EQUAL(kint64min, val.as_largest_int());
  JSONTEST_ASSERT_EQUAL(-9223372036854775808.0, val.as_double());
  JSONTEST_ASSERT_EQUAL(-9223372036854775808.0, val.as_float());
  JSONTEST_ASSERT_EQUAL(true, val.as_bool());
  JSONTEST_ASSERT_STRING_EQUAL("-9.2233720368547758e+18",
							   normalize_floating_point_str(val.as_string()));

  // 10^19
  const uint64_t ten_to_19 = static_cast<uint64_t>(1e19);
  val = json::value(uint64_t(ten_to_19));

  JSONTEST_ASSERT_EQUAL(json::vt_uint, val.type());

  checks = IsCheck();
  checks.isUInt64_ = true;
  checks.isIntegral_ = true;
  checks.isDouble_ = true;
  checks.isNumeric_ = true;
  JSONTEST_ASSERT_PRED(checkIs(val, checks));

  JSONTEST_ASSERT(!val.is_convertible_to(json::vt_null));
  JSONTEST_ASSERT(!val.is_convertible_to(json::vt_int));
  JSONTEST_ASSERT(!val.is_convertible_to(json::vt_uint));

  JSONTEST_ASSERT_EQUAL(ten_to_19, val.as_uint64());
  JSONTEST_ASSERT_EQUAL(ten_to_19, val.as_largest_uint());
  JSONTEST_ASSERT_EQUAL(uint64_to_double(ten_to_19), val.as_double());
  JSONTEST_ASSERT_EQUAL(float(uint64_to_double(ten_to_19)), val.as_float());
  JSONTEST_ASSERT_EQUAL(true, val.as_bool());
  JSONTEST_ASSERT_STRING_EQUAL("10000000000000000000", val.as_string());

  // 10^19 (double constructor). Note that 10^19 is not exactly representable
  // as a double.
  val = json::value(uint64_to_double(ten_to_19));

  JSONTEST_ASSERT_EQUAL(json::vt_real, val.type());

  checks = IsCheck();
  checks.isUInt64_ = true;
  checks.isIntegral_ = true;
  checks.isDouble_ = true;
  checks.isNumeric_ = true;
  JSONTEST_ASSERT_PRED(checkIs(val, checks));

  JSONTEST_ASSERT(!val.is_convertible_to(json::vt_null));
  JSONTEST_ASSERT(!val.is_convertible_to(json::vt_int));
  JSONTEST_ASSERT(!val.is_convertible_to(json::vt_uint));

  JSONTEST_ASSERT_EQUAL(1e19, val.as_double());
  JSONTEST_ASSERT_EQUAL(1e19, val.as_float());
  JSONTEST_ASSERT_EQUAL(true, val.as_bool());
  JSONTEST_ASSERT_STRING_EQUAL("1e+19",
							   normalize_floating_point_str(val.as_string()));

  // uint64 max
  val = json::value(uint64_t(kuint64max));

  JSONTEST_ASSERT_EQUAL(json::vt_uint, val.type());

  checks = IsCheck();
  checks.isUInt64_ = true;
  checks.isIntegral_ = true;
  checks.isDouble_ = true;
  checks.isNumeric_ = true;
  JSONTEST_ASSERT_PRED(checkIs(val, checks));

  JSONTEST_ASSERT(!val.is_convertible_to(json::vt_null));
  JSONTEST_ASSERT(!val.is_convertible_to(json::vt_int));
  JSONTEST_ASSERT(!val.is_convertible_to(json::vt_uint));

  JSONTEST_ASSERT_EQUAL(kuint64max, val.as_uint64());
  JSONTEST_ASSERT_EQUAL(kuint64max, val.as_largest_uint());
  JSONTEST_ASSERT_EQUAL(uint64_to_double(kuint64max), val.as_double());
  JSONTEST_ASSERT_EQUAL(float(uint64_to_double(kuint64max)), val.as_float());
  JSONTEST_ASSERT_EQUAL(true, val.as_bool());
  JSONTEST_ASSERT_STRING_EQUAL("18446744073709551615", val.as_string());

  // uint64 max (floating point constructor). Note that kuint64max is not
  // exactly representable as a double, and will be rounded up to be higher.
  val = json::value(uint64_to_double(kuint64max));

  JSONTEST_ASSERT_EQUAL(json::vt_real, val.type());

  checks = IsCheck();
  checks.isDouble_ = true;
  checks.isNumeric_ = true;
  JSONTEST_ASSERT_PRED(checkIs(val, checks));

  JSONTEST_ASSERT(!val.is_convertible_to(json::vt_null));
  JSONTEST_ASSERT(!val.is_convertible_to(json::vt_int));
  JSONTEST_ASSERT(!val.is_convertible_to(json::vt_uint));

  JSONTEST_ASSERT_EQUAL(18446744073709551616.0, val.as_double());
  JSONTEST_ASSERT_EQUAL(18446744073709551616.0, val.as_float());
  JSONTEST_ASSERT_EQUAL(true, val.as_bool());
  JSONTEST_ASSERT_STRING_EQUAL("1.8446744073709552e+19",
							   normalize_floating_point_str(val.as_string()));
#endif
}

JSONTEST_FIXTURE(ValueTest, nonIntegers) {
  IsCheck checks;
  json::value val;

  // Small positive number
  val = json::value(1.5);

  JSONTEST_ASSERT_EQUAL(json::vt_real, val.type());

  checks = IsCheck();
  checks.isDouble_ = true;
  checks.isNumeric_ = true;
  JSONTEST_ASSERT_PRED(checkIs(val, checks));

  JSONTEST_ASSERT(val.is_convertible_to(json::vt_int));
  JSONTEST_ASSERT(val.is_convertible_to(json::vt_uint));
  JSONTEST_ASSERT(val.is_convertible_to(json::vt_real));
  JSONTEST_ASSERT(val.is_convertible_to(json::vt_bool));
  JSONTEST_ASSERT(val.is_convertible_to(json::vt_string));
  JSONTEST_ASSERT(!val.is_convertible_to(json::vt_null));
  JSONTEST_ASSERT(!val.is_convertible_to(json::vt_array));
  JSONTEST_ASSERT(!val.is_convertible_to(json::vt_object));

  JSONTEST_ASSERT_EQUAL(1.5, val.as_double());
  JSONTEST_ASSERT_EQUAL(1.5, val.as_float());
  JSONTEST_ASSERT_EQUAL(1, val.as_int());
  JSONTEST_ASSERT_EQUAL(1, val.as_largest_int());
  JSONTEST_ASSERT_EQUAL(1, val.as_uint());
  JSONTEST_ASSERT_EQUAL(1, val.as_largest_uint());
  JSONTEST_ASSERT_EQUAL(true, val.as_bool());
  JSONTEST_ASSERT_EQUAL("1.5", val.as_string());

  // Small negative number
  val = json::value(-1.5);

  JSONTEST_ASSERT_EQUAL(json::vt_real, val.type());

  checks = IsCheck();
  checks.isDouble_ = true;
  checks.isNumeric_ = true;
  JSONTEST_ASSERT_PRED(checkIs(val, checks));

  JSONTEST_ASSERT(val.is_convertible_to(json::vt_int));
  JSONTEST_ASSERT(val.is_convertible_to(json::vt_real));
  JSONTEST_ASSERT(val.is_convertible_to(json::vt_bool));
  JSONTEST_ASSERT(val.is_convertible_to(json::vt_string));
  JSONTEST_ASSERT(!val.is_convertible_to(json::vt_null));
  JSONTEST_ASSERT(!val.is_convertible_to(json::vt_uint));
  JSONTEST_ASSERT(!val.is_convertible_to(json::vt_array));
  JSONTEST_ASSERT(!val.is_convertible_to(json::vt_object));

  JSONTEST_ASSERT_EQUAL(-1.5, val.as_double());
  JSONTEST_ASSERT_EQUAL(-1.5, val.as_float());
  JSONTEST_ASSERT_EQUAL(-1, val.as_int());
  JSONTEST_ASSERT_EQUAL(-1, val.as_largest_int());
  JSONTEST_ASSERT_EQUAL(true, val.as_bool());
  JSONTEST_ASSERT_EQUAL("-1.5", val.as_string());

  // A bit over int32 max
  val = json::value(kint32max + 0.5);

  JSONTEST_ASSERT_EQUAL(json::vt_real, val.type());

  checks = IsCheck();
  checks.isDouble_ = true;
  checks.isNumeric_ = true;
  JSONTEST_ASSERT_PRED(checkIs(val, checks));

  JSONTEST_ASSERT(val.is_convertible_to(json::vt_uint));
  JSONTEST_ASSERT(val.is_convertible_to(json::vt_real));
  JSONTEST_ASSERT(val.is_convertible_to(json::vt_bool));
  JSONTEST_ASSERT(val.is_convertible_to(json::vt_string));
  JSONTEST_ASSERT(!val.is_convertible_to(json::vt_null));
  JSONTEST_ASSERT(!val.is_convertible_to(json::vt_int));
  JSONTEST_ASSERT(!val.is_convertible_to(json::vt_array));
  JSONTEST_ASSERT(!val.is_convertible_to(json::vt_object));

  JSONTEST_ASSERT_EQUAL(2147483647.5, val.as_double());
  JSONTEST_ASSERT_EQUAL(float(2147483647.5), val.as_float());
  JSONTEST_ASSERT_EQUAL(2147483647U, val.as_uint());
#ifdef JSON_HAS_INT64
  JSONTEST_ASSERT_EQUAL(2147483647L, val.as_largest_int());
  JSONTEST_ASSERT_EQUAL(2147483647U, val.as_largest_uint());
#endif
  JSONTEST_ASSERT_EQUAL(true, val.as_bool());
  JSONTEST_ASSERT_EQUAL("2147483647.5",
						normalize_floating_point_str(val.as_string()));

  // A bit under int32 min
  val = json::value(kint32min - 0.5);

  JSONTEST_ASSERT_EQUAL(json::vt_real, val.type());

  checks = IsCheck();
  checks.isDouble_ = true;
  checks.isNumeric_ = true;
  JSONTEST_ASSERT_PRED(checkIs(val, checks));

  JSONTEST_ASSERT(val.is_convertible_to(json::vt_real));
  JSONTEST_ASSERT(val.is_convertible_to(json::vt_bool));
  JSONTEST_ASSERT(val.is_convertible_to(json::vt_string));
  JSONTEST_ASSERT(!val.is_convertible_to(json::vt_null));
  JSONTEST_ASSERT(!val.is_convertible_to(json::vt_int));
  JSONTEST_ASSERT(!val.is_convertible_to(json::vt_uint));
  JSONTEST_ASSERT(!val.is_convertible_to(json::vt_array));
  JSONTEST_ASSERT(!val.is_convertible_to(json::vt_object));

  JSONTEST_ASSERT_EQUAL(-2147483648.5, val.as_double());
  JSONTEST_ASSERT_EQUAL(float(-2147483648.5), val.as_float());
#ifdef JSON_HAS_INT64
  JSONTEST_ASSERT_EQUAL(-int64_t(1) << 31, val.as_largest_int());
#endif
  JSONTEST_ASSERT_EQUAL(true, val.as_bool());
  JSONTEST_ASSERT_EQUAL("-2147483648.5",
						normalize_floating_point_str(val.as_string()));

  // A bit over uint32 max
  val = json::value(kuint32max + 0.5);

  JSONTEST_ASSERT_EQUAL(json::vt_real, val.type());

  checks = IsCheck();
  checks.isDouble_ = true;
  checks.isNumeric_ = true;
  JSONTEST_ASSERT_PRED(checkIs(val, checks));

  JSONTEST_ASSERT(val.is_convertible_to(json::vt_real));
  JSONTEST_ASSERT(val.is_convertible_to(json::vt_bool));
  JSONTEST_ASSERT(val.is_convertible_to(json::vt_string));
  JSONTEST_ASSERT(!val.is_convertible_to(json::vt_null));
  JSONTEST_ASSERT(!val.is_convertible_to(json::vt_int));
  JSONTEST_ASSERT(!val.is_convertible_to(json::vt_uint));
  JSONTEST_ASSERT(!val.is_convertible_to(json::vt_array));
  JSONTEST_ASSERT(!val.is_convertible_to(json::vt_object));

  JSONTEST_ASSERT_EQUAL(4294967295.5, val.as_double());
  JSONTEST_ASSERT_EQUAL(float(4294967295.5), val.as_float());
#ifdef JSON_HAS_INT64
  JSONTEST_ASSERT_EQUAL((int64_t(1) << 32) - 1, val.as_largest_int());
  JSONTEST_ASSERT_EQUAL((uint64_t(1) << 32) - uint64_t(1),
						val.as_largest_uint());
#endif
  JSONTEST_ASSERT_EQUAL(true, val.as_bool());
  JSONTEST_ASSERT_EQUAL("4294967295.5",
						normalize_floating_point_str(val.as_string()));

  val = json::value(1.2345678901234);
  JSONTEST_ASSERT_STRING_EQUAL("1.2345678901234001",
							   normalize_floating_point_str(val.as_string()));

  // A 16-digit floating point number.
  val = json::value(2199023255552000.0f);
  JSONTEST_ASSERT_EQUAL(float(2199023255552000), val.as_float());
  JSONTEST_ASSERT_STRING_EQUAL("2199023255552000",
							   normalize_floating_point_str(val.as_string()));

  // A very large floating point number.
  val = json::value(3.402823466385289e38);
  JSONTEST_ASSERT_EQUAL(float(3.402823466385289e38), val.as_float());
  JSONTEST_ASSERT_STRING_EQUAL("3.402823466385289e+38",
							   normalize_floating_point_str(val.as_string()));

  // An even larger floating point number.
  val = json::value(1.2345678e300);
  JSONTEST_ASSERT_EQUAL(double(1.2345678e300), val.as_double());
  JSONTEST_ASSERT_STRING_EQUAL("1.2345678e+300",
							   normalize_floating_point_str(val.as_string()));
}

void ValueTest::checkConstMemberCount(const json::value& value,
									  unsigned int expectedCount) {
  unsigned int count = 0;
  json::value::const_iterator itEnd = value.end();
  for (json::value::const_iterator it = value.begin(); it != itEnd; ++it) {
	++count;
  }
  JSONTEST_ASSERT_EQUAL(expectedCount, count) << "json::value::const_iterator";
}

void ValueTest::checkMemberCount(json::value& value,
								 unsigned int expectedCount) {
  JSONTEST_ASSERT_EQUAL(expectedCount, value.size());

  unsigned int count = 0;
  json::value::iterator itEnd = value.end();
  for (json::value::iterator it = value.begin(); it != itEnd; ++it) {
	++count;
  }
  JSONTEST_ASSERT_EQUAL(expectedCount, count) << "json::value::iterator";

  JSONTEST_ASSERT_PRED(checkConstMemberCount(value, expectedCount));
}

ValueTest::IsCheck::IsCheck()
	: isObject_(false), isArray_(false), isBool_(false), isString_(false),
	  isNull_(false), isInt_(false), isInt64_(false), isUInt_(false),
	  isUInt64_(false), isIntegral_(false), isDouble_(false),
	  isNumeric_(false) {}

void ValueTest::checkIs(const json::value& value, const IsCheck& check) {
  JSONTEST_ASSERT_EQUAL(check.isObject_, value.is_object());
  JSONTEST_ASSERT_EQUAL(check.isArray_, value.is_array());
  JSONTEST_ASSERT_EQUAL(check.isBool_, value.is_bool());
  JSONTEST_ASSERT_EQUAL(check.isDouble_, value.isDouble());
  JSONTEST_ASSERT_EQUAL(check.isInt_, value.is_int());
  JSONTEST_ASSERT_EQUAL(check.isUInt_, value.is_uint());
  JSONTEST_ASSERT_EQUAL(check.isIntegral_, value.isIntegral());
  JSONTEST_ASSERT_EQUAL(check.isNumeric_, value.isNumeric());
  JSONTEST_ASSERT_EQUAL(check.isString_, value.isString());
  JSONTEST_ASSERT_EQUAL(check.isNull_, value.is_null());

#ifdef JSON_HAS_INT64
  JSONTEST_ASSERT_EQUAL(check.isInt64_, value.is_int64());
  JSONTEST_ASSERT_EQUAL(check.isUInt64_, value.isUInt64());
#else
  JSONTEST_ASSERT_EQUAL(false, value.is_int64());
  JSONTEST_ASSERT_EQUAL(false, value.isUInt64());
#endif
}

JSONTEST_FIXTURE(ValueTest, compareNull) {
  JSONTEST_ASSERT_PRED(checkIsEqual(json::value(), json::value()));
}

JSONTEST_FIXTURE(ValueTest, compareInt) {
  JSONTEST_ASSERT_PRED(checkIsLess(0, 10));
  JSONTEST_ASSERT_PRED(checkIsEqual(10, 10));
  JSONTEST_ASSERT_PRED(checkIsEqual(-10, -10));
  JSONTEST_ASSERT_PRED(checkIsLess(-10, 0));
}

JSONTEST_FIXTURE(ValueTest, compareUInt) {
  JSONTEST_ASSERT_PRED(checkIsLess(0u, 10u));
  JSONTEST_ASSERT_PRED(checkIsLess(0u, json::value::max_uint));
  JSONTEST_ASSERT_PRED(checkIsEqual(10u, 10u));
}

JSONTEST_FIXTURE(ValueTest, compareDouble) {
  JSONTEST_ASSERT_PRED(checkIsLess(0.0, 10.0));
  JSONTEST_ASSERT_PRED(checkIsEqual(10.0, 10.0));
  JSONTEST_ASSERT_PRED(checkIsEqual(-10.0, -10.0));
  JSONTEST_ASSERT_PRED(checkIsLess(-10.0, 0.0));
}

JSONTEST_FIXTURE(ValueTest, compareString) {
  JSONTEST_ASSERT_PRED(checkIsLess("", " "));
  JSONTEST_ASSERT_PRED(checkIsLess("", "a"));
  JSONTEST_ASSERT_PRED(checkIsLess("abcd", "zyui"));
  JSONTEST_ASSERT_PRED(checkIsLess("abc", "abcd"));
  JSONTEST_ASSERT_PRED(checkIsEqual("abcd", "abcd"));
  JSONTEST_ASSERT_PRED(checkIsEqual(" ", " "));
  JSONTEST_ASSERT_PRED(checkIsLess("ABCD", "abcd"));
  JSONTEST_ASSERT_PRED(checkIsEqual("ABCD", "ABCD"));
}

JSONTEST_FIXTURE(ValueTest, compareBoolean) {
  JSONTEST_ASSERT_PRED(checkIsLess(false, true));
  JSONTEST_ASSERT_PRED(checkIsEqual(false, false));
  JSONTEST_ASSERT_PRED(checkIsEqual(true, true));
}

JSONTEST_FIXTURE(ValueTest, compareArray) {
  // array compare size then content
  json::value emptyArray(json::vt_array);
  json::value l1aArray;
  l1aArray.append(0);
  json::value l1bArray;
  l1bArray.append(10);
  json::value l2aArray;
  l2aArray.append(0);
  l2aArray.append(0);
  json::value l2bArray;
  l2bArray.append(0);
  l2bArray.append(10);
  JSONTEST_ASSERT_PRED(checkIsLess(emptyArray, l1aArray));
  JSONTEST_ASSERT_PRED(checkIsLess(emptyArray, l2aArray));
  JSONTEST_ASSERT_PRED(checkIsLess(l1aArray, l2aArray));
  JSONTEST_ASSERT_PRED(checkIsLess(l2aArray, l2bArray));
  JSONTEST_ASSERT_PRED(checkIsEqual(emptyArray, json::value(emptyArray)));
  JSONTEST_ASSERT_PRED(checkIsEqual(l1aArray, json::value(l1aArray)));
  JSONTEST_ASSERT_PRED(checkIsEqual(l2bArray, json::value(l2bArray)));
}

JSONTEST_FIXTURE(ValueTest, compareObject) {
  // object compare size then content
  json::value emptyObject(json::vt_object);
  json::value l1aObject;
  l1aObject["key1"] = 0;
  json::value l1bObject;
  l1aObject["key1"] = 10;
  json::value l2aObject;
  l2aObject["key1"] = 0;
  l2aObject["key2"] = 0;
  JSONTEST_ASSERT_PRED(checkIsLess(emptyObject, l1aObject));
  JSONTEST_ASSERT_PRED(checkIsLess(emptyObject, l2aObject));
  JSONTEST_ASSERT_PRED(checkIsLess(l1aObject, l2aObject));
  JSONTEST_ASSERT_PRED(checkIsEqual(emptyObject, json::value(emptyObject)));
  JSONTEST_ASSERT_PRED(checkIsEqual(l1aObject, json::value(l1aObject)));
  JSONTEST_ASSERT_PRED(checkIsEqual(l2aObject, json::value(l2aObject)));
}

JSONTEST_FIXTURE(ValueTest, compareType) {
  // object of different type are ordered according to their type
  JSONTEST_ASSERT_PRED(checkIsLess(json::value(), json::value(1)));
  JSONTEST_ASSERT_PRED(checkIsLess(json::value(1), json::value(1u)));
  JSONTEST_ASSERT_PRED(checkIsLess(json::value(1u), json::value(1.0)));
  JSONTEST_ASSERT_PRED(checkIsLess(json::value(1.0), json::value("a")));
  JSONTEST_ASSERT_PRED(checkIsLess(json::value("a"), json::value(true)));
  JSONTEST_ASSERT_PRED(
	  checkIsLess(json::value(true), json::value(json::vt_array)));
  JSONTEST_ASSERT_PRED(checkIsLess(json::value(json::vt_array),
								   json::value(json::vt_object)));
}

void ValueTest::checkIsLess(const json::value& x, const json::value& y) {
  JSONTEST_ASSERT(x < y);
  JSONTEST_ASSERT(y > x);
  JSONTEST_ASSERT(x <= y);
  JSONTEST_ASSERT(y >= x);
  JSONTEST_ASSERT(!(x == y));
  JSONTEST_ASSERT(!(y == x));
  JSONTEST_ASSERT(!(x >= y));
  JSONTEST_ASSERT(!(y <= x));
  JSONTEST_ASSERT(!(x > y));
  JSONTEST_ASSERT(!(y < x));
  JSONTEST_ASSERT(x.compare(y) < 0);
  JSONTEST_ASSERT(y.compare(x) >= 0);
}

void ValueTest::checkIsEqual(const json::value& x, const json::value& y) {
  JSONTEST_ASSERT(x == y);
  JSONTEST_ASSERT(y == x);
  JSONTEST_ASSERT(x <= y);
  JSONTEST_ASSERT(y <= x);
  JSONTEST_ASSERT(x >= y);
  JSONTEST_ASSERT(y >= x);
  JSONTEST_ASSERT(!(x < y));
  JSONTEST_ASSERT(!(y < x));
  JSONTEST_ASSERT(!(x > y));
  JSONTEST_ASSERT(!(y > x));
  JSONTEST_ASSERT(x.compare(y) == 0);
  JSONTEST_ASSERT(y.compare(x) == 0);
}

JSONTEST_FIXTURE(ValueTest, typeChecksThrowExceptions) {
#if JSON_USE_EXCEPTION

  json::value intVal(1);
  json::value strVal("Test");
  json::value objVal(json::vt_object);
  json::value arrVal(json::vt_array);

  JSONTEST_ASSERT_THROWS(intVal["test"]);
  JSONTEST_ASSERT_THROWS(strVal["test"]);
  JSONTEST_ASSERT_THROWS(arrVal["test"]);

  JSONTEST_ASSERT_THROWS(intVal.remove_member("test"));
  JSONTEST_ASSERT_THROWS(strVal.remove_member("test"));
  JSONTEST_ASSERT_THROWS(arrVal.remove_member("test"));

  JSONTEST_ASSERT_THROWS(intVal.get_member_names());
  JSONTEST_ASSERT_THROWS(strVal.get_member_names());
  JSONTEST_ASSERT_THROWS(arrVal.get_member_names());

  JSONTEST_ASSERT_THROWS(intVal[0]);
  JSONTEST_ASSERT_THROWS(objVal[0]);
  JSONTEST_ASSERT_THROWS(strVal[0]);

  JSONTEST_ASSERT_THROWS(intVal.clear());

  JSONTEST_ASSERT_THROWS(intVal.resize(1));
  JSONTEST_ASSERT_THROWS(strVal.resize(1));
  JSONTEST_ASSERT_THROWS(objVal.resize(1));

  JSONTEST_ASSERT_THROWS(intVal.as_cstring());

  JSONTEST_ASSERT_THROWS(objVal.as_string());
  JSONTEST_ASSERT_THROWS(arrVal.as_string());

  JSONTEST_ASSERT_THROWS(strVal.as_int());
  JSONTEST_ASSERT_THROWS(objVal.as_int());
  JSONTEST_ASSERT_THROWS(arrVal.as_int());

  JSONTEST_ASSERT_THROWS(strVal.as_uint());
  JSONTEST_ASSERT_THROWS(objVal.as_uint());
  JSONTEST_ASSERT_THROWS(arrVal.as_uint());

  JSONTEST_ASSERT_THROWS(strVal.as_int64());
  JSONTEST_ASSERT_THROWS(objVal.as_int64());
  JSONTEST_ASSERT_THROWS(arrVal.as_int64());

  JSONTEST_ASSERT_THROWS(strVal.as_uint64());
  JSONTEST_ASSERT_THROWS(objVal.as_uint64());
  JSONTEST_ASSERT_THROWS(arrVal.as_uint64());

  JSONTEST_ASSERT_THROWS(strVal.as_double());
  JSONTEST_ASSERT_THROWS(objVal.as_double());
  JSONTEST_ASSERT_THROWS(arrVal.as_double());

  JSONTEST_ASSERT_THROWS(strVal.as_float());
  JSONTEST_ASSERT_THROWS(objVal.as_float());
  JSONTEST_ASSERT_THROWS(arrVal.as_float());

  JSONTEST_ASSERT_THROWS(strVal.as_bool());
  JSONTEST_ASSERT_THROWS(objVal.as_bool());
  JSONTEST_ASSERT_THROWS(arrVal.as_bool());

#endif
}

JSONTEST_FIXTURE(ValueTest, offsetAccessors) {
  json::value x;
  JSONTEST_ASSERT(x.get_offset_start() == 0);
  JSONTEST_ASSERT(x.get_offset_limit() == 0);
  x.set_offset_start(10);
  x.set_offset_limit(20);
  JSONTEST_ASSERT(x.get_offset_start() == 10);
  JSONTEST_ASSERT(x.get_offset_limit() == 20);
  json::value y(x);
  JSONTEST_ASSERT(y.get_offset_start() == 10);
  JSONTEST_ASSERT(y.get_offset_limit() == 20);
  json::value z;
  z.swap(y);
  JSONTEST_ASSERT(z.get_offset_start() == 10);
  JSONTEST_ASSERT(z.get_offset_limit() == 20);
  JSONTEST_ASSERT(y.get_offset_start() == 0);
  JSONTEST_ASSERT(y.get_offset_limit() == 0);
}

JSONTEST_FIXTURE(ValueTest, static_string) {
  char mutant[] = "hello";
  json::static_string ss(mutant);
  std::string regular(mutant);
  mutant[1] = 'a';
  JSONTEST_ASSERT_STRING_EQUAL("hallo", ss.c_str());
  JSONTEST_ASSERT_STRING_EQUAL("hello", regular.c_str());
  {
	json::value root;
	root["top"] = ss;
	JSONTEST_ASSERT_STRING_EQUAL("hallo", root["top"].as_string());
	mutant[1] = 'u';
	JSONTEST_ASSERT_STRING_EQUAL("hullo", root["top"].as_string());
  }
  {
	json::value root;
	root["top"] = regular;
	JSONTEST_ASSERT_STRING_EQUAL("hello", root["top"].as_string());
	mutant[1] = 'u';
	JSONTEST_ASSERT_STRING_EQUAL("hello", root["top"].as_string());
  }
}

JSONTEST_FIXTURE(ValueTest, CommentBefore) {
  json::value val; // fill val
  val.set_comment("// this comment should appear before", json::comment_before);
  json::stream_writer_builder wbuilder;
  wbuilder.settings_["commentStyle"] = "All";
  {
	char const expected[] = "// this comment should appear before\nnull";
	std::string result = json::write_string(wbuilder, val);
	JSONTEST_ASSERT_STRING_EQUAL(expected, result);
	std::string res2 = val.toStyledString();
	std::string exp2 = "\n";
	exp2 += expected;
	exp2 += "\n";
	JSONTEST_ASSERT_STRING_EQUAL(exp2, res2);
  }
  json::value other = "hello";
  val.swap_payload(other);
  {
	char const expected[] = "// this comment should appear before\n\"hello\"";
	std::string result = json::write_string(wbuilder, val);
	JSONTEST_ASSERT_STRING_EQUAL(expected, result);
	std::string res2 = val.toStyledString();
	std::string exp2 = "\n";
	exp2 += expected;
	exp2 += "\n";
	JSONTEST_ASSERT_STRING_EQUAL(exp2, res2);
	JSONTEST_ASSERT_STRING_EQUAL("null\n", other.toStyledString());
  }
  val = "hello";
  // val.set_comment("// this comment should appear before", json::comment_placement::comment_before);
  // Assignment over-writes comments.
  {
	char const expected[] = "\"hello\"";
	std::string result = json::write_string(wbuilder, val);
	JSONTEST_ASSERT_STRING_EQUAL(expected, result);
	std::string res2 = val.toStyledString();
	std::string exp2 = "";
	exp2 += expected;
	exp2 += "\n";
	JSONTEST_ASSERT_STRING_EQUAL(exp2, res2);
  }
}

JSONTEST_FIXTURE(ValueTest, zeroes) {
  char const cstr[] = "h\0i";
  std::string binary(cstr, sizeof(cstr));  // include trailing 0
  JSONTEST_ASSERT_EQUAL(4U, binary.length());
  json::stream_writer_builder b;
  {
	json::value root;
	root = binary;
	JSONTEST_ASSERT_STRING_EQUAL(binary, root.as_string());
  }
  {
	char const top[] = "top";
	json::value root;
	root[top] = binary;
	JSONTEST_ASSERT_STRING_EQUAL(binary, root[top].as_string());
	json::value removed;
	bool did;
	did = root.remove_member(top, top + sizeof(top) - 1U,
		&removed);
	JSONTEST_ASSERT(did);
	JSONTEST_ASSERT_STRING_EQUAL(binary, removed.as_string());
	did = root.remove_member(top, top + sizeof(top) - 1U,
		&removed);
	JSONTEST_ASSERT(!did);
	JSONTEST_ASSERT_STRING_EQUAL(binary, removed.as_string()); // still
  }
}

JSONTEST_FIXTURE(ValueTest, zeroesInKeys) {
  char const cstr[] = "h\0i";
  std::string binary(cstr, sizeof(cstr));  // include trailing 0
  JSONTEST_ASSERT_EQUAL(4U, binary.length());
  {
	json::value root;
	root[binary] = "there";
	JSONTEST_ASSERT_STRING_EQUAL("there", root[binary].as_string());
	JSONTEST_ASSERT(!root.is_member("h"));
	JSONTEST_ASSERT(root.is_member(binary));
	JSONTEST_ASSERT_STRING_EQUAL("there", root.get(binary, json::value::null_ref).as_string());
	json::value removed;
	bool did;
	did = root.remove_member(binary.data(), binary.data() + binary.length(),
		&removed);
	JSONTEST_ASSERT(did);
	JSONTEST_ASSERT_STRING_EQUAL("there", removed.as_string());
	did = root.remove_member(binary.data(), binary.data() + binary.length(),
		&removed);
	JSONTEST_ASSERT(!did);
	JSONTEST_ASSERT_STRING_EQUAL("there", removed.as_string()); // still
	JSONTEST_ASSERT(!root.is_member(binary));
	JSONTEST_ASSERT_STRING_EQUAL("", root.get(binary, json::value::null_ref).as_string());
  }
}

struct WriterTest : JsonTest::TestCase {};

JSONTEST_FIXTURE(WriterTest, dropNullPlaceholders) {
  json::fast_writer writer;
  json::value vt_null;
  JSONTEST_ASSERT(writer.write(vt_null) == "null\n");

  writer.dropNullPlaceholders();
  JSONTEST_ASSERT(writer.write(vt_null) == "\n");
}

struct StreamWriterTest : JsonTest::TestCase {};

JSONTEST_FIXTURE(StreamWriterTest, dropNullPlaceholders) {
  json::stream_writer_builder b;
  json::value vt_null;
  b.settings_["dropNullPlaceholders"] = false;
  JSONTEST_ASSERT(json::write_string(b, vt_null) == "null");
  b.settings_["dropNullPlaceholders"] = true;
  JSONTEST_ASSERT(json::write_string(b, vt_null) == "");
}

JSONTEST_FIXTURE(StreamWriterTest, writeZeroes) {
  std::string binary("hi", 3);  // include trailing 0
  JSONTEST_ASSERT_EQUAL(3, binary.length());
  std::string expected("\"hi\\u0000\"");  // unicoded zero
  json::stream_writer_builder b;
  {
	json::value root;
	root = binary;
	JSONTEST_ASSERT_STRING_EQUAL(binary, root.as_string());
	std::string out = json::write_string(b, root);
	JSONTEST_ASSERT_EQUAL(expected.size(), out.size());
	JSONTEST_ASSERT_STRING_EQUAL(expected, out);
  }
  {
	json::value root;
	root["top"] = binary;
	JSONTEST_ASSERT_STRING_EQUAL(binary, root["top"].as_string());
	std::string out = json::write_string(b, root["top"]);
	JSONTEST_ASSERT_STRING_EQUAL(expected, out);
  }
}

struct ReaderTest : JsonTest::TestCase {};

JSONTEST_FIXTURE(ReaderTest, parseWithNoErrors) {
  json::reader reader;
  json::value root;
  bool ok = reader.parse("{ \"property\" : \"value\" }", root);
  JSONTEST_ASSERT(ok);
  JSONTEST_ASSERT(reader.get_formatted_messages().size() == 0);
  JSONTEST_ASSERT(reader.get_structured_errors().size() == 0);
}

JSONTEST_FIXTURE(ReaderTest, parseWithNoErrorsTestingOffsets) {
  json::reader reader;
  json::value root;
  bool ok = reader.parse("{ \"property\" : [\"value\", \"value2\"], \"obj\" : "
						 "{ \"nested\" : 123, \"bool\" : true}, \"null\" : "
						 "null, \"false\" : false }",
						 root);
  JSONTEST_ASSERT(ok);
  JSONTEST_ASSERT(reader.get_formatted_messages().size() == 0);
  JSONTEST_ASSERT(reader.get_structured_errors().size() == 0);
  JSONTEST_ASSERT(root["property"].get_offset_start() == 15);
  JSONTEST_ASSERT(root["property"].get_offset_limit() == 34);
  JSONTEST_ASSERT(root["property"][0].get_offset_start() == 16);
  JSONTEST_ASSERT(root["property"][0].get_offset_limit() == 23);
  JSONTEST_ASSERT(root["property"][1].get_offset_start() == 25);
  JSONTEST_ASSERT(root["property"][1].get_offset_limit() == 33);
  JSONTEST_ASSERT(root["obj"].get_offset_start() == 44);
  JSONTEST_ASSERT(root["obj"].get_offset_limit() == 76);
  JSONTEST_ASSERT(root["obj"]["nested"].get_offset_start() == 57);
  JSONTEST_ASSERT(root["obj"]["nested"].get_offset_limit() == 60);
  JSONTEST_ASSERT(root["obj"]["bool"].get_offset_start() == 71);
  JSONTEST_ASSERT(root["obj"]["bool"].get_offset_limit() == 75);
  JSONTEST_ASSERT(root["null"].get_offset_start() == 87);
  JSONTEST_ASSERT(root["null"].get_offset_limit() == 91);
  JSONTEST_ASSERT(root["false"].get_offset_start() == 103);
  JSONTEST_ASSERT(root["false"].get_offset_limit() == 108);
  JSONTEST_ASSERT(root.get_offset_start() == 0);
  JSONTEST_ASSERT(root.get_offset_limit() == 110);
}

JSONTEST_FIXTURE(ReaderTest, parseWithOneError) {
  json::reader reader;
  json::value root;
  bool ok = reader.parse("{ \"property\" :: \"value\" }", root);
  JSONTEST_ASSERT(!ok);
  JSONTEST_ASSERT(reader.get_formatted_messages() ==
				  "* Line 1, Column 15\n  Syntax error: value, object or array "
				  "expected.\n");
  std::vector<json::reader::structured_error> errors =
	  reader.get_structured_errors();
  JSONTEST_ASSERT(errors.size() == 1);
  JSONTEST_ASSERT(errors.at(0).offset_start == 14);
  JSONTEST_ASSERT(errors.at(0).offset_limit == 15);
  JSONTEST_ASSERT(errors.at(0).message ==
				  "Syntax error: value, object or array expected.");
}

JSONTEST_FIXTURE(ReaderTest, parseChineseWithOneError) {
  json::reader reader;
  json::value root;
  bool ok = reader.parse("{ \"pr佐藤erty\" :: \"value\" }", root);
  JSONTEST_ASSERT(!ok);
  JSONTEST_ASSERT(reader.get_formatted_messages() ==
				  "* Line 1, Column 19\n  Syntax error: value, object or array "
				  "expected.\n");
  std::vector<json::reader::structured_error> errors =
	  reader.get_structured_errors();
  JSONTEST_ASSERT(errors.size() == 1);
  JSONTEST_ASSERT(errors.at(0).offset_start == 18);
  JSONTEST_ASSERT(errors.at(0).offset_limit == 19);
  JSONTEST_ASSERT(errors.at(0).message ==
				  "Syntax error: value, object or array expected.");
}

JSONTEST_FIXTURE(ReaderTest, parseWithDetailError) {
  json::reader reader;
  json::value root;
  bool ok = reader.parse("{ \"property\" : \"v\\alue\" }", root);
  JSONTEST_ASSERT(!ok);
  JSONTEST_ASSERT(reader.get_formatted_messages() ==
				  "* Line 1, Column 16\n  Bad escape sequence in string\nSee "
				  "Line 1, Column 20 for detail.\n");
  std::vector<json::reader::structured_error> errors =
	  reader.get_structured_errors();
  JSONTEST_ASSERT(errors.size() == 1);
  JSONTEST_ASSERT(errors.at(0).offset_start == 15);
  JSONTEST_ASSERT(errors.at(0).offset_limit == 23);
  JSONTEST_ASSERT(errors.at(0).message == "Bad escape sequence in string");
}

struct CharReaderTest : JsonTest::TestCase {};

JSONTEST_FIXTURE(CharReaderTest, parseWithNoErrors) {
  json::char_reader_builder b;
  json::char_reader* reader(b.newCharReader());
  std::string errs;
  json::value root;
  char const doc[] = "{ \"property\" : \"value\" }";
  bool ok = reader->parse(
	  doc, doc + std::strlen(doc),
	  &root, &errs);
  JSONTEST_ASSERT(ok);
  JSONTEST_ASSERT(errs.size() == 0);
  delete reader;
}

JSONTEST_FIXTURE(CharReaderTest, parseWithNoErrorsTestingOffsets) {
  json::char_reader_builder b;
  json::char_reader* reader(b.newCharReader());
  std::string errs;
  json::value root;
  char const doc[] =
						 "{ \"property\" : [\"value\", \"value2\"], \"obj\" : "
						 "{ \"nested\" : 123, \"bool\" : true}, \"null\" : "
						 "null, \"false\" : false }";
  bool ok = reader->parse(
	  doc, doc + std::strlen(doc),
	  &root, &errs);
  JSONTEST_ASSERT(ok);
  JSONTEST_ASSERT(errs.size() == 0);
  delete reader;
}

JSONTEST_FIXTURE(CharReaderTest, parseWithOneError) {
  json::char_reader_builder b;
  json::char_reader* reader(b.newCharReader());
  std::string errs;
  json::value root;
  char const doc[] =
	  "{ \"property\" :: \"value\" }";
  bool ok = reader->parse(
	  doc, doc + std::strlen(doc),
	  &root, &errs);
  JSONTEST_ASSERT(!ok);
  JSONTEST_ASSERT(errs ==
				  "* Line 1, Column 15\n  Syntax error: value, object or array "
				  "expected.\n");
  delete reader;
}

JSONTEST_FIXTURE(CharReaderTest, parseChineseWithOneError) {
  json::char_reader_builder b;
  json::char_reader* reader(b.newCharReader());
  std::string errs;
  json::value root;
  char const doc[] =
	  "{ \"pr佐藤erty\" :: \"value\" }";
  bool ok = reader->parse(
	  doc, doc + std::strlen(doc),
	  &root, &errs);
  JSONTEST_ASSERT(!ok);
  JSONTEST_ASSERT(errs ==
				  "* Line 1, Column 19\n  Syntax error: value, object or array "
				  "expected.\n");
  delete reader;
}

JSONTEST_FIXTURE(CharReaderTest, parseWithDetailError) {
  json::char_reader_builder b;
  json::char_reader* reader(b.newCharReader());
  std::string errs;
  json::value root;
  char const doc[] =
	  "{ \"property\" : \"v\\alue\" }";
  bool ok = reader->parse(
	  doc, doc + std::strlen(doc),
	  &root, &errs);
  JSONTEST_ASSERT(!ok);
  JSONTEST_ASSERT(errs ==
				  "* Line 1, Column 16\n  Bad escape sequence in string\nSee "
				  "Line 1, Column 20 for detail.\n");
  delete reader;
}

JSONTEST_FIXTURE(CharReaderTest, parseWithStackLimit) {
  json::char_reader_builder b;
  json::value root;
  char const doc[] =
	  "{ \"property\" : \"value\" }";
  {
  b.settings_["stackLimit"] = 2;
  json::char_reader* reader(b.newCharReader());
  std::string errs;
  bool ok = reader->parse(
	  doc, doc + std::strlen(doc),
	  &root, &errs);
  JSONTEST_ASSERT(ok);
  JSONTEST_ASSERT(errs == "");
  JSONTEST_ASSERT_EQUAL("value", root["property"]);
  delete reader;
  }
  {
  b.settings_["stackLimit"] = 1;
  json::char_reader* reader(b.newCharReader());
  std::string errs;
  JSONTEST_ASSERT_THROWS(reader->parse(
	  doc, doc + std::strlen(doc),
	  &root, &errs));
  delete reader;
  }
}

struct CharReaderStrictModeTest : JsonTest::TestCase {};

JSONTEST_FIXTURE(CharReaderStrictModeTest, dupKeys) {
  json::char_reader_builder b;
  json::value root;
  char const doc[] =
	  "{ \"property\" : \"value\", \"key\" : \"val1\", \"key\" : \"val2\" }";
  {
	b.strict_mode(&b.settings_);
	json::char_reader* reader(b.newCharReader());
	std::string errs;
	bool ok = reader->parse(
		doc, doc + std::strlen(doc),
		&root, &errs);
	JSONTEST_ASSERT(!ok);
	JSONTEST_ASSERT_STRING_EQUAL(
		"* Line 1, Column 41\n"
		"  Duplicate key: 'key'\n",
		errs);
	JSONTEST_ASSERT_EQUAL("val1", root["key"]); // so far
	delete reader;
  }
}
struct CharReaderFailIfExtraTest : JsonTest::TestCase {};

JSONTEST_FIXTURE(CharReaderFailIfExtraTest, issue164) {
  // This is interpretted as a string value followed by a colon.
  json::char_reader_builder b;
  json::value root;
  char const doc[] =
	  " \"property\" : \"value\" }";
  {
  b.settings_["failIfExtra"] = false;
  json::char_reader* reader(b.newCharReader());
  std::string errs;
  bool ok = reader->parse(
	  doc, doc + std::strlen(doc),
	  &root, &errs);
  JSONTEST_ASSERT(ok);
  JSONTEST_ASSERT(errs == "");
  JSONTEST_ASSERT_EQUAL("property", root);
  delete reader;
  }
  {
  b.settings_["failIfExtra"] = true;
  json::char_reader* reader(b.newCharReader());
  std::string errs;
  bool ok = reader->parse(
	  doc, doc + std::strlen(doc),
	  &root, &errs);
  JSONTEST_ASSERT(!ok);
  JSONTEST_ASSERT_STRING_EQUAL(errs,
	  "* Line 1, Column 13\n"
	  "  Extra non-whitespace after JSON value.\n");
  JSONTEST_ASSERT_EQUAL("property", root);
  delete reader;
  }
  {
  b.settings_["failIfExtra"] = false;
  b.strict_mode(&b.settings_);
  json::char_reader* reader(b.newCharReader());
  std::string errs;
  bool ok = reader->parse(
	  doc, doc + std::strlen(doc),
	  &root, &errs);
  JSONTEST_ASSERT(!ok);
  JSONTEST_ASSERT_STRING_EQUAL(errs,
	  "* Line 1, Column 13\n"
	  "  Extra non-whitespace after JSON value.\n");
  JSONTEST_ASSERT_EQUAL("property", root);
  delete reader;
  }
}
JSONTEST_FIXTURE(CharReaderFailIfExtraTest, issue107) {
  // This is interpretted as an int value followed by a colon.
  json::char_reader_builder b;
  json::value root;
  char const doc[] =
	  "1:2:3";
  b.settings_["failIfExtra"] = true;
  json::char_reader* reader(b.newCharReader());
  std::string errs;
  bool ok = reader->parse(
	  doc, doc + std::strlen(doc),
	  &root, &errs);
  JSONTEST_ASSERT(!ok);
  JSONTEST_ASSERT_STRING_EQUAL(
	  "* Line 1, Column 2\n"
	  "  Extra non-whitespace after JSON value.\n",
	  errs);
  JSONTEST_ASSERT_EQUAL(1, root.as_int());
  delete reader;
}
JSONTEST_FIXTURE(CharReaderFailIfExtraTest, commentAfterObject) {
  json::char_reader_builder b;
  json::value root;
  {
  char const doc[] =
	  "{ \"property\" : \"value\" } //trailing\n//comment\n";
  b.settings_["failIfExtra"] = true;
  json::char_reader* reader(b.newCharReader());
  std::string errs;
  bool ok = reader->parse(
	  doc, doc + std::strlen(doc),
	  &root, &errs);
  JSONTEST_ASSERT(ok);
  JSONTEST_ASSERT_STRING_EQUAL("", errs);
  JSONTEST_ASSERT_EQUAL("value", root["property"]);
  delete reader;
  }
}
JSONTEST_FIXTURE(CharReaderFailIfExtraTest, commentAfterArray) {
  json::char_reader_builder b;
  json::value root;
  char const doc[] =
	  "[ \"property\" , \"value\" ] //trailing\n//comment\n";
  b.settings_["failIfExtra"] = true;
  json::char_reader* reader(b.newCharReader());
  std::string errs;
  bool ok = reader->parse(
	  doc, doc + std::strlen(doc),
	  &root, &errs);
  JSONTEST_ASSERT(ok);
  JSONTEST_ASSERT_STRING_EQUAL("", errs);
  JSONTEST_ASSERT_EQUAL("value", root[1u]);
  delete reader;
}
JSONTEST_FIXTURE(CharReaderFailIfExtraTest, commentAfterBool) {
  json::char_reader_builder b;
  json::value root;
  char const doc[] =
	  " true /*trailing\ncomment*/";
  b.settings_["failIfExtra"] = true;
  json::char_reader* reader(b.newCharReader());
  std::string errs;
  bool ok = reader->parse(
	  doc, doc + std::strlen(doc),
	  &root, &errs);
  JSONTEST_ASSERT(ok);
  JSONTEST_ASSERT_STRING_EQUAL("", errs);
  JSONTEST_ASSERT_EQUAL(true, root.as_bool());
  delete reader;
}
struct CharReaderAllowDropNullTest : JsonTest::TestCase {};

JSONTEST_FIXTURE(CharReaderAllowDropNullTest, issue178) {
  json::char_reader_builder b;
  b.settings_["allowDroppedNullPlaceholders"] = true;
  json::value root;
  std::string errs;
  json::char_reader* reader(b.newCharReader());
  {
	char const doc[] = "{\"a\":,\"b\":true}";
	bool ok = reader->parse(
		doc, doc + std::strlen(doc),
		&root, &errs);
	JSONTEST_ASSERT(ok);
	JSONTEST_ASSERT_STRING_EQUAL("", errs);
	JSONTEST_ASSERT_EQUAL(2u, root.size());
	JSONTEST_ASSERT_EQUAL(json::vt_null, root.get("a", true));
  }
  {
	char const doc[] = "{\"a\":}";
	bool ok = reader->parse(
		doc, doc + std::strlen(doc),
		&root, &errs);
	JSONTEST_ASSERT(ok);
	JSONTEST_ASSERT_STRING_EQUAL("", errs);
	JSONTEST_ASSERT_EQUAL(1u, root.size());
	JSONTEST_ASSERT_EQUAL(json::vt_null, root.get("a", true));
  }
  {
	char const doc[] = "[]";
	bool ok = reader->parse(
		doc, doc + std::strlen(doc),
		&root, &errs);
	JSONTEST_ASSERT(ok);
	JSONTEST_ASSERT(errs == "");
	JSONTEST_ASSERT_EQUAL(0u, root.size());
	JSONTEST_ASSERT_EQUAL(json::vt_array, root);
  }
  {
	char const doc[] = "[null]";
	bool ok = reader->parse(
		doc, doc + std::strlen(doc),
		&root, &errs);
	JSONTEST_ASSERT(ok);
	JSONTEST_ASSERT(errs == "");
	JSONTEST_ASSERT_EQUAL(1u, root.size());
  }
  {
	char const doc[] = "[,]";
	bool ok = reader->parse(
		doc, doc + std::strlen(doc),
		&root, &errs);
	JSONTEST_ASSERT(ok);
	JSONTEST_ASSERT_STRING_EQUAL("", errs);
	JSONTEST_ASSERT_EQUAL(2u, root.size());
  }
  {
	char const doc[] = "[,,,]";
	bool ok = reader->parse(
		doc, doc + std::strlen(doc),
		&root, &errs);
	JSONTEST_ASSERT(ok);
	JSONTEST_ASSERT_STRING_EQUAL("", errs);
	JSONTEST_ASSERT_EQUAL(4u, root.size());
  }
  {
	char const doc[] = "[null,]";
	bool ok = reader->parse(
		doc, doc + std::strlen(doc),
		&root, &errs);
	JSONTEST_ASSERT(ok);
	JSONTEST_ASSERT_STRING_EQUAL("", errs);
	JSONTEST_ASSERT_EQUAL(2u, root.size());
  }
  {
	char const doc[] = "[,null]";
	bool ok = reader->parse(
		doc, doc + std::strlen(doc),
		&root, &errs);
	JSONTEST_ASSERT(ok);
	JSONTEST_ASSERT(errs == "");
	JSONTEST_ASSERT_EQUAL(2u, root.size());
  }
  {
	char const doc[] = "[,,]";
	bool ok = reader->parse(
		doc, doc + std::strlen(doc),
		&root, &errs);
	JSONTEST_ASSERT(ok);
	JSONTEST_ASSERT_STRING_EQUAL("", errs);
	JSONTEST_ASSERT_EQUAL(3u, root.size());
  }
  {
	char const doc[] = "[null,,]";
	bool ok = reader->parse(
		doc, doc + std::strlen(doc),
		&root, &errs);
	JSONTEST_ASSERT(ok);
	JSONTEST_ASSERT_STRING_EQUAL("", errs);
	JSONTEST_ASSERT_EQUAL(3u, root.size());
  }
  {
	char const doc[] = "[,null,]";
	bool ok = reader->parse(
		doc, doc + std::strlen(doc),
		&root, &errs);
	JSONTEST_ASSERT(ok);
	JSONTEST_ASSERT_STRING_EQUAL("", errs);
	JSONTEST_ASSERT_EQUAL(3u, root.size());
  }
  {
	char const doc[] = "[,,null]";
	bool ok = reader->parse(
		doc, doc + std::strlen(doc),
		&root, &errs);
	JSONTEST_ASSERT(ok);
	JSONTEST_ASSERT(errs == "");
	JSONTEST_ASSERT_EQUAL(3u, root.size());
  }
  {
	char const doc[] = "[[],,,]";
	bool ok = reader->parse(
		doc, doc + std::strlen(doc),
		&root, &errs);
	JSONTEST_ASSERT(ok);
	JSONTEST_ASSERT_STRING_EQUAL("", errs);
	JSONTEST_ASSERT_EQUAL(4u, root.size());
	JSONTEST_ASSERT_EQUAL(json::vt_array, root[0u]);
  }
  {
	char const doc[] = "[,[],,]";
	bool ok = reader->parse(
		doc, doc + std::strlen(doc),
		&root, &errs);
	JSONTEST_ASSERT(ok);
	JSONTEST_ASSERT_STRING_EQUAL("", errs);
	JSONTEST_ASSERT_EQUAL(4u, root.size());
	JSONTEST_ASSERT_EQUAL(json::vt_array, root[1u]);
  }
  {
	char const doc[] = "[,,,[]]";
	bool ok = reader->parse(
		doc, doc + std::strlen(doc),
		&root, &errs);
	JSONTEST_ASSERT(ok);
	JSONTEST_ASSERT(errs == "");
	JSONTEST_ASSERT_EQUAL(4u, root.size());
	JSONTEST_ASSERT_EQUAL(json::vt_array, root[3u]);
  }
  delete reader;
}

struct CharReaderAllowSingleQuotesTest : JsonTest::TestCase {};

JSONTEST_FIXTURE(CharReaderAllowSingleQuotesTest, issue182) {
  json::char_reader_builder b;
  b.settings_["allowSingleQuotes"] = true;
  json::value root;
  std::string errs;
  json::char_reader* reader(b.newCharReader());
  {
	char const doc[] = "{'a':true,\"b\":true}";
	bool ok = reader->parse(
		doc, doc + std::strlen(doc),
		&root, &errs);
	JSONTEST_ASSERT(ok);
	JSONTEST_ASSERT_STRING_EQUAL("", errs);
	JSONTEST_ASSERT_EQUAL(2u, root.size());
	JSONTEST_ASSERT_EQUAL(true, root.get("a", false));
	JSONTEST_ASSERT_EQUAL(true, root.get("b", false));
  }
  {
	char const doc[] = "{'a': 'x', \"b\":'y'}";
	bool ok = reader->parse(
		doc, doc + std::strlen(doc),
		&root, &errs);
	JSONTEST_ASSERT(ok);
	JSONTEST_ASSERT_STRING_EQUAL("", errs);
	JSONTEST_ASSERT_EQUAL(2u, root.size());
	JSONTEST_ASSERT_STRING_EQUAL("x", root["a"].as_string());
	JSONTEST_ASSERT_STRING_EQUAL("y", root["b"].as_string());
  }
}

struct CharReaderAllowZeroesTest : JsonTest::TestCase {};

JSONTEST_FIXTURE(CharReaderAllowZeroesTest, issue176) {
  json::char_reader_builder b;
  b.settings_["allowSingleQuotes"] = true;
  json::value root;
  std::string errs;
  json::char_reader* reader(b.newCharReader());
  {
	char const doc[] = "{'a':true,\"b\":true}";
	bool ok = reader->parse(
		doc, doc + std::strlen(doc),
		&root, &errs);
	JSONTEST_ASSERT(ok);
	JSONTEST_ASSERT_STRING_EQUAL("", errs);
	JSONTEST_ASSERT_EQUAL(2u, root.size());
	JSONTEST_ASSERT_EQUAL(true, root.get("a", false));
	JSONTEST_ASSERT_EQUAL(true, root.get("b", false));
  }
  {
	char const doc[] = "{'a': 'x', \"b\":'y'}";
	bool ok = reader->parse(
		doc, doc + std::strlen(doc),
		&root, &errs);
	JSONTEST_ASSERT(ok);
	JSONTEST_ASSERT_STRING_EQUAL("", errs);
	JSONTEST_ASSERT_EQUAL(2u, root.size());
	JSONTEST_ASSERT_STRING_EQUAL("x", root["a"].as_string());
	JSONTEST_ASSERT_STRING_EQUAL("y", root["b"].as_string());
  }
}

struct BuilderTest : JsonTest::TestCase {};

JSONTEST_FIXTURE(BuilderTest, settings) {
  {
	json::value errs;
	json::char_reader_builder rb;
	JSONTEST_ASSERT_EQUAL(false, rb.settings_.is_member("foo"));
	JSONTEST_ASSERT_EQUAL(true, rb.validate(&errs));
	rb["foo"] = "bar";
	JSONTEST_ASSERT_EQUAL(true, rb.settings_.is_member("foo"));
	JSONTEST_ASSERT_EQUAL(false, rb.validate(&errs));
  }
  {
	json::value errs;
	json::stream_writer_builder wb;
	JSONTEST_ASSERT_EQUAL(false, wb.settings_.is_member("foo"));
	JSONTEST_ASSERT_EQUAL(true, wb.validate(&errs));
	wb["foo"] = "bar";
	JSONTEST_ASSERT_EQUAL(true, wb.settings_.is_member("foo"));
	JSONTEST_ASSERT_EQUAL(false, wb.validate(&errs));
  }
}

struct IteratorTest : JsonTest::TestCase {};

JSONTEST_FIXTURE(IteratorTest, distance) {
  json::value json;
  json["k1"] = "a";
  json["k2"] = "b";
  int dist = 0;
  std::string str;
  for (json::value_iterator it = json.begin(); it != json.end(); ++it) {
	dist = it - json.begin();
	str = it->as_string().c_str();
  }
  JSONTEST_ASSERT_EQUAL(1, dist);
  JSONTEST_ASSERT_STRING_EQUAL("b", str);
}

JSONTEST_FIXTURE(IteratorTest, names) {
  json::value json;
  json["k1"] = "a";
  json["k2"] = "b";
  json::value_iterator it = json.begin();
  JSONTEST_ASSERT(it != json.end());
  JSONTEST_ASSERT_EQUAL(json::value("k1"), it.key());
  JSONTEST_ASSERT_STRING_EQUAL("k1", it.name());
  JSONTEST_ASSERT_EQUAL(-1, it.index());
  ++it;
  JSONTEST_ASSERT(it != json.end());
  JSONTEST_ASSERT_EQUAL(json::value("k2"), it.key());
  JSONTEST_ASSERT_STRING_EQUAL("k2", it.name());
  JSONTEST_ASSERT_EQUAL(-1, it.index());
  ++it;
  JSONTEST_ASSERT(it == json.end());
}

JSONTEST_FIXTURE(IteratorTest, indexes) {
  json::value json;
  json[0] = "a";
  json[1] = "b";
  json::value_iterator it = json.begin();
  JSONTEST_ASSERT(it != json.end());
  JSONTEST_ASSERT_EQUAL(json::value(json::array_index(0)), it.key());
  JSONTEST_ASSERT_STRING_EQUAL("", it.name());
  JSONTEST_ASSERT_EQUAL(0, it.index());
  ++it;
  JSONTEST_ASSERT(it != json.end());
  JSONTEST_ASSERT_EQUAL(json::value(json::array_index(1)), it.key());
  JSONTEST_ASSERT_STRING_EQUAL("", it.name());
  JSONTEST_ASSERT_EQUAL(1, it.index());
  ++it;
  JSONTEST_ASSERT(it == json.end());
}

int main(int argc, const char* argv[]) {
  JsonTest::Runner runner;
  JSONTEST_REGISTER_FIXTURE(runner, ValueTest, checkNormalizeFloatingPointStr);
  JSONTEST_REGISTER_FIXTURE(runner, ValueTest, memberCount);
  JSONTEST_REGISTER_FIXTURE(runner, ValueTest, objects);
  JSONTEST_REGISTER_FIXTURE(runner, ValueTest, arrays);
  JSONTEST_REGISTER_FIXTURE(runner, ValueTest, null);
  JSONTEST_REGISTER_FIXTURE(runner, ValueTest, strings);
  JSONTEST_REGISTER_FIXTURE(runner, ValueTest, bools);
  JSONTEST_REGISTER_FIXTURE(runner, ValueTest, integers);
  JSONTEST_REGISTER_FIXTURE(runner, ValueTest, nonIntegers);
  JSONTEST_REGISTER_FIXTURE(runner, ValueTest, compareNull);
  JSONTEST_REGISTER_FIXTURE(runner, ValueTest, compareInt);
  JSONTEST_REGISTER_FIXTURE(runner, ValueTest, compareUInt);
  JSONTEST_REGISTER_FIXTURE(runner, ValueTest, compareDouble);
  JSONTEST_REGISTER_FIXTURE(runner, ValueTest, compareString);
  JSONTEST_REGISTER_FIXTURE(runner, ValueTest, compareBoolean);
  JSONTEST_REGISTER_FIXTURE(runner, ValueTest, compareArray);
  JSONTEST_REGISTER_FIXTURE(runner, ValueTest, compareObject);
  JSONTEST_REGISTER_FIXTURE(runner, ValueTest, compareType);
  JSONTEST_REGISTER_FIXTURE(runner, ValueTest, offsetAccessors);
  JSONTEST_REGISTER_FIXTURE(runner, ValueTest, typeChecksThrowExceptions);
  JSONTEST_REGISTER_FIXTURE(runner, ValueTest, static_string);
  JSONTEST_REGISTER_FIXTURE(runner, ValueTest, CommentBefore);
  //JSONTEST_REGISTER_FIXTURE(runner, ValueTest, nulls);
  JSONTEST_REGISTER_FIXTURE(runner, ValueTest, zeroes);
  JSONTEST_REGISTER_FIXTURE(runner, ValueTest, zeroesInKeys);

  JSONTEST_REGISTER_FIXTURE(runner, WriterTest, dropNullPlaceholders);
  JSONTEST_REGISTER_FIXTURE(runner, StreamWriterTest, dropNullPlaceholders);
  JSONTEST_REGISTER_FIXTURE(runner, StreamWriterTest, writeZeroes);

  JSONTEST_REGISTER_FIXTURE(runner, ReaderTest, parseWithNoErrors);
  JSONTEST_REGISTER_FIXTURE(
	  runner, ReaderTest, parseWithNoErrorsTestingOffsets);
  JSONTEST_REGISTER_FIXTURE(runner, ReaderTest, parseWithOneError);
  JSONTEST_REGISTER_FIXTURE(runner, ReaderTest, parseChineseWithOneError);
  JSONTEST_REGISTER_FIXTURE(runner, ReaderTest, parseWithDetailError);

  JSONTEST_REGISTER_FIXTURE(runner, CharReaderTest, parseWithNoErrors);
  JSONTEST_REGISTER_FIXTURE(
	  runner, CharReaderTest, parseWithNoErrorsTestingOffsets);
  JSONTEST_REGISTER_FIXTURE(runner, CharReaderTest, parseWithOneError);
  JSONTEST_REGISTER_FIXTURE(runner, CharReaderTest, parseChineseWithOneError);
  JSONTEST_REGISTER_FIXTURE(runner, CharReaderTest, parseWithDetailError);
  JSONTEST_REGISTER_FIXTURE(runner, CharReaderTest, parseWithStackLimit);

  JSONTEST_REGISTER_FIXTURE(runner, CharReaderStrictModeTest, dupKeys);

  JSONTEST_REGISTER_FIXTURE(runner, CharReaderFailIfExtraTest, issue164);
  JSONTEST_REGISTER_FIXTURE(runner, CharReaderFailIfExtraTest, issue107);
  JSONTEST_REGISTER_FIXTURE(runner, CharReaderFailIfExtraTest, commentAfterObject);
  JSONTEST_REGISTER_FIXTURE(runner, CharReaderFailIfExtraTest, commentAfterArray);
  JSONTEST_REGISTER_FIXTURE(runner, CharReaderFailIfExtraTest, commentAfterBool);

  JSONTEST_REGISTER_FIXTURE(runner, CharReaderAllowDropNullTest, issue178);

  JSONTEST_REGISTER_FIXTURE(runner, CharReaderAllowSingleQuotesTest, issue182);

  JSONTEST_REGISTER_FIXTURE(runner, CharReaderAllowZeroesTest, issue176);

  JSONTEST_REGISTER_FIXTURE(runner, BuilderTest, settings);

  JSONTEST_REGISTER_FIXTURE(runner, IteratorTest, distance);
  JSONTEST_REGISTER_FIXTURE(runner, IteratorTest, names);
  JSONTEST_REGISTER_FIXTURE(runner, IteratorTest, indexes);

  return runner.runCommandLine(argc, argv);
}
