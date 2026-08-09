#include <hxcpp.h>
#include <thread>
#include <array>
#include <atomic>
#include <hx/thread/CountingSemaphore.hpp>
#include "ThreadImpl.hpp"

#if defined(HX_CAFE)
#include <pthread.h>
#endif

namespace
{
	struct ThreadInfoHolder
	{
		hx::Object** root = nullptr;

		~ThreadInfoHolder()
		{
			#if !defined(HX_CAFE)
			if (root)
			{
				hx::GCRemoveRoot(root);

				delete root;
			}
			#endif
		}

		void set(hx::Object** _root)
		{
			root = _root;
		}

		hx::thread::ThreadImpl_obj* get() const
		{
			return root ? reinterpret_cast<hx::thread::ThreadImpl_obj*>(*root) : nullptr;
		}
	};

	#if defined(HX_CAFE)
	static void threadInfoDestructor(void *v)
	{
		delete static_cast<ThreadInfoHolder *>(v);
	}

	static pthread_key_t sThreadInfoKey;
	static pthread_once_t sThreadInfoKeyOnce = PTHREAD_ONCE_INIT;

	static void makeThreadInfoKey()
	{
		pthread_key_create(&sThreadInfoKey, threadInfoDestructor);
	}

	static ThreadInfoHolder *getThreadInfoHolder()
	{
		pthread_once(&sThreadInfoKeyOnce, makeThreadInfoKey);
		ThreadInfoHolder *h = static_cast<ThreadInfoHolder *>(pthread_getspecific(sThreadInfoKey));
		if (!h)
		{
			h = new ThreadInfoHolder();
			// inicializar según lo que haga el constructor original
			pthread_setspecific(sThreadInfoKey, h);
		}
		return h;
	}

#define tls (*getThreadInfoHolder())

#else
	thread_local ThreadInfoHolder tls;
#endif

	std::atomic_int nextThreadnumber(0);

	void run(hx::thread::ThreadImpl thread, hx::thread::Thread_obj::CreateFunction job, hx::thread::CountingSemaphore semaphore)
	{
		#if defined(HX_CAFE)
		// Reservar root en heap y registrarlo en el GC
        hx::Object **root = new hx::Object*;
        *root = thread.GetPtr();

        hx::GCAddRoot(root);

        // Guardarlo en TLS para que Thread::current() lo vea
        tls.set(root);

        // Preparar area en stack para que el GC pueda escanear la pila conservativamente.
        hx::thread::ThreadImpl_obj* info[2];
        info[0] = thread.GetPtr();
        info[1] = nullptr;

        hx::SetTopOfStack(reinterpret_cast<int*>(&info[1]), true);

        // Indicar al creador que el hilo arrancó
        semaphore->release();

        // Ejecutar el trabajo del hilo
        job();

        // Limpieza: quitar TLS y root del GC mientras el hilo aún está registrado
        tls.set(nullptr);            // evita que el destructor del pthread key intente borrar nada
        hx::GCRemoveRoot(root);
        delete root;

        // Desregistrar hilo en hxcpp (libera contextos, etc.)
        hx::UnregisterCurrentThread();
		#else
		// info[1] will the the "top of stack" - values under this
		// (ie info[0] and other stack values) will be in the GC conservative range
		// 
		// I'm assuming the array of two elements is some trick to ensure it's on the stack.
		// Maybe we could use OS specific functions to get the stack range?

		auto root = new hx::Object* { thread.GetPtr() };

		hx::GCAddRoot(root);

		tls.set(root);

		auto info = std::array<hx::thread::ThreadImpl_obj*, 2>{ thread.GetPtr(), nullptr };

		hx::SetTopOfStack(reinterpret_cast<int*>(&info[1]), true);

		// Release the creation function
		semaphore->release();

		job();

		hx::UnregisterCurrentThread();
		#endif
	}
}

