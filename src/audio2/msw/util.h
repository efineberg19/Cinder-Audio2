#pragma once

#include <memory>

struct tWAVEFORMATEX;
typedef struct tWAVEFORMATEX WAVEFORMATEX;

namespace audio2 { namespace msw {

struct ComReleaser {
	template <typename T>
	void operator()(T* ptr)	{ ptr->Release(); }
};

//! Creates a unique_ptr whose deleter will properly decrement the reference count of a COM object
// TODO: move to cinder's msw utils
template<typename T>
inline std::unique_ptr<T, ComReleaser> makeComUnique( T *p )	{ return std::unique_ptr<T, ComReleaser>( p ); }

struct VoiceDeleter {
	template <typename T>
	void operator()( T *voice )	{ voice->DestroyVoice(); }
};

template<typename T>
inline std::unique_ptr<T, VoiceDeleter> makeVoiceUnique( T *p )	{ return std::unique_ptr<T, VoiceDeleter>( p ); }

//! return pointer type is actually a WAVEFORMATEXTENSIBLE, identifiable by the wFormat tag
std::shared_ptr<::WAVEFORMATEX> interleavedFloatWaveFormat( size_t numChannels, size_t sampleRate );

} } // namespace audio2::msw
