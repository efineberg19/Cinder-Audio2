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
	
template <typename SampleT>
class BufferT {
public:
	typedef SampleT SampleType;
	enum Format { Interleaved, NonInterleaved };

	BufferT( size_t numChannels = 0, size_t numFrames = 0, Format initialFormat = Format::Interleaved ) : mNumChannels( numChannels ), mNumFrames( numFrames ), mFormat( initialFormat ) {}

	SampleT* getChannel( size_t ch ) {
		CI_ASSERT( ch < mNumChannels );
		asFormat( NonInterleaved );
		
		return &mData[ch * mNumFrames];
	}

	size_t getSize() const	{ return mNumChannels * mNumFrames; }

	SampleT* getData() { return mData.data(); }

	void asFormat( Format fmt ) {
		if( fmt == NonInterleaved && mFormat == Interleaved )
			deinterleaveInplacePow2( mData.data(), mData.size() );
		else if( fmt == Interleaved && mFormat == NonInterleaved )
			CI_ASSERT( false ); // TODO: interleave
	}

private:
	std::vector<SampleT> mData;
	size_t mNumChannels, mNumFrames;
	Format mFormat;
};

typedef BufferT<float> Buffer;

} // namespace audio2