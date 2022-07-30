#include "posixfio_tl.hpp"

#include <cerrno>
#include <cassert>
#include <cstring>
#include <new>
#include <algorithm>



// Use the next two lines to limit I/O request sizes, ONLY FOR DEBUGGING OR MANUAL TESTING;
// use the third one to remind yourself of the former.
//#define POSIXFIO_DBG_LIMIT_DIRECT_RD 2041
//#define POSIXFIO_DBG_LIMIT_DIRECT_WR 2042
//#define POSIXFIO_DBG_LIMIT_LEAST_RD 2031
//#define POSIXFIO_DBG_LIMIT_LEAST_WR 2032
//#pragma message "Temporary I/O limits have been enabled in \"" __FILE__ "\""



namespace posixfio {

	namespace buffer_op {

		ssize_t bfRead(
				FileView file,
				void* buf, size_t* bufBeginPtr, size_t* bufEndPtr,
				void* dst, size_t count
		) {
			//         | ..... | DataDataDataDataData | .......................... |
			// Layout: | begin | window = end - begin | available = capacity - end |
			// All bytes before `begin` have already been read
			// All bytes between `begin` and `end` are queued to be read
			#define BYTES_(PTR_) reinterpret_cast<byte_t*>(PTR_)
			assert(buf);
			assert(bufEndPtr);
			assert(bufBeginPtr);
			auto initBufEnd = *bufEndPtr;
			auto initBufBegin = *bufBeginPtr;
			auto initWindowSize = initBufEnd - initBufBegin;  assert(initBufEnd >= initBufBegin);
			if(count < initWindowSize) {
				// Enough available bytes
				memcpy(dst, BYTES_(buf) + initBufBegin, count);
				*bufBeginPtr += count;
				return count;
			} else {
				#ifdef POSIXFIO_NOTHROW
					#define CHECK_ERR_ { if(rd < 0) [[unlikely]] { return rd; } }
				#else
					#define CHECK_ERR_ { assert(rd >= 0); }
				#endif
				size_t directRdCount = count - initWindowSize;
				memcpy(dst, BYTES_(buf) + initBufBegin, initWindowSize);
				#ifdef POSIXFIO_DBG_LIMIT_DIRECT_RD
					directRdCount = std::min(directRdCount, decltype(directRdCount)(POSIXFIO_DBG_LIMIT_DIRECT_RD));
				#endif
				ssize_t rd = file.read(BYTES_(dst) + initWindowSize, directRdCount);
				CHECK_ERR_
				*bufBeginPtr = 0;
				*bufEndPtr = 0;
				#undef CHECK_ERR_
				return rd + initWindowSize;
			}
			#undef BYTES_
		}

		ssize_t bfWrite(
				FileView file,
				void* buf, size_t* bufBeginPtr, size_t* bufEndPtr, size_t bufCapacity,
				const void* src, size_t count
		) {
			//         | ..... | DataDataDataDataData | .......................... |
			// Layout: | begin | window = end - begin | available = capacity - end |
			// All bytes before `begin` are already written
			// All bytes between `begin` and `end` are queued to be written
			#define BYTES_(PTR_) reinterpret_cast<byte_t*>(PTR_)
			#define CBYTES_(PTR_) reinterpret_cast<const byte_t*>(PTR_)
			assert(buf);
			assert(bufEndPtr);
			assert(bufBeginPtr);
			const auto initBufEnd = *bufEndPtr;
			const auto initBufBegin = *bufBeginPtr;
			assert(initBufEnd >= initBufBegin);
			const auto initAvailSpace = bufCapacity - initBufEnd;
			if(count <= initAvailSpace) {
				// Enough available space in the buffer
				memcpy(BYTES_(buf) + initBufEnd, src, count);
				*bufEndPtr += count;
				return count;
			} else {
				#ifdef POSIXFIO_NOTHROW
					#define CHECK_ERR_ { assert(wr != 0);  if(wr < 0) [[unlikely]] { return wr; } }
				#else
					#define CHECK_ERR_ { assert(wr > 0); }
				#endif
				size_t bufferedWrCount = bufCapacity - initBufBegin;
				size_t directWrCount = count - bufferedWrCount;
				assert(bufferedWrCount + directWrCount == count);
				ssize_t wr = 0;
				if(bufferedWrCount > 0) {
					memcpy(BYTES_(buf) + initBufEnd, src, initAvailSpace);
					wr = writeLeast(file,
						BYTES_(buf) + initBufBegin,
						1, bufferedWrCount );
					CHECK_ERR_
					assert(size_t(wr) <= bufferedWrCount);
				}
				if(size_t(wr) < bufferedWrCount) {
					size_t shift = initBufBegin + wr;
					size_t newBufEnd = bufCapacity - shift;
					assert(bufCapacity > shift);
					memmove(buf, BYTES_(buf) + shift, newBufEnd);
					*bufBeginPtr = 0;
					*bufEndPtr = newBufEnd;
					assert(newBufEnd >= initBufEnd);
					return wr + (newBufEnd - initBufEnd);
				} else {
					#ifdef POSIXFIO_DBG_LIMIT_DIRECT_WR
						directWrCount = std::min(directWrCount, decltype(directWrCount)(POSIXFIO_DBG_LIMIT_DIRECT_WR));
					#endif
					wr = file.write(CBYTES_(src) + bufferedWrCount, directWrCount);
					CHECK_ERR_
					assert(size_t(wr) <= directWrCount);
					*bufBeginPtr = 0;
					*bufEndPtr = 0;
					return wr + bufferedWrCount;
				}
				#undef CHECK_ERR_
			}

			#undef BYTES_
			#undef CBYTES_
		}

	}



