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

#include "audio2/Context.h"
#include "audio2/GeneratorNode.h"
#include "audio2/EffectNode.h"
#include "audio2/RingBuffer.h"
#include "audio2/cocoa/CinderCoreAudio.h"

#include <AudioUnit/AudioUnit.h>

namespace audio2 { namespace cocoa {

class DeviceAudioUnit;

struct RenderCallbackContext {
	Node *currentNode;
	Buffer buffer;
};

class NodeAudioUnit {
  public:
	NodeAudioUnit() : mAudioUnit( nullptr ), mRenderBus( 0 ), mShouldUseGraphRenderCallback( true )	{}
	virtual ~NodeAudioUnit();
	virtual ::AudioUnit getAudioUnit() const	{ return mAudioUnit; }
	::AudioUnitScope getRenderBus() const	{ return mRenderBus; }

	bool shouldUseGraphRenderCallback() const	{ return mShouldUseGraphRenderCallback; }
  protected:
	::AudioUnit			mAudioUnit;
	::AudioUnitScope	mRenderBus;
	bool				mShouldUseGraphRenderCallback;
};

class LineOutAudioUnit : public LineOutNode, public NodeAudioUnit {
  public:
	LineOutAudioUnit( DeviceRef device, const Format &format = Format() );
	virtual ~LineOutAudioUnit() = default;

	void initialize() override;
	void uninitialize() override;

	void start() override;
	void stop() override;

	::AudioUnit getAudioUnit() const override;
	DeviceRef getDevice() override;

  private:
	std::shared_ptr<DeviceAudioUnit> mDevice;
};

class LineInAudioUnit : public LineInNode, public NodeAudioUnit {
  public:
	LineInAudioUnit( DeviceRef device, const Format &format = Format() );
	virtual ~LineInAudioUnit();

	void initialize() override;
	void uninitialize() override;

	void start() override;
	void stop() override;

	::AudioUnit getAudioUnit() const override;
	DeviceRef getDevice() override;

	void process( Buffer *buffer ) override;

  private:
	static OSStatus inputCallback( void *data, ::AudioUnitRenderActionFlags *flags, const ::AudioTimeStamp *timeStamp, UInt32 bus, UInt32 numFrames, ::AudioBufferList *bufferList );

	std::shared_ptr<DeviceAudioUnit> mDevice;
	std::unique_ptr<RingBuffer> mRingBuffer;
	AudioBufferListPtr mBufferList;
};

// TODO: when stopped / mEnabled = false; kAudioUnitProperty_BypassEffect should be used
class EffectAudioUnit : public EffectNode, public NodeAudioUnit {
  public:
	EffectAudioUnit( UInt32 subType, const Format &format = Format() );
	virtual ~EffectAudioUnit();

	void initialize() override;
	void uninitialize() override;

	void setParameter( ::AudioUnitParameterID param, float val );

  private:
	UInt32		mEffectSubType;
};

class MixerAudioUnit : public MixerNode, public NodeAudioUnit {
  public:
	MixerAudioUnit( const Format &format = Format() );
	virtual ~MixerAudioUnit();

	void initialize() override;
	void uninitialize() override;

	size_t getNumBusses() override;
	void setNumBusses( size_t count ) override;
	void setMaxNumBusses( size_t count );

	bool isBusEnabled( size_t bus ) override;
	void setBusEnabled( size_t bus, bool enabled = true ) override;
	void setBusVolume( size_t bus, float volume ) override;
	float getBusVolume( size_t bus ) override;
	void setBusPan( size_t bus, float pan ) override;
	float getBusPan( size_t bus ) override;

  private:
	void checkBusIsValid( size_t bus );
};

class ConverterAudioUnit : public Node, public NodeAudioUnit {
  public:
	ConverterAudioUnit( NodeRef source, NodeRef dest );
	virtual ~ConverterAudioUnit();

	void initialize() override;
	void uninitialize() override;

  private:
	size_t mSourceNumChannels;
	RenderCallbackContext mRenderContext;

	friend class ContextAudioUnit;
};

class ContextAudioUnit : public Context {
  public:
	virtual ~ContextAudioUnit();

	virtual ContextRef			createContext() override																{ return ContextRef( new ContextAudioUnit() ); }
	virtual LineOutNodeRef		createLineOut( DeviceRef device, const Node::Format &format = Node::Format() ) override	{ return LineOutNodeRef( new LineOutAudioUnit( device, format ) ); }
	virtual LineInNodeRef		createLineIn( DeviceRef device, const Node::Format &format = Node::Format() ) override	{ return LineInNodeRef( new LineInAudioUnit( device, format ) ); }
	virtual MixerNodeRef		createMixer( const Node::Format &format = Node::Format() ) override						{ return MixerNodeRef( new MixerAudioUnit( format ) ); }

	void initialize() override;
	void uninitialize() override;

  private:

	void connectRenderCallback( NodeRef node, RenderCallbackContext *context, ::AURenderCallback callback, bool recursive );

	static OSStatus renderCallback( void *data, ::AudioUnitRenderActionFlags *flags, const ::AudioTimeStamp *timeStamp, UInt32 busNumber, UInt32 numFrames, ::AudioBufferList *bufferList );
	static OSStatus renderCallbackRoot( void *data, ::AudioUnitRenderActionFlags *flags, const ::AudioTimeStamp *timeStamp, UInt32 busNumber, UInt32 numFrames, ::AudioBufferList *bufferList );
	static OSStatus renderCallbackConverter( void *data, ::AudioUnitRenderActionFlags *flags, const ::AudioTimeStamp *timeStamp, UInt32 busNumber, UInt32 numFrames, ::AudioBufferList *bufferList );

	// TODO: consider making these abstract methods in Context
	void initNode( NodeRef node );
	void uninitNode( NodeRef node );

	
	RenderCallbackContext mRenderContext;
};

} } // namespace audio2::cocoa