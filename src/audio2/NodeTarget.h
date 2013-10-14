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

#include "audio2/Node.h"

namespace cinder { namespace audio2 {

typedef std::shared_ptr<class NodeTarget>		NodeTargetRef;
typedef std::shared_ptr<class NodeLineOut>		NodeLineOutRef;

class NodeTarget : public Node {
  public:
	virtual ~NodeTarget() {}

	virtual size_t getSampleRate()				= 0;
	virtual size_t getFramesPerBlock()			= 0;

	//! Returns the total number of frames that have already been processed in the dsp loop.
	virtual uint64_t getNumProcessedFrames()	= 0;

  protected:
	NodeTarget( const Format &format = Format() ) : Node( format ) {}

  private:
	// NodeTarget does not have outputs, overridden to assert this method isn't called
	const NodeRef& connect( const NodeRef &dest, size_t outputBus, size_t inputBus ) override;
};

class NodeLineOut : public NodeTarget {
  public:
	virtual ~NodeLineOut() {}

	const DeviceRef& getDevice() const		{ return mDevice; }

	size_t getSampleRate() override			{ return getDevice()->getSampleRate(); }
	size_t getFramesPerBlock() override		{ return getDevice()->getFramesPerBlock(); }

	//! Enables clip detection, so that values over \a threshold will be interpreted as a clip (enabled by default). \note Implementations may silence the buffer to prevent speaker damage.
	void enableClipDetection( bool enable = true, float threshold = 2.0f );
	//! Returns the frame of the last buffer clip or 0 if none since the last time this method was called.
	virtual uint64_t getLastClip() = 0;

	virtual void deviceParamsWillChange();
	virtual void deviceParamsDidChange();

  protected:
	NodeLineOut( const DeviceRef &device, const Format &format = Format() );

	DeviceRef mDevice;

	bool	mWasEnabledBeforeParamsChange, mClipDetectionEnabled;
	float	mClipThreshold;
};

} } // namespace cinder::audio2