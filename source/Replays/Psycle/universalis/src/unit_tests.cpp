
#define BOOST_LIB_NAME boost_unit_test_framework
#define BOOST_DYN_LINK
#include <boost/config/auto_link.hpp>

#define BOOST_TEST_DYN_LINK
#define BOOST_AUTO_TEST_MAIN
#include <boost/test/auto_unit_test.hpp>

#define UNIVERSALIS__OS__LOGGERS__LEVELS__COMPILED_THRESHOLD trace

#include <universalis/stdlib/chrono.hpp>
#include <universalis/stdlib/thread.hpp>
#include <universalis/stdlib/mutex.hpp>
#include <universalis/stdlib/condition_variable.hpp>
#include <universalis/stdlib/cmath.hpp>
#include <universalis/os/clocks.hpp>
#include <universalis/os/detail/clocks.hpp>
#include <universalis/os/sched.hpp>
#include <universalis/os/aligned_alloc.hpp>
#include <universalis/cpu/atomic_compare_and_swap.hpp>
