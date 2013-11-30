#pragma once

#include "cinder/audio2/Buffer.h"
#include "utils.h"

BOOST_AUTO_TEST_SUITE( test_buffer )

using namespace ci;
using namespace ci::audio2;

BOOST_AUTO_TEST_CASE( test_size )
{
	Buffer buffer( 4, 2 );
    BOOST_REQUIRE_EQUAL( buffer.getSize(), 8 );
}

BOOST_AUTO_TEST_CASE( test_copy )
{
	Buffer a( 4, 2 );
	fillRandom( &a );
	Buffer b( a );

	BOOST_REQUIRE_EQUAL( a.getSize(), b.getSize() );
	BOOST_REQUIRE_EQUAL( a.getNumFrames(), b.getNumFrames() );
	BOOST_REQUIRE_EQUAL( a.getNumChannels(), b.getNumChannels() );
	//BOOST_REQUIRE_EQUAL( a.isSilent(), b.isSilent() );

	float maxErr = maxError( a, b );
	BOOST_CHECK( maxErr < ACCEPTABLE_FLOAT_ERROR );
}

BOOST_AUTO_TEST_CASE( test_interleave_out_of_place )
{
	BufferInterleavedT<int> interleaved( 4, 2 );
	BufferT<int> nonInterleaved( 4, 2 );

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
	BufferInterleavedT<int> interleaved( 4, 2 );
	BufferT<int> nonInterleaved( 3, 2 );

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
	BufferInterleavedT<int> interleaved( 3, 2 );
	BufferT<int> nonInterleaved( 4, 2 );

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

BOOST_AUTO_TEST_SUITE_END()