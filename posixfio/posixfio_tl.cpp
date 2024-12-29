#include "posixfio_tl.hpp"

#include <cerrno>
#include <cassert>
#include <cstring>
#include <new>
#include <algorithm>



// Use the next four lines to limit I/O request sizes, ONLY FOR DEBUGGING OR MANUAL TESTING;
// use the fifth one to remind yourself of the former.
//#define POSIXFIO_DBG_LIMIT_DIRECT_RD 2041
//#define POSIXFIO_DBG_LIMIT_DIRECT_WR 2042
//#define POSIXFIO_DBG_LIMIT_LEAST_RD 2031
//#define POSIXFIO_DBG_LIMIT_LEAST_WR 2032
//#pragma message "Temporary I/O limits have been enabled in \"" __FILE__ "\""



namespace posixfio {

	namespace _buffer_op_impl::v0_6_1 {

		ssize_t bfRead(
				FileView file,
				void* buf, size_t* bufBeginPtr, size_t* bufEndPtr, size_t bufCapacity,
				void* dst, size_t count
		) {
			//         | ..... | DataDataDataDataData | .......................... |
			// Layout: | begin | window = end - begin | available = capacity - end |
			// All bytes before `begin` have already been read
			// All bytes between `begin` and `end` are queued to be read
			#define BYTES_(PTR_) reinterpret_cast<byte_t*>(PTR_)
			#ifdef POSIXFIO_NOTHROW
				#define CHECK_ERR_ { if(rd < 0) [[unlikely]] { return rd; } }
			#else
				#define CHECK_ERR_ { assert(rd >= 0); }
			#endif
			assert(buf);
			assert(bufEndPtr);
			assert(bufBeginPtr);
			const auto initBufEnd = *bufEndPtr;
			const auto initBufBegin = *bufBeginPtr;
			const auto windowSize = initBufEnd - initBufBegin;  assert(initBufEnd >= initBufBegin);
			const bool smallReadRequest = (count < bufCapacity);
			const bool someBytesBuffered = (windowSize > 0);
			const bool someSpaceAvailable = (initBufEnd < bufCapacity);
			if(someBytesBuffered) {
				count = std::min(count, windowSize);
				memcpy(dst, BYTES_(buf) + initBufBegin, count);
				*bufBeginPtr += count;
				return count;
			}
			else if(smallReadRequest && someSpaceAvailable) {
				// Populate the buffer and extract some bytes
				ssize_t rd = file.read(BYTES_(buf), bufCapacity);
				CHECK_ERR_
				size_t retn = std::min<size_t>(count, rd);
				*bufBeginPtr = retn;
				*bufEndPtr = rd;
				memcpy(BYTES_(dst), BYTES_(buf), retn);
				return retn;
			}
			else {
				// Read-through
				size_t directRdCount = count - windowSize;
				memcpy(dst, BYTES_(buf) + initBufBegin, windowSize);
				*bufBeginPtr = 0;
				*bufEndPtr = 0;
				#ifdef POSIXFIO_DBG_LIMIT_DIRECT_RD
					directRdCount = std::min<size_t>(directRdCount, POSIXFIO_DBG_LIMIT_DIRECT_RD);
				#endif
				ssize_t rd = file.read(BYTES_(dst) + windowSize, directRdCount);
				CHECK_ERR_
				return windowSize + rd;
			}
			#undef CHECK_ERR_
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
			#ifdef POSIXFIO_NOTHROW
				#define CHECK_ERR_ { assert(wr != 0);  if(wr < 0) [[unlikely]] { return wr; } }
			#else
				#define CHECK_ERR_ { assert(wr > 0); }
			#endif
			assert(buf);
			assert(bufEndPtr);
			assert(bufBeginPtr);
			const auto initBufEnd = *bufEndPtr;
			const auto initBufBegin = *bufBeginPtr;
			assert(initBufEnd >= initBufBegin);
			const auto initAvailSpace = bufCapacity - initBufEnd;
			const auto bufferedWrCount = initBufEnd - initBufBegin;
			const bool smallWriteRequest = (count < bufCapacity);
			const bool someSpaceAvailable = (initAvailSpace > 0);
			const bool bufferNotEmpty = (initBufBegin < initBufEnd);
			if(someSpaceAvailable && (smallWriteRequest || bufferNotEmpty)) {
				count = std::min(count, initAvailSpace);
				memcpy(BYTES_(buf) + *bufEndPtr, src, count);
				*bufEndPtr += count;
				assert(*bufEndPtr <= bufCapacity);
				return count;
			} else {
				if(bufferedWrCount > 0) {
					// Flush the buffer
					ssize_t wr;
					size_t wrTotal = 0;
					do {
						// Nothing can be done until the buffer is flushed;
						// a possible solution would report a 0-byte write, but, while
						// technically allowed, a user could reasonably believe that, like a read op,
						// a write op would write *at least* one byte.
						size_t flushCount = bufferedWrCount - wrTotal;
						wr = file.write(BYTES_(buf), flushCount);
						CHECK_ERR_
						*bufBeginPtr += wr;
						wrTotal += wr;
						assert(*bufBeginPtr <= *bufEndPtr);
						assert(size_t(wrTotal) <= bufferedWrCount);
					} while (wrTotal < bufferedWrCount);
					assert(wr > 0 /* An error should already have been thrown or returned at this point */);
					assert(wrTotal == bufferedWrCount /* Needs to write out the entire buffer */);
				}
				// At this point, the buffer is guaranteed to be empty
				assert(*bufBeginPtr == *bufEndPtr);
				*bufBeginPtr = 0;
				if(count < bufCapacity) {
					// Write some bytes to the buffer, eventually report a partial write
					count = std::min(count, bufCapacity);
					memcpy(BYTES_(buf), src, count);
					*bufEndPtr = count;
					return count;
				} else {
					// Write-through
					*bufEndPtr = 0;
					ssize_t wr = file.write(CBYTES_(src), count);
					CHECK_ERR_;
					return wr;
				}
			}

			#undef CHECK_ERR_
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
				if(wr < 0) [[unlikely]] return wr;
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
				if(wr < 0) [[unlikely]] return wr;
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
		mv.buffer_ = nullptr;
	}


