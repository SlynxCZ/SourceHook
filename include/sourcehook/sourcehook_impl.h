/* ======== SourceHook ========
* vim: set ts=4 sw=4 tw=99 noet:
* Copyright (C) 2004-2010 Metamod:Source Development Team
* No warranties of any kind
*
* License: zlib/libpng
*
* Author(s): Pavol "PM OnoTo" Marko
* ============================
*/

#ifndef __SOURCEHOOK_IMPL_H__
#define __SOURCEHOOK_IMPL_H__

#include "sourcehook.h"
#include "sh_memory.h"
#include "sh_list.h"
#include "sh_vector.h"
#include "sh_tinyhash.h"
#include "sh_stack.h"

/*

IMPLEMENTATION INFO

---------------------------------------
 :TODO: update ???

Protos ("Prototypes")
	The purpose of protos is to provide the amount of type information about a function
	which is required to be able to execute a function call without corrupting the stack.
	Our protos do not fully do this, but they provide the size of the return value, the number of
	parameters, and the size of each parameter, which is enough for most situations.

	There are two version of protos:
	OLD:
		C-Style strings.

		0_void:
		"attrib"
		1_void:
		"attrib|param1_type"
		2_void:
		"attrib|param1_type|param2_type
		0:
		"attrib|ret_type"
		1:
		"attrib|ret_type|param1_type"
		2:
		"attrib|ret_type|param2_type"

		Old protos are deprecated.

	NEW:
		New protos are in fact pointers to the ProtoInfo structure (see sourcehook.h)

	Old protos begin with a non-zero byte, new protos begin with a zero byte.

	Protos are usually stored in a CProto instance.

---------------------------------------
Hook managers and hook manager containers
	Each hookman container is tied to _one_ proto/vtable index/vtable offset info.
	Hookman containers then contain a list of hook managers provided by plugins, sorted by version.
	(higher versions come first)

	Duplicate hook managers are ignored (ie. hook managers where proto, vtable index, vtable offset,
	plugin, version are the same as in an already exisiting hook manager)

	A new hook manager is always added to the end of the version group in the corresponding
	hook container. 

	If the new hook manager was added to the beginning of the container (which only happens if
	it is the first one or if it has a higher version than the previously first hook manager),
	the now second hook manager is shut down and the new hook manager takes its job.

	A "hook manager container id" (HMCI) consits of three values: proto, vtbl index, vtbl offset.
---------------------------------------
Hooks 
	When adding a hook, first the proposed hook manager is added to the corresponding
	hook manager container as described above.

	Then the first hook manager in the the manhook container is chosen to handle the function.

	Removing a hook does not neccessarily unreigster the plugin's hook manager. In order to do this,
	use RemoveHookManager or UnloadPlugin/

	Hooks can be paused - they remain in memory but they are not called. In SH, the hook iterator
	classes handle pausing transparently.

	The hook loop is supposed to call ShouldContinue before each iteration. This makes hook handlers
	able to remove themselves.

---------------------------------------
Call classes

	!! deprecated !!   - see below (new SH_CALL)

	Call classes are identified by a this pointer and an instance size

	We use the instance size because a derived class instance and a base class instance could
	have the same this pointers, and we want to avoid that the derived class
	(which could be bigger) gets the same callclass as the base class (mainly if the
	base class' callclass was requested first).

	Call classes are reference counted.

	The original function pointers are stored in a vector (in order to allow fast random access).
	These vectors are stored as the value type of a hash. The key type is int and represents the
	vtable offset.

	If the hash key doesn't exist or the vtblidx is out of range or the corresponding element in the
	vector is NULL, there is no hook on that function.

---------------------------------------
Recalls
	Recalls are used for the META_RETURN_(VALUE_)NEWPARAMS macros, ie. to change the parameters
	in the hook loop on the fly.
	
	First, the macro calls DoRecall(), then it calls the function the hook is attached to -> it 
	calls the hookfunc. SourceHook makes sure that the newly invoked hook loop starts where the last
	one left off and that status variables like status, previous result, override return are kept.
	When this recurisvely called hookfunc returns, the macro returns what it returned
	(using MRES_SUPERCEDE). CSourceHookImpl returns false from ShouldContinue so the original hook loop
	is abandonned.

Post Recalls
	People wanted to be able to use META_RETURN_(VALUE_)NEWPARAMS from post hooks as well. Crazy people!
	Anyway, for this, we have to know where a hook handler is. Is it executing pre or post hooks at the moment?
	The only way we can know this is watching when it calls CHookList::GetIter(). So CHookList gets a new variable:
	m_RequestedFlag. It also gets two new functions: RQFlagReset() and RQFlagGet().
	HookLoopBegin() calls RQFlagReset on both hooklists of the iface; then DoRecall() checks whether the postlist's 
	RQ flag is set. if yes, the hook loop is in post mode.

	So, what a about a recall in post mode? The first ShouldContinue returns false and sets Status to supercede. 
	This way the pre hooks and the function call will be skipped. Then, then next ShouldContinue returns true, so we get
	into the post hooks. HA!

Return Values in Post Recalls
	The easy case is when we already have an override return value. In this case, the status register gets transferred,
	and so does the override return pointer. So, basically, everything is ok.

	However, what happens if we don't? ie. the status register is on MRES_IGNORED? In this case we'd have to transfer the
	orig ret value. But we can't: There's no way to tell the hookfunc: "Use this as orig ret pointer". It uses its own.
	So, we emulate it. GetOrigRet will return the orig ret pointer from the old hook loop. If still no one overrides it,
	we'd have to return it. BUT! HOW TO DO THIS? Check out SH_RETURN(). First calls HookLoopEnd(), then decides whether
	to return the override retval or the orig retval. But it doesn't ask for a new override return. So we give the function
	the last orig return value as its new override return value; but leave status where it is so everything works, and in
	HookLoopEnd we make sure that status is high enough so that the override return will be returned. crazy.

	All this stuff could be much less complicated if I didn't try to preserve binary compatibility :)

VP Hooks
	VP hooks are hooks which are called on a vfnptr, regardless of the this pointer with which it was called. They are
	implemented as a special CIface instance with m_Ptr = NULL. All Hook Lists have a new "ListCatIterator" which
	virtually concatenates the NULL-interface-hook-list with their normal hook list.


	I'm afraid that with the addition of Recalls and VP Hooks, SourceHook is now a pretty complex and hacked-together
	binary compatible beast which is pretty hard to maintain unless you've written it :)

New SH_CALL
	The addition of VP hooks messed up the Call Classes concept (see above) - call classes are bound to an
	instance pointer; they only work on one of the hooked instances. But VP hooks are called on all instances.
	
	That's why now, SH_CALL takes an instance pointer instead of a callclass pointer. It basically does this:
	1) call SH_GLOB_PTR->SetIgnoreHooks(vfnptr)
	2) call this->*mfp
	3) call SH_GLOB_PTR->ResetIgnoreHooks(vfnptr)

	SourceHook stroes the "ignored vfnptr" and makes CVfnPtr::FindIface return NULL if the CVfnPtr instance
	corresponds to the ignored vfnptr. This way the hook manager thinks that the instance isn't hooked, and calls
	the original function. Everything works fine. This works even for VP hooks.
*/

