#ifdef HX_TLS_H_OVERRIDE
// Users can define their own header to use here, but there is no API
// compatibility gaurantee for future changes.
#include HX_TLS_H_OVERRIDE
#else

#ifndef HX_TLS_INCLUDED
#define HX_TLS_INCLUDED

#ifdef HX_WINDOWS

#ifdef HX_WINRT
// Nothing
#else

#if !defined(__GNUC__) && !defined(__BORLANDC__)
#include <intrin.h>
#endif

extern "C"
{
   __declspec(dllimport) int __stdcall TlsSetValue(unsigned long dwTlsIndex, void *lpTlsValue);

   __declspec(dllimport) void *__stdcall TlsGetValue(unsigned long dwTlsIndex);

   __declspec(dllimport) unsigned long __stdcall TlsAlloc(void);
}

namespace hx
{

   template <typename DATA, bool FAST = false>
   struct TLSData
   {
      static const size_t kMaxInlineSlots = 64;

      TLSData()
      {
         mSlot = TlsAlloc();
         TlsSetValue(mSlot, 0);
#ifdef HXCPP_M64
         mFastOffset = mSlot * sizeof(void *) + 0x1480;
#else
         if (FAST || mSlot < kMaxInlineSlots)
            mFastOffset = mSlot * sizeof(void *) + 0xE10;
         else
            mFastOffset = mSlot - kMaxInlineSlots;
#endif
      }
      inline DATA *operator=(DATA *inData)
      {
         TlsSetValue(mSlot, inData);
         return inData;
      }

      inline operator DATA *()
      {
#if !defined(HXCPP_M64) && (_MSC_VER >= 1400)
         const size_t kTibExtraTlsOffset = 0xF94;

         if (FAST || mSlot < kMaxInlineSlots)
            return (DATA *)__readfsdword(mFastOffset);

         DATA **extra = (DATA **)(__readfsdword(kTibExtraTlsOffset));
         return extra[mFastOffset];
#elif (_MSC_VER >= 1400) & !defined(HXCPP_DEBUG) && !defined(HXCPP_ARM64) // 64 bit version...
         if (mSlot < 64)
            return (DATA *)__readgsqword(mFastOffset);
         else
            return (DATA *)TlsGetValue(mSlot);
#else
         return (DATA *)TlsGetValue(mSlot);
#endif
      }

      int mSlot;
      int mFastOffset;
   };

} // end namespace hx

#define DECLARE_TLS_DATA(TYPE, NAME) \
   ::hx::TLSData<TYPE> NAME;
#define DECLARE_FAST_TLS_DATA(TYPE, NAME) \
   ::hx::TLSData<TYPE, true> NAME;
#define EXTERN_TLS_DATA(TYPE, NAME) \
   extern ::hx::TLSData<TYPE> NAME;
#define EXTERN_FAST_TLS_DATA(TYPE, NAME) \
   extern ::hx::TLSData<TYPE, true> NAME;

#endif
#elif defined(HX_CAFE)

#include <coreinit/thread.h>
#include <coreinit/debug.h>
#include <coreinit/atomic.h> 

namespace hx
{

   // Global slot counter, kept OUTSIDE the template so that every
   // instantiation of TLSData<...> (regardless of the DATA type) shares the
   // same counter. If the counter lived inside the template, each distinct
   // TLSData<TypeA,...> / TLSData<TypeB,...> would get its own independent
   // counter starting at 0, causing different TLS variables to silently
   // collide on the same OSThreadSpecificID slot.
   struct TLSSlotAllocator
   {
      static int Alloc()
      {
         static volatile uint32_t sNextSlot = 0;
         int slot = OSAddAtomic((volatile int32_t*)&sNextSlot, 1);

         // OS_THREAD_SPECIFIC_0 .. OS_THREAD_SPECIFIC_13 (0..13) are available
         // for application use. OS_THREAD_SPECIFIC_WUT_RESERVED_0/1 (14, 15)
         // are reserved by WUT for internal use and must never be touched.
         if (slot > OS_THREAD_SPECIFIC_13)
         {
            OSFatal("HXCPP Critical Error: exhausted available OSThreadSpecific slots "
                    "(0-13). Cannot allocate another thread-local variable.");
         }

         return slot;
      }
   };

