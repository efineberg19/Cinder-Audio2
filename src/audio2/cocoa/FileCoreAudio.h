#pragma once

#include "audio2/File.h"
#include "audio2/GeneratorNode.h"
#include "audio2/cocoa/Util.h"

#include <AudioToolbox/ExtendedAudioFile.h>

namespace audio2 { namespace cocoa {

class SourceFileCoreAudio : public SourceFile {
  public:
	SourceFileCoreAudio( ci::DataSourceRef dataSource, size_t numChannels = 0, size_t sampleRate = 0 );

	size_t		read( Buffer *buffer, size_t readPosition ) override;
	void		seek( size_t readPosition ) override;
	BufferRef	loadBuffer() override;

	void	setSampleRate( size_t sampleRate ) override;
	void	setNumChannels( size_t channels ) override;

  private:
	void updateOutputFormat();
	
	std::shared_ptr<::OpaqueExtAudioFile> mExtAudioFile;
	AudioBufferListRef mBufferList;
};

} } // namespace audio2::cocoa
