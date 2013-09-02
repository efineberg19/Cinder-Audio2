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

#pragma once

#include "audio2/CinderAssert.h"
#include "audio2/Dsp.h"

#include <vector>
#include <memory>
#include <cstdlib>

namespace cinder { namespace audio2 {

//! Audio buffer that stores channels of type \a T in contiguous arrays.
template <typename T>
class BufferT {
public:
	typedef T SampleType;
	enum Layout { CONTIGUOUS, INTERLEAVED };

	BufferT( size_t numFrames = 0, size_t numChannels = 1, Layout layout = CONTIGUOUS )
	: mNumFrames( numFrames ), mNumChannels( numChannels ), mLayout( layout ), mSilent( true )
	{
		mData.resize( numChannels * numFrames );
	}

	// TODO: consider adding getChannelIter, which knows how to iterate over both interleaved and non-interleaved samples
	T* getChannel( size_t ch ) {
		CI_ASSERT_MSG( mLayout == CONTIGUOUS, "Cannot get raw pointer to channel from an interleaved Buffer" );
		CI_ASSERT_MSG( ch < mNumChannels, "ch out of range" );
		return &mData[ch * mNumFrames];
	}

	const T* getChannel( size_t ch ) const {
		CI_ASSERT_MSG( mLayout == CONTIGUOUS, "Cannot get raw pointer to channel from an interleaved Buffer" );
		CI_ASSERT_MSG( ch < mNumChannels, "ch out of range" );
		return &mData[ch * mNumFrames];
	}

	void zero() {
		std::memset( mData.data(), 0, mData.size() * sizeof( T ) );
	}
	
	void zero( size_t startFrame, size_t numFrames ) {
		CI_ASSERT( startFrame + numFrames <= mNumFrames );
		if( mLayout == Layout::INTERLEAVED )
			std::memset( &mData[startFrame * mNumChannels], 0, numFrames * mNumChannels * sizeof( T ) );
		else {
			for( size_t ch = 0; ch < mNumChannels; ch++ )
				std::memset( &getChannel( ch )[startFrame], 0, numFrames * sizeof( float ) );
		}
	}

	size_t getNumFrames() const		{ return mNumFrames; }
	size_t getNumChannels() const	{ return mNumChannels; }
	size_t getSize() const			{ return mData.size(); }
	Layout getLayout() const		{ return mLayout; }

	void setSilent( bool b = true )	{ mSilent = b; }
	bool isSilent() const			{ return mSilent; }

//	bool isCompatible( const BufferT *other ) { return mNumChannels == other->mNumChannels && mNumFrames == other->mNumFrames && mLayout == other->mLayout; }

	T* getData() { return mData.data(); }
	const T* getData() const { return mData.data(); }

	T& operator[]( size_t n ) {
		CI_ASSERT( n < getSize() );
		return mData[n];
	}

	const T& operator[]( size_t n ) const {
		CI_ASSERT( n < getSize() );
		return mData[n];
	}

	// TODO: add copy constructor different OtherT
	// FIXME: this breaks when non-interleaved and sizes don't match
	template <typename OtherT>
	void set( const BufferT<OtherT> &other )
	{
		size_t count = std::min( getSize(), other.getSize() );
		for( size_t i = 0; i < count; i++ )
			mData[i] = other.getData()[i];
	}

protected:
	std::vector<T> mData; // TODO: switch to T*
	size_t mNumChannels, mNumFrames;
	bool mSilent;
	Layout mLayout;
};

//! DynamicBufferT is a resizable BufferT
// TODO: enable move operator to convert BufferT to this
template <typename T>
class DynamicBufferT : public BufferT<T> {
  public:

	void resize( size_t numFrames, size_t numChannels ) {
		BufferT<T>::mNumFrames = numFrames;
		BufferT<T>::mNumChannels = numChannels;
		BufferT<T>::mData.resize( BufferT<T>::mNumFrames * BufferT<T>::mNumChannels );
	}
};

template<typename T>
inline void interleaveStereoBuffer( BufferT<T> *nonInterleaved, BufferT<T> *interleaved )
{
	CI_ASSERT( interleaved->getNumChannels() == 2 && nonInterleaved->getNumChannels() == 2 );
	CI_ASSERT( interleaved->getSize() <= nonInterleaved->getSize() );

	size_t numFrames = interleaved->getNumFrames();
	T *left = nonInterleaved->getChannel( 0 );
	T *right = nonInterleaved->getChannel( 1 );

	T *mixed = interleaved->getData();

	size_t i, j;
	for( i = 0, j = 0; i < numFrames; i++, j += 2 ) {
		mixed[j] = left[i];
		mixed[j + 1] = right[i];
	}
}

template<typename T>
inline void deinterleaveStereoBuffer( BufferT<T> *interleaved, BufferT<T> *nonInterleaved )
{
	CI_ASSERT( interleaved->getNumChannels() == 2 && nonInterleaved->getNumChannels() == 2 );
	CI_ASSERT( nonInterleaved->getSize() <= interleaved->getSize() );

	size_t numFrames = nonInterleaved->getNumFrames();
	T *left = nonInterleaved->getChannel( 0 );
	T *right = nonInterleaved->getChannel( 1 );
	T *mixed = interleaved->getData();

	size_t i, j;
	for( i = 0, j = 0; i < numFrames; i++, j += 2 ) {
		left[i] = mixed[j];
		right[i] = mixed[j + 1];
	}
}


template<typename T>
struct FreeDeleter {
	void operator()( T *x ) { free( x ); }
};

template<typename T>
std::unique_ptr<T, FreeDeleter<T> > makeAlignedArray( size_t size ) {
	return std::unique_ptr<T, FreeDeleter<T> >( static_cast<float *>( calloc( size, sizeof( T ) ) ) );
}

typedef std::unique_ptr<float, FreeDeleter<float>> AlignedArrayPtr;
	

typedef BufferT<float> Buffer;
typedef BufferT<double> Bufferd;
typedef DynamicBufferT<float> DynamicBuffer;
typedef DynamicBufferT<double> DynamicBufferd;

typedef std::shared_ptr<Buffer> BufferRef;
typedef std::shared_ptr<Bufferd> BufferdRef;
typedef std::shared_ptr<DynamicBuffer> DynamicBufferRef;
typedef std::shared_ptr<DynamicBufferd> DynamicdBufferRef;

} } // namespace cinder::audio2