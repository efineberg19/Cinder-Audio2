/*
 Copyright (c) 2013, The Cinder Project

 This code is intended to be used with the Cinder C++ library, http://libcinder.org

 Redistribution and use in source and binary forms, with or without modification, are permitted provided that
 the following conditions are met:

    * Redistributions of source code must retain the above copyright notice, this list of conditions and
	the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and
	the following disclaimer in the documentation and/or other materials provided with the distribution.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
 WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 POSSIBILITY OF SUCH DAMAGE.
*/

#include "audio2/audio.h"
#include "audio2/Context.h"
#include "audio2/NodeSource.h"

using namespace std;
using namespace ci;

namespace cinder { namespace audio2 {

class Mixer {
public:

	static Mixer *get();

	//! returns the number of connected busses.
	void	setBusVolume( size_t busId, float volume );
	float	getBusVolume( size_t busId );
	void	setBusPan( size_t busId, float pos );
	float	getBusPan( size_t busId );

	void	addVoice( const VoiceRef &source, const VoiceOptions &options );

private:
	Mixer();

	struct Bus {
		VoiceRef		mVoice;
		NodeGainRef		mGain;
		NodePan2dRef	mPan;
	};

	std::vector<Bus> mBusses;

	NodeGainRef mMasterGain;
};


Mixer* Mixer::get()
{
	static unique_ptr<Mixer> sMixer;
	if( ! sMixer )
		sMixer.reset( new Mixer );

	return sMixer.get();
}

Mixer::Mixer()
{
	Context *ctx = Context::master();
	mMasterGain = ctx->makeNode( new NodeGain() );

	mMasterGain->addConnection( ctx->getTarget() );

	ctx->start();
}

void Mixer::addVoice( const VoiceRef &source, const VoiceOptions &options )
{
	Context *ctx = Context::master();

	source->mBusId = mBusses.size();
	mBusses.push_back( Mixer::Bus() );
	Mixer::Bus &bus = mBusses.back();

	bus.mVoice = source;

	NodeRef node = source->getNode();
	if( options.isVolumeEnabled() ) {
		bus.mGain = ctx->makeNode( new NodeGain() );
		node = node->connect( bus.mGain );
	}
	if( options.isPanEnabled() ) {
		bus.mPan = ctx->makeNode( new NodePan2d() );
		node = node->connect( bus.mPan );
	}

	node->connect( mMasterGain );	
}

void Mixer::setBusVolume( size_t busId, float volume )
{
	mBusses[busId].mGain->setGain( volume );
}

float Mixer::getBusVolume( size_t busId )
{
	return mBusses[busId].mGain->getGain();
}

void Mixer::setBusPan( size_t busId, float pos )
{
	auto pan = mBusses[busId].mPan;
	if( pan )
		pan->setPos( pos );
}

float Mixer::getBusPan( size_t busId )
{
	auto pan = mBusses[busId].mPan;
	if( pan )
		return mBusses[busId].mPan->getPos();

	return 0.0f;
}

void Voice::setVolume( float volume )
{
	Mixer::get()->setBusVolume( mBusId, volume );
}

void Voice::setPan( float pan )
{
	Mixer::get()->setBusPan( mBusId, pan );
}

VoiceSamplePlayer::VoiceSamplePlayer( const DataSourceRef &dataSource )
{
	size_t sampleRate = Context::master()->getSampleRate();
	SourceFileRef sourceFile = SourceFile::create( dataSource, 0, sampleRate );

	// maximum samples for default buffer playback is 1 second stereo at 48k samplerate
	const size_t kMaxFramesForBufferPlayback = 48000 * 2;

	if( sourceFile->getNumFrames() < kMaxFramesForBufferPlayback )
		mSamplePlayer = Context::master()->makeNode( new NodeBufferPlayer( sourceFile->loadBuffer() ) ); // TODO: cache buffer so other loads don't need to do this
	else
		mSamplePlayer = Context::master()->makeNode( new NodeFilePlayer( sourceFile ) );
}

VoiceRef makeVoice( const DataSourceRef &dataSource, const VoiceOptions &options )
{
	auto result = VoiceRef( new VoiceSamplePlayer( dataSource ) );
	Mixer::get()->addVoice( result, options );

	return result;
}

void play( const VoiceRef &source )
{
	source->getNode()->start();
}

} } // namespace cinder::audio2