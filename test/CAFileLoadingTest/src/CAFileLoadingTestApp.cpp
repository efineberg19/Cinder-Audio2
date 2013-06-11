#include "cinder/app/AppNative.h"
#include "cinder/gl/gl.h"
#include "cinder/cocoa/CinderCocoa.h"

#include "audio2/audio.h"
#include "audio2/Debug.h"
#include "audio2/cocoa/Util.h"
#include "audio2/Plot.h"

// TODO: put the Audio File services stuff back in here and try to get it working with VBR data

//#define FILE_NAME "tone440.wav"
//#define FILE_NAME "tone440_float.wav"
//#define FILE_NAME "tone440.mp3"
#define FILE_NAME "tone440L220R.mp3"

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

	ContextRef mContext;

	audio2::Buffer mBuffer;
	size_t mNumChannels, mNumSamples;

	WaveformPlot mWaveformPlot;
};

void CAFileLoadingTestApp::prepareSettings( Settings *settings )
{
    settings->setWindowSize( 1000, 500 );
}

void CAFileLoadingTestApp::setup()
{
	mContext = Context::instance()->createContext();
	OutputNodeRef output = Context::instance()->createOutput();
	mContext->setRoot( output );

	DataSourceRef dataSource = loadResource( FILE_NAME );

	Url url( dataSource->getFilePath().string() );
	CFURLRef audioFileUrl = ci::cocoa::createCfUrl( url );

	ExtAudioFileRef inputFile;
	OSStatus status = ExtAudioFileOpenURL( audioFileUrl, &inputFile );
	CI_ASSERT( status == noErr );

	CFRelease( audioFileUrl );

    AudioStreamBasicDescription fileStreamFormat;
	UInt32 propSize = sizeof( fileStreamFormat );
    status = ExtAudioFileGetProperty( inputFile, kExtAudioFileProperty_FileDataFormat, &propSize, &fileStreamFormat );
	CI_ASSERT( status == noErr );
    LOG_V << "File stream format:\n";
	audio2::cocoa::printASBD( fileStreamFormat );

    mNumChannels = fileStreamFormat.mChannelsPerFrame;

    SInt64 numFrames;
    propSize = sizeof( numFrames );
    status = ExtAudioFileGetProperty( inputFile, kExtAudioFileProperty_FileLengthFrames, &propSize, &numFrames );
	CI_ASSERT( status == noErr );
    LOG_V << "number of frames: " << numFrames << endl;

	AudioStreamBasicDescription outputFormat = audio2::cocoa::nonInterleavedFloatABSD( mNumChannels, 44100 );

	LOG_V << "setting client data format to:\n";
	audio2::cocoa::printASBD( outputFormat );
	status = ExtAudioFileSetProperty( inputFile, kExtAudioFileProperty_ClientDataFormat, sizeof( outputFormat ), &outputFormat );
	CI_ASSERT( status == noErr );

	UInt32 outputBufferSize = 32 * 1024; // 32 KB is a good starting point
	UInt32 sizePerPacket = outputFormat.mBytesPerPacket;
	UInt32 packetsPerBuffer = outputBufferSize / sizePerPacket;
    int currReadPos = 0;

	mBuffer = audio2::Buffer( mNumChannels, numFrames );
	audio2::cocoa::AudioBufferListRef bufferList = audio2::cocoa::createNonInterleavedBufferList( mNumChannels, outputBufferSize );

	while( true ) {
		size_t framesLeft = numFrames - currReadPos;
		if( framesLeft <= 0 ) {
			LOG_V << "read done, framesLeft: " << framesLeft << endl;
			break;
		}

		UInt32 frameCount = std::min( framesLeft, packetsPerBuffer );
		LOG_V << "frameCount: " << frameCount << endl;

        for( int i = 0; i < mNumChannels; i++ ) {
            bufferList->mBuffers[i].mDataByteSize = frameCount * sizeof( float );
            bufferList->mBuffers[i].mData = &mBuffer.getChannel( i )[currReadPos];
        }

		// read from the extaudiofile
		status = ExtAudioFileRead( inputFile, &frameCount, bufferList.get() );
		CI_ASSERT( status == noErr );

        currReadPos += frameCount;
	}

	LOG_V << "load complete.\n";

	mWaveformPlot.load( &mBuffer, getWindowBounds() );
}

void CAFileLoadingTestApp::mouseDown( MouseEvent event )
{
    size_t step = mBuffer.getNumFrames() / getWindowWidth();
    size_t xLoc = event.getX() * step;
    LOG_V << "samples starting at " << xLoc << ":\n";
    for( int i = 0; i < 100; i++ ) {
        if( mNumChannels == 1 ) {
            console() << mBuffer.getChannel( 0 )[xLoc + i] << ", ";
        } else {
            console() << "[" << mBuffer.getChannel( 0 )[xLoc + i] << ", " << mBuffer.getChannel( 0 )[xLoc + i] << "], ";
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
	gl::draw( mWaveformPlot );
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
