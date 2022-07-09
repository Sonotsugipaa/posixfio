#include "posixfio_tl.hpp"

#include <cerrno>
#include <cassert>
#include <cstring>
#include <new>
#include <algorithm>

#include <unistd.h>



// Use the next two lines to limit I/O request sizes, ONLY FOR DEBUGGING OR MANUAL TESTING;
// use the third one to remind yourself of the former.
//#define POSIXFIO_DBG_LIMIT_RD 421
//#define POSIXFIO_DBG_LIMIT_WR 397
//#pragma message "Temporary I/O limits have been enabled in \"" __FILE__ "\""



namespace posixfio {

	namespace buffer_op {

		ssize_t bfRead(
				FileView file,
				void* buf, size_t* bufOffsetPtr, size_t* bufSizePtr, size_t bufCapacity,
				void* dst, size_t count
		) {
			//         | ...... | DataDataDataDataDataDa | ........................... |
			// Layout: | offset | window = size - offset | available = capacity - size |
			// All bytes before `offset` have already been read
			// All bytes between `offset` and `size` are queued to be read
			#define BYTES_(PTR_) reinterpret_cast<byte_t*>(PTR_)
			assert(buf);
			assert(bufSizePtr);
			assert(bufSizePtr);
			assert(bufOffsetPtr);
			auto initBufSize = *bufSizePtr;
			auto initBufOff = *bufOffsetPtr;
			auto initWindowSize = initBufSize - initBufOff;  assert(initBufSize >= initBufOff);
			if(count < initWindowSize) {
				// Enough available bytes
				memcpy(dst, BYTES_(buf) + initBufOff, count);
				*bufOffsetPtr += count;
				return count;
			} else {
				/* Can optimize here:
				 * read into the buffer once, in order to avoid unnecessarily small reads;
				 * not more than once, to avoid unnecesary buffering.
				 * */ (void) bufCapacity;
				#ifdef POSIXFIO_NOTHROW
					#define CHECK_ERR_ { if(rd < 0) [[unlikely]] { return rd; } }
				#else
					#define CHECK_ERR_ { assert(rd >= 0); }
				#endif
				size_t directRdCount = count - initWindowSize;
				memcpy(dst, BYTES_(buf) + initBufOff, initWindowSize);
				#ifdef POSIXFIO_DBG_LIMIT_RD
					directRdCount = std::min(directRdCount, decltype(directRdCount)(POSIXFIO_DBG_LIMIT_RD));
				#endif
				ssize_t rd = file.read(BYTES_(dst) + initWindowSize, directRdCount);
				CHECK_ERR_
				*bufOffsetPtr = 0;
				*bufSizePtr = 0;
				#undef CHECK_ERR_
				return rd + initWindowSize;
			}
			#undef BYTES_
		}

