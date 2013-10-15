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
#include "audio2/NodeEffect.h"

#include <memory>

namespace cinder { namespace audio2 {

typedef std::shared_ptr<class Voice> VoiceRef;

class Voice {
  public:
	virtual NodeRef getNode() const = 0;

	void setVolume( float volume );
	void setPan( float pos );

  protected:
	Voice() : mBusId( 0 ) {}

  private:
	size_t mBusId;
	friend class Mixer;
};

class VoiceSamplePlayer : public Voice {
public:
	VoiceSamplePlayer( const DataSourceRef &dataSource );

	NodeRef getNode() const override				{ return mSamplePlayer; }
	NodeSamplePlayerRef getSamplePlayer() const		{ return mSamplePlayer; }
private:
	NodeSamplePlayerRef mSamplePlayer;
};


struct VoiceOptions {
	VoiceOptions() : mVolumeEnabled( true ), mPanEnabled( false ) {}

	VoiceOptions& enablePan( bool enable = true )	{ mPanEnabled = enable; return *this; }

	bool isVolumeEnabled() const	{ return mVolumeEnabled; }
	bool isPanEnabled() const		{ return mPanEnabled; }

private:
	bool mVolumeEnabled, mPanEnabled;
};

VoiceRef makeVoice( const DataSourceRef &dataSource, const VoiceOptions &options = VoiceOptions() );

void play( const VoiceRef &source );

//void play( const std::function<Buffer *, other params> &callback );

} } // namespace cinder::audio2