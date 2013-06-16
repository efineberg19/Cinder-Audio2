#pragma once

#include "audio2/File.h"
#include "audio2/GeneratorNode.h"
#include "audio2/msw/util.h"

struct IMFSourceReader;

namespace audio2 { namespace msw {

class SourceFileMediaFoundation : public SourceFile {
  public:
	enum Format { INT_16, FLOAT_32 };

	SourceFileMediaFoundation( ci::DataSourceRef dataSource, size_t numChannels = 0, size_t sampleRate = 0 );
	virtual ~SourceFileMediaFoundation();

	size_t		read( Buffer *buffer ) override;
	BufferRef	loadBuffer() override;
	void		seek( size_t readPosition ) override;

	void	setSampleRate( size_t sampleRate ) override;
	void	setNumChannels( size_t channels ) override;

  private:
	void updateOutputFormat();
	void storeAttributes();
	
	std::unique_ptr<::IMFSourceReader, ComReleaser> mSourceReader;
	Format mSampleFormat;

	size_t mReadPos; // TODO: remove if not needed
	float mSeconds;
	bool mCanSeek;
};

} } // namespace audio2::msw
