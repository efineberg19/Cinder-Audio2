#include "audio2/msw/util.h"

#include <xaudio2.h>

namespace audio2 { namespace msw {

	std::shared_ptr<::WAVEFORMATEX> interleavedFloatWaveFormat( size_t numChannels, size_t sampleRate )
	{
		::WAVEFORMATEXTENSIBLE *wfx = (::WAVEFORMATEXTENSIBLE *)calloc( 1, sizeof( ::WAVEFORMATEXTENSIBLE ) );
		wfx->Format.wFormatTag           = WAVE_FORMAT_EXTENSIBLE ;
		wfx->Format.nSamplesPerSec       = sampleRate;
		wfx->Format.nChannels            = numChannels;
		wfx->Format.wBitsPerSample       = 32;
		wfx->Format.nBlockAlign          = wfx->Format.nChannels * wfx->Format.wBitsPerSample / 8;
		wfx->Format.nAvgBytesPerSec      = wfx->Format.nSamplesPerSec * wfx->Format.nBlockAlign;
		wfx->Format.cbSize               = sizeof( ::WAVEFORMATEXTENSIBLE ) - sizeof( ::WAVEFORMATEX );
		wfx->Samples.wValidBitsPerSample = wfx->Format.wBitsPerSample;
		wfx->SubFormat                   = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
		wfx->dwChannelMask				= 0; // this could be a very complicated bit mask of channel order, but 0 means 'first channel is left, second channel is right, etc'

		return std::shared_ptr<::WAVEFORMATEX>( (::WAVEFORMATEX *)wfx, free );;
	}

} } // namespace audio2::msw