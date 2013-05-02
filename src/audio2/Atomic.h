#pragma once

#include "cinder/Cinder.h"

#if (defined( _MSC_VER ) && ( _MSC_VER >= 1700 )) || defined( _LIBCPP_VERSION )
	#include <atomic>
#else
	#include <boost/atomic.hpp>
	namespace std {
		using boost::atomic;
	}
#endif