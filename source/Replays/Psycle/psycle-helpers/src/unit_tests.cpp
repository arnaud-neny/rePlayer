
#define BOOST_LIB_NAME boost_unit_test_framework
#define BOOST_DYN_LINK
#include <boost/config/auto_link.hpp>

#define BOOST_TEST_DYN_LINK
#define BOOST_AUTO_TEST_MAIN
#include <boost/test/auto_unit_test.hpp>

#define UNIVERSALIS__OS__LOGGERS__LEVELS__COMPILED_THRESHOLD trace

#include <psycle/helpers/math/math.hpp>
#include <psycle/helpers/dsp.hpp>
#include <psycle/helpers/ring_buffer.hpp>