		ssize_t bfWrite(
				FileView file,
				void* buf, size_t* bufOffsetPtr, size_t* bufSizePtr, size_t bufCapacity,
				const void* src, size_t count
		) {
			//         | ...... | DataDataDataDataDataDa | ........................... |
			// Layout: | offset | window = size - offset | available = capacity - size |
			// All bytes before `offset` are already written
			// All bytes between `offset` and `size` are queued to be written
			#define BYTES_(PTR_) reinterpret_cast<byte_t*>(PTR_)
			#define CBYTES_(PTR_) reinterpret_cast<const byte_t*>(PTR_)
			assert(buf);
			assert(bufSizePtr);
			assert(bufSizePtr);
			assert(bufOffsetPtr);
			auto initBufSize = *bufSizePtr;
			auto initBufOff = *bufOffsetPtr;
			assert(initBufSize >= initBufOff);
			auto initAvailSpace = bufCapacity - initBufSize;
			if(count <= initAvailSpace) {
				// Enough available space in the buffer
				memcpy(BYTES_(buf) + initBufSize, src, count);
				*bufSizePtr += count;
				return count;
			} else {
				/* Can optimize here:
				 * write the buffer once, in order to avoid unnecessarily small writes;
				 * not more than once, to avoid unnecesary buffering.
				 * */
				#ifdef POSIXFIO_NOTHROW
					#define CHECK_ERR_ { assert(wr != 0);  if(wr < 0) [[unlikely]] { return wr; } }
				#else
					#define CHECK_ERR_ { assert(wr > 0); }
				#endif
				size_t bufferedWrCount = bufCapacity - initBufOff;
				size_t directWrCount = count - bufferedWrCount;
				assert(bufferedWrCount + directWrCount == count);
				ssize_t wr;
				if(bufferedWrCount > 0) {
					memcpy(BYTES_(buf) + initBufSize, src, initAvailSpace);
					wr = writeAll(file,
						BYTES_(buf) + initBufOff,
						bufferedWrCount );
					CHECK_ERR_
					assert(size_t(wr) == bufferedWrCount);
				}
				#ifdef POSIXFIO_DBG_LIMIT_WR
					directWrCount = std::min(directWrCount, decltype(directWrCount)(POSIXFIO_DBG_LIMIT_WR));
				#endif
				wr = file.write(CBYTES_(src) + bufferedWrCount, directWrCount);
				CHECK_ERR_
				assert(size_t(wr) <= directWrCount);
				*bufOffsetPtr = 0;
				*bufSizePtr = 0;
				#undef CHECK_ERR_
				return wr + bufferedWrCount;
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



	InputBuffer::InputBuffer():
			file_()
			#ifndef NDEBUG
				, buffer_(nullptr)
			#endif
	{ }


	InputBuffer::InputBuffer(InputBuffer&& mv):
			#define MV_(MEMBER_) MEMBER_(std::move(mv.MEMBER_))
			#define CP_(MEMBER_) MEMBER_(mv.MEMBER_)
				MV_(file_),
				CP_(offset_),
				CP_(size_),
				CP_(capacity_),
				CP_(buffer_),
				CP_(direction_)
			#undef MV_
			#undef CP_
	{
		#ifndef NDEBUG
			mv.buffer_ = nullptr;
		#endif
	}


	InputBuffer::InputBuffer(FileView file, size_t cap):
			file_(file),
			offset_(0),
			size_(0),
			capacity_(cap),
			buffer_(new byte_t[cap]),
			direction_(false)
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


	InputBuffer& InputBuffer::operator=(InputBuffer&& mv) {
		this->~InputBuffer();
		return * new (this) InputBuffer(std::move(mv));
	}


	ssize_t InputBuffer::read(void* userBuf, size_t count) {
		return buffer_op::bfRead(file_, buffer_, &offset_, &size_, capacity_, userBuf, count);
	}



	OutputBuffer::OutputBuffer():
			file_()
			#ifndef NDEBUG
				, buffer_(nullptr)
			#endif
	{ }


	OutputBuffer::OutputBuffer(OutputBuffer&& mv):
			#define MV_(MEMBER_) MEMBER_(std::move(mv.MEMBER_))
			#define CP_(MEMBER_) MEMBER_(mv.MEMBER_)
				MV_(file_),
				CP_(offset_),
				CP_(size_),
				CP_(capacity_),
				CP_(buffer_),
				CP_(direction_)
			#undef MV_
			#undef CP_
	{
		#ifndef NDEBUG
			mv.buffer_ = nullptr;
		#endif
	}


	OutputBuffer::OutputBuffer(FileView file, size_t cap):
			file_(file),
			offset_(0),
			size_(0),
			capacity_(cap),
			buffer_(new byte_t[cap]),
			direction_(false)
	{
		assert(capacity_ > 0);
		if(capacity_ < 1) capacity_ = 1;
	}


	OutputBuffer::~OutputBuffer() {
		if(file_) {
			assert(size_ >= offset_);
			if(size_ > offset_)  writeAll(file_, reinterpret_cast<byte_t*>(buffer_) + offset_, size_ - offset_);
			delete[] buffer_;
			#ifndef NDEBUG
				buffer_ = nullptr;
			#endif
		}
	}


	OutputBuffer& OutputBuffer::operator=(OutputBuffer&& mv) {
		this->~OutputBuffer();
		return * new (this) OutputBuffer(std::move(mv));
	}


	ssize_t OutputBuffer::write(const void* userBuf, size_t count) {
		return buffer_op::bfWrite(file_, buffer_, &offset_, &size_, capacity_, userBuf, count);
	}

	void OutputBuffer::flush() {
		writeAll(file_, reinterpret_cast<byte_t*>(buffer_) + offset_, size_ - offset_);
	}

}
