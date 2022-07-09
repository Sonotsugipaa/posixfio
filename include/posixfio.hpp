#pragma once

extern "C" {
	#include <fcntl.h>
}

#include <cstdint>
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
	private:
		fd_t fd_;

	public:
		static constexpr fd_t NULL_FD = -1;

		static File open(const char* pathname, int flags, mode_t mode = 00660);
		static File creat(const char* pathname, mode_t mode);
		static File openat(fd_t dirfd, const char* pathname, int flags, mode_t mode = 0);

		File();
		File(fd_t);
		File(const File&);
		File(File&&);
		~File();

		bool close();

		File& operator=(const File&);
		File& operator=(File&&);

		inline fd_t disown() { fd_t r = fd_;  fd_ = NULL_FD;  return r; }

		inline File dup() const { return File(*this); }
		File dup2(fd_t fildes2) const;

		ssize_t read(void* buf, size_t count);
		ssize_t write(const void* buf, size_t count);
		off_t lseek(off_t offset, int whence);

		bool ftruncate(off_t length);
		bool fsync();
		bool fdatasync();

		inline operator bool() const { return fd_ >= 0; }
		inline bool operator!() const { return ! operator bool(); }
		inline fd_t fd() const { return fd_; }
		inline operator fd_t() const { return fd_; }
	};


	struct Pipe {
		File rd, wr;

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
		inline FileView(const File& f): File(f.fd()) { }
		inline ~FileView() { disown(); }

		inline bool close() { disown(); return true; }
	};


	#ifdef POSIXFIO_NOTHROW
		}
	#endif

}
