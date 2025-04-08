#pragma once

#ifndef T_MACHINE_INCLUDED
#define T_MACHINE_INCLUDED

// Platform-specific channel order definitions
#if defined(_WIN32) || defined(i386)
#define TNZ_MACHINE_CHANNEL_ORDER_BGRM 1
#elif defined(__sgi)
#define TNZ_MACHINE_CHANNEL_ORDER_MBGR 1
#elif defined(LINUX) || defined(FREEBSD) || defined(HAIKU)
#define TNZ_MACHINE_CHANNEL_ORDER_BGRM 1
#elif defined(MACOSX)
#define TNZ_MACHINE_CHANNEL_ORDER_MRGB 1
#else
#error "Unknown platform - cannot determine channel order"
#endif

// Endianness check - now properly handled by CMake
#ifndef TNZ_LITTLE_ENDIAN
#error "TNZ_LITTLE_ENDIAN must be defined (0 for big-endian, 1 for little-endian)"
#endif

#endif
