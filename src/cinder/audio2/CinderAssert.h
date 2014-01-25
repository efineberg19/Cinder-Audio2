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

// This file has no include guards, so users can redefine the behavior of assertions per compilation unit
//
// Parameters users can define:
// - CI_DISABLE_ASSERTS: disables all asserts, they become no-op's.
// - CI_ENABLE_ASSERT_HANDLER: if this is set, users must define assertionFailed() and assertionFailedMessage()
//	 to handle a failed assertion.
// - CI_ASSERT_DEBUG_BREAK: overrides default assertion behavoir to break into the debugger instead of
//	 aborting. Cannot be used in conjunction with CI_ENABLE_ASSERT_HANDLER.

#if defined( CI_ASSERT )
	#undef CI_ASSERT
#endif
#if defined( CI_ASSERT_MSG )
	#undef CI_ASSERT_MSG
#endif

#if ! defined( CI_DISABLE_ASSERTS )

	#if defined( CI_ASSERT_DEBUG_BREAK )

		#if defined( CI_ENABLE_ASSERT_HANDLER )
			#error "assert handler already enabled, cannot enable debug break assert handler"
		#endif
		#define CI_ENABLE_ASSERT_HANDLER

	#endif // defined( CI_ASSERT_DEBUG_BREAK )

	#if defined( CI_ENABLE_ASSERT_HANDLER )

		#if defined ( _MSC_VER ) && ! defined( __func__ )
			#define __func__ __FUNCTION__
		#endif

		#if defined( CI_ASSERT_DEBUG_BREAK )

			#if ! defined( CI_ASSERT_DEBUG_BREAK_H )
				#define CI_ASSERT_DEBUG_BREAK_H

				// defined in CinderAssert.cpp
				namespace cinder {
					void assertionFailedBreak( char const *expr, char const *function, char const *file, long line );
					void assertionFailedMessageBreak( char const *expr, char const *msg, char const *function, char const *file, long line );
				}

				#define CI_ASSERT( expr ) ( (expr) ? ( (void)0) : ::cinder::assertionFailedBreak( #expr, __func__, __FILE__, __LINE__ ) )
				#define CI_ASSERT_MSG( expr, msg ) ( (expr) ? ( (void)0) : ::cinder::assertionFailedMessageBreak( #expr, msg, __func__, __FILE__, __LINE__ ) )
			#endif // CI_ASSERT_DEBUG_BREAK_H

		#else // CI_ASSERT_DEBUG_BREAK

				// User must define these functions
				namespace cinder {
					//! Called when CI_ASSERT() fails
					void assertionFailed( char const *expr, char const *function, char const *file, long line );
					//! Called when CI_ASSERT_MSG() fails
					void assertionFailedMessage( char const *expr, char const *msg, char const *function, char const *file, long line );
				}

			#define CI_ASSERT( expr ) ( (expr) ? ( (void)0) : ::cinder::assertionFailed( #expr, __func__, __FILE__, __LINE__ ) )
			#define CI_ASSERT_MSG( expr, msg ) ( (expr) ? ( (void)0) : ::cinder::assertionFailedMessage( #expr, msg, __func__, __FILE__, __LINE__ ) )

		#endif // user defined handlers

	#else // defined( CI_ENABLE_ASSERT_HANDLER )

		#include <cassert>
		#define CI_ASSERT( expr ) assert( expr )
		#define CI_ASSERT_MSG( expr, msg ) assert( expr && msg )

	#endif // defined( CI_ASSERT_DEBUG_BREAK )

#else

	#define CI_ASSERT(expr) ((void)0)
	#define CI_ASSERT_MSG( expr, msg ) ((void)0)

#endif