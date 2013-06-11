#pragma once

#include "audio2/Buffer.h"

#include "cinder/DataSource.h"

namespace audio2 {
	
typedef std::shared_ptr<class SourceFile> SourceFileRef;
typedef std::shared_ptr<class TargetFile> TargetFileRef;

// ???: InputFile / TargetFile?

class SourceFile {
  public:
	SourceFile( ci::DataSourceRef dataSource, size_t outputNumChannels, size_t outputSampleRate ) : mSampleRate( 0 ), mNumChannels( 0 ), mNumFrames( 0 ), mOutputNumChannels( outputNumChannels ), mOutputSampleRate( outputSampleRate ), mNumFramesPerRead( 4096 )
	{}

	virtual size_t getOutputSampleRate() const				{ return mOutputSampleRate; }
	virtual void setOutputSampleRate( size_t sampleRate )	{ mOutputSampleRate = sampleRate; }
	virtual size_t getOutputNumChannels() const				{ return mOutputNumChannels; }
	virtual void setOutputNumChannels( size_t channels )	{ mOutputNumChannels = channels; }
	virtual size_t getNumFramesPerRead() const				{ return mNumFramesPerRead; }
	virtual void setNumFramesPerRead( size_t count )		{ mNumFramesPerRead = count; }

	size_t getNumFrames() const				{ return mNumFrames; }

	virtual BufferRef loadBuffer() = 0;

  protected:
	size_t mSampleRate, mNumChannels, mNumFrames, mOutputSampleRate, mOutputNumChannels, mNumFramesPerRead;
};

class TargetFile {

};

//BufferRef loadAudio( SourceFileRef sourcefile );

} // namespace audio2