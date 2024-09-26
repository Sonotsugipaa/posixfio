#pragma once

extern "C" {
	#include <fcntl.h>
	#include <sys/mman.h>
}

#include <cstdint>



namespace posixfio {

	#define DEFINE_OP_OR_(SCOPED_ENUM_T_, UT_) constexpr SCOPED_ENUM_T_ operator|(const SCOPED_ENUM_T_& l, const SCOPED_ENUM_T_& r) noexcept { return SCOPED_ENUM_T_(UT_(l) | UT_(r)); }


	enum class MemSyncFlags : int {
		eNone       = 0,
		eAsync      = MS_ASYNC,
		eSync       = MS_SYNC,
		eInvalidate = MS_INVALIDATE
	};
	DEFINE_OP_OR_(MemSyncFlags, int)


	enum class MemProtFlags : int {
		eNone  = PROT_NONE,
		eRead  = PROT_READ,
		eWrite = PROT_WRITE,
		eExec  = PROT_EXEC
	};
	DEFINE_OP_OR_(MemProtFlags, int)


	enum class MemMapFlags : int {
		eNone    = 0,
		eShared  = MAP_SHARED,
		ePrivate = MAP_PRIVATE,
		eFixed   = MAP_FIXED
	};
	DEFINE_OP_OR_(MemMapFlags, int)


	enum class OpenFlags : int {
		eNone      = 0,
		eRdonly    = O_RDONLY,
		eWronly    = O_WRONLY,
		eRdwr      = O_RDWR,
		eAppend    = O_APPEND,
		eAsync     = O_ASYNC,
		eCloexec   = O_CLOEXEC,
		eCreat     = O_CREAT,
		eDirect    = O_DIRECT,
		eDirectory = O_DIRECTORY,
		eDsync     = O_DSYNC,
		eExcl      = O_EXCL,
		eLargefile = O_LARGEFILE,
		eNoatime   = O_NOATIME,
		eNoctty    = O_NOCTTY,
		eNofollow  = O_NOFOLLOW,
		eNonblock  = O_NONBLOCK,
		eNdelay    = O_NDELAY,
		ePath      = O_PATH,
		eSync      = O_SYNC,
		eTmpfile   = O_TMPFILE,
		eTrunc     = O_TRUNC
	};
	DEFINE_OP_OR_(OpenFlags, int)


	enum class Whence : int {
		eSet = SEEK_SET,
		eCur = SEEK_CUR,
		eEnd = SEEK_END
	};
	DEFINE_OP_OR_(Whence, int)


	#undef DEFINE_OP_OR_

}
