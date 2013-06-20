#include "audio2/TapNode.h"
#include "audio2/RingBuffer.h"
#include "audio2/Fft.h"

#include "audio2/Debug.h"

using namespace std;

namespace audio2 {

// ----------------------------------------------------------------------------------------------------
// MARK: - TapNode
// ----------------------------------------------------------------------------------------------------

TapNode::TapNode( size_t numBufferedFrames )
: Node(), mNumBufferedFrames( numBufferedFrames )
{
	mTag = "BufferTap";
	mFormat.setAutoEnabled();
}

TapNode::~TapNode()
{
}

// TODO: make it possible for tap size to be auto-configured to input size
// - methinks it requires all nodes to be able to keep a blocksize
void TapNode::initialize()
{
	mCopiedBuffer = Buffer( mFormat.getNumChannels(), mNumBufferedFrames, Buffer::Format::NonInterleaved );
	for( size_t ch = 0; ch < mFormat.getNumChannels(); ch++ )
		mRingBuffers.push_back( unique_ptr<RingBuffer>( new RingBuffer( mNumBufferedFrames ) ) );

	mInitialized = true;
}

const Buffer& TapNode::getBuffer()
{
	for( size_t ch = 0; ch < mFormat.getNumChannels(); ch++ )
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
	for( size_t ch = 0; ch < mFormat.getNumChannels(); ch++ )
		mRingBuffers[ch]->write( buffer->getChannel( ch ), buffer->getNumFrames() );
}

// ----------------------------------------------------------------------------------------------------
// MARK: - SpectrumTapNode
// ----------------------------------------------------------------------------------------------------

SpectrumTapNode::SpectrumTapNode( size_t fftSize, size_t windowSize )
: mFftSize( fftSize ), mWindowSize( windowSize ), mNumFramesCopied( 0 ), mApplyWindow( true )
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

	mFft = unique_ptr<Fft>( new Fft( mFftSize ) );

	if( ! mWindowSize )
		mWindowSize = mFftSize;

	mBuffer = audio2::Buffer( 1, mFftSize );
	mMagSpectrum.resize( mFftSize / 2 );
	
	LOG_V << "complete. fft size: " << mFftSize << ", window size: " << mWindowSize << endl;
}


// TODO: specify pad, accumulate the required number of samples
// Currently copies the smaller of the two
void SpectrumTapNode::process( audio2::Buffer *buffer )
{
	copyToInternalBuffer( buffer );
}

const std::vector<float>& SpectrumTapNode::getMagSpectrum()
{
	lock_guard<mutex> lock( mMutex );
	if( mNumFramesCopied == mBuffer.getNumFrames() ) {

		if( mApplyWindow )
			applyWindow();

		mFft->compute( &mBuffer );

		auto &real = mFft->getReal();
		auto &imag = mFft->getImag();

		// Blow away the packed nyquist component.
		// TODO: verify where I copied this from... is it vDSP specific?
		imag[0] = 0.0f;

		// compute normalized magnitude spectrum
		// TODO: try using vDSP_zvabs for this
		const float kMagScale = 1.0f / mFft->getSize();
		for( size_t i = 0; i < mMagSpectrum.size(); i++ ) {
			complex<float> c( real[i], imag[i] );
			mMagSpectrum[i] = abs( c ) * kMagScale;
		}
		mNumFramesCopied = 0;
		mBuffer.zero();
	}
	return mMagSpectrum;
}

// TODO: should really be using a Converter to go stereo (or more) -> mono
// - a good implementation will use equal-power scaling as if the mono signal was two stereo channels panned to center
void SpectrumTapNode::copyToInternalBuffer( Buffer *buffer )
{
	lock_guard<mutex> lock( mMutex );

	if( mBuffer.getNumFrames() == mNumFramesCopied )
		return;


	size_t numCopyFrames = std::min( buffer->getNumFrames(), mBuffer.getNumFrames() - mNumFramesCopied ); // TODO: return if zero
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

// TODO: replace this with table lookup
void SpectrumTapNode::applyWindow()
{
	// Blackman window
	double alpha = 0.16;
	double a0 = 0.5 * (1 - alpha);
	double a1 = 0.5;
	double a2 = 0.5 * alpha;
	double oneOverN = 1.0 / static_cast<double>( mWindowSize );

	for( size_t i = 0; i < mWindowSize; ++i ) {
		double x = static_cast<double>(i) * oneOverN;
		double window = a0 - a1 * cos( 2.0 * M_PI * x ) + a2 * cos( 4.0 * M_PI * x );
		mBuffer[i] *= float(window);
	}
}


} // namespace audio2