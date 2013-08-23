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

#include "audio2/Context.h"

#include "cinder/Cinder.h"
#include "cinder/Exception.h"

namespace cinder { namespace audio2 {

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

} } // namespace cinder::audio2