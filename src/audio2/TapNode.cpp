#include "audio2/TapNode.h"
#include "audio2/RingBuffer.h"
#include "audio2/Fft.h"

#include "audio2/Debug.h"

#include "cinder/CinderMath.h"

#include <complex>

using namespace std;
using namespace ci;

namespace audio2 {

// ----------------------------------------------------------------------------------------------------
// MARK: - TapNode
// ----------------------------------------------------------------------------------------------------

TapNode::TapNode( size_t numBufferedFrames, const Format &format )
: Node( format ), mNumBufferedFrames( numBufferedFrames )
{
	mTag = "BufferTap";
	setAutoEnabled();
}

TapNode::~TapNode()
{
}

// TODO: make it possible for tap size to be auto-configured to input size
// - methinks it requires all nodes to be able to keep a blocksize
void TapNode::initialize()
{
	mCopiedBuffer = Buffer( getNumChannels(), mNumBufferedFrames );
	for( size_t ch = 0; ch < getNumChannels(); ch++ )
		mRingBuffers.push_back( unique_ptr<RingBuffer>( new RingBuffer( mNumBufferedFrames ) ) );

	mInitialized = true;
}

const Buffer& TapNode::getBuffer()
{
	for( size_t ch = 0; ch < getNumChannels(); ch++ )
		mRingBuffers[ch]->read( mCopiedBuffer.getChannel( ch ), mCopiedBuffer.getNumFrames() );

	return mCopiedBuffer;
}

// FIXME: samples will go out of whack if only one channel is pulled. add a fillCopiedBuffer private method
const float *TapNode::getChannel( size_t channel )
{
	CI_ASSERT( channel < mCopiedBuffer.getNumChannels() );

	float *buf = mCopiedBuffer.getChannel( channel );
	mRingBuffers[channel]->read( buf, mCopiedBuffer.getNumFrames() );

	return buf;
}

void TapNode::process( Buffer *buffer )
{
	for( size_t ch = 0; ch < getNumChannels(); ch++ )
		mRingBuffers[ch]->write( buffer->getChannel( ch ), buffer->getNumFrames() );
}

// ----------------------------------------------------------------------------------------------------
// MARK: - SpectrumTapNode
// ----------------------------------------------------------------------------------------------------

SpectrumTapNode::SpectrumTapNode( size_t fftSize, size_t windowSize, WindowType windowType, const Format &format )
: Node( format ), mFftSize( fftSize ), mWindowSize( windowSize ), mWindowType( windowType ),
	mNumFramesCopied( 0 ), mApplyWindow( true ), mSmoothingFactor( 0.65f )
{
	mTag = "SpectrumTapNode";
}

SpectrumTapNode::~SpectrumTapNode()
{
}

void SpectrumTapNode::initialize()
{
	if( ! mFftSize )
		mFftSize = getContext()->getNumFramesPerBlock();
	if( ! isPowerOf2( mFftSize ) )
		mFftSize = nextPowerOf2( mFftSize );
		

	mFft = unique_ptr<Fft>( new Fft( mFftSize ) );

	mBuffer = audio2::Buffer( 1, mFftSize );
	mMagSpectrum.resize( mFftSize / 2 );

	if( ! mWindowSize  )
		mWindowSize = mFftSize;
	else if( ! isPowerOf2( mWindowSize ) )
		mWindowSize = nextPowerOf2( mWindowSize );

	mWindow = makeAlignedArray<float>( mWindowSize );

	switch ( mWindowType ) {
		case WindowType::BLACKMAN:
			generateBlackmanWindow( mWindow.get(), mWindowSize );
			break;
		case WindowType::HAMM:
			generateHammWindow( mWindow.get(), mWindowSize );
			break;
		case WindowType::HANN:
			generateHannWindow( mWindow.get(), mWindowSize );
			break;
		default:
			// rect window, just fill with 1's
			for( size_t i = 0; i < mWindowSize; i++ )
				mWindow.get()[i] = 1.0f;
			break;
	}

	LOG_V << "complete. fft size: " << mFftSize << ", window size: " << mWindowSize << endl;
}

// TODO: should really be using a Converter to go stereo (or more) -> mono
// - a good implementation will use equal-power scaling as if the mono signal was two stereo channels panned to center
void SpectrumTapNode::process( audio2::Buffer *buffer )
{
	lock_guard<mutex> lock( mMutex );

	if( mNumFramesCopied == mWindowSize )
		return;

	size_t numCopyFrames = std::min( buffer->getNumFrames(), mWindowSize - mNumFramesCopied ); // TODO: return if zero
	size_t numSourceChannels = buffer->getNumChannels();
	float *offsetBuffer = &mBuffer[mNumFramesCopied];
	if( numSourceChannels == 1 ) {
		memcpy( offsetBuffer, buffer->getData(), numCopyFrames * sizeof( float ) );
	}
	else {
		// naive average of all channels
		float scale = 1.0f / numSourceChannels;
		for( size_t ch = 0; ch < numSourceChannels; ch++ ) {
			for( size_t i = 0; i < numCopyFrames; i++ )
				offsetBuffer[i] += buffer->getChannel( ch )[i] * scale;
		}
	}

	mNumFramesCopied += numCopyFrames;
}

const std::vector<float>& SpectrumTapNode::getMagSpectrum()
{
	lock_guard<mutex> lock( mMutex );
	if( mNumFramesCopied == mWindowSize ) {

		if( mApplyWindow ) {
			float *win = mWindow.get();
			for( size_t i = 0; i < mWindowSize; ++i )
				mBuffer[i] *= win[i];
		}

		mFft->compute( &mBuffer );

		auto &real = mFft->getReal();
		auto &imag = mFft->getImag();

		// remove nyquist component.
		imag[0] = 0.0f;

		// compute normalized magnitude spectrum
		const float kMagScale = 1.0f / mFft->getSize();
		for( size_t i = 0; i < mMagSpectrum.size(); i++ ) {
			complex<float> c( real[i], imag[i] );
			mMagSpectrum[i] = mMagSpectrum[i] * mSmoothingFactor + abs( c ) * kMagScale * ( 1.0f - mSmoothingFactor );
		}
		mNumFramesCopied = 0;
		mBuffer.zero();
	}
	return mMagSpectrum;
}

void SpectrumTapNode::setSmoothingFactor( float factor )
{
	mSmoothingFactor = ( factor < 0.0f ) ? 0.0f : ( ( factor > 1.0f ) ? 1.0f : factor );
}


} // namespace audio2