	ssize_t readAll(FileView file, void* buf, size_t count) {
		#define BYTES_(PTR_) reinterpret_cast<byte_t*>(PTR_)
		const auto initCount = count;
		ssize_t rd = 1 /* Must be != 0 */;
		while(count > 0 && rd > 0) {
			rd = file.read(buf, count);
			#ifdef POSIXFIO_NOTHROW
				if(rd < 0) return rd;
			#else
				assert(rd >= 0);
			#endif
			assert(count >= size_t(rd));
			buf = BYTES_(buf) + rd;
			count -= size_t(rd);
		}
		return initCount - count;
		#undef BYTES_
	}


	ssize_t readLeast(FileView file, void* buf, size_t least, size_t count) {
		#define BYTES_(PTR_) reinterpret_cast<byte_t*>(PTR_)
		#ifdef POSIXFIO_DBG_LIMIT_LEAST_RD
			count = std::min<size_t>(count, POSIXFIO_DBG_LIMIT_LEAST_RD);
		#endif
		{ // Ensure that `least` <= `count`
			#ifdef NDEBUG
				least = std::min(least, count);
			#else
				assert(least <= count);
			#endif
		}
		const auto initCount = count;
		ssize_t rd = 1 /* Must be != 0 */;
		while(least > 0 && rd > 0) {
			rd = file.read(buf, count);
			#ifdef POSIXFIO_NOTHROW
				if(rd < 0) return rd;
			#else
				assert(rd >= 0);
			#endif
			{
				auto uRd = size_t(rd);
				assert(count >= uRd);
				buf = BYTES_(buf) + rd;
				count -= uRd;
				least = std::max<ssize_t>(0, least - uRd);
			}
		}
		return initCount - count;
		#undef BYTES_
	}


	ssize_t writeAll(FileView file, const void* buf, size_t count) {
		#define CBYTES_(PTR_) reinterpret_cast<const byte_t*>(PTR_)
		const auto initCount = count;
		while(count > 0) {
			ssize_t wr = file.write(buf, count);
			#ifdef POSIXFIO_NOTHROW
				assert(wr != 0);
				if(wr < 0) return wr;
			#else
				assert(wr > 0);
			#endif
			assert(count >= size_t(wr));
			buf = CBYTES_(buf) + wr;
			count -= size_t(wr);
		}
		assert(count == 0); // It's cargo cult programming at this point, BUT it is VERY important and critical for this function to COMPLETELY write the buffer if no IO error occurs.
		return initCount;
		#undef CBYTES_
	}


	ssize_t writeLeast(FileView file, const void* buf, size_t least, size_t count) {
		#define CBYTES_(PTR_) reinterpret_cast<const byte_t*>(PTR_)
		#ifdef POSIXFIO_DBG_LIMIT_LEAST_WR
			count = std::min<size_t>(count, POSIXFIO_DBG_LIMIT_LEAST_WR);
		#endif
		{ // Ensure that `least` <= `count`
			#ifdef NDEBUG
				least = std::min(least, count);
			#else
				assert(least <= count);
			#endif
		}
		const auto initCount = count;
		while(least > 0) {
			ssize_t wr = file.write(buf, count);
			#ifdef POSIXFIO_NOTHROW
				assert(wr != 0);
				if(wr < 0) return wr;
			#else
				assert(wr > 0);
			#endif
			{
				auto uWr = size_t(wr);
				assert(count >= uWr);
				buf = CBYTES_(buf) + wr;
				count -= uWr;
				least = std::max<ssize_t>(0, ssize_t(least) - wr);
			}
		}
		assert(count == 0);
		return initCount - count;
		#undef CBYTES_
	}



