#pragma once

#include "cinder/DataSource.h"

// TODO: is it worth combining Url streaming into this stuff as one I/O set of base classes?

namespace audio2 {

	// ???: InputFile / TargetFile?

	class SourceFile {
		SourceFile( ci::DataSourceRef dataSource );
	};

	class TargetFile {

	};


} // namespace audio2