#include "sourcehook_impl_cproto.h"
#include "sourcehook_impl_chookmaninfo.h"
#include "sourcehook_impl_chook.h"
#include "sourcehook_impl_ciface.h"
#include "sourcehook_impl_cvfnptr.h"
#include "sourcehook_impl_chookidman.h"
#include <cstdio>
#include <stdarg.h>
#include <shared_mutex>
#include <unordered_map>

extern SourceHook::ISourceHook *g_SHPtr;

namespace SourceHook
{
	/**
	*	@brief The SourceHook implementation
	*/

	namespace Impl
	{
		enum SH_LOG {
			VERBOSE,
			NORMAL,
			TEST,
			NONE,
		};

		extern SH_LOG sh_log_level;

		template<typename... Args>
		inline void SH_DEBUG_LOG(SH_LOG log_level, const char* message, Args... args)
		{
#ifndef SOURCEHOOK_TESTS
			if (log_level < sh_log_level) {
				return;
			}

			SH_GLOB_SHPTR->LogDebug(message, args...);
#endif
		}

		// A CIface::GetHookListMutex() shared lock, made safe for the same
		// thread to nest -- which real hook usage genuinely does: a handler
		// can, directly or via something it calls, dispatch a *fresh* (non-
		// recall) call back into the same hooked interface from within its
		// own Pre/Post execution, which needs to shared-lock the very same
		// mutex this thread is already holding shared for the outer call.
		// A bare std::shared_mutex has no such guarantee -- the standard
		// explicitly allows an implementation to deadlock a same-thread
		// nested lock_shared() if a writer happened to queue in between the
		// two calls (writer-starvation avoidance). This is the same problem,
		// and the same fix, as KHook's DetourCapsule::RecursiveLockUnlockShared
		// (detour.cpp): a thread_local refcount per mutex instance, only
		// really locking/unlocking on the first-in/last-out transition, so
		// nested shared acquisitions on the *same* thread never touch the
		// underlying mutex a second time.
		//
		// Plain Acquire()/Release() static calls, not an RAII scope guard:
		// the acquiring call (CSourceHookImpl::SetupHookLoop, on the fresh-
		// context path) and the matching release (CSourceHookImpl::
		// EndContext) are two separate function invocations, potentially far
		// apart in the call stack (the hooked function itself runs, plus
		// every Pre/Post handler, in between) -- there's no single scope to
		// attach an RAII object to. CHookContext::m_LockedIface (below)
		// records which CIface's lock a given context is holding, so
		// EndContext knows what to release, and whether to at all.
		class CReentrantSharedLock
		{
		public:
			static void Acquire(std::shared_mutex &mtx)
			{
				int &count = Counts()[&mtx];
				if (count++ == 0)
					mtx.lock_shared();
			}

