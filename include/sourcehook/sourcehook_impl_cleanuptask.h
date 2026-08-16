/* ======== SourceHook ========
* Copyright (C) 2004-2010 Metamod:Source Development Team
* No warranties of any kind
*
* License: zlib/libpng
*
* Author(s): Pavol "PM OnoTo" Marko
* ============================
*/

#ifndef __SOURCEHOOK_IMPL_CLEANUPTASK_H__
#define __SOURCEHOOK_IMPL_CLEANUPTASK_H__


namespace SourceHook
{
	namespace Impl
	{
		class ICleanupTask
		{
		public:
			// Every derived task deletes itself (via CleanupAndDeleteThis())
			// polymorphically from a call site that only ever sees an
			// ICleanupTask*, never the concrete derived type -- a virtual
			// destructor here is what makes that safe by construction,
			// rather than by every derived class individually happening to
			// only ever be deleted through its own most-derived `this`.
			virtual ~ICleanupTask() = default;
			virtual void CleanupAndDeleteThis() = 0;
		};
	}
}

// __SOURCEHOOK_IMPL_CLEANUPTASK_H__
#endif

