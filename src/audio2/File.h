#pragma once

#include "cinder/DataSource.h"

// TODO: is it worth combining Url streaming into this stuff as one I/O set of base classes?

namespace audio2 {
	
typedef std::shared_ptr<class SourceFile> SourceFileRef;
typedef std::shared_ptr<class TargetFile> TargetFileRef;

// ???: InputFile / TargetFile?

class SourceFile {
  public:
	SourceFile( ci::DataSourceRef dataSource );
  private:
};

class TargetFile {

};


} // namespace audio2