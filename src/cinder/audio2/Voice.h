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

#pragma once

#include "cinder/audio2/NodeInput.h"
#include "cinder/audio2/SamplePlayer.h"
#include "cinder/audio2/Source.h"

#include <memory>

namespace cinder { namespace audio2 {

typedef std::shared_ptr<class Voice> VoiceRef;
typedef std::shared_ptr<class VoiceSamplePlayer> VoiceSamplePlayerRef;

//! \brief Interface for performing high-level audio playback tasks.
//!
//! A Voice is an abstraction of something you can hear, and as such they send audio
//! to your computers output device. Currently supports file playback and processing audio with a callback function.
//! Each Voice has controls for setting the volume and pan of its signal, and supports
//! play(), pause(), and stop() functionality.
//!
//! Underneath, playback is managed by a Node, which can be retrieved via the virtual getNode() method to
//! perform more complex tasks.
//!
class Voice {
  public:
	//! Optional parameters passed into Voice::create() methods.
	struct Options {
		Options() : mChannels( 0 ), mMaxFramesForBufferPlayback( 96000 ) {}

		//! Sets the number of channels for the Voice.
		Options& channels( size_t ch )							{ mChannels = ch; return *this; }
		//! Sets the maximum number of frames acceptable for a VoiceSamplePlayer to use in-memory buffer playback via BufferPlayer (default = 96,000).
		//! If the file is larger than this, it will be streamed from disk using a FilePlayer.
		Options& maxFramesForBufferPlayback( size_t frames )	{ mMaxFramesForBufferPlayback = frames; return *this; }

		size_t			getChannels() const						{ return mChannels; }
		size_t			getMaxFramesForBufferPlayback() const	{ return mMaxFramesForBufferPlayback; }

	protected:
		size_t			mChannels, mMaxFramesForBufferPlayback;
	};

	//! Creates a Voice that manages sample playback of an audio file pointed at with \a sourceFile.
	static VoiceSamplePlayerRef create( const SourceFileRef &sourceFile, const Options &options = Options() );
	//! Creates a Voice that continuously calls \a callbackFn to process a Buffer of samples.
	static VoiceRef create( const CallbackProcessorFn &callbackFn, const Options &options = Options() );

	//! Starts the Voice. Does nothing if currently playing. \note In the case of a VoiceSamplePlayer and the sample has reached EOF, play() will start from the beginning.
	virtual void play();
	//! Pauses the Voice inits current state. play() will resume from here.
	virtual void pause();
	//! Stops the Voice, resetting its state to the same as when it was created.
	virtual void stop();
	//! Returns whether the Voice is currently playing or not.
	virtual bool isPlaying() const;
	//! Returns the underlining Node that is matching by this Voice. The Node type is determined by the Voice subclassed.
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

//! \brief Concrete Voice for sample playback.
//!
//! Depending on the size of the specified file, playback will either be done in-memory (with BufferPlayer)
//! or streaming (with FilePlayer). The maximum frames for in-memory playback can be specified with Voice::Options::maxFramesForBufferPlayback()
//! Create with Voice::create( const SourceFileRef &sourceFile, const Options &options )
class VoiceSamplePlayer : public Voice {
  public:

	NodeRef getNode() const override			{ return mNode; }
	SamplePlayerRef getSamplePlayer() const		{ return mNode; }

	virtual void play() override;
	virtual void stop() override;

  protected:
	VoiceSamplePlayer( const SourceFileRef &sourceFile, const Options &options );
	SamplePlayerRef mNode;

	friend class Voice;
};

//! Concrete Voice for processing audio with a callback function. \see CallbackProcessorFn
class VoiceCallbackProcessor : public Voice {
  public:
	NodeRef getNode() const override				{ return mNode; }

  protected:
	VoiceCallbackProcessor( const CallbackProcessorFn &callbackFn, const Options &options );

	CallbackProcessorRef mNode;
	friend class Voice;
};

} } // namespace cinder::audio2