#include "audio2/cocoa/Util.h"

#include <AudioToolbox/AudioToolbox.h>

namespace audio2 { namespace cocoa {

#if defined( CINDER_COCOA )

	void printASBD( const ::AudioStreamBasicDescription &asbd ) {
		char formatIDString[5];
		UInt32 formatID = CFSwapInt32HostToBig (asbd.mFormatID);
		bcopy (&formatID, formatIDString, 4);
		formatIDString[4] = '\0';

		printf( "  Sample Rate:         %10.0f\n",  asbd.mSampleRate );
		printf( "  Format ID:           %10s\n",    formatIDString );
		printf( "  Format Flags:        %10X\n",    (unsigned int)asbd.mFormatFlags );
		printf( "  Bytes per Packet:    %10d\n",    (unsigned int)asbd.mBytesPerPacket );
		printf( "  Frames per Packet:   %10d\n",    (unsigned int)asbd.mFramesPerPacket );
		printf( "  Bytes per Frame:     %10d\n",    (unsigned int)asbd.mBytesPerFrame );
		printf( "  Channels per Frame:  %10d\n",    (unsigned int)asbd.mChannelsPerFrame );
		printf( "  Bits per Channel:    %10d\n",    (unsigned int)asbd.mBitsPerChannel );
	}

#endif // defined( CINDER_COCOA )

} } // namespace audio2::cocoa