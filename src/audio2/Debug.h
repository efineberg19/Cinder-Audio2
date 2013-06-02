#pragma once

#include "audio2/assert.h"

// note: this file will be removed at a later point

#include "cinder/app/App.h"

#if defined( CINDER_COCOA )
	#define LOG_FUNCTION_CALL __PRETTY_FUNCTION__
#else
	#define LOG_FUNCTION_CALL __FUNCTION__
#endif

#define LOG_V ci::app::console() << LOG_FUNCTION_CALL << " | "
#define LOG_E LOG_V << __LINE__ << " | " << " *** ERROR *** : "
