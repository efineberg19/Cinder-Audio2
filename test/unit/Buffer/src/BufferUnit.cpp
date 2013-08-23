
#define BOOST_TEST_MODULE BufferUnit

#include <boost/test/included/unit_test.hpp>

#include "audio2/Buffer.h"

using namespace ci::audio2;

BOOST_AUTO_TEST_CASE( TestSize )
{
	Buffer buffer( 2, 4 );
    BOOST_REQUIRE_EQUAL( buffer.getSize(), 8 );
}

BOOST_AUTO_TEST_CASE( test_in_place )
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

BOOST_AUTO_TEST_CASE( test_out_of_place )
{
	BufferT<int> interleaved( 2, 4, BufferT<int>::Layout::INTERLEAVED );
	BufferT<int> nonInterleaved( 2, 4 );

	nonInterleaved[0] = 10;
	nonInterleaved[1] = 11;
	nonInterleaved[2] = 12;
	nonInterleaved[3] = 13;
	nonInterleaved[4] = 20;
	nonInterleaved[5] = 21;
	nonInterleaved[6] = 22;
	nonInterleaved[7] = 23;

	interleaveStereoBuffer( &nonInterleaved, &interleaved );

    BOOST_CHECK_EQUAL( interleaved[0], 10 );
    BOOST_CHECK_EQUAL( interleaved[1], 20 );
    BOOST_CHECK_EQUAL( interleaved[2], 11 );
    BOOST_CHECK_EQUAL( interleaved[3], 21 );
    BOOST_CHECK_EQUAL( interleaved[4], 12 );
    BOOST_CHECK_EQUAL( interleaved[5], 22 );
    BOOST_CHECK_EQUAL( interleaved[6], 13 );
    BOOST_CHECK_EQUAL( interleaved[7], 23 );

	deinterleaveStereoBuffer( &interleaved, &nonInterleaved );

	BOOST_CHECK_EQUAL( nonInterleaved[0], 10 );
    BOOST_CHECK_EQUAL( nonInterleaved[1], 11 );
    BOOST_CHECK_EQUAL( nonInterleaved[2], 12 );
    BOOST_CHECK_EQUAL( nonInterleaved[3], 13 );
    BOOST_CHECK_EQUAL( nonInterleaved[4], 20 );
    BOOST_CHECK_EQUAL( nonInterleaved[5], 21 );
    BOOST_CHECK_EQUAL( nonInterleaved[6], 22 );
    BOOST_CHECK_EQUAL( nonInterleaved[7], 23 );
}

BOOST_AUTO_TEST_CASE( test_mismatched_deinterleave )
{
	BufferT<int> interleaved( 2, 4, BufferT<int>::Layout::INTERLEAVED );
	BufferT<int> nonInterleaved( 2, 3 );

	interleaved[0] = 10;
	interleaved[1] = 20;
	interleaved[2] = 11;
	interleaved[3] = 21;
	interleaved[4] = 12;
	interleaved[5] = 22;
	interleaved[6] = 13;
	interleaved[7] = 23;

	deinterleaveStereoBuffer( &interleaved, &nonInterleaved );

	BOOST_CHECK_EQUAL( nonInterleaved[0], 10 );
    BOOST_CHECK_EQUAL( nonInterleaved[1], 11 );
    BOOST_CHECK_EQUAL( nonInterleaved[2], 12 );
    BOOST_CHECK_EQUAL( nonInterleaved[3], 20 );
    BOOST_CHECK_EQUAL( nonInterleaved[4], 21 );
    BOOST_CHECK_EQUAL( nonInterleaved[5], 22 );
}

BOOST_AUTO_TEST_CASE( test_mismatched_interleave )
{
	BufferT<int> interleaved( 2, 3, BufferT<int>::Layout::INTERLEAVED );
	BufferT<int> nonInterleaved( 2, 4 );

	nonInterleaved[0] = 10;
	nonInterleaved[1] = 11;
	nonInterleaved[2] = 12;
	nonInterleaved[3] = 13;
	nonInterleaved[4] = 20;
	nonInterleaved[5] = 21;
	nonInterleaved[6] = 22;
	nonInterleaved[7] = 23;

	interleaveStereoBuffer( &nonInterleaved, &interleaved );

    BOOST_CHECK_EQUAL( interleaved[0], 10 );
    BOOST_CHECK_EQUAL( interleaved[1], 20 );
    BOOST_CHECK_EQUAL( interleaved[2], 11 );
    BOOST_CHECK_EQUAL( interleaved[3], 21 );
    BOOST_CHECK_EQUAL( interleaved[4], 12 );
    BOOST_CHECK_EQUAL( interleaved[5], 22 );
}
