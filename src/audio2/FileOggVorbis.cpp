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

#include "audio2/FileOggVorbis.h"
#include "audio2/audio.h"
#include "audio2/Debug.h"

using namespace std;

namespace cinder { namespace audio2 {

SourceFileImplOggVorbis::SourceFileImplOggVorbis( const DataSourceRef &dataSource, size_t numChannels, size_t sampleRate )
	: SourceFile( dataSource, numChannels, sampleRate )
{
	int status = ov_fopen( dataSource->getFilePath().c_str(), &mOggVorbisFile );
	if( status )
		throw AudioFileExc( string( "Failed to open Ogg Vorbis file with error: " ), (int32_t)status );


	LOG_V << "open success, ogg file info: " << endl;
	// print comments plus a few lines about the bitstream we're decoding
	char **ptr=ov_comment( &mOggVorbisFile, -1 )->user_comments;
	while( *ptr )
		app::console() << *ptr++ << endl;

	vorbis_info *vi = ov_info( &mOggVorbisFile, -1 );
    mNumChannels = vi->channels;
    mSampleRate = vi->rate;

	app::console() << "\tversion: " << vi->version << endl;
	app::console() << "\tBitstream is " << mNumChannels << " channel, " << mSampleRate << "Hz" << endl;
	app::console() << "\tEncoded by: " << ov_comment( &mOggVorbisFile, -1 )->vendor << endl;

	ogg_int64_t totalFrames = ov_pcm_total( &mOggVorbisFile, -1 );
    mNumFrames = static_cast<uint32_t>( totalFrames );
	app::console() << "\tframes: " << mNumFrames << endl;
}

SourceFileImplOggVorbis::~SourceFileImplOggVorbis()
{
	ov_clear( &mOggVorbisFile );
}

size_t SourceFileImplOggVorbis::read( Buffer *buffer )
{
	return 0;
}

BufferRef SourceFileImplOggVorbis::loadBuffer()
{
	if( mReadPos != 0 )
		seek( 0 );

	BufferRef result( new Buffer( mNumFrames, mNumChannels ) );

	while( true ) {
        // clarification on ov_read_float params: returns framesRead. buffer is array of channels
        // http://lists.xiph.org/pipermail/vorbis-dev/2002-January/005500.html
        float **pcm;
		int current_section;
        long numFramesRead = ov_read_float( &mOggVorbisFile, &pcm, (int)mMaxFramesPerRead, &current_section );
		//        console() << numFramesRead << ", ";
		if ( ! numFramesRead ) {
            break; // EOF
		}
        else if( numFramesRead < 0 ) {
            LOG_E << "stream error." << endl;
            return result;
		}
        else {
            for( int i = 0; i < mNumChannels; i++ ) {
                memcpy( &result->getChannel( i )[mReadPos], pcm[i], numFramesRead * sizeof(float) ); // TODO: use copy or something else abstracted
            }
            mReadPos += numFramesRead;
		}
	}
	//    console() << endl;

	return result;
}

void SourceFileImplOggVorbis::seek( size_t readPositionFrames )
{
	if( readPositionFrames >= mNumFrames )
		return;

	int status = ov_pcm_seek( &mOggVorbisFile, (ogg_int64_t)readPositionFrames );
	CI_ASSERT( ! status );

	mReadPos = readPositionFrames;
}

} } // namespace cinder::audio2