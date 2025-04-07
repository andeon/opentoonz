// texport_config.h
#pragma once

#ifndef DVAPI  // Only define if not already defined
  #ifdef _WIN32
    #ifdef TOONZLIB_EXPORTS
      #define DVAPI __declspec(dllexport)
    #else
      #define DVAPI __declspec(dllimport)
    #endif
  #else
    #define DVAPI
  #endif
#endif

#ifndef DVVAR  // Only define if not already defined
  #define DVVAR DVAPI  // For consistency
#endif

#ifdef _WIN32
  #pragma warning(push)
  #pragma warning(disable : 4251)
  #pragma warning(disable : 4275)
#endif