   // Lazily allocates its OSThreadSpecificID slot on first use (in Set/Get),
   // rather than in the constructor. This avoids relying on C++ static
   // initialization order: on Wii U, TLSData instances are often declared as
   // global objects, and if the slot were allocated in the constructor there
   // would be no guarantee the constructor has run before the first Set()/Get()
   // call from another global's initialization or from InitAlloc().
   template <typename DATA, bool FAST = false>
   struct TLSData
   {
      TLSData() : mIndex(-1) {}

      void EnsureSlot()
      {
         if (mIndex < 0)
            mIndex = TLSSlotAllocator::Alloc();
      }

      DATA *Get()
      {
         if (mIndex < 0)
            return 0;
         return (DATA *)OSGetThreadSpecific(static_cast<OSThreadSpecificID>(mIndex));
      }
      void Set(DATA *inData)
      {
         EnsureSlot();
         OSSetThreadSpecific(static_cast<OSThreadSpecificID>(mIndex), (void *)inData);
      }
      inline DATA *operator=(DATA *inData)
      {
         Set(inData);
         return inData;
      }
      inline operator DATA *() { return Get(); }

      int mIndex;
   };

} // end namespace hx

#define DECLARE_TLS_DATA(TYPE, NAME) \
   ::hx::TLSData<TYPE> NAME;
#define DECLARE_FAST_TLS_DATA(TYPE, NAME) \
   ::hx::TLSData<TYPE, true> NAME;
#define EXTERN_TLS_DATA(TYPE, NAME) \
   extern ::hx::TLSData<TYPE> NAME;
#define EXTERN_FAST_TLS_DATA(TYPE, NAME) \
   extern ::hx::TLSData<TYPE, true> NAME;

#else

#include <pthread.h>

namespace hx
{

   template <typename DATA, bool FAST = false>
   struct TLSData
   {
      TLSData()
      {
         pthread_key_create(&mSlot, 0);
      }
      DATA *Get()
      {
         return (DATA *)pthread_getspecific(mSlot);
      }
      void Set(DATA *inData)
      {
         pthread_setspecific(mSlot, inData);
      }
      inline DATA *operator=(DATA *inData)
      {
         pthread_setspecific(mSlot, inData);
         return inData;
      }
      inline operator DATA *() { return (DATA *)pthread_getspecific(mSlot); }

      pthread_key_t mSlot;
   };

} // end namespace hx

#endif

#ifdef HX_WINRT

#define DECLARE_TLS_DATA(TYPE, NAME) \
   __declspec(thread) TYPE *NAME = nullptr;
#define DECLARE_FAST_TLS_DATA(TYPE, NAME) \
   __declspec(thread) TYPE *NAME = nullptr;
#define EXTERN_TLS_DATA(TYPE, NAME) \
   __declspec(thread) extern TYPE *NAME;
#define EXTERN_FAST_TLS_DATA(TYPE, NAME) \
   __declspec(thread) extern TYPE *NAME;

#elif !defined(HX_CAFE)

#define DECLARE_TLS_DATA(TYPE, NAME) \
   ::hx::TLSData<TYPE> NAME;
#define DECLARE_FAST_TLS_DATA(TYPE, NAME) \
   ::hx::TLSData<TYPE, true> NAME;
#define EXTERN_TLS_DATA(TYPE, NAME) \
   extern ::hx::TLSData<TYPE> NAME;
#define EXTERN_FAST_TLS_DATA(TYPE, NAME) \
   extern ::hx::TLSData<TYPE, true> NAME;

#endif

#endif

#endif