#pragma once

#include <memory>
#include <AudioToolbox/AudioToolbox.h>

struct AudioStreamBasicDescription;

namespace audio2 { namespace cocoa {

void printASBD( const ::AudioStreamBasicDescription &asbd );

struct AudioBufferListDeleter {
	void operator()( AudioBufferList *bufferList ) { free( bufferList ); }
};

typedef std::unique_ptr<AudioBufferList, AudioBufferListDeleter> AudioBufferListRef;

// TODO: channelSize should be samplesPerChannel (assume they are float). Should also consider adopting the CAPublicUitility way of doing this (I think it does it on the stack)
AudioBufferListRef createNonInterleavedBufferList( size_t numChannels, size_t channelSize );

AudioComponent findAudioComponent( const AudioComponentDescription &componentDescription );
void findAndCreateAudioComponent( const AudioComponentDescription &componentDescription, AudioComponentInstance *componentInstance );

} } // namespace audio2::cocoa