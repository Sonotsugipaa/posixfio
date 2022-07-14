#pragma once

extern "C" {
	#include <fcntl.h>
}

#include <cstdint>
#include <new>
#include <type_traits>



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
	class Errno;
	class File;
	class Pipe;
	class FileView;


	struct Errno {
		int value;

		constexpr Errno(int value): value(value) { }
		constexpr ~Errno() { }
		constexpr operator int() const { return value; }
	};


	class File {
		friend FileView;

	private:
		fd_t fd_;

	public:
		static constexpr fd_t NULL_FD = -1;

		/** POSIX-compliant. */
		static File open(const char* pathname, int flags, mode_t mode = 00660);

		/** POSIX-compliant. */
		static File creat(const char* pathname, mode_t mode);

		/** POSIX-compliant. */
		static File openat(fd_t dirfd, const char* pathname, int flags, mode_t mode = 0);


		File();
		File(fd_t);
		File(const File&);
		File(File&&);
		~File();

		/** Almost POSIX-compliant: returns `false` exclusively when an error occurs. */
		bool close();

		File& operator=(const File&);
		File& operator=(File&&);

		/** Sets the internal file descriptor to `NULL_FD`, then returns its old value. */
		inline fd_t disown() { fd_t r = fd_;  fd_ = NULL_FD;  return r; }

		/** POSIX-compliant. */
		inline File dup() const { return File(*this); }

		/** POSIX-compliant. */
		File dup2(fd_t fildes2) const;

		/** POSIX-compliant. */
		ssize_t read(void* buf, size_t count);

		/** POSIX-compliant. */
		ssize_t write(const void* buf, size_t count);

		/** POSIX-compliant. */
		off_t lseek(off_t offset, int whence);

		/** Almost POSIX-compliant: returns `false` exclusively when an error occurs. */
		bool ftruncate(off_t length);

		/** Almost POSIX-compliant: returns `false` exclusively when an error occurs. */
		bool fsync();

		/** Almost POSIX-compliant: returns `false` exclusively when an error occurs. */
		bool fdatasync();

		inline operator bool() const { return fd_ >= 0; }
		inline bool operator!() const { return ! operator bool(); }
		inline fd_t fd() const { return fd_; }
		inline operator fd_t() const { return fd_; }
	};


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


	class FileView : public File {
	public:
		using File::File;
		inline FileView(fd_t fd): File(fd) { }
		inline FileView(const File& f): File(f.fd_) { }
		inline FileView(const FileView& cp): File(cp.fd_) { }
		inline ~FileView() { disown(); }

		inline FileView& operator=(fd_t fd) { fd_ = fd;  return *this; }
		inline FileView& operator=(const File& f) { fd_ = f.fd_;  return *this; }
		inline FileView& operator=(const FileView& fv) { fd_ = fv.fd_;  return *this; }

		inline bool close() { disown(); return true; }
	};


	#ifdef POSIXFIO_NOTHROW
		}
	#endif

}
