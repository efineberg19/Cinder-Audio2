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

SourceFileCoreAudio::SourceFileCoreAudio( ci::DataSourceRef dataSource, size_t outputNumChannels, size_t outputSampleRate )
: SourceFile( dataSource, outputNumChannels, outputSampleRate )
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

    mNumChannels = fileFormat.mChannelsPerFrame;
	mSampleRate = fileFormat.mSampleRate;

    SInt64 numFrames;
    propSize = sizeof( numFrames );
    status = ExtAudioFileGetProperty( audioFile, kExtAudioFileProperty_FileLengthFrames, &propSize, &numFrames );
	CI_ASSERT( status == noErr );
	mNumFrames = static_cast<size_t>( numFrames );

	if( ! mOutputNumChannels )
		mOutputNumChannels = mNumChannels;
	if( ! mOutputSampleRate )
		mOutputSampleRate = mSampleRate;


	AudioStreamBasicDescription outputFormat = audio2::cocoa::nonInterleavedFloatABSD( mOutputNumChannels, mOutputSampleRate );

    LOG_V << "file format:\n";
	audio2::cocoa::printASBD( fileFormat );
    LOG_V << "output format:\n";
	audio2::cocoa::printASBD( outputFormat );
    LOG_V << "number of frames: " << numFrames << endl;

	status = ::ExtAudioFileSetProperty( audioFile, kExtAudioFileProperty_ClientDataFormat, sizeof( outputFormat ), &outputFormat );
	CI_ASSERT( status == noErr );

}

BufferRef SourceFileCoreAudio::loadBuffer()
{
	BufferRef result( new Buffer( mOutputNumChannels, mNumFrames ) );
	audio2::cocoa::AudioBufferListRef bufferList = audio2::cocoa::createNonInterleavedBufferList( mOutputNumChannels, mNumFramesPerRead ); // TODO: make this an ivar

	size_t currReadPos = 0;
	while( true ) {
		size_t framesLeft = mNumFrames - currReadPos;
		if( framesLeft <= 0 )
			break;

		UInt32 frameCount = std::min( framesLeft, mNumFramesPerRead );
        for( int i = 0; i < mOutputNumChannels; i++ ) {
            bufferList->mBuffers[i].mDataByteSize = frameCount * sizeof( float );
            bufferList->mBuffers[i].mData = &result->getChannel( i )[currReadPos];
        }

		// read from the extaudiofile
		OSStatus status = ::ExtAudioFileRead( mExtAudioFile.get(), &frameCount, bufferList.get() );
		CI_ASSERT( status == noErr );

        currReadPos += frameCount;
	}
	return result;
}

// ----------------------------------------------------------------------------------------------------
// MARK: - FileInputNodeCoreAudio
// ----------------------------------------------------------------------------------------------------

FileInputNodeCoreAudio::FileInputNodeCoreAudio( ci::DataSourceRef dataSource )
: FileInputNode(), mSourceFile( new SourceFileCoreAudio( dataSource ) )
{
}


void FileInputNodeCoreAudio::process( Buffer *buffer )
{

}

} } // namespace audio2::cocoa
