#pragma once

extern "C" {
	#include <fcntl.h>
	#include <sys/mman.h>
}

#include <cstdint>
#include <new>
#include <type_traits>

#include "posixfio/constants.hpp"

#ifdef POSIXFIO_STL_STRINGVIEW
	#include <string_view>
#endif



namespace posixfio {

	#ifdef POSIXFIO_NOTHROW
		inline namespace no_throw {
	#endif


	using fd_t = int;

	// These aliases are the easiest solution for portability, but they may limit how
	// functions are implemented for each environment.
	using mode_t = unsigned;
	using ssize_t = std::make_signed_t<size_t>;
	using off_t = ssize_t;

	// Class forward declarations
	class Errcode;
	class File;
	class Pipe;
	class FileView;

	using Errno = Errcode;  // `errcode` replaces `errno`, since the latter is a macro


	struct Errcode {
		int errcode;

		constexpr Errcode(int errcode): errcode(errcode) { }
		constexpr operator int() const { return errcode; }
	};

	struct FileError : public Errcode {
		fd_t fd;

		constexpr FileError(fd_t fd, int errcode): Errcode(errcode), fd(fd) { }
		constexpr operator int() = delete;
	};


	class MemMapping {
	private:
		friend FileView;
		void* addr;
		size_t len;

	public:
		MemMapping() noexcept: addr(nullptr), len(0) { }
		MemMapping(const MemMapping&) = delete;
		MemMapping(MemMapping&&) noexcept;
		~MemMapping();
		MemMapping& operator=(const MemMapping&) = delete;
		MemMapping& operator=(MemMapping&&) noexcept;

		/** Clears the pointer to the allocation, but does not unmap. */
		void disown() { addr = nullptr; len = 0; }

		/** Almost POSIX-compliant: returns `false` exclusively when an error occurs. */
		bool munmap();

		/** Almost POSIX-compliant: returns `false` exclusively when an error occurs. */
		bool mlock();

		/** Almost POSIX-compliant: returns `false` exclusively when an error occurs. */
		bool munlock();

		/** Almost POSIX-compliant: returns `false` exclusively when an error occurs. */
		bool msync(MemSyncFlags flags = MemSyncFlags::eSync);

		template<typename T = void> inline T* get() noexcept { return reinterpret_cast<T*>(addr); }
		template<typename T = void> inline const T* get() const noexcept { return reinterpret_cast<const T*>(addr); }

		inline size_t size() const { return len; }
		inline operator bool() const { return addr != nullptr; }
	};


	class FileView {
		friend File;

	private:
		fd_t fd_;

	public:
		static constexpr fd_t NULL_FD = -1;

		FileView() noexcept: fd_(NULL_FD) { }
		FileView(fd_t fd) noexcept: fd_(fd) { }
		FileView(const FileView&) noexcept = default;
		FileView(FileView&&) noexcept = default;
		FileView& operator=(const FileView&) = default;
		FileView& operator=(FileView&&) = default;

		/** Sets the internal file descriptor to `NULL_FD`, then returns its old value. */
		inline fd_t disown() noexcept { fd_t r = fd_;  fd_ = NULL_FD;  return r; }

		/** POSIX-compliant. */
		inline File dup() const;

		/** POSIX-compliant. */
		File dup2(fd_t fildes2) const;

		/** POSIX-compliant. */
		ssize_t read(void* buf, size_t count);

		/** POSIX-compliant. */
		ssize_t write(const void* buf, size_t count);

		/** POSIX-compliant. */
		off_t lseek(off_t offset, Whence whence);

		/** Almost POSIX-compliant: returns `false` exclusively when an error occurs. */
		bool ftruncate(off_t length);

		/** Almost POSIX-compliant: returns `false` exclusively when an error occurs. */
		bool fsync();

		/** Almost POSIX-compliant: returns `false` exclusively when an error occurs. */
		bool fdatasync();

		/** POSIX-compliant. */
		[[nodiscard]]
		MemMapping mmap(void* addr, size_t len, MemProtFlags prot, MemMapFlags flags, off_t off);

		/** POSIX-compliant. */
		[[nodiscard]]
		inline MemMapping mmap(size_t len, MemProtFlags prot, MemMapFlags flags, off_t off) { return mmap(nullptr, len, prot, flags, off); }

		inline operator bool() const noexcept { return fd_ >= 0; }
		inline fd_t fd() const noexcept { return fd_; }
		inline operator fd_t() const noexcept { return fd_; }
	};


	class File : public FileView {
	public:
		/** POSIX-compliant. */
		static File open(const char* pathname, OpenFlags flags, mode_t mode = 00660);

		/** POSIX-compliant. */
		static File creat(const char* pathname, mode_t mode);

		/** POSIX-compliant. */
		static File openat(fd_t dirfd, const char* pathname, OpenFlags flags, mode_t mode = 00660);

		#ifdef POSIXFIO_STL_STRINGVIEW
			/** POSIX-compliant. */
			static File open(std::string_view pathname, OpenFlags flags, mode_t mode = 00660);

			/** POSIX-compliant. */
			static File creat(std::string_view pathname, mode_t mode);

			/** POSIX-compliant. */
			static File openat(fd_t dirfd, std::string_view pathname, OpenFlags flags, mode_t mode = 00660);
		#endif

		inline File(): FileView(NULL_FD) { }
		inline File(File&& mv): FileView(mv.fd_) { mv.disown(); }
		inline File(const File& cp): File(FileView(cp.fd_)) { }
		inline File& operator=(const File& cp) { return operator=(FileView(cp.fd_)); }

		explicit File(const FileView&);
		~File();
		File& operator=(const FileView&);
		File& operator=(File&&);

		/** Almost POSIX-compliant: returns `false` exclusively when an error occurs. */
		bool close();
	};
	inline File FileView::dup() const { return File(*this); }


	struct Pipe {
		File rd, wr;

		/** POSIX-compliant. */
		static Pipe create();

		inline Pipe() { }
		Pipe(const Pipe&) = default;
		Pipe(Pipe&&) = default;
		~Pipe() = default;

		Pipe& operator=(const Pipe&) = default;
		Pipe& operator=(Pipe&&) = default;

		inline ssize_t read(void* buf, size_t count) { return rd.read(buf, count); };
		inline ssize_t write(void* buf, size_t count) { return wr.write(buf, count); };

		inline operator bool() const { return rd && wr; }
		inline bool operator!() const { return ! operator bool(); }
	};


	#ifdef POSIXFIO_NOTHROW
		}
	#endif

}