	InputBuffer::InputBuffer() noexcept:
			file_()
			#ifndef NDEBUG
				, buffer_(nullptr)
			#endif
	{ }


	InputBuffer::InputBuffer(InputBuffer&& mv) noexcept:
			#define MV_(MEMBER_) MEMBER_(std::move(mv.MEMBER_))
			#define CP_(MEMBER_) MEMBER_(mv.MEMBER_)
				MV_(file_),
				CP_(begin_),
				CP_(end_),
				CP_(capacity_),
				CP_(buffer_)
			#undef MV_
			#undef CP_
	{
		#ifndef NDEBUG
			mv.buffer_ = nullptr;
		#endif
	}


	InputBuffer::InputBuffer(FileView file, size_t cap):
			file_(file),
			begin_(0),
			end_(0),
			capacity_(cap),
			buffer_(new byte_t[cap])
	{
		assert(capacity_ > 0);
		if(capacity_ < 1) capacity_ = 1;
	}


	InputBuffer::~InputBuffer() {
		if(file_) {
			delete[] buffer_;
			file_.close();
			#ifndef NDEBUG
				buffer_ = nullptr;
			#endif
		}
	}


	InputBuffer& InputBuffer::operator=(InputBuffer&& mv) noexcept {
		this->~InputBuffer();
		return * new (this) InputBuffer(std::move(mv));
	}


	ssize_t InputBuffer::read(void* userBuf, size_t count) {
		return buffer_op::bfRead(file_, buffer_, &begin_, &end_, userBuf, count);
	}


	ssize_t InputBuffer::fill() {
		if(end_ < capacity_) {
			ssize_t rd = file_.read(buffer_, capacity_ - end_);
			if(rd > 0) end_ += rd;
			return rd;
		} else {
			return 0;
		}
	}


	ssize_t InputBuffer::fwd() {
		if(begin_ + 1 >= end_) {
			if(end_ >= capacity_)  discard();
			ssize_t fl = fill();
			if(fl <= 0) { return fl; }
		} else {
			++ begin_;
		}
		return 1;
	}



	OutputBuffer::OutputBuffer() noexcept:
			file_()
			#ifndef NDEBUG
				, buffer_(nullptr)
			#endif
	{ }


	OutputBuffer::OutputBuffer(OutputBuffer&& mv) noexcept:
			#define MV_(MEMBER_) MEMBER_(std::move(mv.MEMBER_))
			#define CP_(MEMBER_) MEMBER_(mv.MEMBER_)
				MV_(file_),
				CP_(begin_),
				CP_(end_),
				CP_(capacity_),
				CP_(buffer_)
			#undef MV_
			#undef CP_
	{
		#ifndef NDEBUG
			mv.buffer_ = nullptr;
		#endif
	}


	OutputBuffer::OutputBuffer(FileView file, size_t cap):
			file_(file),
			begin_(0),
			end_(0),
			capacity_(cap),
			buffer_(new byte_t[cap])
	{
		assert(capacity_ > 0);
		if(capacity_ < 1) capacity_ = 1;
	}


	OutputBuffer::~OutputBuffer() {
		if(file_) {
			assert(end_ >= begin_);
			if(end_ > begin_)  writeAll(file_, reinterpret_cast<byte_t*>(buffer_) + begin_, end_ - begin_);
			delete[] buffer_;
			#ifndef NDEBUG
				buffer_ = nullptr;
			#endif
		}
	}


	OutputBuffer& OutputBuffer::operator=(OutputBuffer&& mv) noexcept {
		this->~OutputBuffer();
		return * new (this) OutputBuffer(std::move(mv));
	}


	ssize_t OutputBuffer::write(const void* userBuf, size_t count) {
		return buffer_op::bfWrite(file_, buffer_, &begin_, &end_, capacity_, userBuf, count);
	}

	void OutputBuffer::flush() {
		writeAll(file_, reinterpret_cast<byte_t*>(buffer_) + begin_, end_ - begin_);
	}

}
