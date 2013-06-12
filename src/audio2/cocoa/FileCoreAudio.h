#pragma once

#include "audio2/File.h"
#include "audio2/GeneratorNode.h"

#include "cinder/Thread.h"

#include <AudioToolbox/ExtendedAudioFile.h>

// TODO: implement FilePlayerNodeCoreAudio
//		- decodes and writes samples to ringbuffer on background thread
//		- pulls samples from ringbuffer in process()
//		- in a real-time graph, file reading needs to be done on a non-audio thread.
//        But in processing mode, the same thread should be used as the one that process is called from

// TODO: use a thread pool to keep the overrall number of read threads to a minimum.

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

class FilePlayerNodeCoreAudio : public FilePlayerNode {
  public:
	FilePlayerNodeCoreAudio( ci::DataSourceRef dataSource );

	void process( Buffer *buffer ) override;

  private:
	std::unique_ptr<std::thread> mReadThread;
	std::shared_ptr<SourceFileCoreAudio> mSourceFile; // TODO: consider storing a shared SourceFile in base class
};

} } // namespace audio2::cocoa
