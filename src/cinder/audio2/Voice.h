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

#pragma once

#include "cinder/DataSource.h"
#include "cinder/audio2/NodeSource.h"
#include "cinder/audio2/File.h"

#include <memory>

namespace cinder { namespace audio2 {

typedef std::shared_ptr<class Voice> VoiceRef;
typedef std::shared_ptr<class VoiceSamplePlayer> VoiceSamplePlayerRef;

class Voice {
  public:

	struct Options {
		Options() : mChannels( 0 ), mMaxFramesForBufferPlayback( 96000 ) {}

		Options& channels( size_t ch )							{ mChannels = ch; return *this; }
		Options& maxFramesForBufferPlayback( size_t frames )	{ mMaxFramesForBufferPlayback = frames; return *this; }

		size_t			getChannels() const						{ return mChannels; }
		size_t			getMaxFramesForBufferPlayback() const	{ return mMaxFramesForBufferPlayback; }

	protected:
		size_t			mChannels, mMaxFramesForBufferPlayback;
	};

	//! Creates a Voice that manages sample playback of an audio file pointed at with \a sourceFile.
	static VoiceSamplePlayerRef create( const SourceFileRef &sourceFile, const Options &options = Options() );
	//! Creates a Voice that continously calls \a callbackFn to process a Buffer of samples.
	static VoiceRef create( CallbackProcessorFn callbackFn, const Options &options = Options() );

	virtual NodeRef getNode() const = 0;

	void setVolume( float volume );
	void setPan( float pos );

	float getVolume() const;
	float getPan() const;

  protected:
	Voice() : mBusId( 0 ) {}

  private:
	size_t mBusId;
	friend class MixerImpl;
};

class VoiceSamplePlayer : public Voice {
  public:

	NodeRef getNode() const override			{ return mNode; }
	SamplePlayerRef getSamplePlayer() const		{ return mNode; }

  protected:
	VoiceSamplePlayer( const SourceFileRef &sourceFile, const Options &options );
	SamplePlayerRef mNode;

	friend class Voice;
};

class VoiceCallbackProcessor : public Voice {
  public:
	NodeRef getNode() const override				{ return mNode; }

  protected:
	VoiceCallbackProcessor( const CallbackProcessorFn &callbackFn, const Options &options );

	CallbackProcessorRef mNode;
	friend class Voice;
};

// TODO: remove this in favor of Voice->play() ?
//	- or even Voice->start() / Voice->stop()
void play( const VoiceRef &voice );

} } // namespace cinder::audio2