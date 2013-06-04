#include "cinder/app/AppNative.h"
#include "cinder/gl/gl.h"
#include "cinder/cocoa/CinderCocoa.h"

#include "audio2/GraphAudioUnit.h"
#include "audio2/Engine.h"
#include "audio2/GeneratorNode.h"
#include "audio2/Debug.h"
#include "audio2/cocoa/Util.h"
#include "audio2/Plot.h"

// TODO NEXT: load signed int wav

// TODO: load mp3
// TODO: load stereo files

// before moving on, compare load times with this and Extended Audio File services

// TODO finally: play file with custom GeneratorNode
// - do this in a new test app

//#define FILE_NAME "tone440.wav"
#define FILE_NAME "tone440_float.wav"

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

	void printErrorCodes();

	GraphRef mGraph;

	vector<float> mSamples;
	size_t mNumChannels;

	WaveformPlot mWaveformPlot;
};

void CAFileLoadingTestApp::prepareSettings( Settings *settings )
{
    settings->setWindowSize( 1000, 500 );
}

//OSStatus converterCallback(	AudioConverterRef audioConverter, UInt32 *ioNumberDataPackets, AudioBufferList *ioData, AudioStreamPacketDescription **outDataPacketDescription, void *inUserData );
OSStatus converterCallback(AudioConverterRef audioConverter, UInt32 *ioNumberDataPackets, AudioBufferList* ioData, AudioStreamPacketDescription **outDataPacketDescription, void *inUserData );

struct ConverterInfo {
	size_t readIndex;
	AudioFileID inputFile;
	vector<float> readBuffer;
	AudioStreamPacketDescription *inputFilePacketDescriptions;
};

