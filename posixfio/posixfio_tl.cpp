#include "posixfio_tl.hpp"

#include <cerrno>
#include <cassert>
#include <cstring>
#include <new>
#include <algorithm>

#include <unistd.h>



namespace posixfio {

	namespace buffer_op {

		ssize_t bfRead(
				FileView file,
				void* buf, size_t* bufSizePtr, size_t* bufOffsetPtr, size_t bufCapacity,
				void* dst, size_t count
		) {
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
				size_t directRdCount = count - initWindowSize;
				memcpy(dst, BYTES_(buf) + initBufOff, initWindowSize);
				ssize_t rd = file.read(BYTES_(dst) + initWindowSize, directRdCount);
				*bufOffsetPtr = 0;
				*bufSizePtr = 0;
				#ifdef POSIXFIO_NOTHROW
					if(rd < 0) [[unlikely]] {
						return rd;
					} else {
						return rd + initWindowSize;
					}
				#else
					assert(rd >= 0);
					return rd + initWindowSize;
				#endif
			}
			#undef BYTES_
		}

		ssize_t bfWrite(
				FileView file,
				void* buf, size_t* bufSizePtr, size_t* bufOffsetPtr, size_t bufCapacity,
				const void* src, size_t count
		) {
			#define BYTES_(PTR_) reinterpret_cast<byte_t*>(PTR_)
			#define CBYTES_(PTR_) reinterpret_cast<const byte_t*>(PTR_)
			assert(buf);
			assert(bufSizePtr);
			assert(bufSizePtr);
			assert(bufOffsetPtr);
			auto initBufSize = *bufSizePtr;
			auto initBufOff = *bufOffsetPtr;
			auto initWindowSize = initBufSize - initBufOff;  assert(initBufSize >= initBufOff);
			if(count < initWindowSize) {
				// Enough available space in the buffer
				memcpy(BYTES_(buf) + initBufSize, src, count);
				*bufSizePtr += count;
				return count;
			} else {
				/* Can optimize here:
				 * write the buffer once, in order to avoid unnecessarily small writes;
				 * not more than once, to avoid unnecesary buffering.
				 * */ (void) bufCapacity;
				size_t directWrCount = count - initWindowSize;
				memcpy(BYTES_(buf) + initBufSize, src, initWindowSize);
				ssize_t wr = bfFlushWrite(file,
					BYTES_(buf),
					initBufOff,
					initBufSize + initWindowSize );
				wr = file.write(CBYTES_(src) + initWindowSize, directWrCount);
				#ifdef POSIXFIO_NOTHROW
					assert(wr != 0);
					if(wr < 0) [[unlikely]] {
						return wr;
					} else {
						return wr + initWindowSize;
					}
				#else
					assert(wr > 0);
					return wr + initWindowSize;
				#endif
			}

			#undef BYTES_
			#undef CBYTES_
		}

		ssize_t bfFlushWrite(FileView file, const void* buf, size_t bufOffset, size_t bufSize) {
			#define CBYTES_(PTR_) reinterpret_cast<const byte_t*>(PTR_)
			const auto initWindowSize = bufSize - bufOffset;
			buf = CBYTES_(buf) + bufOffset;
			bufSize = initWindowSize;
			while(bufSize > 0) {
				ssize_t wr = file.write(buf, bufSize);
				#ifdef POSIXFIO_NOTHROW
					assert(wr != 0);
					if(wr < 0) return wr;
				#else
					assert(wr > 0);
				#endif
				assert(bufSize >= size_t(wr));
				buf = CBYTES_(buf) + wr;
				bufSize -= size_t(wr);
			}
			assert(bufSize == 0); // It's cargo cult programming at this point, BUT it is VERY important and critical for this function to COMPLETELY write the buffer if no IO error occurs.
			return initWindowSize;
			#undef CBYTES_
		}

	}



	HeapInputBuffer::HeapInputBuffer():
			file_()
			#ifndef NDEBUG
				, buffer_(nullptr)
			#endif
	{ }


	HeapInputBuffer::HeapInputBuffer(HeapInputBuffer&& mv):
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


	HeapInputBuffer::HeapInputBuffer(FileView file, size_t cap):
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


	HeapInputBuffer::~HeapInputBuffer() {
		if(file_) {
			delete[] buffer_;
			file_.close();
			#ifndef NDEBUG
				buffer_ = nullptr;
			#endif
		}
	}


	HeapInputBuffer& HeapInputBuffer::operator=(HeapInputBuffer&& mv) {
		this->~HeapInputBuffer();
		return * new (this) HeapInputBuffer(std::move(mv));
	}


	ssize_t HeapInputBuffer::read(void* userBuf, size_t count) {
		return buffer_op::bfRead(file_, buffer_, &offset_, &size_, capacity_, userBuf, count);
	}



	HeapOutputBuffer::HeapOutputBuffer():
			file_()
			#ifndef NDEBUG
				, buffer_(nullptr)
			#endif
	{ }


	HeapOutputBuffer::HeapOutputBuffer(HeapOutputBuffer&& mv):
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


	HeapOutputBuffer::HeapOutputBuffer(FileView file, size_t cap):
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


	HeapOutputBuffer::~HeapOutputBuffer() {
		if(file_) {
			assert(size_ >= offset_);
			if(size_ > offset_)  buffer_op::bfFlushWrite(file_, buffer_, offset_, size_);
			delete[] buffer_;
			#ifndef NDEBUG
				buffer_ = nullptr;
			#endif
		}
	}


	HeapOutputBuffer& HeapOutputBuffer::operator=(HeapOutputBuffer&& mv) {
		this->~HeapOutputBuffer();
		return * new (this) HeapOutputBuffer(std::move(mv));
	}


	ssize_t HeapOutputBuffer::write(const void* userBuf, size_t count) {
		return buffer_op::bfWrite(file_, buffer_, &offset_, &size_, capacity_, userBuf, count);
	}

	void HeapOutputBuffer::flush() {
		buffer_op::bfFlushWrite(file_, buffer_, offset_, size_);
	}

}
