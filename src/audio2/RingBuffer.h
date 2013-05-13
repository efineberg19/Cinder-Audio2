#pragma once

#include <boost/lockfree/spsc_queue.hpp>
#include <vector>

class RingBuffer {
public:
	RingBuffer() : mLockFreeQueue( 0 ) {}

	// note: give lockfree one extra sample, which it uses internally to track the positions of head and tail
	// reported here: https://svn.boost.org/trac/boost/ticket/8560
	RingBuffer( size_t size ) : mLockFreeQueue( size + 1 ), mSize( size ) {}
	~RingBuffer() {}

	//! pushes vector \t samples to internal buffer, overwriting oldest samples
	void write( const std::vector<float> &samples )	{ write( samples.data(), samples.size() ); }
	//! fills vector \t samples and removes them from internal buffer. returns the amount actually copied.
	size_t read( std::vector<float> *samples )	{ return read( samples->data(), samples->size() ); }

	//! pushes \t count \t samples to buffer, overwriting oldest samples
	void write( const float *samples, size_t count ) {
		if( count > mSize )
			count = mSize;

		size_t numPushed = mLockFreeQueue.push( samples, count );
		if( count > numPushed ) {
			size_t numLeft = count - numPushed;
			
			// ???: is there a more efficient way to overwrite?
			float old;
			for( int i = 0; i < numLeft; i++ )
				mLockFreeQueue.pop( old );
			numPushed = mLockFreeQueue.push( &samples[numPushed], numLeft );
			assert( numPushed == numLeft );
		}
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
