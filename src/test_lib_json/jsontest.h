// Derived from public-domain/MIT-licensed code at
// https://github.com/open-source-parsers/jsoncpp. Thanks, Baptiste Lepilleur!

#ifndef _168447578ee3470da61b7ef63813d636
#define _168447578ee3470da61b7ef63813d636

#include <json/config.h>
#include <json/value.h>
#include <json/writer.h>
#include <stdio.h>
#include <deque>
#include <sstream>
#include <string>

// //////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////
// Mini Unit Testing framework
// //////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////

/** \brief Unit testing framework.
 * \warning: all assertions are non-aborting, test case execution will continue
 *           even if an assertion namespace.
 *           This constraint is for portability: the framework needs to compile
 *           on Visual Studio 6 and must not require exception usage.
 */
namespace JsonTest {

class Failure {
public:
    const char* file_;
    unsigned int line_;
    std::string expr_;
    std::string message_;
    unsigned int nestingLevel_;
};

/// Context used to create the assertion callstack on failure.
/// Must be a POD to allow inline initialisation without stepping
/// into the debugger.
struct PredicateContext {
    typedef unsigned int Id;
    Id id_;
    const char* file_;
    unsigned int line_;
    const char* expr_;
    PredicateContext* next_;
    /// Related Failure, set when the PredicateContext is converted
    /// into a Failure.
    Failure* failure_;
};

class TestResult {
public:
    TestResult();

    /// \internal Implementation detail for assertion macros
    /// Not encapsulated to prevent step into when debugging failed assertions
    /// Incremented by one on assertion predicate entry, decreased by one
    /// by addPredicateContext().
    PredicateContext::Id predicateId_;

    /// \internal Implementation detail for predicate macros
    PredicateContext* predicateStackTail_;

    void setTestName(std::string const& name);

    /// Adds an assertion failure.
    TestResult&
    addFailure(const char* file, unsigned int line, const char* expr = 0);

    /// Removes the last PredicateContext added to the predicate stack
    /// chained list.
    /// Next messages will be targed at the PredicateContext that was removed.
    TestResult& popPredicateContext();

    bool failed() const;

    void printFailure(bool printTestName) const;

    // Generic operator that will work with anything ostream can deal with.
    template <typename T>
    TestResult& operator<<(T const& value)
    {
        std::ostringstream oss;
        oss.precision(16);
        oss.setf(std::ios_base::floatfield);
        oss << value;
        return addToLastFailure(oss.str());
    }

    // Specialized versions.
    TestResult& operator<<(bool value);
    // std:ostream does not support 64bits integers on all STL implementation
    TestResult& operator<<(int64_t value);
    TestResult& operator<<(uint64_t value);

private:
    TestResult& addToLastFailure(std::string const& message);
    unsigned int getAssertionNestingLevel() const;
    /// Adds a failure or a predicate context
    void addFailureInfo(const char* file,
        unsigned int line,
        const char* expr,
        unsigned int nestingLevel);
    static std::string indentText(std::string const& text,
        std::string const& indent);

    typedef std::deque<Failure> Failures;
    Failures failures_;
    std::string name_;
    PredicateContext rootPredicateNode_;
    PredicateContext::Id lastUsedPredicateId_;
    /// Failure which is the target of the messages added using operator <<
    Failure* messageTarget_;
};

class TestCase {
public:
    TestCase();

    virtual ~TestCase();

    void run(TestResult& result);

    virtual const char* testName() const = 0;

protected:
    TestResult* result_;

private:
    virtual void runTestCase() = 0;
};

/// Function pointer type for TestCase factory
typedef TestCase* (*TestCaseFactory)();

class Runner {
public:
    Runner();

    /// Adds a test to the suite
    Runner& add(TestCaseFactory factory);

    /// Runs test as specified on the command-line
    /// If no command-line arguments are provided, run all tests.
    /// If --list-tests is provided, then print the list of all test cases
    /// If --test <testname> is provided, then run test testname.
    int runCommandLine(int argc, const char* argv[]) const;

    /// Runs all the test cases
    bool runallTest(bool printSummary) const;

    /// Returns the number of test case in the suite
    unsigned int testCount() const;

    /// Returns the name of the test case at the specified index
    std::string testNameAt(unsigned int index) const;

