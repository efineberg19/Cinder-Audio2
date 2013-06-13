#include "audio2/cocoa/FileCoreAudio.h"
#include "audio2/cocoa/Util.h"
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
: SourceFile( dataSource, numChannels, sampleRate )
{
	printExtensions();

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

size_t SourceFileCoreAudio::read( BufferRef buffer, size_t readPosition )
{
	CI_ASSERT( buffer->getNumChannels() == mNumChannels );
	CI_ASSERT( buffer->getNumFrames() >= mNumFrames );

	UInt32 frameCount = static_cast<UInt32>( buffer->getNumFrames() );

	for( int i = 0; i < mNumChannels; i++ ) {
		mBufferList->mBuffers[i].mDataByteSize = frameCount * sizeof( float );
		mBufferList->mBuffers[i].mData = &buffer->getChannel( i )[readPosition];
	}

	// read from the extaudiofile
	OSStatus status = ::ExtAudioFileRead( mExtAudioFile.get(), &frameCount, mBufferList.get() );
	CI_ASSERT( status == noErr );


	return frameCount;
}

BufferRef SourceFileCoreAudio::loadBuffer()
{
	BufferRef result( new Buffer( mNumChannels, mNumFrames ) );

	size_t currReadPos = 0;
	while( currReadPos < mNumFrames ) {
		UInt32 frameCount = std::min( mNumFrames - currReadPos, mNumFramesPerRead );

        for( int i = 0; i < mNumChannels; i++ ) {
            mBufferList->mBuffers[i].mDataByteSize = frameCount * sizeof( float );
            mBufferList->mBuffers[i].mData = &result->getChannel( i )[currReadPos];
        }

		// read from the extaudiofile
		OSStatus status = ::ExtAudioFileRead( mExtAudioFile.get(), &frameCount, mBufferList.get() );
		CI_ASSERT( status == noErr );

        currReadPos += frameCount;
	}
	return result;
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
	::AudioStreamBasicDescription outputFormat = audio2::cocoa::nonInterleavedFloatABSD( mNumChannels, mSampleRate );
	OSStatus status = ::ExtAudioFileSetProperty( mExtAudioFile.get(), kExtAudioFileProperty_ClientDataFormat, sizeof( outputFormat ), &outputFormat );
	CI_ASSERT( status == noErr );

	mBufferList = audio2::cocoa::createNonInterleavedBufferList( mNumChannels, mNumFramesPerRead );
}

} } // namespace audio2::cocoa
