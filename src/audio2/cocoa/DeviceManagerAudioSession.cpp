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

#include "audio2/cocoa/DeviceManagerAudioSession.h"
#include "audio2/CinderAssert.h"
#include "audio2/Debug.h"

#include <cmath>

#include <AudioToolbox/AudioToolbox.h>

using namespace std;

namespace cinder { namespace audio2 { namespace cocoa {

const string kRemoteIOKey = "iOS-RemoteIO";

template <typename ResultT>
inline void audioSessionProperty( ::AudioSessionPropertyID property, ResultT &result )
{
	UInt32 resultSize = sizeof( result );
	OSStatus status = ::AudioSessionGetProperty( property, &resultSize, &result );
	CI_ASSERT( status == noErr );
}

// ----------------------------------------------------------------------------------------------------
// MARK: - DeviceManagerAudioSession
// ----------------------------------------------------------------------------------------------------

DeviceManagerAudioSession::DeviceManagerAudioSession()
: DeviceManager(), mSessionIsActive( false ), mInputEnabled( false )
{
	// TODO: install interrupt listener
	OSStatus status = ::AudioSessionInitialize( NULL, NULL, NULL, NULL );
	CI_ASSERT( status == noErr );

	activateSession();
}

DeviceRef DeviceManagerAudioSession::getDefaultOutput()
{
	return static_pointer_cast<Device>( getRemoteIODevice() );
}

DeviceRef DeviceManagerAudioSession::getDefaultInput()
{
	return getDefaultOutput();
}

DeviceRef DeviceManagerAudioSession::findDeviceByName( const std::string &name )
{
	return getDefaultOutput();
}

DeviceRef DeviceManagerAudioSession::findDeviceByKey( const std::string &key )
{
	return getDefaultOutput();
}

const std::vector<DeviceRef>& DeviceManagerAudioSession::getDevices()
{
	if( mDevices.empty() )
		mDevices.push_back( getDefaultOutput() );
	
	return mDevices;
}

void DeviceManagerAudioSession::setInputEnabled( bool b )
{
	UInt32 category = kAudioSessionCategory_AmbientSound;
	if( b ) {
		LOG_V << "setting category to kAudioSessionCategory_PlayAndRecord" << endl;
		category = kAudioSessionCategory_PlayAndRecord;
	}
	OSStatus status = ::AudioSessionSetProperty( kAudioSessionProperty_AudioCategory, sizeof( category ), &category );
	CI_ASSERT( status == noErr );
}

std::string DeviceManagerAudioSession::getName( const DeviceRef &device )
{
	return kRemoteIOKey;
}

size_t DeviceManagerAudioSession::getNumInputChannels( const DeviceRef &device )
{
	if( ! isInputEnabled() ) {
//		LOG_V << "Warning: input is disabled due to session category, so no inputs." << endl;
		return 0;
	}
	
	UInt32 result;
	audioSessionProperty( kAudioSessionProperty_CurrentHardwareInputNumberChannels, result );
	return static_cast<size_t>( result );
}

size_t DeviceManagerAudioSession::getNumOutputChannels( const DeviceRef &device )
{
	UInt32 result;
	audioSessionProperty( kAudioSessionProperty_CurrentHardwareOutputNumberChannels, result );
	return static_cast<size_t>( result );
}

size_t DeviceManagerAudioSession::getSampleRate( const DeviceRef &device )
{
	Float64 result;
	audioSessionProperty( kAudioSessionProperty_CurrentHardwareSampleRate, result );
	return static_cast<size_t>( result );
}

size_t DeviceManagerAudioSession::getFramesPerBlock( const DeviceRef &device )
{
	Float32 durationSeconds;
	audioSessionProperty( kAudioSessionProperty_CurrentHardwareIOBufferDuration, durationSeconds );
	return std::lround( static_cast<Float32>( getSampleRate( device ) ) * durationSeconds );
}

void DeviceManagerAudioSession::setSampleRate( const DeviceRef &device, size_t sampleRate )
{
	// TODO
}

void DeviceManagerAudioSession::setFramesPerBlock( const DeviceRef &device, size_t framesPerBlock )
{
	// TODO
}

// ----------------------------------------------------------------------------------------------------
// MARK: - Private
// ----------------------------------------------------------------------------------------------------

const DeviceRef& DeviceManagerAudioSession::getRemoteIODevice()
{
	if( ! mRemoteIODevice )
		mRemoteIODevice = addDevice( kRemoteIOKey );

	return mRemoteIODevice;
}

void DeviceManagerAudioSession::activateSession()
{
	OSStatus status = ::AudioSessionSetActive( true );
	CI_ASSERT( status == noErr );

	mSessionIsActive = true;
}

uint32_t DeviceManagerAudioSession::getSessionCategory()
{
	UInt32 result;
	audioSessionProperty( kAudioSessionProperty_AudioCategory, result );
	return static_cast<uint32_t>( result );
}

} } } // namespace cinder::audio2::cocoa