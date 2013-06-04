#include "cinder/app/AppNative.h"
#include "cinder/gl/gl.h"
#include "cinder/cocoa/CinderCocoa.h"

#include "audio2/GraphAudioUnit.h"
#include "audio2/Engine.h"
#include "audio2/GeneratorNode.h"
#include "audio2/Debug.h"
#include "audio2/cocoa/Util.h"
#include "audio2/Plot.h"

// TODO NEXT: use converter to load signed int wav and mp3
// TODO: load stereo files

// before moving on, compare load times with this and Extended Audio File services

// TODO finally: play file with custom GeneratorNode
// - do this in a new test app

using namespace ci;
using namespace ci::app;
using namespace std;

using namespace audio2;

class CAFileLoadingTestApp : public AppNative {
  public:
	void prepareSettings( Settings *settings );
	void setup();
	void mouseDown( MouseEvent event );	
	void update();
	void draw();

	GraphRef mGraph;

	vector<float> mSamples;

	WaveformPlot mWaveformPlot;
};

void CAFileLoadingTestApp::prepareSettings( Settings *settings )
{
    settings->setWindowSize( 1000, 500 );
}

OSStatus converterCallback(	AudioConverterRef audioConverter, UInt32 *ioNumberDataPackets, AudioBufferList *ioData, AudioStreamPacketDescription **outDataPacketDescription, void *inUserData );

struct ConverterInfo {
	size_t readIndex;
	AudioFileID inputFile;
	vector<float> readBuffer;
};

