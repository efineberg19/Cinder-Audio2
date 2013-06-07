#pragma once

#include "audio2/assert.h"

#include <vector>

namespace audio2 {

// TODO: following paper provides a 0(N) solution for interleaving: "A Simple In-Place Algorithm for In-Shuffle"
// - http://arxiv.org/abs/0805.1598
// - also explained in: http://cs.stackexchange.com/q/332

// http://stackoverflow.com/q/7780279/506584
template<typename T>
void deinterleaveInplacePow2( T* arr, int length ) {
	if(length<=1) return;

	int i = 1;
	for(i = 1; i*2<length; i++){
		//swap i with i*2
		T temp = arr[i];
		arr[i] = arr[i*2];
		arr[i*2] = temp;
	}
	deinterleaveInplacePow2( arr+i, length-i );
}
	
template <typename T>
class BufferT {
public:
	typedef T SampleType;
	enum Format { Interleaved, NonInterleaved };

	BufferT( size_t numChannels = 0, size_t numFrames = 0, Format initialFormat = Format::Interleaved ) : mNumChannels( numChannels ), mNumFrames( numFrames ), mFormat( initialFormat )
	{
		mData.resize( numChannels * numFrames );
	}

	// TODO: getChannel(), in this form is bad news for interleaved data. options:
	// - implicitly de-interleave and change the format (call asFormat( fmt ) )
	// - if interleaved, return null.
	//		- afb suggestion: variant called getChannelIter - result knows how to iterate over interleaved samples 
	T* getChannel( size_t ch ) {
		CI_ASSERT( mFormat == NonInterleaved );
		CI_ASSERT( ch < mNumChannels );
		return &mData[ch * mNumFrames];
	}

	const T* getChannel( size_t ch ) const {
		CI_ASSERT( mFormat == NonInterleaved );
		CI_ASSERT( ch < mNumChannels );
		return &mData[ch * mNumFrames];
	}

	size_t getNumFrames() const	{ return mNumFrames; }
	size_t getNumChannels() const	{ return mNumChannels; }
	size_t getSize() const	{ return mData.size(); }

	void asFormat( Format fmt ) {
		if( fmt == NonInterleaved && mFormat == Interleaved )
			deinterleaveInplacePow2( mData.data(), mData.size() );
		else if( fmt == Interleaved && mFormat == NonInterleaved )
			CI_ASSERT( false ); // TODO: interleave
	}

	T* getData() { return mData.data(); }

	T& operator[]( size_t n ) {
		CI_ASSERT( n < getSize() );
		return mData[n];
	}

	const T& operator[]( size_t n ) const {
		CI_ASSERT( n < getSize() );
		return mData[n];
	}


private:
	std::vector<T> mData;
	size_t mNumChannels, mNumFrames;
	Format mFormat;
};

template<typename T>
inline void interleaveStereoBuffer( BufferT<T> *nonInterleaved, BufferT<T> *interleaved )
{
	CI_ASSERT( nonInterleaved->getSize() == interleaved->getSize() );

	size_t numFrames = interleaved->getNumFrames();
	T *left = nonInterleaved->getData();
	T *right = &nonInterleaved->getData()[numFrames];
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
	CI_ASSERT( nonInterleaved->getSize() == interleaved->getSize() );

	size_t numFrames = interleaved->getNumFrames();
	T *left = nonInterleaved->getData();
	T *right = &nonInterleaved->getData()[numFrames];
	T *mixed = interleaved->getData();

	size_t i, j;
	for( i = 0, j = 0; i < numFrames; i++, j += 2 ) {
		left[i] = mixed[j];
		right[i] = mixed[j + 1];
	}
}

typedef BufferT<float> Buffer;

} // namespace audio2