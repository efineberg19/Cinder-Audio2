/*
 Copyright (c) 2014, The Cinder Project

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

#include "cinder/audio2/CinderAssert.h"

#include <vector>
#include <memory>
#include <cstdlib>
#include <algorithm>

// TODO: It'd be nice to have a 'BufferView' or similar functionality
// - would not own the internal buffer, but would point to another one, with offset + size
// - alt. would be for BufferBaseT to take an alternate constructor
//		- requires T* as data storage gain, which is also necessary to ensure 16byte alignment

namespace cinder { namespace audio2 {

//! Audio buffer that stores channels of type \a T in contiguous arrays.
template <typename T>
class BufferBaseT {
  public:
	typedef T SampleType;

	BufferBaseT( size_t numFrames, size_t numChannels )
	: mNumFrames( numFrames ), mNumChannels( numChannels ), mData( numFrames * numChannels )
	{
	}

	size_t getNumFrames() const		{ return mNumFrames; }
	size_t getNumChannels() const	{ return mNumChannels; }
	size_t getSize() const			{ return mNumFrames * mNumChannels; }

	bool isEmpty() const			{ return mNumFrames == 0; }

	T* getData() { return mData.data(); }
	const T* getData() const { return mData.data(); }

	T& operator[]( size_t n )
	{
		CI_ASSERT( n < getSize() );
		return mData[n];
	}

	const T& operator[]( size_t n ) const
	{
		CI_ASSERT( n < getSize() );
		return mData[n];
	}

	void zero()
	{
		std::memset( mData.data(), 0, mData.size() * sizeof( T ) );
	}

  protected:
	std::vector<T> mData;
	size_t mNumChannels, mNumFrames;
};

template <typename T>
class BufferT : public BufferBaseT<T> {
  public:

	BufferT( size_t numFrames = 0, size_t numChannels = 1 ) : BufferBaseT<T>( numFrames, numChannels ) {}

	T* getChannel( size_t ch )
	{
		CI_ASSERT_MSG( ch < this->mNumChannels, "ch out of range" );
		return &this->mData[ch * this->mNumFrames];
	}

	const T* getChannel( size_t ch ) const
	{
		CI_ASSERT_MSG( ch < this->mNumChannels, "ch out of range" );
		return &this->mData[ch * this->mNumFrames];
	}

	using BufferBaseT<T>::zero;

	void zero( size_t startFrame, size_t numFrames )
	{
		CI_ASSERT( startFrame + numFrames <= this->mNumFrames );
		for( size_t ch = 0; ch < this->mNumChannels; ch++ )
			std::memset( &getChannel( ch )[startFrame], 0, numFrames * sizeof( T ) );
	}

	//! Copies min( this, other ) channels and frames from \a other to internal storage.
	void copy( const BufferT<T> &other )
	{
		size_t numFrames = std::min( this->getNumFrames(), other.getNumFrames() );
		size_t numChannels = std::min( this->getNumChannels(), other.getNumChannels() );

		for( size_t ch = 0; ch < numChannels; ch++ )
			std::memmove( this->getChannel( ch ), other.getChannel( ch ), numFrames * sizeof( T ) );
	}

	//! Copies \a numFrames frames from \a other into internal storage.
	//! \note requires channel counts to match.
	void copy( const BufferT<T> &other, size_t numFrames )
	{
		CI_ASSERT( this->getNumChannels() == other.getNumChannels() );
		CI_ASSERT( numFrames <= this->getNumFrames() );
		CI_ASSERT( numFrames <= other.getNumFrames() );

		for( size_t ch = 0; ch < this->getNumChannels(); ch++ )
			std::memmove( this->getChannel( ch ), other.getChannel( ch ), numFrames * sizeof( T ) );
	}

	//! Copies \a numFrames frames from \a other offset by \a otherFrameOffset into internal storage offset by \a frameOffset.
	//! \note requires channel counts to match.
	void copyOffset( const BufferT<T> &other, size_t numFrames, size_t frameOffset, size_t otherFrameOffset )
	{
		CI_ASSERT( this->getNumChannels() == other.getNumChannels() );
		CI_ASSERT( frameOffset + numFrames <= this->getNumFrames() );
		CI_ASSERT( otherFrameOffset + numFrames <= other.getNumFrames() );

		for( size_t ch = 0; ch < this->getNumChannels(); ch++ )
			std::memmove( this->getChannel( ch ) + frameOffset, other.getChannel( ch ) + otherFrameOffset, numFrames * sizeof( T ) );
	}
};

template <typename T>
class BufferInterleavedT : public BufferBaseT<T> {
  public:
	BufferInterleavedT( size_t numFrames = 0, size_t numChannels = 1 ) : BufferBaseT<T>( numFrames, numChannels ) {}

	using BufferBaseT<T>::zero;

	void zero( size_t startFrame, size_t numFrames )
	{
		CI_ASSERT( startFrame + numFrames <= this->mNumFrames );
		std::memset( &this->mData[startFrame * this->mNumChannels], 0, numFrames * this->mNumChannels * sizeof( T ) );
	}
};

//! BufferT variant that contains audio data in the frequency domain. Its channels relate to the results of an
//! FFT transform, channel = 0 is real and channel = 1 is imaginary. The reasoning for subclassing \a BufferT is
//! so that a BufferSpectralT can be handled by generic processing \a Node's as well, which can operate on both
//! time and frequency domain signals.
template <typename T>
class BufferSpectralT : public BufferT<T> {
  public:
	BufferSpectralT( size_t numFrames = 0 ) : BufferT<T>( numFrames / 2, 2 ) {}

	T* getReal()				{ return &this->mData[0]; }
	const T* getReal() const	{ return &this->mData[0]; }

	T* getImag()				{ return &this->mData[this->mNumFrames]; }
	const T* getImag() const	{ return &this->mData[this->mNumFrames]; }

};

//! BufferDynamicT is a resizable, reshapeable BufferT. The internally alloceted buffer will grow as needed,
//! but it will not shrink unless shrinkToFit() is called.
//! TODO: enable move operator to convert BufferT to this
template <typename T>
class BufferDynamicT : public BufferT<T> {
  public:
	BufferDynamicT( size_t numFrames = 0, size_t numChannels = 1 ) : BufferT<T>( numFrames, numChannels ),
		mAllocatedSize( numFrames * numChannels )
	{}

	void setSize( size_t numFrames, size_t numChannels )
	{
		this->mNumFrames = numFrames;
		this->mNumChannels = numChannels;
		resizeIfNecessary();
	}

	void setNumFrames( size_t numFrames )
	{
		this->mNumFrames = numFrames;
		resizeIfNecessary();
	}

	void setNumChannels( size_t numChannels )
	{
		this->mNumChannels = numChannels;
		resizeIfNecessary();
	}

	//! Shrinks the allocated size to match the specified size, freeing any extra memory.
	void shrinkToFit()
	{
		mAllocatedSize = this->getSize();
		this->mData.resize( mAllocatedSize );
	}

	size_t getAllocatedSize() const		{ return mAllocatedSize; }

  private:
	void resizeIfNecessary()
	{
		size_t size = this->getSize();
		if( mAllocatedSize < size ) {
			mAllocatedSize = size;
			this->mData.resize( mAllocatedSize );
		}
	}

	size_t mAllocatedSize;
};

//! Simple functor wrapping free(), suitable for unique_ptr's that allocate memory with malloc, calloc and realloc.
template<typename T>
struct FreeDeleter {
	void operator()( T *x ) { std::free( x ); }
};

//! Returns an array of \a size elements of type \a T, aligned by \a alignment.
template<typename T>
std::unique_ptr<T, FreeDeleter<T> > makeAlignedArray( size_t size, size_t alignment = 16 )
{
	void *ptr = std::calloc( size, sizeof( T ) );

	// FIXME: broken on windows, this is returning NULL.
	// unnecessary at the moment anyway, so it is commented out until fixed
	//ptr = std::align( alignment, size, ptr, size );
	//CI_ASSERT( ptr );
	
	return std::unique_ptr<T, FreeDeleter<T> >( static_cast<T *>( ptr ) );
}

typedef std::unique_ptr<float, FreeDeleter<float> > AlignedArrayPtr;
typedef std::unique_ptr<double, FreeDeleter<double> > AlignedArrayPtrd;

typedef BufferT<float>				Buffer;
typedef BufferInterleavedT<float>	BufferInterleaved;
typedef BufferSpectralT<float>		BufferSpectral;
typedef BufferDynamicT<float>		BufferDynamic;

typedef std::shared_ptr<Buffer>				BufferRef;
typedef std::shared_ptr<BufferInterleaved>	BufferInterleavedRef;
typedef std::shared_ptr<BufferSpectral>		BufferSpectralRef;
typedef std::shared_ptr<BufferDynamic>		BufferDynamicRef;

} } // namespace cinder::audio2