#pragma once

#include <posixfio.hpp>

#include <utility>
#include <cstddef>
#include <cstring>



namespace posixfio {

	namespace buffer_op {

		/* Functions in this namespace are only to be used by this library,
		 * and their signatures may change at any time in any way.
		 * */

		ssize_t bfRead(FileView, void* buf, size_t* bufOffsetPtr, size_t* bufSizePtr, size_t bufCapacity, void* dst, size_t count);
		ssize_t bfWrite(FileView, void* buf, size_t* bufOffsetPtr, size_t* bufSizePtr, size_t bufCapacity, const void* src, size_t count);

	};



	using byte_t = unsigned char;


	ssize_t readAll(FileView, void* buf, size_t count);

	ssize_t writeAll(FileView, const void* buf, size_t count);


	class HeapInputBuffer {
	private:
		FileView file_;
		size_t offset_;
		size_t size_;
		size_t capacity_;
		byte_t* buffer_;
		bool direction_;

	public:
		HeapInputBuffer();
		HeapInputBuffer(const HeapInputBuffer&) = delete;
		HeapInputBuffer(HeapInputBuffer&&);
		HeapInputBuffer(FileView, size_t capacity);
		~HeapInputBuffer();

		HeapInputBuffer& operator=(HeapInputBuffer&&);

		inline const FileView file() const { return file_; }

		ssize_t read(void* buf, size_t count);

		inline void discard() {
			offset_ = 0;
			size_ = 0;
		}
	};


	class HeapOutputBuffer {
	private:
		FileView file_;
		size_t offset_;
		size_t size_;
		size_t capacity_;
		byte_t* buffer_;
		bool direction_;

	public:
		HeapOutputBuffer();
		HeapOutputBuffer(const HeapOutputBuffer&) = delete;
		HeapOutputBuffer(HeapOutputBuffer&&);
		HeapOutputBuffer(FileView, size_t capacity);
		~HeapOutputBuffer();

		HeapOutputBuffer& operator=(HeapOutputBuffer&&);

		inline const FileView file() const { return file_; }

		ssize_t write(const void* buf, size_t count);

		void flush();
	};


	template<size_t capacity = 4096>
	class ArrayInputBuffer {
	private:
		FileView file_;
		size_t bufferOffset_;
		size_t bufferSize_;
		byte_t buffer_[capacity];

	public:
		ArrayInputBuffer() = default;
		ArrayInputBuffer(const ArrayInputBuffer&) = delete;

		ArrayInputBuffer(ArrayInputBuffer&& mv):
				file_(std::move(mv.file_)),
				bufferOffset_(0),
				bufferSize_(std::move(mv.bufferSize_))
		{
			memcpy(buffer_,
				mv.buffer_,
				bufferSize_ - bufferOffset_ );
		}

		ArrayInputBuffer(FileView file):
				file_(std::move(file)),
				bufferOffset_(0),
				bufferSize_(0)
		{ }

		ArrayInputBuffer& operator=(ArrayInputBuffer&& mv) {
			this->~ArrayInputBuffer();
			return * new (this) ArrayInputBuffer(std::move(mv));
		}

		const FileView file() const { return file_; }

		ssize_t read(void* buf, size_t count) {
			return buffer_op::bfRead(file_, buffer_, &bufferOffset_, &bufferSize_, capacity, buf, count);
		}

		void discard() {
			bufferOffset_ = 0;
			bufferSize_ = 0;
		}
	};


	template<size_t capacity = 4096>
	class ArrayOutputBuffer {
	private:
		FileView file_;
		size_t bufferOffset_;
		size_t bufferSize_;
		byte_t buffer_[capacity];

	public:
		ArrayOutputBuffer() = default;
		ArrayOutputBuffer(const ArrayOutputBuffer&) = delete;

		ArrayOutputBuffer(ArrayOutputBuffer&& mv):
				file_(std::move(mv.file_)),
				bufferOffset_(0),
				bufferSize_(std::move(mv.bufferSize_))
		{
			memcpy(buffer_, mv.buffer_, bufferSize_);
		}

		ArrayOutputBuffer(FileView file):
				file_(std::move(file)),
				bufferOffset_(0),
				bufferSize_(0)
		{ }

		~ArrayOutputBuffer() {
			if(file_) {
				if(bufferSize_ > bufferOffset_) {
					writeAll(file_,
						reinterpret_cast<byte_t*>(buffer_) + bufferOffset_,
						bufferSize_ - bufferOffset_ );
				}
			}
		}

		ArrayOutputBuffer& operator=(ArrayOutputBuffer&& mv) {
			this->~ArrayOutputBuffer();
			return * new (this) ArrayOutputBuffer(std::move(mv));
		}

		const FileView file() const { return file_; }

		ssize_t write(const void* buf, size_t count) {
			return buffer_op::bfWrite(file_, buffer_, &bufferOffset_, &bufferSize_, capacity, buf, count);
		}

		void flush() {
			writeAll(file_,
				reinterpret_cast<byte_t*>(buffer_) + bufferOffset_,
				bufferSize_ - bufferOffset_ );
		}
	};

}
