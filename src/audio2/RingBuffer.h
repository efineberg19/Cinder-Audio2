#pragma once

#include <boost/lockfree/spsc_queue.hpp>

class RingBuffer {
public:
	RingBuffer() : mLockFreeQueue( 0 ) {}

	// note: give lockfree one extra sample, which it uses internally to track the positions of head and tail
	// reported here: https://svn.boost.org/trac/boost/ticket/8560
	RingBuffer( size_t size ) : mLockFreeQueue( size + 1 ), mSize( size ) {}
	~RingBuffer() {}

	//! pushes to buffer, overwriting oldest samples
	void write( const float *samples, size_t count ) {
		if( count > mSize ) {
			count = mSize;
		}
		size_t numPushed = mLockFreeQueue.push( samples, count );
		if( count > numPushed ) {
			size_t numLeft = count - numPushed;
			// TODO: there's gotta be a more efficient way to just move the write head foward, rather than sequencially popping off trash
			float old;
			for( int i = 0; i < numLeft; i++ ) {
				mLockFreeQueue.pop( old );
			}
			numPushed = mLockFreeQueue.push( &samples[numPushed], numLeft );
			assert( numPushed == numLeft );
		}
	}

	size_t read( float *samples, size_t count ) {
		if( count > mSize ) {
			count = mSize;
		}
		return mLockFreeQueue.pop( samples, count );
	}

private:
	boost::lockfree::spsc_queue<float> mLockFreeQueue;
	size_t mSize;
};