    /// Runs the test case at the specified index using the specified TestResult
    void runTestAt(unsigned int index, TestResult& result) const;

    static void print_usage(const char* appName);

private: // prevents copy construction and assignment
    Runner(Runner const& other);
    Runner& operator=(Runner const& other);

private:
    void listTests() const;
    bool testIndex(std::string const& testName, unsigned int& index) const;
    static void preventDialogOnCrash();

private:
    typedef std::deque<TestCaseFactory> Factories;
    Factories tests_;
};

template <typename T, typename U>
TestResult& checkEqual(TestResult& result,
    T expected,
    U actual,
    const char* file,
    unsigned int line,
    const char* expr)
{
    if (static_cast<U>(expected) != actual) {
        result.addFailure(file, line, expr);
        result << "Expected: " << static_cast<U>(expected) << "\n";
        result << "Actual  : " << actual;
    }
    return result;
}

TestResult& checkStringEqual(TestResult& result,
    std::string const& expected,
    std::string const& actual,
    const char* file,
    unsigned int line,
    const char* expr);

} // namespace JsonTest

/// \brief Asserts that the given expression is true.
/// JSONTEST_ASSERT( x == y ) << "x=" << x << ", y=" << y;
/// JSONTEST_ASSERT( x == y );
#define JSONTEST_ASSERT(expr) \
    if (expr) {               \
    }                         \
    else                      \
    result_->addFailure(__FILE__, __LINE__, #expr)

/// \brief Asserts that the given predicate is true.
/// The predicate may do other assertions and be a member function of the
/// fixture.
#define JSONTEST_ASSERT_PRED(expr)                                                                                       \
    {                                                                                                                    \
        JsonTest::PredicateContext _minitest_Context = { result_->predicateId_, __FILE__, __LINE__, #expr, NULL, NULL }; \
        result_->predicateStackTail_->next_ = &_minitest_Context;                                                        \
        result_->predicateId_ += 1;                                                                                      \
        result_->predicateStackTail_ = &_minitest_Context;                                                               \
        (expr);                                                                                                          \
        result_->popPredicateContext();                                                                                  \
    }

/// \brief Asserts that two values are equals.
#define JSONTEST_ASSERT_EQUAL(expected, actual) \
    JsonTest::checkEqual(*result_,              \
        expected,                               \
        actual,                                 \
        __FILE__,                               \
        __LINE__,                               \
        #expected " == " #actual)

/// \brief Asserts that two values are equals.
#define JSONTEST_ASSERT_STRING_EQUAL(expected, actual) \
    JsonTest::checkStringEqual(*result_,               \
        std::string(expected),                         \
        std::string(actual),                           \
        __FILE__,                                      \
        __LINE__,                                      \
        #expected " == " #actual)

/// \brief Asserts that a given expression throws an exception
#define JSONTEST_ASSERT_THROWS(expr)                                                      \
    {                                                                                     \
        bool _threw = false;                                                              \
        try {                                                                             \
            expr;                                                                         \
        }                                                                                 \
        catch (...) {                                                                     \
            _threw = true;                                                                \
        }                                                                                 \
        if (!_threw)                                                                      \
            result_->addFailure(__FILE__, __LINE__, "expected exception thrown: " #expr); \
    }

/// \brief Begin a fixture test case.
#define JSONTEST_FIXTURE(FixtureType, name)                                     \
    class Test##FixtureType##name : public FixtureType {                        \
    public:                                                                     \
        static JsonTest::TestCase* factory()                                    \
        {                                                                       \
            return new Test##FixtureType##name();                               \
        }                                                                       \
                                                                                \
    public: /* overidden from TestCase */                                       \
        virtual const char* testName() const { return #FixtureType "/" #name; } \
        virtual void runTestCase();                                             \
    };                                                                          \
                                                                                \
    void Test##FixtureType##name::runTestCase()

#define JSONTEST_FIXTURE_FACTORY(FixtureType, name) \
    &Test##FixtureType##name::factory

#define JSONTEST_REGISTER_FIXTURE(runner, FixtureType, name) \
    (runner).add(JSONTEST_FIXTURE_FACTORY(FixtureType, name))

#endif // ifndef JSONTEST_H_INCLUDED
