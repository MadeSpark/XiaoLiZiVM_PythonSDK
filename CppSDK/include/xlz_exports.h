#pragma once

#if defined(_WIN32)
#define XLZ_CALL __stdcall
#if defined(XLZSDK_EXPORTS)
#define XLZ_API extern "C" __declspec(dllexport)
#else
#define XLZ_API extern "C" __declspec(dllimport)
#endif
#else
#error "This SDK supports Windows only."
#endif

