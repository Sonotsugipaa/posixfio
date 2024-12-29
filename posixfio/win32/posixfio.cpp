#ifndef POSIXFIO_STL_STRINGVIEW
	#define POSIXFIO_STL_STRINGVIEW // The library builds the `std::string_view` variants of File constructors, regardless of their declaration in other translation units
#endif

#include "../../include/win32/posixfio_tl.hpp"

#include <cerrno>
#include <cassert>
#include <utility> // std::move
#include <new>
#include <exception>
#include <string>
#include <string_view>



inline namespace posixfio_w32_impl {

	constexpr DWORD desired_access_from_openflags(posixfio::OpenFlags f) {
		using enum posixfio::OpenFlags;
		using Bits = posixfio::OpenFlagBits;
		DWORD r = 0;
		if(Bits(f) & Bits(eRdonly)) r =     FILE_GENERIC_READ;
		if(Bits(f) & Bits(eWronly)) r = r | FILE_GENERIC_WRITE;
		return r;
	}

	constexpr DWORD sharing_mode_from_openflags(posixfio::OpenFlags f) {
		using enum posixfio::OpenFlags;
		using Bits = posixfio::OpenFlagBits;
		DWORD r = 0;
		if(Bits(f) & Bits(eTrunc))  r =     FILE_SHARE_DELETE;
		if(Bits(f) & Bits(eRdonly)) r = r | FILE_SHARE_READ;
		if(Bits(f) & Bits(eWronly)) r = r | FILE_SHARE_WRITE;
		return r;
	}

	constexpr DWORD creation_disposition_from_openflags(posixfio::OpenFlags f) {
		using enum posixfio::OpenFlags;
		using Bits = posixfio::OpenFlagBits;
		bool trunc = Bits(f) & Bits(eTrunc);
		bool creat = Bits(f) & Bits(eCreat);
		if(trunc && creat) return CREATE_ALWAYS;
		if(creat) return OPEN_ALWAYS;
		if(trunc) return TRUNCATE_EXISTING;
		return OPEN_EXISTING;
	}

	constexpr DWORD flags_and_attributes_from_openflags(posixfio::OpenFlags f) {
		using enum posixfio::OpenFlags;
		using Bits = posixfio::OpenFlagBits;
		DWORD r = FILE_FLAG_POSIX_SEMANTICS;
		if(Bits(f) & Bits(eTmpfile)) r = r | FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE;
		else                         r = r | FILE_ATTRIBUTE_NORMAL;
		if(Bits(f) & Bits(eDirect))  r = r | FILE_FLAG_NO_BUFFERING;
		if(Bits(f) & Bits(eSync))    r = r | FILE_FLAG_WRITE_THROUGH;
		if(Bits(f) & Bits(eDsync))   r = r | FILE_FLAG_WRITE_THROUGH;
		return r;
	}


	template <typename T>
	constexpr void mk_dword_2(DWORD dst[2], T s) {
		static_assert(2 * sizeof(DWORD) == sizeof(T));
		dst[0] =      (DWORD(s)         ) & (~DWORD(0));
		dst[1] = DWORD(    T(s) >> T(32)) & (~DWORD(0));
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
		#define POSIXFIO_THROWERRNO(FD_, DO_) { if(FD_ != INVALID_HANDLE_VALUE) { errno = fetch_file_error(FD_).errcode; } DO_ }
		namespace no_throw {
	#else
		#define POSIXFIO_THROWERRNO(FD_, DO_) { auto e = fetch_file_error(FD_);  errno = e.errcode;  throw e; }
	#endif


	MemMapping::MemMapping(MemMapping&& mv) noexcept:
		handle(mv.handle),
		addr(mv.addr),
		len(mv.len)
	{
		mv.disown();
	}


	MemMapping& MemMapping::operator=(MemMapping&& mv) noexcept {
		this->~MemMapping();
		return * new (this) MemMapping(std::move(mv));
	}


	MemMapping::~MemMapping() {
		if(addr != nullptr) {
			assert(len > 0);
			munmap();
			handle = INVALID_HANDLE_VALUE;
			addr = nullptr;
			len = 0;
		}
	};


	bool MemMapping::munmap() {
		assert(addr != nullptr);
		assert(len > 0);
		bool r;
		r = bool(UnmapViewOfFile(addr));
		if(! r) POSIXFIO_THROWERRNO(handle, (void) 0);
		r = r | bool(CloseHandle(handle));
		if(! r) POSIXFIO_THROWERRNO(handle, (void) 0);
		return r;
	}


	bool MemMapping::msync(MemSyncFlags flags) {
		assert(addr != nullptr);
		assert(len > 0);
		bool r = FlushViewOfFile(addr, len);
		if(! r) POSIXFIO_THROWERRNO(handle, (void) 0);
		return r;
	}


	File File::open(const char* pathname, OpenFlags flags, posixfio::mode_t mode) {
		(void) mode;
		assert(flags < OpenFlags::eUnsupported);

		File r = CreateFileA(
			pathname,
			desired_access_from_openflags(OpenFlags(flags)),
			sharing_mode_from_openflags(OpenFlags(flags)),
			{ },
			creation_disposition_from_openflags(OpenFlags(flags)),
			flags_and_attributes_from_openflags(OpenFlags(flags)),
			nullptr );

		if(! r) {
			POSIXFIO_THROWERRNO(NULL_FD, (void) 0);
		} else {
			if(OpenFlagBits(flags) & OpenFlagBits(OpenFlags::eAppend)) {
				LARGE_INTEGER li = { .QuadPart = 0 };
				SetFilePointerEx(r, li, nullptr, FILE_END);
			}
		}

		return r;
	}


	File File::open(std::string_view pathname, OpenFlags flags, posixfio::mode_t mode) {
		auto len = pathname.size();
		auto bf = new char[len + 1];
		std::exception_ptr exception = nullptr;
		memcpy(bf, pathname.data(), len);
		bf[len] = '\0';
		File r;
		try        { r = File::open(bf, flags, mode); }
		catch(...) { exception = std::current_exception(); }
		delete[] bf;
		if(exception != nullptr) [[unlikely]] std::rethrow_exception(exception);
		return r;
	}


	File File::creat(std::string_view pathname, mode_t mode) {
		return File::open(pathname, OpenFlags::eCreat, mode);
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


	off_t File::lseek(off_t offset, Whence whence) {
		static_assert(DWORD(Whence::eSet) == FILE_BEGIN);
		static_assert(DWORD(Whence::eCur) == FILE_CURRENT);
		static_assert(DWORD(Whence::eEnd) == FILE_END);
		LARGE_INTEGER offsetLi = { .QuadPart = offset };
		LARGE_INTEGER r;
		bool seek = SetFilePointerEx(fd_, offsetLi, &r, DWORD(whence));
		if(! seek) POSIXFIO_THROWERRNO(fd_, return -1);
		return r.QuadPart;
	}


	bool File::ftruncate(off_t length) {
		off_t cur;
		#ifdef POSIXFIO_NOTHROW
			#warning "This is untested"
			cur = lseek(0, SEEK_CUR);
			if(cur == -1) [[unlikely]] return false;
			if(cur != length) {
				off_t truncAt = lseek(length, SEEK_SET);
				if(truncAt == -1) [[unlikely]] {
					auto prevErrno = errno;
					lseek(0, SEEK_CUR);
					errno = prevErrno;
					return false;
				}
			}
			bool r = SetEndOfFile(fd_);
			#define COMMA ,
				if(! r) POSIXFIO_THROWERRNO(fd_, lseek(cur COMMA SEEK_SET););
			#undef COMMA
			if(cur != length) cur = lseek(cur, SEEK_SET);
			if(! r) POSIXFIO_THROWERRNO(fd_, return false);
			return true;
		#else
			cur = lseek(0, Whence::eCur);
			if(cur != length) {
				try {
					lseek(length, Whence::eSet); // Could realistically fail for `length` being out of bounds
				} catch(...) {
					lseek(cur, Whence::eSet);
					std::rethrow_exception(std::current_exception());
				}
				bool r = SetEndOfFile(fd_);
				if(! r) POSIXFIO_THROWERRNO(fd_, (void) 0);
				lseek(cur, Whence::eSet);
			} else {
				bool r = SetEndOfFile(fd_);
				if(! r) POSIXFIO_THROWERRNO(fd_, (void) 0);
			}
			return true;
		#endif
	}


	MemMapping File::mmap(void* addr, size_t len, MemProtFlags prot, MemMapFlags flags, off_t off) {
		if(len < 1) return MemMapping();
		MemMapping r;
		SECURITY_ATTRIBUTES sec;
		sec.nLength = sizeof(SECURITY_ATTRIBUTES);
		sec.lpSecurityDescriptor = nullptr;
		sec.bInheritHandle = bool(MemMapFlagBits(flags) & MemMapFlagBits(MemMapFlags::eShared));
		DWORD protFlag;
		DWORD desiredAccess;
		DWORD off2[2]; mk_dword_2(off2, off);
		DWORD len2[2]; mk_dword_2(len2, len);
		switch(int(prot)) {
			default: std::unreachable(); [[fallthrough]];
			case 0:
				protFlag = 0; desiredAccess = 0; break;
			case int(MemProtFlags::eRead):
				protFlag = PAGE_READONLY; desiredAccess = FILE_MAP_READ; break;
			case int(MemProtFlags::eExec):
				protFlag = PAGE_EXECUTE; desiredAccess = 0; break;
			case int(MemProtFlags::eWrite): [[fallthrough]];
			case int(MemProtFlags::eRead) | int(MemProtFlags::eWrite):
				protFlag = PAGE_READWRITE; desiredAccess = FILE_MAP_WRITE; break;
			case int(MemProtFlags::eRead) | int(MemProtFlags::eExec):
				protFlag = PAGE_EXECUTE_READ; desiredAccess = FILE_MAP_READ; break;
			case int(MemProtFlags::eWrite) | int(MemProtFlags::eExec): [[fallthrough]];
			case int(MemProtFlags::eRead) | int(MemProtFlags::eWrite) | int(MemProtFlags::eExec):
				protFlag = PAGE_EXECUTE_READWRITE; desiredAccess = FILE_MAP_WRITE; break;
		}
		r.handle = CreateFileMappingA(fd_, &sec, protFlag, len2[1], len2[0], nullptr);
		if(r.handle == 0) [[unlikely]] r.handle = NULL_FD; // INVALID_HANDLE_VALUE != 0, but MapViewOfFile can't have a 0. WHY WINDOWS WHY WHY WHY WHY WHY WHY WHY WHY WHY WHY WHY WHY WHY WHY WHY WHY WHY WHY WHY WHY? ARE YOU REGARDED?!?
		if(r.handle == NULL_FD) POSIXFIO_THROWERRNO(fd_, (void) 0);
		r.addr = MapViewOfFile(r.handle, desiredAccess, off2[1], off2[0], len);
		if(r.addr == nullptr) {
			bool handleClosed = CloseHandle(r.handle);
			assert(handleClosed); (void) handleClosed;
			#define COMMA ,
				POSIXFIO_THROWERRNO(fd_, return { .handle = NULL_FD COMMA .addr = nullptr COMMA len = 0 });
			#undef COMMA
		}
		r.len = len;
		return r;
	}


	#ifdef POSIXFIO_NOTHROW
		}
	#endif

}
