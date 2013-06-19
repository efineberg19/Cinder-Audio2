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
