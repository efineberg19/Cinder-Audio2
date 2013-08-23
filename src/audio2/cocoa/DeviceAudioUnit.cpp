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

#include "audio2/cocoa/DeviceAudioUnit.h"
#include "audio2/cocoa/CinderCoreAudio.h"
#include "audio2/audio.h"
#include "audio2/CinderAssert.h"
#include "audio2/Debug.h"

using namespace std;
using namespace ci;

namespace cinder { namespace audio2 { namespace cocoa {

// ----------------------------------------------------------------------------------------------------
// MARK: - DeviceAudioUnit
// ----------------------------------------------------------------------------------------------------

DeviceAudioUnit::DeviceAudioUnit( const std::string &key, const ::AudioComponentDescription &component )
: Device( key ), mComponentDescription( component ), mComponentInstance( NULL ), mInputConnected( false ), mOutputConnected( false )
{
}

DeviceAudioUnit::~DeviceAudioUnit()
{
}

void DeviceAudioUnit::initialize()
{
	if( mInitialized ) {
		LOG_E << "already initialized." << endl;
		return;
	}

	UInt32 enableInput = static_cast<UInt32>( mInputConnected );
	::AudioUnitSetProperty( getComponentInstance(), kAudioOutputUnitProperty_EnableIO, kAudioUnitScope_Input, Bus::Input, &enableInput, sizeof( enableInput ) );

	UInt32 enableOutput = static_cast<UInt32>( mOutputConnected );
	::AudioUnitSetProperty( getComponentInstance(), kAudioOutputUnitProperty_EnableIO, kAudioUnitScope_Output, Bus::Output, &enableOutput, sizeof( enableOutput ) );

	DeviceManager::instance()->setActiveDevice( mKey );

	OSStatus status = ::AudioUnitInitialize( getComponentInstance() );
	CI_ASSERT( status == noErr );

	mInitialized = true;
	LOG_V << "complete." << endl;
}

void DeviceAudioUnit::uninitialize()
{
	if( ! mInitialized )
		return;

	if( mComponentInstance ) {
		OSStatus status = ::AudioUnitUninitialize( mComponentInstance );
		CI_ASSERT( status == noErr );
		status = ::AudioComponentInstanceDispose( mComponentInstance );
		CI_ASSERT( status == noErr );

		mComponentInstance = NULL;
	}
	mInitialized = mInputConnected = mOutputConnected = false;

	LOG_V << "complete." << endl;
}

void DeviceAudioUnit::start()
{
	if( ! mInitialized || mEnabled ) {
		LOG_E << boolalpha << "(returning) mInitialized: " << mInitialized << ", mEnabled: " << mEnabled << dec << endl;
		return;
	}

	mEnabled = true;
	OSStatus status = ::AudioOutputUnitStart( mComponentInstance );
	CI_ASSERT( status == noErr );

	LOG_V << "started" << endl;
}

void DeviceAudioUnit::stop()
{
	if( ! mInitialized || ! mEnabled ) {
		LOG_E << boolalpha << "(returning) mInitialized: " << mInitialized << ", mEnabled: " << mEnabled << dec << endl;
		return;
	}

	mEnabled = false;
	OSStatus status = ::AudioOutputUnitStop( mComponentInstance );
	CI_ASSERT( status == noErr );

	LOG_V << "stopped" << endl;
}

const ::AudioComponentInstance& DeviceAudioUnit::getComponentInstance()
{
	if( ! mComponentInstance )
		cocoa::findAndCreateAudioComponent( mComponentDescription, &mComponentInstance );
	
	return mComponentInstance;
}

} } } // namespace cinder::audio2::cocoa