/*
 Use the "hxRunLibrary" code to run the static version of the code
*/
#include <hxcpp.h>
#include <string>

#include <whb/log.h>

extern "C"
{
void __hxcpp_lib_main();


std::string sgResultBuffer;

HXCPP_EXTERN_CLASS_ATTRIBUTES const HX_CHAR *hxRunLibrary()
{
   try { 
      __hxcpp_lib_main();
      return 0;
   }
   catch ( Dynamic d ) {
      sgResultBuffer = d->toString().__s;
      OSReport("[hxRunLibrary] ERROR: %s\n", sgResultBuffer.c_str());
      WHBLogPrintf("[hxRunLibrary] ERROR: %s\n", sgResultBuffer.c_str());
      return sgResultBuffer.c_str();
   }
}



}
