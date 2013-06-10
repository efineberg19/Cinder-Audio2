#pragma once

#include "audio2/File.h"
#include "audio2/GeneratorNode.h"

#include "cinder/Thread.h"

// TODO: in a real-time graph, file reading needs to be done on a non-audio thread.
//       But in processing mode, the same thread should be used as the one that process is called from

// TODO: use a thread pool to keep the overrall number of read threads to a minimum.

// TODO: implement file caching, possibly to a node that just reads an audio2::Buffer
// - might be a good idea to subclass FileBuffer : public Buffer, which contains file specific properties

namespace audio2 { namespace cocoa {

class SourceFileCoreAudio : public SourceFile {
  public:
	SourceFileCoreAudio( ci::DataSourceRef dataSource );
  private:
};

class FileInputNodeCoreAudio : public FileInputNode {
  public:
	FileInputNodeCoreAudio( SourceFileRef sourceFile );

	void process( Buffer *buffer ) override;

  private:
	std::unique_ptr<std::thread> mReadThread;
};

} } // namespace audio2::cocoa
