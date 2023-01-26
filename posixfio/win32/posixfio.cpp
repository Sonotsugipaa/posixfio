#include "posixfio.hpp"

#include <cerrno>
#include <cassert>
#include <utility> // std::move
#include <new>



inline namespace posixfio_w32_impl {

	DWORD desired_access_from_openflags(OpenFlagBits f) {
		#define CMP_(W_, OF_) ( (W_) * (((OF_) & f) != 0) )
		return
			CMP_(FILE_GENERIC_READ,  O_RDONLY) |
			CMP_(FILE_GENERIC_WRITE, O_WRONLY);
		#undef CMP_
	}

	DWORD sharing_mode_from_openflags(OpenFlagBits f) {
		#define CMP_(W_, OF_) ( (W_) * (((OF_) & f) != 0) )
		return
			CMP_(FILE_SHARE_DELETE, O_TRUNC) |
			CMP_(FILE_SHARE_READ,   O_RDONLY) |
			CMP_(FILE_SHARE_WRITE,  O_WRONLY);
		#undef CMP_
	}

	DWORD creation_disposition_from_openflags(OpenFlagBits f) {
		#define CMP_(W_, OF_) ( (W_) * (((OF_) & f) != 0) )
		return
			CMP_(CREATE_ALWAYS,     O_CREAT & O_TRUNC) |
			CMP_(CREATE_NEW,        O_CREAT & ~O_TRUNC) |
			CMP_(TRUNCATE_EXISTING, O_TRUNC & ~O_CREAT);
		#undef CMP_
	}

	DWORD flags_and_attributes_from_openflags(OpenFlagBits f) {
		#define CMP_(OF_, W_) ( (W_) * (((OF_) & f) != 0) )
		DWORD r =
			CMP_(O_TMPFILE, FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE);
		if(r == 0) r = FILE_ATTRIBUTE_NORMAL;
		r = r |
			CMP_(O_DIRECT,         FILE_FLAG_NO_BUFFERING) |
			CMP_(O_SYNC | O_DSYNC, FILE_FLAG_WRITE_THROUGH) |
			FILE_FLAG_POSIX_SEMANTICS;
		return r;
		#undef CMP_
	}


	posixfio::FileError fetch_file_error(posixfio::fd_t fd) {
		auto err = GetLastError();
		switch(err) {
			case ERROR_FILE_NOT_FOUND:      [[fallthrough]];
			case ERROR_PATH_NOT_FOUND:      return { fd, ENOENT };
			case ERROR_TOO_MANY_OPEN_FILES: return { fd, EMFILE };
			case ERROR_ACCESS_DENIED:       [[fallthrough]];
			case ERROR_WRITE_PROTECT:       return { fd, EACCES };
			case ERROR_INVALID_HANDLE:      return { fd, EBADF };
			case ERROR_OUTOFMEMORY:         [[fallthrough]];
			case ERROR_NOT_ENOUGH_MEMORY:   return { fd, ENOMEM };
			case ERROR_SHARING_VIOLATION:   return { fd, EBUSY };
			case ERROR_SEEK:                return { fd, ENXIO };
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
