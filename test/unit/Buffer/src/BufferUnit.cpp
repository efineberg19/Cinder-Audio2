
#define BOOST_TEST_MODULE BufferUnit

#include <boost/test/included/unit_test.hpp>

#include "audio2/Buffer.h"

using namespace audio2;

BOOST_AUTO_TEST_CASE( TestSize )
{
	Buffer buffer( 2, 4 );
    BOOST_REQUIRE_EQUAL( buffer.getSize(), 8 );
}

BOOST_AUTO_TEST_CASE( TestDeinterleavePow2 )
{
	std::vector<int> arr( 4 );
	arr[0] = 0;
	arr[1] = 2;
	arr[2] = 1;
	arr[3] = 3;

	deinterleaveInplacePow2( arr.data(), arr.size() );

    BOOST_CHECK_EQUAL( arr[0], 0 );
    BOOST_CHECK_EQUAL( arr[1], 1 );
    BOOST_CHECK_EQUAL( arr[2], 2 );
    BOOST_CHECK_EQUAL( arr[3], 3 );
}

