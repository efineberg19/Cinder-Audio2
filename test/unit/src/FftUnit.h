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
		Buffer data( sizeFft );
		BufferSpectral spectral( sizeFft );

		fillRandom( &data );
		Buffer dataCopy( data ); // TODO: ensure data is not modified after forward sigh maxErr. ???: will this already be handled by const *?

		fft.forward( &data, &spectral );
		fft.inverse( &spectral, &data );

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