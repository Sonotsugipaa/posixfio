#pragma once

#include <cstdint>
#include <IntSafe.h>



namespace posixfio {

	#define DEFINE_OP_OR_(SCOPED_ENUM_T_, UT_) constexpr SCOPED_ENUM_T_ operator|(const SCOPED_ENUM_T_& l, const SCOPED_ENUM_T_& r) noexcept { return SCOPED_ENUM_T_(UT_(l) | UT_(r)); }


	using MemSyncFlagBits = DWORD;
	enum class MemSyncFlags : MemSyncFlagBits {
		eNone       = 0,
		eAsync      = 1 << 0,
		eSync       = 1 << 1,
		eInvalidate = 1 << 2
	};
	DEFINE_OP_OR_(MemSyncFlags, MemSyncFlagBits)


	using MemProtFlagBits = DWORD;
	enum class MemProtFlags : MemProtFlagBits {
		eNone  = 0,
		eRead  = 1 << 2,
		eWrite = 1 << 1,
		eExec  = 1 << 0
	};
	DEFINE_OP_OR_(MemProtFlags, MemProtFlagBits)

		
	using MemMapFlagBits = DWORD;
	enum class MemMapFlags : MemMapFlagBits {
		eNone    = 0,
		eShared  = 1 << 0,
		ePrivate = 1 << 1,
		eFixed   = 1 << 2
	};
	DEFINE_OP_OR_(MemMapFlags, MemMapFlagBits)

		
	using OpenFlagBits = DWORD;
	enum class OpenFlags : OpenFlagBits {
		eUnsupported = DWORD(0x7FFFFFFF),
		eNone        = 0,
		eRdonly      = 0x00001,
		eWronly      = 0x00002,
		eRdwr        = 0x00003,
		eAppend      = 0x00004,
		eAsync       = 0x00008 | DWORD(0x7FFFFFFF),
		eCloexec     = 0x00010 | DWORD(0x7FFFFFFF),
		eCreat       = 0x00020,
		eDirect      = 0x00040,
		eDirectory   = 0x00080 | DWORD(0x7FFFFFFF),
		eDsync       = 0x00100,
		eExcl        = 0x00200 | DWORD(0x7FFFFFFF),
		eLargefile   = 0x00400,
		eNoatime     = 0x00800,
		eNoctty      = 0x01000 | DWORD(0x7FFFFFFF),
		eNofollow    = 0x02000 | DWORD(0x7FFFFFFF),
		eNonblock    = 0x04000 | DWORD(0x7FFFFFFF),
		eNdelay      = 0x04000 | DWORD(0x7FFFFFFF),
		ePath        = 0x08000 | DWORD(0x7FFFFFFF),
		eSync        = 0x10000,
		eTmpfile     = 0x20000,
		eTrunc       = 0x40000
	};
	DEFINE_OP_OR_(OpenFlags, OpenFlagBits)


	enum class Whence : DWORD {
		eSet = 0 /* FILE_BEGIN */,
		eCur = 1 /* FILE_CURRENT */,
		eEnd = 2 /* FILE_END */
	};
	DEFINE_OP_OR_(Whence, DWORD)


	#undef DEFINE_OP_OR_

}
