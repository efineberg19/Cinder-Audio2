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

#include "cinder/audio2/NodeTarget.h"
#include "cinder/audio2/Context.h"
#include "cinder/audio2/Exception.h"
#include "cinder/audio2/Debug.h"

using namespace std;

namespace cinder { namespace audio2 {

const NodeRef& NodeTarget::connect( const NodeRef &dest, size_t outputBus, size_t inputBus )
{
	CI_ASSERT_MSG( 0, "NodeTarget does not support outputs" );
	return dest;
}

LineOut::LineOut( const DeviceRef &device, const Format &format )
	: NodeTarget( format ), mDevice( device ), mClipDetectionEnabled( true ), mClipThreshold( 2.0f )
{
	CI_ASSERT( mDevice );

	mWillChangeConn = mDevice->getSignalParamsWillChange().connect( bind( &LineOut::deviceParamsWillChange, this ) );
	mDidChangeConn = mDevice->getSignalParamsDidChange().connect( bind( &LineOut::deviceParamsDidChange, this ) );

	if( mChannelMode != ChannelMode::SPECIFIED ) {
		mChannelMode = ChannelMode::SPECIFIED;
		setNumChannels( 2 );
	}

	if( mDevice->getNumOutputChannels() < mNumChannels )
		throw AudioFormatExc( "Device can not accommodate specified number of channels." );
}

void LineOut::deviceParamsWillChange()
{
	LOG_V( "bang" );
	mWasEnabledBeforeParamsChange = mEnabled;

	getContext()->stop();
	getContext()->uninitializeAllNodes();
}

void LineOut::deviceParamsDidChange()
{
	LOG_V( "bang" );
	getContext()->initializeAllNodes();

	getContext()->setEnabled( mWasEnabledBeforeParamsChange );
}

void LineOut::enableClipDetection( bool enable, float threshold )
{
	lock_guard<mutex> lock( getContext()->getMutex() );
	mClipDetectionEnabled = enable;
	mClipThreshold = threshold;
}

} } // namespace cinder::audio2