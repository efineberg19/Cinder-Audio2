#pragma once

#include "audio2/File.h"
#include "audio2/GeneratorNode.h"

#include <AudioToolbox/ExtendedAudioFile.h>

namespace audio2 { namespace cocoa {

class SourceFileCoreAudio : public SourceFile {
  public:
	SourceFileCoreAudio( ci::DataSourceRef dataSource, size_t outputNumChannels = 0, size_t outputSampleRate = 0 );

	BufferRef loadBuffer() override;

	// TODO: need a method that fills chunks
//	void read( );

  private:
	std::shared_ptr<::OpaqueExtAudioFile> mExtAudioFile;
};

} } // namespace audio2::cocoa
