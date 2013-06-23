#pragma once

#include "audio2/Context.h"

#include "cinder/Cinder.h"
#include "cinder/Exception.h"

namespace audio2 {

class Converter {
  public:
	struct Format {
		Format() : mSampleRate( 0 ), mChannels( 0 )	{}

		Format& sampleRate( size_t sr )	{ mSampleRate = sr; return *this; }
		Format& channels( size_t ch )	{ mChannels = ch; return *this; }

		size_t getSampleRate() const	{ return mSampleRate; }
		size_t getChannels() const		{ return mChannels; }
	  private:
		size_t mSampleRate, mChannels;
	};

	Converter( const Format &sourceFormat, const Format &destFormat ) : mSourceFormat( sourceFormat ), mDestFormat( destFormat ) {}
	virtual ~Converter() {}

	virtual void convert( Buffer *sourceBuffer, Buffer *destBuffer ) = 0;

protected:
	Format mSourceFormat, mDestFormat;
};

void printGraph( ContextRef graph );
void printDevices();

class AudioExc : public ci::Exception {
public:
	AudioExc( const std::string &description ) : mDescription( description )	{}
	virtual const char* what() const throw()	{ return mDescription.c_str(); }
protected:
	std::string mDescription;
};

class AudioDeviceExc : public AudioExc {
public:
	AudioDeviceExc( const std::string &description ) : AudioExc( description )	{}
};

class AudioContextExc : public AudioExc {
public:
	AudioContextExc( const std::string &description ) : AudioExc( description )	{}
};

class AudioFormatExc : public AudioExc {
public:
	AudioFormatExc( const std::string &description ) : AudioExc( description )	{}
};

class AudioParamExc : public AudioExc {
public:
	AudioParamExc( const std::string &description ) : AudioExc( description )	{}
};

class AudioFileExc : public AudioExc {
public:
	AudioFileExc( const std::string &description ) : AudioExc( description )	{}
};

} // namespace audio2