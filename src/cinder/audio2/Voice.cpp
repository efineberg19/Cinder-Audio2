/*
 Copyright (c) 2014, The Cinder Project

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

#include "cinder/audio2/Voice.h"
#include "cinder/audio2/Context.h"
#include "cinder/audio2/NodeEffect.h"

#include <map>

using namespace std;
using namespace ci;

namespace cinder { namespace audio2 {

// ----------------------------------------------------------------------------------------------------
// MARK: - MixerImpl
// ----------------------------------------------------------------------------------------------------

// TODO: replace this private Mixer, which composits a Gain + NodePan per voice, into a NodeMixer
// that has a custom pullInputs and performs that gain / pan as a post-processing step

class MixerImpl {
public:

	static MixerImpl *get();

	//! returns the number of connected busses.
	void	setBusVolume( size_t busId, float volume );
	float	getBusVolume( size_t busId );
	void	setBusPan( size_t busId, float pos );
	float	getBusPan( size_t busId );

	void	addVoice( const VoiceRef &source );

	BufferRef loadBuffer( const SourceFileRef &sourceFile, size_t numChannels );

private:
	MixerImpl();

	struct Bus {
		VoiceRef		mVoice;
		GainRef		mGain;
		Pan2dRef	mPan;
	};

	std::vector<Bus> mBusses;
	std::map<std::pair<SourceFileRef, size_t>, BufferRef> mBufferCache;		// key is [shared_ptr, num channels]

	GainRef mMasterGain;
};

MixerImpl* MixerImpl::get()
{
	static unique_ptr<MixerImpl> sMixer;
	if( ! sMixer )
		sMixer.reset( new MixerImpl );

	return sMixer.get();
}

MixerImpl::MixerImpl()
{
	Context *ctx = Context::master();
	mMasterGain = ctx->makeNode( new Gain() );

	mMasterGain->addConnection( ctx->getOutput() );

	ctx->start();
}

void MixerImpl::addVoice( const VoiceRef &source )
{
	Context *ctx = Context::master();

	source->mBusId = mBusses.size();
	mBusses.push_back( MixerImpl::Bus() );
	MixerImpl::Bus &bus = mBusses.back();

	bus.mVoice = source;
	bus.mGain = ctx->makeNode( new Gain() );
	bus.mPan = ctx->makeNode( new Pan2d() );

	source->getNode()->connect( bus.mGain );
	bus.mGain->connect( bus.mPan );
	bus.mPan->addConnection( mMasterGain );
}

BufferRef MixerImpl::loadBuffer( const SourceFileRef &sourceFile, size_t numChannels )
{
	auto key = make_pair( sourceFile, numChannels );
	auto cached = mBufferCache.find( key );
	if( cached != mBufferCache.end() )
		return cached->second;
	else {
		BufferRef result = sourceFile->loadBuffer();
		mBufferCache.insert( make_pair( key, result ) );
		return result;
	}
}

void MixerImpl::setBusVolume( size_t busId, float volume )
{
	mBusses[busId].mGain->setValue( volume );
}

float MixerImpl::getBusVolume( size_t busId )
{
	return mBusses[busId].mGain->getValue();
}

void MixerImpl::setBusPan( size_t busId, float pos )
{
	auto pan = mBusses[busId].mPan;
	if( pan )
		pan->setPos( pos );
}

float MixerImpl::getBusPan( size_t busId )
{
	auto pan = mBusses[busId].mPan;
	if( pan )
		return mBusses[busId].mPan->getPos();

	return 0.0f;
}

// ----------------------------------------------------------------------------------------------------
// MARK: - Voice
// ----------------------------------------------------------------------------------------------------

VoiceRef Voice::create( const CallbackProcessorFn &callbackFn, const Options &options )
{
	VoiceRef result( new VoiceCallbackProcessor( callbackFn, options ) );
	MixerImpl::get()->addVoice( result );

	return result;
}

VoiceSamplePlayerRef Voice::create( const SourceFileRef &sourceFile, const Options &options )
{
	VoiceSamplePlayerRef result( new VoiceSamplePlayer( sourceFile, options ) );
	MixerImpl::get()->addVoice( result );

	return result;
}

float Voice::getVolume() const
{
	return MixerImpl::get()->getBusVolume( mBusId );
}

float Voice::getPan() const
{
	return MixerImpl::get()->getBusPan( mBusId );
}

void Voice::setVolume( float volume )
{
	MixerImpl::get()->setBusVolume( mBusId, volume );
}

void Voice::setPan( float pan )
{
	MixerImpl::get()->setBusPan( mBusId, pan );
}

void Voice::play()
{
	getNode()->start();
}

void Voice::pause()
{
	getNode()->stop();
}

void Voice::stop()
{
	getNode()->stop();
}

bool Voice::isPlaying() const
{
	return getNode()->isEnabled();
}

// ----------------------------------------------------------------------------------------------------
// MARK: - VoiceSamplePlayer
// ----------------------------------------------------------------------------------------------------

VoiceSamplePlayer::VoiceSamplePlayer( const SourceFileRef &sourceFile, const Options &options )
{
	sourceFile->setOutputFormat( audio2::master()->getSampleRate(), options.getChannels() );

	if( sourceFile->getNumFrames() <= options.getMaxFramesForBufferPlayback() ) {
		BufferRef buffer = MixerImpl::get()->loadBuffer( sourceFile, options.getChannels() );
		mNode = Context::master()->makeNode( new BufferPlayer( buffer ) );
	} else
		mNode = Context::master()->makeNode( new FilePlayer( sourceFile ) );

	mNode->setStartAtBeginning( false ); // allows Node to be 'paused', since seek needs to be done manually this way.
}

void VoiceSamplePlayer::play()
{
	if( mNode->isEof() )
		mNode->seek( 0 );

	mNode->start();
}

void VoiceSamplePlayer::stop()
{
	mNode->stop();
	mNode->seek( 0 );
}

// ----------------------------------------------------------------------------------------------------
// MARK: - VoiceCallbackProcessor
// ----------------------------------------------------------------------------------------------------

VoiceCallbackProcessor::VoiceCallbackProcessor( const CallbackProcessorFn &callbackFn, const Options &options )
{
	mNode = Context::master()->makeNode( new CallbackProcessor( callbackFn, Node::Format().channels( options.getChannels() ) ) );
}

} } // namespace cinder::audio2