			static void Release(std::shared_mutex &mtx)
			{
				auto &counts = Counts();
				auto it = counts.find(&mtx);
				if (it != counts.end() && --(it->second) == 0)
				{
					mtx.unlock_shared();
					counts.erase(it);
				}
			}

			// True if *this* thread currently holds `mtx` shared via Acquire()
			// above -- used by CMaybeExclusiveLock (below) to detect the
			// "AddHook()/RemoveHookByID() called on the same iface from
			// inside one of its own currently-running Pre/Post handlers"
			// case, which a plain std::unique_lock would self-deadlock on
			// (this thread already holds the mutex shared; requesting
			// exclusive on top of that blocks forever, since a shared_mutex
			// has no safe same-thread shared->exclusive upgrade).
			static bool HeldByThisThread(std::shared_mutex &mtx)
			{
				auto &counts = Counts();
				auto it = counts.find(&mtx);
				return it != counts.end() && it->second > 0;
			}

		private:
			static std::unordered_map<std::shared_mutex *, int> &Counts()
			{
				static thread_local std::unordered_map<std::shared_mutex *, int> s_Counts;
				return s_Counts;
			}
		};

		// RAII wrapper for the AddHook()/RemoveHookByID() mutation sites:
		// takes a real exclusive lock on `mtx`, UNLESS this thread already
		// holds it shared (CReentrantSharedLock::HeldByThisThread), in which
		// case it does nothing and the mutation proceeds "protected" only by
		// the fact that no *other* thread can be holding the exclusive lock
		// right now (a shared_mutex guarantees that while any shared holder,
		// including this thread, is active). That's sound against a
		// concurrent writer; it is NOT a full substitute for the exclusive
		// lock against a *different* thread that's concurrently holding the
		// mutex shared and iterating the very list this thread is about to
		// mutate -- reentrant self-mutation from within a running handler
		// while a genuinely different thread is simultaneously dispatching
		// through the *same* iface is a narrower, residual gap this pass
		// does not fully close (documented here rather than silently
		// accepted: closing it fully needs either a true lock-free list or
		// deferring same-thread reentrant mutations the way SH already
		// defers plugin unloads via PendingUnload). What this reliably
		// prevents is the *common*, guaranteed-deadlock case: a handler
		// adding/removing its own hook on the interface it's currently
		// being called through.
		class CMaybeExclusiveLock
		{
		public:
			explicit CMaybeExclusiveLock(std::shared_mutex &mtx) : m_Mutex(mtx)
			{
				m_TookLock = !CReentrantSharedLock::HeldByThisThread(mtx);
				if (m_TookLock)
					m_Mutex.lock();
			}

