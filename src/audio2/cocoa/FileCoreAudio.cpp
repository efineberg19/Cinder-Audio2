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

namespace cinder { namespace audio2 { namespace cocoa {

//static void printExtensions()
//{
//	::CFArrayRef extensionsCF;
//	UInt32 propSize = sizeof( extensionsCF );
//	OSStatus status = ::AudioFileGetGlobalInfo( kAudioFileGlobalInfo_AllExtensions, 0, NULL, &propSize, &extensionsCF );
//	CI_ASSERT( status == noErr );
//
//	CFIndex extCount = ::CFArrayGetCount( extensionsCF );
//	LOG_V << "extension count: " << extCount << endl;
//	vector<string> extensions;
//	for( CFIndex index = 0; index < extCount; ++index ) {
//		string ext = ci::cocoa::convertCfString( (CFStringRef)::CFArrayGetValueAtIndex( extensionsCF, index ) );
//		cout << ext << ", ";
//		extensions.push_back( ext );
//	}
//	std::cout << endl;
//
//	::CFRelease( extensionsCF );
//}

// ----------------------------------------------------------------------------------------------------
// MARK: - SourceFileCoreAudio
// ----------------------------------------------------------------------------------------------------

SourceFileCoreAudio::SourceFileCoreAudio( const DataSourceRef &dataSource, size_t numChannels, size_t sampleRate )
	: SourceFile( dataSource, numChannels, sampleRate ), mReadPos( 0 )
{
//	printExtensions();

	CI_ASSERT_MSG( dataSource->isFilePath(), "at present only data source type supported is file" ); // TODO: decide whether to just load url here.

	::CFURLRef sourceUrl = ci::cocoa::createCfUrl( Url( dataSource->getFilePath().string() ) );

	::ExtAudioFileRef audioFile;
	OSStatus status = ::ExtAudioFileOpenURL( sourceUrl, &audioFile );
	if( status != noErr )
		throw AudioFileExc( string( "could not open audio source file: " ) + dataSource->getFilePath().string(), (int32_t)status );

	::CFRelease( sourceUrl );
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

	UInt32 frameCount = (UInt32)std::min( mNumFrames - mReadPos, mNumFramesPerRead );
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
	
	BufferRef result( new Buffer( mNumFrames, mNumChannels ) );

	while( mReadPos < mNumFrames ) {
		UInt32 frameCount = (UInt32)std::min( mNumFrames - mReadPos, mNumFramesPerRead );

        for( int ch = 0; ch < mNumChannels; ch++ ) {
            mBufferList->mBuffers[ch].mDataByteSize = frameCount * sizeof( float );
            mBufferList->mBuffers[ch].mData = &result->getChannel( ch )[mReadPos];
        }

		OSStatus status = ::ExtAudioFileRead( mExtAudioFile.get(), &frameCount, mBufferList.get() );
		CI_ASSERT( status == noErr );

        mReadPos += frameCount;
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
	mBufferList = createNonInterleavedBufferListShallow( mNumChannels );
}

// ----------------------------------------------------------------------------------------------------
// MARK: - TargetFileCoreAudio
// ----------------------------------------------------------------------------------------------------

TargetFileCoreAudio::TargetFileCoreAudio( const DataTargetRef &dataTarget, size_t sampleRate, size_t numChannels, const std::string &extension )
	: TargetFile( dataTarget, sampleRate, numChannels )
{
	::CFURLRef targetUrl = ci::cocoa::createCfUrl( Url( dataTarget->getFilePath().string() ) );
	::AudioFileTypeID fileType = getFileTypeIdFromExtension( extension );
	::AudioStreamBasicDescription fileAsbd = createFloatAsbd( mNumChannels, mSampleRate, true );
	::AudioStreamBasicDescription clientAsbd = createFloatAsbd( mNumChannels, mSampleRate, false );

	::ExtAudioFileRef audioFile;
	OSStatus status = ::ExtAudioFileCreateWithURL( targetUrl, fileType, &fileAsbd, nullptr, kAudioFileFlags_EraseFile, &audioFile );
	if( status != noErr )
		throw AudioFileExc( string( "could not open audio target file: " ) + dataTarget->getFilePath().string(), (int32_t)status );

	::CFRelease( targetUrl );
	mExtAudioFile = shared_ptr<::OpaqueExtAudioFile>( audioFile, ::ExtAudioFileDispose );

	status = ::ExtAudioFileSetProperty( mExtAudioFile.get(), kExtAudioFileProperty_ClientDataFormat, sizeof( clientAsbd ), &clientAsbd );
	CI_ASSERT( status == noErr );

	mBufferList = createNonInterleavedBufferListShallow( mNumChannels );
}

void TargetFileCoreAudio::write( const Buffer *buffer, size_t frameOffset, size_t numFrames )
{
	if( ! numFrames )
		numFrames = buffer->getNumFrames();

	CI_ASSERT( frameOffset + numFrames <= buffer->getNumFrames() );

	for( int ch = 0; ch < mNumChannels; ch++ ) {
		mBufferList->mBuffers[ch].mDataByteSize = (UInt32)numFrames * sizeof( float );
		mBufferList->mBuffers[ch].mData = (void *)( buffer->getChannel( ch ) + frameOffset );
	}

	OSStatus status = ::ExtAudioFileWrite( mExtAudioFile.get(), (UInt32)numFrames, mBufferList.get() );
	CI_ASSERT( status == noErr );
}

// TODO: this doesn't map so well. Better to provide an enum of all known formats?
::AudioFileTypeID TargetFileCoreAudio::getFileTypeIdFromExtension( const std::string &ext )
{
	if( ext == "aiff" )
		return kAudioFileAIFFType;
	else if( ext == "wav" )
		return kAudioFileWAVEType;
	else if( ext == "mp3" )
		return kAudioFileMP3Type;
	else if( ext == "m4a" )
		return kAudioFileM4AType;
	else if( ext == "aac" )
		return kAudioFileAAC_ADTSType;

	throw AudioFileExc( string( "unexpected extension: " ) + ext );
}


} } } // namespace cinder::audio2::cocoa
