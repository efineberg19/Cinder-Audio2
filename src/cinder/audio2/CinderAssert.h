/*
 Copyright (c) 2014, The Cinder Project

 This code is intended to be used with the Cinder C++ library, http://libcinder.org

 Redistribution and use in source and binary forms, with or without modification, are permitted provided that
 the following conditions are met:

    * Redistributions of source code must retain the above copyright notice, this list of conditions and
	the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and
	the following disclaimer in the documentation and/or other materials provided with the distribution.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
 WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 POSSIBILITY OF SUCH DAMAGE.
*/

// define to break into the debugger when assertion fails, rather than abort.
//#define CI_ASSERT_DEBUG_BREAK

// No include guards, so that NDEBUG can redefine the behavior of assertions per compilation unit

#ifdef NDEBUG
#   ifdef CI_ASSERT
#       undef CI_ASSERT
#   endif
#   ifdef CI_ASSERT_MSG
#       undef CI_ASSERT_MSG
#   endif
#   if defined( CI_ASSERT_DEBUG_BREAK )
namespace cinder {
	void assertion_failed( char const * expr, char const * function, char const * file, long line );
	void assertion_failed_msg( char const * expr, char const * msg, char const * function, char const * file, long line );
}
#       if defined ( _MSC_VER )
#           define __func__ __FUNCTION__
#       endif
#       if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901)
#           define CI_ASSERT( expr ) ((expr)? ((void)0): ::cinder::assertion_failed( #expr, __func__, __FILE__, __LINE__ ))
#           define CI_ASSERT_MSG( expr, msg ) ((expr)? ((void)0): ::cinder::assertion_failed_msg( #expr, msg, __func__, __FILE__, __LINE__ ))
#       else
#           define CI_ASSERT( expr ) ((expr)? ((void)0): ::cinder::assertion_failed( #expr, "", __FILE__, __LINE__ ))
#           define CI_ASSERT( expr, msg ) ((expr)? ((void)0): ::cinder::assertion_failed( #expr, "", __FILE__, __LINE__ ))
#       endif
#   else
#       define CI_ASSERT(expr) assert(expr)
#       define CI_ASSERT_MSG( expr, msg ) assert(expr)
#   endif
#else
#   define CI_ASSERT(expr) ((void)0)
#   define CI_ASSERT_MSG( expr, msg ) ((void)0)
#endif