			~CMaybeExclusiveLock()
			{
				if (m_TookLock)
					m_Mutex.unlock();
			}

			CMaybeExclusiveLock(const CMaybeExclusiveLock &) = delete;
			CMaybeExclusiveLock &operator=(const CMaybeExclusiveLock &) = delete;

		private:
			std::shared_mutex &m_Mutex;
			bool m_TookLock;
		};

		struct CHookContext : IHookContext
		{
			CHookContext() : m_CleanupTask(NULL)
			{
			}

			enum State
			{
				State_Born,
				State_Pre,
				State_PreVP,
				State_Post,
				State_PostVP,
				State_OrigCall,
				State_Dead,

				// Special
				State_Ignore,
				State_Recall_Pre,
				State_Recall_PreVP,
				State_Recall_Post,
				State_Recall_PostVP
			};

			int m_State;
			List<CHook>::iterator m_Iter;

			CVfnPtr *pVfnPtr;
			CIface *pIface;

			// The CIface (if any) whose GetHookListMutex() *this* context
			// acquired a CReentrantSharedLock on -- set once, on the fresh-
			// context path in SetupHookLoop, and released (if non-null) in
			// EndContext. Deliberately not just "pIface != NULL": the
			// Ignore(SH_CALL)/Recall reuse paths in SetupHookLoop also set
			// pIface but must NOT lock/unlock anything, since they're
			// reusing an oldctx that's already inside its own locked window.
			CIface *m_LockedIface = nullptr;

			META_RES *pStatus;
			META_RES *pPrevRes;
			META_RES *pCurRes;

			void *pThisPtr;
			const void *pOrigRet;
			void *pOverrideRet;
			void *pIfacePtr;

			bool m_CallOrig;

			ICleanupTask *m_CleanupTask;

			void SkipPaused(List<CHook>::iterator &iter, List<CHook> &list)
			{
				while (iter != list.end() && iter->IsPaused())
					++iter;
			}
		public:
			void HookRemoved(List<CHook>::iterator oldhookiter, List<CHook>::iterator nexthookiter);
			void IfaceRemoved(CIface *iface);
			void VfnPtrRemoved(CVfnPtr *vfnptr);

			ISHDelegate *GetNext();
			void *GetOverrideRetPtr();
			const void *GetOrigRetPtr();
			bool ShouldCallOrig();
			void DoCleanupTaskAndDeleteIt();
		};

		class CVfnPtrList : public List<CVfnPtr>
		{
		public:
			CVfnPtr *GetVfnPtr(void *p);
		};

		typedef CStack<CHookContext> HookContextStack;

		class UnloadListener
		{
		public:
			virtual void ReadyToUnload(Plugin plug) = 0;
		};

		class PendingUnload
		{
			UnloadListener *listener_;
			Plugin plug_;
			bool deactivated_;

		public:
			PendingUnload(UnloadListener *listener, Plugin plug)
			  : listener_(listener), plug_(plug), deactivated_(false)
			{ }

			Plugin plugin() const
			{
				return plug_;
			}

			UnloadListener *listener() const
			{
				return listener_;
			}

			void deactivate()
			{
				deactivated_ = true;
			}
			bool deactivated() const
			{
				return deactivated_;
			}
		};

		class CSourceHookImpl : public ISourceHook
		{
		private:
			CHookManList m_HookManList;
			CVfnPtrList m_VfnPtrs;
			CHookIDManager m_HookIDMan;
			List<PendingUnload *> m_PendingUnloads;
			DebugLogFunc m_LogFunc;

