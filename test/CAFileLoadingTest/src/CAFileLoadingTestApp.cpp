#include "cinder/app/AppNative.h"
#include "cinder/gl/gl.h"
#include "cinder/cocoa/CinderCocoa.h"

#include "audio2/audio.h"
#include "audio2/NodeSource.h"
#include "audio2/Debug.h"
#include "audio2/cocoa/CinderCoreAudio.h"
#include "Plot.h"

// Note: this was an attempt at using Core Audio's Audio File + Audio Converter Services api's. Currently SourceFileCoreAudio uses
// Extended Audio File Services because it is easier to handle the many different types of audio files with confidence.
// But, in the end we'll already need to use Audio Converter Services for ogg files and other various things..

// FIXME: load VBR mp3
// - the 'packetCount' is throwing it off - 155 frames, when it is really 155 packes and each packet is many frames

//#define FILE_NAME "tone440.wav"
//#define FILE_NAME "tone440_float.wav"
#define FILE_NAME "tone440.mp3"

using namespace ci;
using namespace ci::app;
using namespace std;

using namespace ci::audio2;

class CAFileLoadingTestApp : public AppNative {
public:
	void prepareSettings( Settings *settings );
	void setup();
	void mouseDown( MouseEvent event );
	void update();
	void draw();

	void printErrorCodes();

	ContextRef mContext;

	vector<float> mSamples;
	size_t mNumChannels, mNumSamples;

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
	mContext = Context::create();

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

	AudioStreamBasicDescription inputAsbd;
	UInt32 asbdSize = sizeof( inputAsbd );

    status = AudioFileGetProperty( converterInfo.inputFile, kAudioFilePropertyDataFormat, &asbdSize, &inputAsbd );
	CI_ASSERT( status == noErr );

	LOG_V << "input ABSD: " << endl;
	audio2::cocoa::printASBD( inputAsbd );

	mNumChannels = inputAsbd.mChannelsPerFrame;

	UInt64 packetCount;
	UInt32 packetCountSize = sizeof( packetCount );

    status = AudioFileGetProperty( converterInfo.inputFile, kAudioFilePropertyAudioDataPacketCount, &packetCountSize, &packetCount );
	CI_ASSERT( status == noErr );

	LOG_V << "packet count: " << packetCount << endl;

	UInt32 isVbr;
	UInt32 propSize = sizeof( isVbr );
	status = AudioFormatGetProperty( kAudioFormatProperty_FormatIsVBR, asbdSize, &inputAsbd, &propSize, &isVbr );
	CI_ASSERT( status == noErr );

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
	status = AudioConverterNew( &inputAsbd, &outputAbsd, &audioConverter );
	CI_ASSERT( status == noErr );



	// allocate packet descriptions if the input file is VBR
	// TODO: rename this back to packets, since that is actually a more useful generalization

	UInt32 framesPerRead = 1000;
	UInt32 outputBufferSize = framesPerRead * sizeof( float );
	converterInfo.readBuffer.resize( framesPerRead );


	UInt32 packetsPerRead;
	if( ! isVbr ) {
		packetsPerRead = framesPerRead;

		mSamples.resize( packetCount );
	}
	else {
		LOG_V << "file is VBR." << endl;

		UInt32 maxPacketSize;
		UInt32 size = sizeof( maxPacketSize );
		status = AudioConverterGetProperty( audioConverter, kAudioConverterPropertyMaximumOutputPacketSize, &size, &maxPacketSize );
		CI_ASSERT( status == noErr );

		packetsPerRead = framesPerRead / maxPacketSize;

		converterInfo.inputFilePacketDescriptions = (AudioStreamPacketDescription *)malloc(sizeof(AudioStreamPacketDescription) * packetsPerRead);

	}

	//	UInt32 sizePerFrame = inputAsbd.mBytesPerPacket;

	//    for( int i = 0; i < mNumChannels; i++ ) {
	//		// added outputBufferSize for the last frame, CoreAudio expects that the entire size of the buffer is valid even if it isn't going to write to it.
	//        mSamples[i].resize( packetCount + framesPerRead );
	//    }

	//	audio2::cocoa::AudioBufferListRef bufferList = audio2::cocoa::createNonInterleavedBufferList( mNumChannels, framesPerRead );

	LOG_V << "reading..." << endl;
	while( converterInfo.readIndex < ( packetCount - 1 ) ) {

		//		for( int i = 0; i < mSamples.size(); i++ ) {
		//            bufferList->mBuffers[i].mData = &mSamples[i][converterInfo.readIndex];
		//        }

		if( mSamples.size() < converterInfo.readIndex + framesPerRead )
			mSamples.resize( converterInfo.readIndex + framesPerRead );

		AudioBufferList bufferList;
		bufferList.mNumberBuffers = 1;
		bufferList.mBuffers[0].mNumberChannels = mNumChannels;
		bufferList.mBuffers[0].mDataByteSize = outputBufferSize;
		bufferList.mBuffers[0].mData = &mSamples[converterInfo.readIndex];

		UInt32 ioOutputDataPackets = std::min( packetsPerRead, (uint32_t)packetCount - converterInfo.readIndex );
		status = AudioConverterFillComplexBuffer( audioConverter, converterCallback, &converterInfo, &ioOutputDataPackets, &bufferList, converterInfo.inputFilePacketDescriptions );
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

	free( converterInfo.inputFilePacketDescriptions );
}


OSStatus converterCallback( AudioConverterRef audioConverter, UInt32 *ioNumberDataPackets, AudioBufferList* ioData, AudioStreamPacketDescription **outDataPacketDescription, void *inUserData )
{
	ConverterInfo *converterInfo = (ConverterInfo *)inUserData;

	UInt32 readBlockSize = *ioNumberDataPackets;
	UInt32 outByteCount = readBlockSize * sizeof( float );
	SInt64 inStartingPacket = converterInfo->readIndex;
	//	OSStatus status = AudioFileReadPacketData( converterInfo->inputFile, true, &outByteCount, converterInfo->inputFilePacketDescriptions, inStartingPacket, &readBlockSize, converterInfo->readBuffer.data() ); // FIXME: doesn't work with mp3
	OSStatus status = AudioFileReadPackets( converterInfo->inputFile, true, &outByteCount, converterInfo->inputFilePacketDescriptions, inStartingPacket, &readBlockSize, converterInfo->readBuffer.data() );
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
	mWaveformPlot.draw();
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
