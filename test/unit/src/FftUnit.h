#pragma once

#include "utils.h"
#include "audio2/Fft.h"

#include <iostream>

BOOST_AUTO_TEST_SUITE( test_fft )

using namespace ci::audio2;

namespace {

	void computeRoundTrip( size_t sizeFft )
	{
		Fft fft( sizeFft );
		Buffer data( 1, sizeFft );
		fillRandom( &data );
		Buffer dataCopy( data );

		fft.forward( &data );
		fft.inverse( &data, fft.getReal(), fft.getImag() );

		float maxErr = maxError( data, dataCopy );
		std::cout << "\tsizeFft: " << sizeFft << ", max error: " << maxErr << std::endl;

		BOOST_REQUIRE( maxErr < ACCEPTABLE_FLOAT_ERROR );
	}

}

BOOST_AUTO_TEST_CASE( test_round_trip )
{
	std::cout << "... Fft round trip max acceptable error: " << ACCEPTABLE_FLOAT_ERROR << std::endl;
	for( size_t i = 0; i < 14; i ++ )
		computeRoundTrip( 2 << i );
}

BOOST_AUTO_TEST_SUITE_END()