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

// note: as of boost 1.53, lockfree requires Boost.Atomic on iOS - so libboost_atomic.a is linked in for that arch
// - while lockfree::spsc_queue works fine with the current version of libc++, other data structs in the lib don't
//   yet due to a known bug, so boost::atomic will be used until then
// - vc2010 also requires boost::atomic

#include "audio2/CinderAssert.h"
#include <boost/lockfree/spsc_queue.hpp>
#include <vector>

namespace cinder { namespace audio2 {

class RingBuffer {
  public:
	RingBuffer() : mLockFreeQueue( 0 ) {}

	// note: give lockfree one extra sample, which it uses internally to track the positions of head and tail
	// reported here: https://svn.boost.org/trac/boost/ticket/8560
	RingBuffer( size_t size ) : mLockFreeQueue( size + 1 ), mSize( size ) {}
	~RingBuffer() {}

	size_t getSize() const	{ return mSize; }

	//! pushes vector \t samples to internal buffer, overwriting oldest samples
	void write( const std::vector<float> &samples )	{ write( samples.data(), samples.size() ); }
	//! fills vector \t samples and removes them from internal buffer. returns the amount actually copied.
	size_t read( std::vector<float> *samples )	{ return read( samples->data(), samples->size() ); }

	//! pushes \t count \t samples to buffer, overwriting oldest samples. returns the amount overwritten or 0.
	size_t write( const float *samples, size_t count ) {
		if( count > mSize )
			count = mSize;

		size_t numPushed = mLockFreeQueue.push( samples, count );
		if( count > numPushed ) {
			size_t numLeft = count - numPushed;
			
			// ???: is there a more efficient way to overwrite?
			float old;
			for( size_t i = 0; i < numLeft; i++ )
				mLockFreeQueue.pop( old );
			numPushed = mLockFreeQueue.push( &samples[numPushed], numLeft );
			CI_ASSERT( numPushed == numLeft );
			return numLeft;
		}
		return 0;
	}

	//! fills array \t samples and removes them from internal buffer. returns the amount actually copied.
	size_t read( float *samples, size_t count ) {
		if( count > mSize )
			count = mSize;
		return mLockFreeQueue.pop( samples, count );
	}

  private:
	boost::lockfree::spsc_queue<float> mLockFreeQueue;
	size_t mSize;
};

//! Other than minor modifications, this ringbuffer is a direct copy of Tim Blechmann's fine work,
//! found as the base structure of boost::lockfree::spsc_queue (ringbuffer_base). Whereas the boost::lockfree
//! data structures are meant for a wide range of applications, this version specifically caters to audio processing.
//!
//! The implementation remains lock-free and thread-safe within a single write thread / single read thread context.
//!
//! \note \a T must be POD.
template <typename T>
class RingBufferT {
public:

	//! Constructs a RingBufferT with \a size maximum elements.
	RingBufferT( size_t size )
	: mAllocatedSize( size + 1 ), mWriteIndex( 0 ), mReadIndex( 0 )
	{
		mData = (T *)calloc( mAllocatedSize, sizeof( T ) );
	}

	~RingBufferT()
	{
		free( mData );
	}

	//! Returns the number of elements. \note Actual allocation size is +1, the extra element being a marker to determine when the internal buffer is full.
	size_t getSize() const
	{
		return mAllocatedSize - 1;
	}

	//! Returns the number of elements available for wrtiing. \note Only safe to call from the write thread.
	size_t getAvailableWrite()
	{
		return getAvailableWrite( mWriteIndex, mReadIndex );
	}

	//! Returns the number of elements available for wrtiing. \note Only safe to call from the read thread.
	size_t getAvailableRead()
	{
		return getAvailableRead( mWriteIndex, mReadIndex );
	}

	//! \note only safe to call from the write thread.
	// TODO: decide what is best to do when buffer fills;
	// assert, fill as much as possible, or overwrite circular.
	void write( const T *array, size_t count )
	{
		const size_t writeIndex = mWriteIndex.load( std::memory_order_relaxed );
		const size_t readIndex = mReadIndex.load( std::memory_order_acquire );

		assert( count <= getAvailableWrite( writeIndex, readIndex ) );

		size_t writeIndexAfter = writeIndex + count;

		if( writeIndex + count > mAllocatedSize ) {
			size_t countA = mAllocatedSize - writeIndex;

			std::copy( array, array + countA, mData + writeIndex );
			std::copy( array + countA, array + count, mData );
			writeIndexAfter -= mAllocatedSize;
		}
		else {
			std::copy( array, array + count, mData + writeIndex );
			if( writeIndexAfter == mAllocatedSize )
				writeIndexAfter = 0;
		}

		mWriteIndex.store( writeIndexAfter, std::memory_order_release );
	}

	//! \note only safe to call from the read thread.
	void read( T *array, size_t count )
	{
		const size_t writeIndex = mWriteIndex.load( std::memory_order_acquire );
		const size_t readIndex = mReadIndex.load( std::memory_order_relaxed );

		assert( count <= getAvailableRead( writeIndex, readIndex ) );

		size_t readIndexAfter = readIndex + count;

		if( readIndex + count > mAllocatedSize ) {
			size_t countA = mAllocatedSize - readIndex;
			size_t countB = count - countA;

			std::copy( mData + readIndex, mData + mAllocatedSize, array );
			std::copy( mData, mData + countB, array + countA );

			readIndexAfter -= mAllocatedSize;
		}
		else {
			std::copy( mData + readIndex, mData + mReadIndex + count, array );
			if( readIndexAfter == mAllocatedSize )
				readIndexAfter = 0;
		}

		mReadIndex.store( readIndexAfter, std::memory_order_release );
	}

private:
	size_t getAvailableWrite( size_t writeIndex, size_t readIndex )
	{
		size_t result = readIndex - writeIndex - 1;
		if( writeIndex >= readIndex )
			result += mAllocatedSize;

		return result;
	}

	size_t getAvailableRead( size_t writeIndex, size_t readIndex )
	{
		if( writeIndex >= readIndex )
			return writeIndex - readIndex;

		return writeIndex + mAllocatedSize - readIndex;
	}
	
	
	T						*mData;
	size_t					mAllocatedSize;
	std::atomic<size_t>		mWriteIndex, mReadIndex;
};


} } // namespace cinder::audio2
