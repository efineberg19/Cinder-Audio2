#pragma once

#include <boost/assert.hpp>

#define CI_ASSERT( expr ) BOOST_ASSERT( expr )
#define CI_ASSERT_MSG( expr, msg ) BOOST_ASSERT_MSG(expr, msg )