void CAFileLoadingTestApp::setup()
{
	mGraph = Engine::instance()->createGraph();
	OutputNodeRef output = Engine::instance()->createOutput();
	mGraph->setRoot( output );

	DataSourceRef dataSource = loadResource( FILE_NAME );

	CFURLRef audioFileUrl = ci::cocoa::createCfUrl( Url( dataSource->getFilePath().string() ) ); // FIXME: broken for .wma sample (check again)
//	CFStringRef pathString = cocoa::createCfString( sample->getFilePath().string() );
//  CFURLRef urlRef = CFURLCreateWithFileSystemPath( kCFAllocatorDefault, pathString, kCFURLPOSIXPathStyle, false );

	ConverterInfo converterInfo = { 0 };
	converterInfo.readIndex = 0;
	converterInfo.inputFilePacketDescriptions = NULL;

	LOG_V << "opening audio file: " << dataSource->getFilePath() << endl;

	OSStatus status = AudioFileOpenURL( audioFileUrl, kAudioFileReadPermission, 0, &converterInfo.inputFile );
	CI_ASSERT( status == noErr );

	CFRelease( audioFileUrl );

	AudioStreamBasicDescription inputAbsd;
	UInt32 absdSize = sizeof( inputAbsd );

    status = AudioFileGetProperty( converterInfo.inputFile, kAudioFilePropertyDataFormat, &absdSize, &inputAbsd );
	CI_ASSERT( status == noErr );

	LOG_V << "input ABSD: " << endl;
	audio2::cocoa::printASBD( inputAbsd );

	mNumChannels = inputAbsd.mChannelsPerFrame;

	UInt64 packetCount;
	UInt32 packetCountSize = sizeof( packetCount );

    status = AudioFileGetProperty( converterInfo.inputFile, kAudioFilePropertyAudioDataPacketCount, &packetCountSize, &packetCount );
	CI_ASSERT( status == noErr );

	LOG_V << "packet count: " << packetCount << endl;


//	AudioStreamBasicDescription outputAbsd = audio2::cocoa::nonInterleavedFloatABSD( 2, output->getDevice()->getSampleRate() );

	const size_t kBytesPerSample = sizeof( float );

	AudioStreamBasicDescription outputAbsd = { 0 };
	outputAbsd.mSampleRate = 44100.0;
	outputAbsd.mFormatID = kAudioFormatLinearPCM;
    outputAbsd.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagsNativeEndian | kLinearPCMFormatFlagIsPacked;
	outputAbsd.mBytesPerPacket = kBytesPerSample * mNumChannels;
	outputAbsd.mFramesPerPacket = 1;
	outputAbsd.mBytesPerFrame = kBytesPerSample * mNumChannels;
	outputAbsd.mChannelsPerFrame = mNumChannels;
	outputAbsd.mBitsPerChannel = 8 * kBytesPerSample;

	LOG_V << "output ABSD: " << endl;
	audio2::cocoa::printASBD( outputAbsd );

	AudioConverterRef audioConverter;
	status = AudioConverterNew( &inputAbsd, &outputAbsd, &audioConverter );
	CI_ASSERT( status == noErr );

	// allocate packet descriptions if the input file is VBR
	UInt32 framesPerRead = 1000;
	UInt32 outputBufferSize = framesPerRead * sizeof( float );
	UInt32 sizePerFrame = inputAbsd.mBytesPerPacket;
	if( sizePerFrame == 0 )	{
		// TODO: check this with VBR / mp3 data
		
		UInt32 size = sizeof(sizePerFrame);
        status = AudioConverterGetProperty( audioConverter, kAudioConverterPropertyMaximumOutputPacketSize, &size, &sizePerFrame );
		CI_ASSERT( status == noErr );


        // make sure the buffer is large enough to hold at least one packet
		if (sizePerFrame > outputBufferSize)
			outputBufferSize = sizePerFrame;

		framesPerRead = outputBufferSize / sizePerFrame;
		converterInfo.inputFilePacketDescriptions = (AudioStreamPacketDescription *)malloc(sizeof(AudioStreamPacketDescription) * framesPerRead);
	}

	converterInfo.readBuffer.resize( framesPerRead );

	mSamples.resize( packetCount );
//    for( int i = 0; i < mNumChannels; i++ ) {
//		// added outputBufferSize for the last frame, CoreAudio expects that the entire size of the buffer is valid even if it isn't going to write to it.
//        mSamples[i].resize( packetCount + framesPerRead );
//    }

//	UInt32 outByteCount;
//	UInt32 readBlockSize = 1000;

	// TODO: had to make the sample buffer a little bigger here for the last frame, make sure to trim. Also check if still necessary.
//	audio2::cocoa::AudioBufferListRef bufferList = audio2::cocoa::createNonInterleavedBufferList( mNumChannels, framesPerRead );

	LOG_V << "reading..." << endl;
	while( converterInfo.readIndex < ( packetCount - 1 ) ) {

//		for( int i = 0; i < mSamples.size(); i++ ) {
//            bufferList->mBuffers[i].mData = &mSamples[i][converterInfo.readIndex];
//        }

		AudioBufferList bufferList;
		bufferList.mNumberBuffers = 1;
		bufferList.mBuffers[0].mNumberChannels = mNumChannels;
		bufferList.mBuffers[0].mDataByteSize = outputBufferSize;
		bufferList.mBuffers[0].mData = converterInfo.readBuffer.data();
		
		UInt32 ioOutputDataPackets = std::min( framesPerRead, (uint32_t)packetCount - converterInfo.readIndex );
		status = AudioConverterFillComplexBuffer( audioConverter, converterCallback, &converterInfo, &ioOutputDataPackets, &bufferList, converterInfo.inputFilePacketDescriptions );
		CI_ASSERT( status == noErr );

		if( ! ioOutputDataPackets )
			break;

		memcpy( &mSamples[converterInfo.readIndex], converterInfo.readBuffer.data(), ioOutputDataPackets * sizeof( float ) );

	}
	
	LOG_V << "read finished, readIndex: " << converterInfo.readIndex << endl;

	LOG_V << "plotting..." << endl;
	mWaveformPlot.load( mSamples, getWindowBounds() );
	LOG_V << "plot finished" << endl;


	status = AudioConverterDispose( audioConverter );
	CI_ASSERT( status == noErr );

	free( converterInfo.inputFilePacketDescriptions );
}


