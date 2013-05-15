#pragma once

// TODO: this check really needs to check deployment target too, not just toolset
#if defined( _USING_V110_SDK71_ )
	#define CINDER_XAUDIO_2_7
#else
	#define CINDER_XAUDIO_2_8

	#pragma comment(lib, "xaudio2.lib")
	#pragma comment(lib, "xapobase.lib")
#endif

namespace audio2 { namespace msw {



} } // namespace audio2::msw
