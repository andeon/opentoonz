// texport_config.h
#pragma once

#ifdef _WIN32
  #ifdef TOONZLIB_EXPORTS
    #define DVAPI __declspec(dllexport)
    #define DVVAR __declspec(dllexport)
  #else
    #define DVAPI __declspec(dllimport)
    #define DVVAR __declspec(dllimport)
  #endif
  
  // Suppress STL export warnings
  #pragma warning(push)
  #pragma warning(disable : 4251)  // STL needs DLL interface
  #pragma warning(disable : 4275)  // Base class not exported
#else
  #define DVAPI
  #define DVVAR
#endif