OSStatus converterCallback( AudioConverterRef audioConverter, UInt32 *ioNumberDataPackets, AudioBufferList* ioData, AudioStreamPacketDescription **outDataPacketDescription, void *inUserData )
{
	ConverterInfo *converterInfo = (ConverterInfo *)inUserData;

	UInt32 readBlockSize = *ioNumberDataPackets;
//	UInt32 readBlockSize = 100;
	UInt32 outByteCount = readBlockSize * sizeof( float );
	SInt64 inStartingPacket = converterInfo->readIndex;
	OSStatus status = AudioFileReadPacketData( converterInfo->inputFile, true, &outByteCount, converterInfo->inputFilePacketDescriptions, inStartingPacket, &readBlockSize, converterInfo->readBuffer.data() );
//	OSStatus status = AudioFileReadPackets( converterInfo->inputFile, true, &outByteCount, converterInfo->inputFilePacketDescriptions, inStartingPacket, &readBlockSize, converterInfo->readBuffer.data() );
	if( status == kAudioFileEndOfFileError )
		LOG_V << "kAudioFileEndOfFileError" << endl;
	else
		CI_ASSERT( status == noErr );
	converterInfo->readIndex += readBlockSize;

	CI_ASSERT( ioData->mNumberBuffers == 1 );
	ioData->mBuffers[0].mData = converterInfo->readBuffer.data();
//	memcpy( ioData->mBuffers[0].mData, converterInfo->readBuffer.data(), outByteCount );
	ioData->mBuffers[0].mDataByteSize = outByteCount;

	if( outDataPacketDescription )
        *outDataPacketDescription = converterInfo->inputFilePacketDescriptions;

	return noErr;
}

void CAFileLoadingTestApp::mouseDown( MouseEvent event )
{
	int step = mSamples.size() / getWindowWidth();
    float xLoc = event.getX() * step;
    LOG_V << "samples starting at " << xLoc << ":\n";
    for( int i = 0; i < 100; i++ ) {
        if( mNumChannels == 1 ) {
            console() << mSamples[xLoc + i] << ", ";
        } else {
			LOG_V << "TODO" << endl;
        }
    }
    console() << endl;
}

void CAFileLoadingTestApp::update()
{
}

void CAFileLoadingTestApp::draw()
{
	gl::clear();

	mWaveformPlot.drawGl();

}

void CAFileLoadingTestApp::printErrorCodes()
{
	console() << "kAudioConverterErr_FormatNotSupported : " << kAudioConverterErr_FormatNotSupported << endl;
	console() << "kAudioConverterErr_OperationNotSupported : " << kAudioConverterErr_OperationNotSupported << endl;
	console() << "kAudioConverterErr_PropertyNotSupported : " << kAudioConverterErr_PropertyNotSupported << endl;
	console() << "kAudioConverterErr_InvalidInputSize : " << kAudioConverterErr_InvalidInputSize << endl;
	console() << "kAudioConverterErr_InvalidOutputSize : " << kAudioConverterErr_InvalidOutputSize << endl;
	console() << "kAudioConverterErr_UnspecifiedError : " << kAudioConverterErr_UnspecifiedError << endl;
	console() << "kAudioConverterErr_BadPropertySizeError : " << kAudioConverterErr_BadPropertySizeError << endl;
	console() << "kAudioConverterErr_RequiresPacketDescriptionsError : " << kAudioConverterErr_RequiresPacketDescriptionsError << endl;
	console() << "kAudioConverterErr_InputSampleRateOutOfRange : " << kAudioConverterErr_InputSampleRateOutOfRange << endl;
	console() << "kAudioConverterErr_OutputSampleRateOutOfRange : " << kAudioConverterErr_OutputSampleRateOutOfRange << endl;
#if defined( CINDER_COCOA_TOUCH )
	console() << "kAudioConverterErr_HardwareInUse : " << kAudioConverterErr_HardwareInUse << endl;
	console() << "kAudioConverterErr_NoHardwarePermission : " << kAudioConverterErr_NoHardwarePermission << endl;
#endif

}

CINDER_APP_NATIVE( CAFileLoadingTestApp, RendererGl )
