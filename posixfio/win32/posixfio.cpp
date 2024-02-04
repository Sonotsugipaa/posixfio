#include "posixfio.hpp"

#include <cerrno>
#include <cassert>
#include <utility> // std::move
#include <new>



inline namespace posixfio_w32_impl {

	DWORD desired_access_from_openflags(OpenFlagBits f) {
		DWORD r = 0;
		if(f & O_RDONLY) r =     FILE_GENERIC_READ;
		if(f & O_WRONLY) r = r | FILE_GENERIC_WRITE;
		return r;
	}

	DWORD sharing_mode_from_openflags(OpenFlagBits f) {
		DWORD r = 0;
		if(f & O_TRUNC)  r =     FILE_SHARE_DELETE;
		if(f & O_RDONLY) r = r | FILE_SHARE_READ;
		if(f & O_WRONLY) r = r | FILE_SHARE_WRITE;
		return r;
	}

	DWORD creation_disposition_from_openflags(OpenFlagBits f) {
		DWORD r = 0;
		bool trunc = f & O_TRUNC;
		bool creat = f & O_CREAT;
		if(trunc && creat) return CREATE_ALWAYS;
		if(creat) return OPEN_ALWAYS;
		if(trunc) return TRUNCATE_EXISTING;
		return OPEN_EXISTING;
	}

	DWORD flags_and_attributes_from_openflags(OpenFlagBits f) {
		DWORD r = FILE_FLAG_POSIX_SEMANTICS;
		if(f & O_TMPFILE) r = r | FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE;
		else              r = r | FILE_ATTRIBUTE_NORMAL;
		if(f & O_DIRECT)  r = r | FILE_FLAG_NO_BUFFERING;
		if(f & O_SYNC)    r = r | FILE_FLAG_WRITE_THROUGH;
		if(f & O_DSYNC)   r = r | FILE_FLAG_WRITE_THROUGH;
		return r;
	}


	posixfio::FileError fetch_file_error(posixfio::fd_t fd) {
		auto err = GetLastError();
		switch(err) {
			case ERROR_FILE_NOT_FOUND:      [[fallthrough]];
			case ERROR_PATH_NOT_FOUND:      return { fd, ENOENT };
			case ERROR_TOO_MANY_OPEN_FILES: return { fd, EMFILE };
			case ERROR_ACCESS_DENIED:       return { fd, EACCES };
			case ERROR_WRITE_PROTECT:       return { fd, EACCES };
			case ERROR_INVALID_HANDLE:      return { fd, EBADF };
			case ERROR_OUTOFMEMORY:         [[fallthrough]];
			case ERROR_NOT_ENOUGH_MEMORY:   return { fd, ENOMEM };
			case ERROR_SHARING_VIOLATION:   return { fd, EBUSY };
			case ERROR_SEEK:                return { fd, ENXIO };
			case ERROR_INVALID_PARAMETER:   return { fd, EINVAL };
			case NO_ERROR:                  return { fd, 0 };
			default: return { fd, EIO };
		}
	}

}



namespace posixfio {

	#ifdef POSIXFIO_NOTHROW
		#define POSIXFIO_THROWERRNO(FD_, DO_) { DO_ }
		namespace no_throw {
	#else
		#define POSIXFIO_THROWERRNO(FD_, DO_) { auto e = fetch_file_error(FD_);  errno = e.errcode;  throw e; }
	#endif


	File File::open(const char* pathname, OpenFlagBits flags, posixfio::mode_t mode) {
		(void) mode;
		assert(flags < OPENFLAGS_UNSUPPORTED);

		File r = CreateFileA(
			pathname,
			desired_access_from_openflags(flags),
			sharing_mode_from_openflags(flags),
			{ },
			creation_disposition_from_openflags(flags),
			flags_and_attributes_from_openflags(flags),
			nullptr );

		if(! r) {
			POSIXFIO_THROWERRNO(NULL_FD, (void) 0);
		} else {
			if(flags & O_APPEND) {
				LARGE_INTEGER li = { .QuadPart = 0 };
				SetFilePointerEx(r, li, nullptr, FILE_END);
			}
		}

		return r;
	}


	File::File(): fd_(NULL_FD) { }

	File::File(fd_t fd): fd_(fd) { }


	File::File(const File& cp) {
		bool success = DuplicateHandle(
			GetCurrentProcess(),
			cp.fd_,
			GetCurrentProcess(),
			&fd_,
			0,
			true,
			DUPLICATE_SAME_ACCESS );
		#ifndef POSIXFIO_NOTHROW
			if(! success) POSIXFIO_THROWERRNO(NULL_FD, (void) 0);
		#endif
	}


	File::File(File&& mv):
			fd_(std::move(mv.fd_))
	{
		mv.fd_ = NULL_FD;
	}


	File::~File() {
		if(fd_ != NULL_FD) {
			assert(CloseHandle(fd_));
			fd_ = NULL_FD;
		}
	}


	bool File::close() {
		if(fd_ != NULL_FD) {
			bool r = CloseHandle(fd_);
			assert(r);
			if(! r) { POSIXFIO_THROWERRNO(fd_, return false); }
			else fd_ = NULL_FD;
		}
		return true;
	}


	#ifdef POSIXFIO_NOTRHOW
	#define MK_OPERATOR_EQ(CONSTR_ARGS_) \
		{ \
			if(! close()) {         \
				fd_ = NULL_FD;       \
				POSIXFIO_THROWERRNO; \
			} else {                \
				return *(new (this) File(CONSTR_ARGS_)); \
			} \
		}
	#else
	#define MK_OPERATOR_EQ(CONSTR_ARGS_) \
		{ \
			this->~File(); \
			return *(new (this) File(CONSTR_ARGS_)); \
		}
	#endif

		File& File::operator=(const File& cp) MK_OPERATOR_EQ(cp)
		File& File::operator=(File&& mv) MK_OPERATOR_EQ(std::move(mv))

	#undef MK_OPERATOR_EQ


	std::make_signed_t<DWORD> File::read(void* buf, DWORD count) {
		DWORD rd;
		bool success = ReadFile(
			fd_,
			buf,
			count,
			&rd,
			nullptr );
		if(! success) {
			if(GetLastError() == ERROR_HANDLE_EOF) return 0;
			POSIXFIO_THROWERRNO(fd_, return -1);
		}
		return rd;
	}

	std::make_signed_t<DWORD> File::write(const void* buf, DWORD count) {
		DWORD wr;
		bool success = WriteFile(
			fd_,
			buf,
			count,
			&wr,
			nullptr );

		if(! success) {
			POSIXFIO_THROWERRNO(fd_, return -1);
		}
		return wr;
	}


	off_t File::lseek(off_t offset, int whence) {
		LARGE_INTEGER li = { .QuadPart = offset };
		bool seek = SetFilePointerEx(fd_, li, nullptr, whence);
		if(! seek) {
			POSIXFIO_THROWERRNO(fd_, (void) 0);
		}
		return seek;
	}


	#ifdef POSIXFIO_NOTHROW
		}
	#endif

}
