#include "../../include/posixfio_tl.hpp"

#include <cerrno>
#include <cassert>
#include <utility> // std::move
#include <new>

#include <unistd.h>



namespace posixfio {

	#ifdef POSIXFIO_NOTHROW
		#define POSIXFIO_THROWERRNO(FD_, DO_) DO_
		namespace no_throw {
	#else
		#define POSIXFIO_THROWERRNO(FD_, DO_) throw FileError(FD_, errno)
	#endif


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
