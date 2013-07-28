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

#include "audio2/cocoa/FileCoreAudio.h"
#include "audio2/cocoa/CinderCoreAudio.h"
#include "audio2/audio.h"
#include "audio2/Debug.h"

#include "cinder/cocoa/CinderCocoa.h"

#include <AudioToolbox/AudioFile.h>

using namespace std;
using namespace ci;

namespace audio2 { namespace cocoa {

static void printExtensions()
{
	::CFArrayRef extensionsCF;
	UInt32 propSize = sizeof( extensionsCF );
	OSStatus status = ::AudioFileGetGlobalInfo( kAudioFileGlobalInfo_AllExtensions, 0, NULL, &propSize, &extensionsCF );
	CI_ASSERT( status == noErr );

	CFIndex extCount = ::CFArrayGetCount( extensionsCF );
	LOG_V << "extension count: " << extCount << endl;
	vector<string> extensions;
	for( CFIndex index = 0; index < extCount; ++index ) {
		string ext = ci::cocoa::convertCfString( (CFStringRef)::CFArrayGetValueAtIndex( extensionsCF, index ) );
		cout << ext << ", ";
		extensions.push_back( ext );
	}
	std::cout << endl;

	::CFRelease( extensionsCF );
}

// ----------------------------------------------------------------------------------------------------
// MARK: - SourceFileCoreAudio
// ----------------------------------------------------------------------------------------------------

SourceFileCoreAudio::SourceFileCoreAudio( ci::DataSourceRef dataSource, size_t numChannels, size_t sampleRate )
: SourceFile( dataSource, numChannels, sampleRate ), mReadPos( 0 )
{
//	printExtensions();

	::CFURLRef audioFileUrl = ci::cocoa::createCfUrl( Url( dataSource->getFilePath().string() ) );

	::ExtAudioFileRef audioFile;
	OSStatus status = ::ExtAudioFileOpenURL( audioFileUrl, &audioFile );
	if( status != noErr )
		throw AudioFileExc( string( "could not open audio file: " ) + dataSource->getFilePath().string() );

	::CFRelease( audioFileUrl );
	mExtAudioFile = shared_ptr<::OpaqueExtAudioFile>( audioFile, ::ExtAudioFileDispose );

	::AudioStreamBasicDescription fileFormat;
	UInt32 propSize = sizeof( fileFormat );
    status = ::ExtAudioFileGetProperty( audioFile, kExtAudioFileProperty_FileDataFormat, &propSize, &fileFormat );
	CI_ASSERT( status == noErr );

    mFileNumChannels = fileFormat.mChannelsPerFrame;
	mFileSampleRate = fileFormat.mSampleRate;

    SInt64 numFrames;
    propSize = sizeof( numFrames );
    status = ::ExtAudioFileGetProperty( audioFile, kExtAudioFileProperty_FileLengthFrames, &propSize, &numFrames );
	CI_ASSERT( status == noErr );
	mNumFrames = static_cast<size_t>( numFrames );

	if( ! mNumChannels )
		mNumChannels = mFileNumChannels;
	if( ! mSampleRate )
		mSampleRate = mFileSampleRate;

	updateOutputFormat();
}

size_t SourceFileCoreAudio::read( Buffer *buffer )
{
	CI_ASSERT( buffer->getNumChannels() == mNumChannels );
	CI_ASSERT( buffer->getNumFrames() >= mNumFramesPerRead );

	if( mReadPos >= mNumFrames )
		return 0;

	UInt32 frameCount = std::min( mNumFrames - mReadPos, mNumFramesPerRead );
	for( int i = 0; i < mNumChannels; i++ ) {
		mBufferList->mBuffers[i].mDataByteSize = frameCount * sizeof( float );
		mBufferList->mBuffers[i].mData = &buffer->getChannel( i )[0];
	}

	OSStatus status = ::ExtAudioFileRead( mExtAudioFile.get(), &frameCount, mBufferList.get() );
	CI_ASSERT( status == noErr );

	mReadPos += frameCount;
	return frameCount;
}

BufferRef SourceFileCoreAudio::loadBuffer()
{
	if( mReadPos != 0 )
		seek( 0 );
	
	BufferRef result( new Buffer( mNumChannels, mNumFrames ) );

	size_t currReadPos = 0;
	while( currReadPos < mNumFrames ) {
		UInt32 frameCount = std::min( mNumFrames - currReadPos, mNumFramesPerRead );

        for( int i = 0; i < mNumChannels; i++ ) {
            mBufferList->mBuffers[i].mDataByteSize = frameCount * sizeof( float );
            mBufferList->mBuffers[i].mData = &result->getChannel( i )[currReadPos];
        }

		OSStatus status = ::ExtAudioFileRead( mExtAudioFile.get(), &frameCount, mBufferList.get() );
		CI_ASSERT( status == noErr );

        currReadPos += frameCount;
	}
	return result;
}

void SourceFileCoreAudio::seek( size_t readPosition )
{
	if( readPosition >= mNumFrames )
		return;

	OSStatus status = ::ExtAudioFileSeek( mExtAudioFile.get(), readPosition );
	CI_ASSERT( status == noErr );

	mReadPos = readPosition;
}

void SourceFileCoreAudio::setSampleRate( size_t sampleRate )
{
	mSampleRate = sampleRate;
	updateOutputFormat();
}

void SourceFileCoreAudio::setNumChannels( size_t numChannels )
{
	mNumChannels = numChannels;
	updateOutputFormat();
}

void SourceFileCoreAudio::updateOutputFormat()
{
	::AudioStreamBasicDescription outputFormat = audio2::cocoa::createFloatAsbd( mNumChannels, mSampleRate );
	OSStatus status = ::ExtAudioFileSetProperty( mExtAudioFile.get(), kExtAudioFileProperty_ClientDataFormat, sizeof( outputFormat ), &outputFormat );
	CI_ASSERT( status == noErr );

	// numFrames will be updated at read time
	mBufferList = audio2::cocoa::createNonInterleavedBufferListShallow( mNumChannels );
}

} } // namespace audio2::cocoa
