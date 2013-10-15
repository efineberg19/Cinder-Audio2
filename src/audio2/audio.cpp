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

class SourceImplSamplePlayer : public Source {
public:
	SourceImplSamplePlayer( const DataSourceRef &dataSource );

	const NodeRef& getNode() override	{ return mSamplePlayer; }
private:
	NodeSamplePlayerRef mSamplePlayer;
};

class Mixer {
public:

	static Mixer *get();

	//! returns the number of connected busses.
	size_t	getNumBusses();
	void	setMaxNumActiveBusses( size_t count );
	void	setBusVolume( size_t bus, float volume );
	float	getBusVolume( size_t bus );
	void	setBusPan( size_t bus, float pan );
	float	getBusPan( size_t bus );

	//! Returns the bus \a source was added to
	size_t	addSource( const SourceRef &source );

private:
	Mixer();

	struct Bus {
		SourceRef   mSource;
		NodeGainRef mGain;
		NodeGainRef mPan;
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

size_t Mixer::addSource( const SourceRef &source )
{
	Mixer::Bus bus;
	bus.mSource = source;

	size_t busId = mBusses.size();
	mBusses.push_back( bus );

	source->getNode()->connect( mMasterGain );
	return busId;
}

SourceImplSamplePlayer::SourceImplSamplePlayer( const DataSourceRef &dataSource )
{
	size_t sampleRate = Context::master()->getSampleRate();
	SourceFileRef sourceFile = SourceFile::create( dataSource, 0, sampleRate );

	// maximum samples for default buffer playback is 1 second stereo at 48k samplerate
	const size_t kMaxFramesForBufferPlayback = 48000 * 2;


	if( sourceFile->getNumFrames() < kMaxFramesForBufferPlayback )
		mSamplePlayer = Context::master()->makeNode( new NodeBufferPlayer( sourceFile->loadBuffer() ) );
	else
		mSamplePlayer = Context::master()->makeNode( new NodeFilePlayer( sourceFile ) );
}

SourceRef load( const DataSourceRef &dataSource )
{
	auto result = SourceRef( new SourceImplSamplePlayer( dataSource ) );
	Mixer::get()->addSource( result );

	return result;
}

void play( const SourceRef &source )
{
	source->getNode()->start();
}

} } // namespace cinder::audio2