void CAFileLoadingTestApp::setup()
{
	mGraph = Engine::instance()->createGraph();
	OutputNodeRef output = Engine::instance()->createOutput();
	mGraph->setRoot( output );


	DataSourceRef dataSource = loadResource( "tone440_float.wav" );

	CFURLRef audioFileUrl = ci::cocoa::createCfUrl( Url( dataSource->getFilePath().string() ) ); // FIXME: broken for .wma sample (check again)
//	CFStringRef pathString = cocoa::createCfString( sample->getFilePath().string() );
//  CFURLRef urlRef = CFURLCreateWithFileSystemPath( kCFAllocatorDefault, pathString, kCFURLPOSIXPathStyle, false );

	LOG_V << "opening audio file: " << dataSource->getFilePath() << endl;

	AudioFileID audioFile;
	OSStatus status = AudioFileOpenURL( audioFileUrl, kAudioFileReadPermission, 0, &audioFile );
	CI_ASSERT( status == noErr );

	CFRelease( audioFileUrl );

	AudioStreamBasicDescription inputAbsd;
	UInt32 absdSize = sizeof( inputAbsd );

    status = AudioFileGetProperty( audioFile, kAudioFilePropertyDataFormat, &absdSize, &inputAbsd );
	CI_ASSERT( status == noErr );

	LOG_V << "input ABSD: " << endl;
	audio2::cocoa::printASBD( inputAbsd );

	size_t numChannels = inputAbsd.mChannelsPerFrame;

	UInt64 packetCount;
	UInt32 packetCountSize = sizeof( packetCount );

    status = AudioFileGetProperty( audioFile, kAudioFilePropertyAudioDataPacketCount, &packetCountSize, &packetCount );
	CI_ASSERT( status == noErr );

	LOG_V << "packet count: " << packetCount << endl;


//	AudioStreamBasicDescription outputAbsd = audio2::cocoa::nonInterleavedFloatABSD( 2, output->getDevice()->getSampleRate() );

	const size_t kBytesPerSample = sizeof( float );

	AudioStreamBasicDescription outputAbsd = { 0 };
	outputAbsd.mSampleRate = 44100.0;
	outputAbsd.mFormatID = kAudioFormatLinearPCM;
    outputAbsd.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagsNativeEndian;
	outputAbsd.mBytesPerPacket = kBytesPerSample * numChannels;
	outputAbsd.mFramesPerPacket = 1;
	outputAbsd.mBytesPerFrame = kBytesPerSample * numChannels;
	outputAbsd.mChannelsPerFrame = numChannels;
	outputAbsd.mBitsPerChannel = 8 * kBytesPerSample;

	LOG_V << "output ABSD: " << endl;
	audio2::cocoa::printASBD( outputAbsd );

	AudioConverterRef audioConverter;
	status = AudioConverterNew( &inputAbsd, &outputAbsd, &audioConverter );
	CI_ASSERT( status == noErr );

	// allocate packet descriptions if the input file is VBR
	UInt32 packetsPerBuffer = 0; // TODO: rename to framesPerBuffer and all other things that contain packet
	UInt32 outputBufferSize = 32 * 1024; // 32 KB is a good starting point. TODO: understand what this is for.
	UInt32 sizePerPacket = inputAbsd.mBytesPerPacket;
	AudioStreamPacketDescription *inputFilePacketDescriptions = NULL;
	if( sizePerPacket == 0 )	{
		UInt32 size = sizeof(sizePerPacket);
        status = AudioConverterGetProperty( audioConverter, kAudioConverterPropertyMaximumOutputPacketSize, &size, &sizePerPacket );
		CI_ASSERT( status == noErr );


        // make sure the buffer is large enough to hold at least one packet
		if (sizePerPacket > outputBufferSize)
			outputBufferSize = sizePerPacket;

		packetsPerBuffer = outputBufferSize / sizePerPacket;
		inputFilePacketDescriptions = (AudioStreamPacketDescription *)malloc(sizeof(AudioStreamPacketDescription) * packetsPerBuffer);
	}
	else {
		packetsPerBuffer = outputBufferSize / sizePerPacket;
	}

	mSamples.resize( packetCount );
//    for( int i = 0; i < numChannels; i++ ) {
//		// added outputBufferSize for the last frame, CoreAudio expects that the entire size of the buffer is valid even if it isn't going to write to it.
//        mSamples[i].resize( packetCount + packetsPerBuffer );
//    }

	ConverterInfo converterInfo = { 0 };
	converterInfo.inputFile = audioFile;
	converterInfo.readBuffer.resize( packetsPerBuffer );

//	UInt32 outByteCount;
//	UInt32 readBlockSize = 1000;

	// TODO: had to make the sample buffer a little bigger here for the last frame, make sure to trim. Also check if still necessary.
//	audio2::cocoa::AudioBufferListRef bufferList = audio2::cocoa::createNonInterleavedBufferList( numChannels, packetsPerBuffer );

	LOG_V << "reading..." << endl;
	while( true ) {

//		for( int i = 0; i < mSamples.size(); i++ ) {
//            bufferList->mBuffers[i].mData = &mSamples[i][converterInfo.readIndex];
//        }

		AudioBufferList bufferList;
		bufferList.mNumberBuffers = 1;
		bufferList.mBuffers[0].mNumberChannels = numChannels;
		bufferList.mBuffers[0].mDataByteSize = outputBufferSize;
		bufferList.mBuffers[0].mData = &mSamples[0]; // FIXME: mSamples is not the right format
		


		UInt32 ioOutputDataPackets = packetsPerBuffer;
		status = AudioConverterFillComplexBuffer( audioConverter, converterCallback, &converterInfo, &ioOutputDataPackets, &bufferList, inputFilePacketDescriptions );
		CI_ASSERT( status == noErr );

		if( ! ioOutputDataPackets )
			break;		
	}
	
	LOG_V << "read finished, readIndex: " << converterInfo.readIndex << endl;

	LOG_V << "plotting..." << endl;
	mWaveformPlot.load( mSamples, getWindowBounds() );
	LOG_V << "plot finished" << endl;


	status = AudioConverterDispose( audioConverter );
	CI_ASSERT( status == noErr );

	free( inputFilePacketDescriptions );
}


OSStatus converterCallback(	AudioConverterRef audioConverter, UInt32 *ioNumberDataPackets, AudioBufferList *ioData, AudioStreamPacketDescription **outDataPacketDescription, void *inUserData )
{
	ConverterInfo *converterInfo = (ConverterInfo *)ioData;

	UInt32 outByteCount = 0;
	UInt32 readBlockSize = *ioNumberDataPackets;
	OSStatus status = AudioFileReadPacketData( converterInfo->inputFile, false, &outByteCount, NULL, converterInfo->readIndex, &readBlockSize, converterInfo->readBuffer.data() );
	if( status == kAudioFileEndOfFileError ) {
		LOG_V << "kAudioFileEndOfFileError, break" << endl;
		converterInfo->readIndex += readBlockSize;
		return noErr; // ???: return status?
	} else
		CI_ASSERT( status == noErr );
	converterInfo->readIndex += readBlockSize;

	return noErr;
}

void CAFileLoadingTestApp::mouseDown( MouseEvent event )
{
}

void CAFileLoadingTestApp::update()
{
}

void CAFileLoadingTestApp::draw()
{
	gl::clear();

	mWaveformPlot.drawGl();

}

CINDER_APP_NATIVE( CAFileLoadingTestApp, RendererGl )
