#pragma once

#include "audio2/Buffer.h"

#include "cinder/DataSource.h"

namespace audio2 {
	
typedef std::shared_ptr<class SourceFile> SourceFileRef;
typedef std::shared_ptr<class TargetFile> TargetFileRef;

class SourceFile {
  public:
	SourceFile( ci::DataSourceRef dataSource, size_t numChannels, size_t sampleRate )
	: mFileSampleRate( 0 ), mFileNumChannels( 0 ), mNumFrames( 0 ), mNumChannels( numChannels ), mSampleRate( sampleRate ), mNumFramesPerRead( 4096 )
	{}

	virtual size_t	getSampleRate() const				{ return mSampleRate; }
	virtual void	setSampleRate( size_t sampleRate )	{ mSampleRate = sampleRate; }
	virtual size_t	getNumChannels() const				{ return mNumChannels; }
	virtual void	setNumChannels( size_t channels )	{ mNumChannels = channels; }
	virtual size_t	getNumFramesPerRead() const			{ return mNumFramesPerRead; }
	virtual void	setNumFramesPerRead( size_t count )	{ mNumFramesPerRead = count; }

	virtual size_t	getNumFrames() const					{ return mNumFrames; }
	virtual size_t	getFileNumChannels() const				{ return mFileNumChannels; }
	virtual size_t	getFileSampleRate() const				{ return mFileSampleRate; }

	//! \note buffer must be large enough to hold \a getNumFramesPerRead()
	virtual size_t read( BufferRef buffer, size_t readPosition ) = 0;

	virtual BufferRef loadBuffer() = 0;

  protected:
	size_t mSampleRate, mNumChannels, mNumFrames, mFileSampleRate, mFileNumChannels, mNumFramesPerRead;
};

class TargetFile {

};

//BufferRef loadAudio( SourceFileRef sourcefile );

} // namespace audio2