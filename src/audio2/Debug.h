#pragma once

// note: this file will be removed at a later point

#include "cinder/app/App.h"

#define LOG_V ci::app::console() << __FUNCTION__ << " | "
#define LOG_E LOG_V << __LINE__ << " | " << " *** ERROR *** : "