			// Per-*calling thread*, not per-CSourceHookImpl-instance: the
			// original single global (non-thread-local) m_ContextStack member
			// was written for Source1's single game thread, and is genuinely
			// unsafe the instant two threads dispatch through ANY vtable hook
			// on this same ISourceHook instance concurrently (e.g. a
			// networking function called from more than one thread) --
			// both would push/pop/read the *same* CStack<CHookContext>
			// concurrently with zero synchronization, corrupting it. A
			// per-thread stack is the correct fix, not a mutex: each thread's
			// hook-loop recursion (SetupHookLoop/EndContext, including
			// recall) only ever needs to see its *own* call chain, never
			// another thread's, so there's no cross-thread state to actually
			// share here at all -- same reasoning as the inline-hook path's
			// own thread_local InlineCallStack() (sourcehook_inline.h).
			static HookContextStack &ContextStack()
			{
				static thread_local HookContextStack s_Stack;
				return s_Stack;
			}

			bool SetHookPaused(int hookid, bool paused);
			CHookManList::iterator RemoveHookManager(CHookManList::iterator iter);
			List<CVfnPtr>::iterator RevertAndRemoveVfnPtr(List<CVfnPtr>::iterator vfnptr_iter);
		public:
			CSourceHookImpl(DebugLogFunc logfunc = printf);
			virtual ~CSourceHookImpl();

			/**
			*	@brief Returns the interface version
			*/
			int GetIfaceVersion();

			/**
			*	@brief Returns the implemnetation version
			*/
			int GetImplVersion();

			int AddHook(Plugin plug, AddHookMode mode, void *iface, int thisptr_offs, HookManagerPubFunc myHookMan,
				ISHDelegate *handler, bool post);

			bool RemoveHook(Plugin plug, void *iface, int thisptr_offs, HookManagerPubFunc myHookMan,
				ISHDelegate *handler, bool post);

			bool RemoveHookByID(int hookid);

			bool PauseHookByID(int hookid);
			bool UnpauseHookByID(int hookid);

			void SetRes(META_RES res);				//!< Sets the meta result
			META_RES GetPrevRes();					//!< Gets the meta result of the
													//!<  previously calledhandler
			META_RES GetStatus();					//!< Gets the highest meta result
			const void *GetOrigRet();				//!< Gets the original result.
													//!<  If not in post function, undefined
			const void *GetOverrideRet();			//!< Gets the override result.
													//!<  If none is specified, NULL
			void *GetIfacePtr();					//!< Gets the interface pointer

			void *GetOverrideRetPtr();				//!< Used for setting the override return value

			/* 
			 * @brief Make sure that a plugin is not used by any
			 * other plugins anymore, and unregister all its hook
			 * managers. If any hooks owned by this plugin are
			 * still on the callstack, defers notifying the listener
			 * until the count has dropped to 0.
			 */
			void UnloadPlugin(Plugin plug, UnloadListener *listener);

			void ResolvePendingUnloads(bool force = false);

			void RemoveHookManager(Plugin plug, HookManagerPubFunc pubFunc);

			void SetIgnoreHooks(void *vfnptr);
			void ResetIgnoreHooks(void *vfnptr);

			void DoRecall();

			void LogDebug(const char *pFormat, ...) override;

			IHookContext *SetupHookLoop(IHookManagerInfo *hi, void *vfnptr, void *thisptr, void **origCallAddr, META_RES *statusPtr,
				META_RES *prevResPtr, META_RES *curResPtr, const void *origRetPtr, void *overrideRetPtr);

			void EndContext(IHookContext *pCtx);

			void *GetOrigVfnPtrEntry(void *vfnptr);

			/**
			*	@brief Shut down the whole system, unregister all hook managers
			*/
			void CompleteShutdown();

			/**
			*	@brief Pauses all hooks of a plugin
			*
			*	@param plug The unique identifier of the plugin
			*/
			void PausePlugin(Plugin plug);

			/**
			*	@brief Unpauses all hooks of a plugin
			*
			*	@param plug The unique identifier of the plugin
			*/
			void UnpausePlugin(Plugin plug);
		};
	}
}

#endif
