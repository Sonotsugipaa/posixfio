#ifndef POSIXFIO_STL_STRINGVIEW
	#define POSIXFIO_STL_STRINGVIEW // The library builds the `std::string_view` variants of File constructors, regardless of their declaration in other translation units
#endif

#include "../../include/unix/posixfio_tl.hpp"

#include <cerrno>
#include <cassert>
#include <utility> // std::move
#include <new>
#include <string>
#include <string_view>

#include <unistd.h>



namespace posixfio {

	#ifdef POSIXFIO_NOTHROW
		#define POSIXFIO_THROWERRNO(FD_, DO_) DO_;
		namespace no_throw {
	#else
		#define POSIXFIO_THROWERRNO(FD_, DO_) throw FileError(FD_, errno)
	#endif


	MemMapping::MemMapping(MemMapping&& mv) noexcept:
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
			addr = nullptr;
			len = 0;
		}
	};


	bool MemMapping::munmap() {
		assert(addr != nullptr);
		assert(len > 0);
		auto res = ::munmap(addr, len);
		if(res == 0) [[likely]] {
			addr = nullptr;
			len = 0;
			return true;
		} else {
			#ifdef POSIXFIO_NOTHROW
				return false;
			#else
				throw Errcode(errno);
			#endif
		}
	}


	bool MemMapping::mlock() {
		assert(addr != nullptr);
		assert(len > 0);
		#ifdef POSIXFIO_NOTHROW
			return 0 == ::mlock(addr, len);
		#else
			if(0 != ::mlock(addr, len)) [[unlikely]] throw Errcode(errno);
			return true;
		#endif
	}


	bool MemMapping::munlock() {
		assert(addr != nullptr);
		assert(len > 0);
		#ifdef POSIXFIO_NOTHROW
			return 0 == ::munlock(addr, len);
		#else
			if(0 != ::munlock(addr, len)) [[unlikely]] throw Errcode(errno);
			return true;
		#endif
	}


	bool MemMapping::msync(MemSyncFlags flags) {
		assert(addr != nullptr);
		assert(len > 0);
		#ifdef POSIXFIO_NOTHROW
			return 0 == ::msync(addr, len, int(flags));
		#else
			if(0 != ::msync(addr, len, int(flags))) [[unlikely]] throw Errcode(errno);
			return true;
		#endif
	}


	File File::open(const char* pathname, int flags, posixfio::mode_t mode) {
		File r = ::open(pathname, flags, mode);
		if(! r) POSIXFIO_THROWERRNO(NULL_FD, (void) 0);
		return r;
	}

	File File::creat(const char* pathname, posixfio::mode_t mode) {
		File r = ::creat(pathname, mode);
		if(! r) POSIXFIO_THROWERRNO(NULL_FD, (void) 0);
		return r;
	}

	File File::openat(fd_t dirfd, const char* pathname, int flags, posixfio::mode_t mode) {
		File r = ::openat(dirfd, pathname, flags, mode);
		if(! r) POSIXFIO_THROWERRNO(NULL_FD, (void) 0);
		return r;
	}


	File File::open(std::string_view pathname, int flags, posixfio::mode_t mode) {
		auto len = pathname.size();
		auto bf = new char[len + 1];
		memcpy(bf, pathname.data(), len);
		bf[len] = '\0';
		auto r = File::open(bf, flags, mode);
		delete[] bf;
		return r;
	}

	File File::creat(std::string_view pathname, posixfio::mode_t mode) {
		auto len = pathname.size();
		auto bf = new char[len + 1];
		memcpy(bf, pathname.data(), len);
		bf[len] = '\0';
		auto r = File::creat(bf, mode);
		delete[] bf;
		return r;
	}

	File File::openat(fd_t dirfd, std::string_view pathname, int flags, posixfio::mode_t mode) {
		auto len = pathname.size();
		auto bf = new char[len + 1];
		memcpy(bf, pathname.data(), len);
		bf[len] = '\0';
		auto r = File::openat(dirfd, bf, flags, mode);
		delete[] bf;
		return r;
	}



	File::File(): fd_(NULL_FD) { }

	File::File(fd_t fd): fd_(fd) { }


	File::File(const File& cp):
			fd_(::dup(cp.fd_))
	{
		#ifndef POSIXFIO_NOTHROW
			if(fd_ < 0) POSIXFIO_THROWERRNO(fd_, (void) 0);
		#endif
	}


	File::File(File&& mv):
			fd_(std::move(mv.fd_))
	{
		mv.fd_ = NULL_FD;
	}


	File::~File() {
		if(fd_ >= 0) {
			#ifdef NDEBUG
				::close(fd_);
			#else
				int r = ::close(fd_);
				assert((r == 0) || (r == -1 /* POSIX indicates `-1` specifically */));
			#endif
			fd_ = NULL_FD;
		}
	}


	bool File::close() {
		if(fd_ >= 0) {
			int r = ::close(fd_);
			assert((r == 0) || (r == -1 /* POSIX indicates `-1` specifically */));
			if(r < 0)  POSIXFIO_THROWERRNO(fd_, return false);
			else  fd_ = NULL_FD;
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


	File File::dup2(fd_t newFd) const {
		fd_t r = ::dup2(fd_, newFd);
		if(r < 0) POSIXFIO_THROWERRNO(fd_, return File());
		return File(r);
	}


	posixfio::ssize_t File::read(void* buf, size_t count) {
		posixfio::ssize_t rd = ::read(fd_, buf, count);
		if(rd < 0) {
			POSIXFIO_THROWERRNO(fd_, return rd);
		}
		return rd;
	}

	posixfio::ssize_t File::write(const void* buf, size_t count) {
		posixfio::ssize_t wr = ::write(fd_, buf, count);
		if(wr < 0) {
			POSIXFIO_THROWERRNO(fd_, return wr);
		}
		return wr;
	}


	off_t File::lseek(off_t offset, int whence) {
		posixfio::ssize_t seek = ::lseek(fd_, offset, whence);
		if(seek < 0) {
			POSIXFIO_THROWERRNO(fd_, (void) 0);
		}
		return seek;
	}


	bool File::ftruncate(off_t length) {
		int trunc = ::ftruncate(fd_, length);
		assert(trunc == 0 || trunc == -1);
		if(trunc < 0) {
			POSIXFIO_THROWERRNO(fd_, (void) 0);
		}
		return trunc;
	}


	bool File::fsync() {
		int res = ::fsync(fd_);
		assert(res == 0 || res == -1);
		if(res < 0) {
			POSIXFIO_THROWERRNO(fd_, (void) 0);
		}
		return res;
	}


	bool File::fdatasync() {
		int res = ::fdatasync(fd_);
		assert(res == 0 || res == -1);
		if(res < 0) {
			POSIXFIO_THROWERRNO(fd_, (void) 0);
		}
		return res;
	}


	MemMapping File::mmap(void* addr, size_t len, MemProtFlags prot, MemMapFlags flags, off_t off) {
		if(len < 1) return MemMapping();
		MemMapping r;
		auto r_addr = ::mmap(addr, len, int(prot), int(flags), fd_, off);
		if(r_addr == MAP_FAILED) [[unlikely]] POSIXFIO_THROWERRNO(fd_, return MemMapping());
		r.addr = r_addr;
		r.len = len;
		return r;
	}


	Pipe Pipe::create() {
		fd_t fd[2];
		Pipe r;
		auto result = pipe(fd);
		assert(result == 0 || result == -1);
		if(result < 0) {
			POSIXFIO_THROWERRNO(File::NULL_FD, (void) 0);
		} else {
			r.rd = fd[0];
			r.wr = fd[1];
		}
		return r;
	}


	#ifdef POSIXFIO_NOTHROW
		}
	#endif

}
