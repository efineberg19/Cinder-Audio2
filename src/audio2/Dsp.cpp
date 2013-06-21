#include "audio2/Dsp.h"

#include "cinder/Cinder.h"

#if defined( CINDER_COCOA )
	#include <Accelerate/Accelerate.h>
#endif

namespace audio2 {

#if defined( CINDER_COCOA )

void generateBlackmanWindow( float *window, size_t length )
{
	vDSP_blkman_window( window, static_cast<vDSP_Length>( length ), 0 );
}

void generateHammWindow( float *window, size_t length )
{
	vDSP_hamm_window( window, static_cast<vDSP_Length>( length ), 0 );
}

void generateHannWindow( float *window, size_t length )
{
	vDSP_hann_window( window, static_cast<vDSP_Length>( length ), 0 );
}

#else

// from WebKit's applyWindow in RealtimeAnalyser.cpp
void generateBlackmanWindow( float *window, size_t length )
{
	double alpha = 0.16;
	double a0 = 0.5 * (1 - alpha);
	double a1 = 0.5;
	double a2 = 0.5 * alpha;
	double oneOverN = 1.0 / static_cast<double>( length );

	for( size_t i = 0; i < length; ++i ) {
		double x = static_cast<double>(i) * oneOverN;
		window[i] = float( a0 - a1 * cos( 2.0 * M_PI * x ) + a2 * cos( 4.0 * M_PI * x ) );
	}
}

void generateHammWindow( float *window, size_t length )
{
	CI_ASSERT( 0 && "not implemented" );
}

void generateHannWindow( float *window, size_t length )
{
	CI_ASSERT( 0 && "not implemented" );
}

#endif

} // namespace audio2