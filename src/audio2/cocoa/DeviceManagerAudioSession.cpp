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

#include "cinder/cocoa/CinderCocoa.h"

#include <cmath>

#import <AVFoundation/AVAudioSession.h>

#if( __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_6_0 )

#include <AudioToolbox/AudioSession.h>	// still needed for IO buffer duration on pre iOS 6

@interface AudioSessionInterruptionHandlerImpl : NSObject <AVAudioSessionDelegate>

- (void)beginInterruption;
- (void)endInterruptionWithFlags:(NSUInteger)flags;

@end

#else

@interface AudioSessionInterruptionHandlerImpl : NSObject

- (void)notifyInterrupted:(NSNotification *)notification;

@end

#endif


using namespace std;

namespace cinder { namespace audio2 { namespace cocoa {

const string kRemoteIOKey = "iOS-RemoteIO";

namespace {

// TODO: remove this for now, assert until otherwise needed
void throwIfError( NSError *error, const std::string &when )
{
	if( error ) {
		string errorString = string( "AVAudioSession error, when: " ) + when + ", localized description: " + ci::cocoa::convertNsString( [error localizedDescription] );
		throw AudioDeviceExc( errorString );
	}
}

} // anonymous namespace

// ----------------------------------------------------------------------------------------------------
// MARK: - DeviceManagerAudioSession
// ----------------------------------------------------------------------------------------------------

DeviceManagerAudioSession::DeviceManagerAudioSession()
: DeviceManager(), mSessionIsActive( false ), mInputEnabled( false ), mSessionInterruptionHandler( nullptr )
{
	activateSession();
}

DeviceManagerAudioSession::~DeviceManagerAudioSession()
{
	if( mSessionInterruptionHandler )
		[mSessionInterruptionHandler release];
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

void DeviceManagerAudioSession::setInputEnabled( bool enable )
{
	NSString *category = AVAudioSessionCategoryAmbient;
	if( enable ) {
		LOG_V << "setting category to AVAudioSessionCategoryPlayAndRecord" << endl;
		category = AVAudioSessionCategoryPlayAndRecord;
	}

	NSError *error;
	[[AVAudioSession sharedInstance] setCategory:category error:&error];
	throwIfError( error, "setting category" );
}

std::string DeviceManagerAudioSession::getName( const DeviceRef &device )
{
	return kRemoteIOKey;
}

size_t DeviceManagerAudioSession::getNumInputChannels( const DeviceRef &device )
{
	if( ! isInputEnabled() )
		return 0;

#if( __IPHONE_OS_VERSION_MIN_REQUIRED >= __IPHONE_6_0 )
	NSInteger result = [[AVAudioSession sharedInstance] inputNumberOfChannels];
#else
	NSInteger result = [[AVAudioSession sharedInstance] currentHardwareInputNumberOfChannels];
#endif

	return static_cast<size_t>( result );
}

size_t DeviceManagerAudioSession::getNumOutputChannels( const DeviceRef &device )
{
#if( __IPHONE_OS_VERSION_MIN_REQUIRED >= __IPHONE_6_0 )
	NSInteger result = [[AVAudioSession sharedInstance] outputNumberOfChannels];
#else
	NSInteger result = [[AVAudioSession sharedInstance] currentHardwareOutputNumberOfChannels];
#endif

	return static_cast<size_t>( result );
}

size_t DeviceManagerAudioSession::getSampleRate( const DeviceRef &device )
{
#if( __IPHONE_OS_VERSION_MIN_REQUIRED >= __IPHONE_6_0 )
	double result = [[AVAudioSession sharedInstance] sampleRate];
#else
	double result = [[AVAudioSession sharedInstance] currentHardwareSampleRate];
#endif

	return static_cast<size_t>( result );
}

size_t DeviceManagerAudioSession::getFramesPerBlock( const DeviceRef &device )
{
#if( __IPHONE_OS_VERSION_MIN_REQUIRED >= __IPHONE_6_0 )
	double durationSeconds = [[AVAudioSession sharedInstance] IOBufferDuration];
#else
	Float32 durationSeconds;
	UInt32 durationSecondsSize = sizeof( durationSeconds );
	OSStatus status = ::AudioSessionGetProperty( kAudioSessionProperty_CurrentHardwareIOBufferDuration, &durationSecondsSize, &durationSeconds );
	CI_ASSERT( status == noErr );
#endif

	return std::lround( static_cast<Float32>( getSampleRate( device ) ) * durationSeconds );
}

void DeviceManagerAudioSession::setSampleRate( const DeviceRef &device, size_t sampleRate )
{
	NSError *error = nil;
#if( __IPHONE_OS_VERSION_MIN_REQUIRED >= __IPHONE_6_0 )
	BOOL didUpdate = [[AVAudioSession sharedInstance] setPreferredSampleRate:sampleRate error:&error];
#else
	BOOL didUpdate = [[AVAudioSession sharedInstance] setPreferredHardwareSampleRate:sampleRate error:&error];
#endif
	throwIfError( error, "setting samplerate" );

	if( ! didUpdate )
		throw AudioDeviceExc( "Failed to update samplerate." );

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
	AVAudioSession *globalSession = [AVAudioSession sharedInstance];

#if( __IPHONE_OS_VERSION_MIN_REQUIRED >= __IPHONE_6_0 )
	[[NSNotificationCenter defaultCenter] addObserver:getSessionInterruptionHandler() selector:@selector(notifyInterrupted:) name:AVAudioSessionInterruptionNotification object:nil];
#else
	globalSession.delegate = getSessionInterruptionHandler();
#endif

	NSError *error = nil;
	bool didActivate = [globalSession setActive:YES error:&error];
	throwIfError( error, "activating session" );
	if( ! didActivate )
		throw AudioDeviceExc( "Failed to activate global AVAudioSession." );

	mSessionIsActive = true;
}

string DeviceManagerAudioSession::getSessionCategory()
{
	NSString *category = [[AVAudioSession sharedInstance] category];
	return ci::cocoa::convertNsString( category );
}

AudioSessionInterruptionHandlerImpl *DeviceManagerAudioSession::getSessionInterruptionHandler()
{
	if( ! mSessionInterruptionHandler ) {
		mSessionInterruptionHandler = [AudioSessionInterruptionHandlerImpl new];
	}
	return mSessionInterruptionHandler;
}

} } } // namespace cinder::audio2::cocoa

// ----------------------------------------------------------------------------------------------------
// MARK: - AudioSessionInterruptionHandlerImpl
// ----------------------------------------------------------------------------------------------------

@implementation AudioSessionInterruptionHandlerImpl

#if( __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_6_0 )

- (void)beginInterruption
{
	LOG_V << "bang" << endl;
}

- (void)endInterruptionWithFlags:(NSUInteger)flags
{
	LOG_V << "bang" << endl;
}

#else // iOS 6+

- (void)notifyInterrupted:(NSNotification*)notification
{
	NSUInteger interruptionType = (NSUInteger)[[notification userInfo] objectForKey:AVAudioSessionInterruptionTypeKey];

	if( interruptionType == AVAudioSessionInterruptionTypeBegan )
		LOG_V << "interruption began" << endl;
	else
		LOG_V << "interruption ended" << endl;
}

#endif // iOS pre 6

@end