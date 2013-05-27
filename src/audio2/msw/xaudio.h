#pragma once

#define XAUDIO2_HELPER_FUNCTIONS

#include <xaudio2.h>
#include <xaudio2fx.h>

#include "XAPOFX.h"

#if( _WIN32_WINNT >= 0x0602 ) // _WIN32_WINNT_WIN8
	#if defined( _USING_V110_SDK71_ )
		#error "Wrong compiler toolset (v110) - switch to v110_xp"
	#endif
	#define CINDER_XAUDIO_2_8
	#pragma comment(lib, "xaudio2.lib")
	#pragma comment(lib, "xapobase.lib")
#else
	#if ! defined( _USING_V110_SDK71_ )
		#error "Wrong compiler toolset (v110) - switch to v110_xp"
	#endif
	#define CINDER_XAUDIO_2_7
	#pragma comment(lib, "XAPOFX.lib")
#endif


namespace audio2 { namespace msw {

} } // namespace audio2::msw