hx::thread::Thread hx::thread::Thread_obj::create(CreateFunction job)
{
#ifdef EMSCRIPTEN
	return hx::Throw(HX_CSTRING("Threads are not supported on Emscripten"));
#else
	auto semaphore = new hx::thread::CountingSemaphore_obj(0);
	auto obj       = new ThreadImpl_obj(nextThreadnumber++, job, semaphore);

	hx::GCSetFinalizer(obj, ThreadImpl_obj::finalise);
	hx::GCPrepareMultiThreaded();
	
	semaphore->acquire();

	return hx::thread::Thread{ obj };
#endif
}

String hx::thread::ThreadImpl_obj::toString()
{
	return String(id);
}

hx::thread::Thread hx::thread::Thread_obj::current()
{
	#if defined(HX_CAFE)
	auto info = tls.get();
	if (nullptr == info)
	{
		info = new ThreadImpl_obj(nextThreadnumber++);

		// Crear root en heap y registrarlo
		hx::Object **root = new hx::Object *;
		*root = info;

		hx::GCSetFinalizer(info, ThreadImpl_obj::finalise);
		hx::GCAddRoot(root);

		tls.set(root);
	}

	return Thread(info);
	#else
	auto info = tls.get();
	if (nullptr == info)
	{
		// Threads created from Haxe have the TLS set, and the GC sets the TLS for the main thread.
		// So, if the TLS is null then we are in a attached foreign thread, which will just get assigned the next Id.
		// 
		// The C++ std has no way of getting the native handle of the current thread, so we need to use OS specific
		// apis to get that so we can get and set thread names.
		info = new ThreadImpl_obj(nextThreadnumber++);

		auto root = new hx::Object* { info };

		hx::GCSetFinalizer(info, ThreadImpl_obj::finalise);
		hx::GCAddRoot(root);

		tls.set(root);
	}

	return Thread{ info };
	#endif
}

int hx::thread::Thread_obj::id()
{
	auto info = tls.get();

	if (nullptr == info)
	{
		return 0;
	}
	else
	{
		return info->id;
	}
}

hx::thread::ThreadImpl_obj::ThreadImpl_obj(const int _id)
	: id(_id)
	, native(new Native())
	, scratch(std::numeric_limits<uint16_t>::max(), std::numeric_limits<uint16_t>::max())
	, slots(0, 0) { }

hx::thread::ThreadImpl_obj::ThreadImpl_obj(const int _id, Thread_obj::CreateFunction _job, CountingSemaphore _semaphore)
	: id(_id)
	, native(new Native(_job, this, _semaphore))
	, scratch(std::numeric_limits<uint16_t>::max(), std::numeric_limits<uint16_t>::max())
	, slots(0, 0)
{
}

hx::thread::ThreadImpl_obj::Native::Native(Thread_obj::CreateFunction _job, ThreadImpl _thread, CountingSemaphore _semaphore)
	: thread(run, _thread, _job, _semaphore)
	, handle(thread.native_handle())
{
	// Only Windows implements thread name getting and setting currently, and this requires the thread to be attached.
	// Threads are normally only detached once the thread object is finalised and this was causing the 32bit linux tests to hit OS resource limits.
	// This does feel like a bit of a bodge and something which will actually need to be addressed to add name setting on Linux.

#ifndef HX_WINDOWS
	thread.detach();
#endif
}

Dynamic hx::thread::ThreadImpl_obj::getSlot(const int id)
{
	return slots[id];
}

void hx::thread::ThreadImpl_obj::setSlot(const int id, const Dynamic& obj)
{
	slots->__SetItem(id, obj);
}

void hx::thread::ThreadImpl_obj::__Mark(HX_MARK_PARAMS)
{
	HX_MARK_MEMBER(scratch);
	HX_MARK_MEMBER(slots);
}

#ifdef HXCPP_VISIT_ALLOCS

void hx::thread::ThreadImpl_obj::__Visit(HX_VISIT_PARAMS)
{
	HX_VISIT_MEMBER(scratch);
	HX_VISIT_MEMBER(slots);
}

void hx::thread::ThreadImpl_obj::finalise(hx::Object* obj)
{
	auto thread = reinterpret_cast<ThreadImpl_obj*>(obj);
	auto native = std::unique_ptr<ThreadImpl_obj::Native>{ thread->native };

	if (native->thread.joinable())
	{
		native->thread.detach();
	}
}

#endif