
#define BOOST_TEST_MODULE FftUnit

#include <boost/test/included/unit_test.hpp>

#include "audio2/Fft.h"

using namespace ci::audio2;

BOOST_AUTO_TEST_CASE( test_size )
{
	Buffer buffer( 2, 4 );
    BOOST_REQUIRE_EQUAL( buffer.getSize(), 8 );
}

