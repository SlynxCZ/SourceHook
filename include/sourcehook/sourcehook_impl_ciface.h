/* ======== SourceHook ========
* Copyright (C) 2004-2010 Metamod:Source Development Team
* No warranties of any kind
*
* License: zlib/libpng
*
* Author(s): Pavol "PM OnoTo" Marko
* ============================
*/

#ifndef __SOURCEHOOK_IMPL_CIFACE_H__
#define __SOURCEHOOK_IMPL_CIFACE_H__

#include <memory>
#include <shared_mutex>

#include "sh_list.h"
#include "sourcehook_impl_chook.h"

namespace SourceHook
{
	namespace Impl
	{
		class CIface
		{
			// *** Data ***
			void *m_Ptr;

			List<CHook> m_PreHooks;
			List<CHook> m_PostHooks;
		public:

			// *** Descriptor ***
			typedef void* Descriptor;

			// *** Interface ***
			inline CIface(void *ptr);
			inline ~CIface();
			inline bool operator==(const Descriptor &other);
			inline void *GetPtr() const;
			inline List<CHook> &GetPreHookList();
			inline List<CHook> &GetPostHookList();
			inline const List<CHook> &GetPreHookList() const;
			inline const List<CHook> &GetPostHookList() const;

			// Guards m_PreHooks/m_PostHooks against a concurrent
			// AddHook()/RemoveHookByID() (sourcehook.cpp) on one thread
			// racing a hook-loop dispatch (SetupHookLoop..EndContext, which
			// iterates these lists live, not a snapshot -- same shape as
			// upstream's original single-threaded design) on another. The
			// dispatching thread holds a *shared* lock for the entire
			// SetupHookLoop..EndContext window (including any recall
			// round-trip -- see CHookContext::m_LockedIface below); Add/
			// RemoveHookByID take the *exclusive* lock only for the brief
			// list mutation itself. This is also why CIface is never
			// actually destroyed once created (see the comment on
			// CSourceHookImpl::RemoveHookByID's "empty" branch in
			// sourcehook.cpp) -- deleting an object out from under a thread
			// that might still be blocked trying to lock its own mutex is
			// undefined behavior no locking discipline alone can fix; not
			// deleting it sidesteps the question entirely, same choice
			// CInlineDispatcher (sourcehook_inline.h) already makes for
			// inline hooks, and the same one KHook's own DetourCapsule
			// makes for vtable hooks (see the code-reading writeup that
			// motivated this whole pass).
			//
			// A shared_ptr, not a plain member: List<T>::push_back()
			// copy-constructs into its node (CVfnPtr::GetIface() builds a
			// throwaway local CIface and copies it in -- see
			// sourcehook_impl_cvfnptr.cpp), so CIface has to stay
			// copy-constructible; std::shared_mutex itself is neither
			// copyable nor movable. The short-lived throwaway temporary and
			// its one real copy inside the list briefly point at the same
			// mutex object, which is harmless -- the temporary is discarded
			// immediately after the copy, single-threaded within GetIface(),
			// before anything else could possibly try to lock it.
			inline std::shared_mutex &GetHookListMutex() const { return *m_HookListMutex; }

		private:
			std::shared_ptr<std::shared_mutex> m_HookListMutex;
		};

		// *** Implementation ***
		inline CIface::CIface(void *ptr)
			: m_Ptr(ptr), m_HookListMutex(std::make_shared<std::shared_mutex>())
		{
		}

		inline CIface::~CIface()
		{
			// Before getting deleted, delete all remaining hook handlers
			for (List<CHook>::iterator iter = m_PreHooks.begin(); iter != m_PreHooks.end(); ++iter)
			{
				iter->GetHandler()->DeleteThis();
			}

			for (List<CHook>::iterator iter = m_PostHooks.begin(); iter != m_PostHooks.end(); ++iter)
			{
				iter->GetHandler()->DeleteThis();
			}
		}

		inline bool CIface::operator==(const Descriptor &other)
		{
			return m_Ptr == other;
		}

		inline void *CIface::GetPtr() const
		{
			return m_Ptr;
		}

		inline List<CHook> &CIface::GetPreHookList()
		{
			return m_PreHooks;
		}

		inline List<CHook> &CIface::GetPostHookList()
		{
			return m_PostHooks;
		}

		inline const List<CHook> &CIface::GetPreHookList() const
		{
			return m_PreHooks;
		}

		inline const List<CHook> &CIface::GetPostHookList() const
		{
			return m_PostHooks;
		}
	}
}

#endif