	InputBuffer::InputBuffer(FileView file, size_t cap):
			file_(file),
			begin_(0),
			end_(0),
			capacity_(cap > 1? cap : size_t(1)),
			buffer_((byte_t*) operator new[](capacity_ * sizeof(capacity_)))
	{
		assert(cap > 0);
	}


	InputBuffer::~InputBuffer() {
		if(file_) {
			delete[] buffer_;
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
		return _buffer_op_impl::bfRead(file_, buffer_, &begin_, &end_, capacity_, userBuf, count);
	}


	ssize_t InputBuffer::readLeast(void* buf, size_t least, size_t count) {
		ssize_t total = 0;
		while(size_t(total) < least) {
			auto rd = _buffer_op_impl::bfRead(file_, buffer_, &begin_, &end_, capacity_, reinterpret_cast<byte_t*>(buf) + total, ssize_t(count) - total);
			if(rd == 0) [[unlikely]] return total;
			if(rd < 0) [[unlikely]] return -1;
			total += rd;
		}
		return total;
	}


	ssize_t InputBuffer::readAll(void* buf, size_t count) {
		return readLeast(buf, count, count);
	}


	ssize_t InputBuffer::fill() {
		if(end_ < capacity_) {
			ssize_t rd = file_.read(buffer_ + end_, capacity_ - end_);
			if(rd >= 0) [[likely]] end_ += rd;
			return rd;
		} else {
			return 0;
		}
	}


	ssize_t InputBuffer::fwd() {
		if(begin_ + 1 >= end_) {
			if(end_ >= capacity_)  discard();
			ssize_t fl = fill();
			if(fl <= 0)  return fl;
		} else {
			++ begin_;
		}
		return 1;
	}



	OutputBuffer::OutputBuffer() noexcept:
			file_(),
			begin_(0),
			end_(0)
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
		mv.buffer_ = nullptr;
	}


	OutputBuffer::OutputBuffer(FileView file, size_t cap):
			file_(file),
			begin_(0),
			end_(0),
			capacity_(cap > 1? cap : size_t(1)),
			buffer_((byte_t*) operator new[](capacity_ * sizeof(capacity_)))
	{
		assert(cap > 0);
	}


	OutputBuffer::~OutputBuffer() {
		if(file_) {
			assert(end_ >= begin_);
			if(end_ > begin_)  posixfio::writeAll(file_, reinterpret_cast<byte_t*>(buffer_) + begin_, end_ - begin_);
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
		return _buffer_op_impl::bfWrite(file_, buffer_, &begin_, &end_, capacity_, userBuf, count);
	}


	ssize_t OutputBuffer::writeLeast(const void* buf, size_t least, size_t count) {
		ssize_t total = 0;
		while(size_t(total) < least) {
			auto wr = _buffer_op_impl::bfWrite(file_, buffer_, &begin_, &end_, capacity_, reinterpret_cast<const byte_t*>(buf) + total, ssize_t(count) - total);
			if(wr == 0) [[unlikely]] return total; // Shouldn't happen at all
			if(wr < 0) [[unlikely]] return -1;
			total += wr;
		}
		return total;
	}


	ssize_t OutputBuffer::writeAll(const void* buf, size_t count) {
		return writeLeast(buf, count, count);
	}


	void OutputBuffer::flush() {
		posixfio::writeAll(file_, reinterpret_cast<byte_t*>(buffer_) + begin_, end_ - begin_);
		begin_ = 0;
		end_ = 0;
	}

}
