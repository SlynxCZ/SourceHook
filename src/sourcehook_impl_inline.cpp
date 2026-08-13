/* ======== SourceHook ========
* Copyright (C) 2004-2026 Metamod:Source Development Team
* No warranties of any kind
*
* License: zlib/libpng
*
* Author(s): Michal "Slynx" Přikryl
* ============================
*/

#include "sourcehook_impl_inline.h"

namespace SourceHook
{
	namespace Impl
	{
		CInlineHookAddressGuard &CInlineHookAddressGuard::Get()
		{
			static CInlineHookAddressGuard s_Instance;
			return s_Instance;
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
