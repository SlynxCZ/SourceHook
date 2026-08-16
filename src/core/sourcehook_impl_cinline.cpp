/* ======== SourceHook ========
* Copyright (C) 2026 Metamod:Source Development Team
* No warranties of any kind
*
* License: zlib/libpng
*
* Author(s): Michal "Slynx" Přikryl
* ============================
*/

#include "../../include/sourcehook/sourcehook_impl_cinline.h"

namespace SourceHook
{
	namespace Impl
	{
		namespace
		{
			// Namespace-scope, not a function-local static: a plugin build
			// commonly compiles with -fno-threadsafe-statics (this repo's
			// own AMBuildScript does, matching InventoryManager_mm_es's) --
			// that flag removes the compiler-generated guard variable that
			// would otherwise make a function-local static's first-use
			// initialization safe against two threads racing to call Get()
			// for the very first time concurrently. A namespace-scope
			// object like this one instead gets ordinary dynamic
			// initialization at library-load time, before any application
			// thread (or even Load()) could possibly reach it -- there's no
			// "first concurrent call" race left to have.
			CInlineHookAddressGuard g_InlineHookAddressGuardInstance;
		}

		CInlineHookAddressGuard &CInlineHookAddressGuard::Get()
		{
			return g_InlineHookAddressGuardInstance;
		}

		bool CInlineHookAddressGuard::Claim(void *addr, const std::type_info &sig)
		{
			std::lock_guard<std::mutex> lock(m_Mutex);

			auto it = m_Claims.find(addr);
			if (it == m_Claims.end())
			{
				m_Claims.emplace(addr, Entry{ &sig, 1 });
				return true;
			}

			if (*it->second.sig != sig)
			{
				// Same address, incompatible declared signature -- reject.
				return false;
			}

			++it->second.refcount;
			return true;
		}

		void CInlineHookAddressGuard::Release(void *addr)
		{
			std::lock_guard<std::mutex> lock(m_Mutex);

			auto it = m_Claims.find(addr);
			if (it == m_Claims.end())
				return;

			if (--it->second.refcount <= 0)
				m_Claims.erase(it);
		}

		std::size_t CInlineHookAddressGuard::TargetCount() const
		{
			std::lock_guard<std::mutex> lock(m_Mutex);
			return m_Claims.size();
		}
	}
}
