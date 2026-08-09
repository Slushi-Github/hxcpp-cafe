#ifdef HX_WINDOWS
#define MBEDTLS_THREADING_ALT
#endif
#ifndef HX_WINDOWS
#define MBEDTLS_THREADING_PTHREAD
#endif

#define MBEDTLS_THREADING_C

#if defined(HX_CAFE)
#define MBEDTLS_TIMING_C
#endif
