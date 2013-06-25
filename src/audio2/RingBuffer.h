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

#include "audio2/assert.h"
#include <boost/lockfree/spsc_queue.hpp>
#include <vector>

namespace audio2 {

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

}
