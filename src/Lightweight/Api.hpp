// SPDX-License-Identifier: Apache-2.0

#pragma once

#if defined(__GNUC__)
    #define LIGHTWEIGHT_NO_EXPORT __attribute__((visibility("hidden")))
    #define LIGHTWEIGHT_EXPORT    __attribute__((visibility("default")))
    #define LIGHTWEIGHT_IMPORT    /*!*/
#elif defined(_MSC_VER)
    #define LIGHTWEIGHT_NO_EXPORT /*!*/
    #define LIGHTWEIGHT_EXPORT    __declspec(export)
    #define LIGHTWEIGHT_IMPORT    __declspec(import)
#endif

#if defined(BUILD_LIGHTWEIGHT) && defined(LIGHTWEIGHT_SHARED)
    #define LIGHTWEIGHT_API LIGHTWEIGHT_EXPORT
#else
    #define LIGHTWEIGHT_API LIGHTWEIGHT_IMPORT
#endif
