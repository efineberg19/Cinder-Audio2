#include "audio2/File.h"

#include "cinder/Cinder.h"

#if defined( CINDER_COCOA )
#include "audio2/cocoa/FileCoreAudio.h"
#elif defined( CINDER_MSW )
#include "audio2/msw/FileMediaFoundation.h"
#endif

namespace audio2 {

// TODO: this should be replaced with a genericized registrar derived from the ImageIo stuff.

SourceFileRef SourceFile::create(  ci::DataSourceRef dataSource, size_t numChannels, size_t sampleRate )
{
#if defined( CINDER_COCOA )
	return SourceFileRef( new cocoa::SourceFileCoreAudio( dataSource, numChannels, sampleRate ) );
#elif defined( CINDER_MSW )
	return SourceFileRef( new msw::SourceFileMediaFoundation( dataSource, numChannels, sampleRate ) );
#endif
}
	

} // namespace audio2