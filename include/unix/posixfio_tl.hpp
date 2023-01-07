#pragma once

#include <posixfio.hpp>

#include <utility>
#include <cstddef>
#include <cstring>



namespace posixfio {

	namespace buffer_op {

		/* This namespace is only to be used internally by this library,
		 * and its signatures may change at any time in any way.
		 * */

		ssize_t bfRead(FileView, void* buf, size_t* bufBegPtr, size_t* bufEndPtr, void* dst, size_t count);
		ssize_t bfWrite(FileView, void* buf, size_t* bufBegPtr, size_t* bufEndPtr, size_t bufCapacity, const void* src, size_t count);

	};



	using byte_t = unsigned char;


	/** Repeatedly reads data from the given FileView, until `count`
	 * bytes have been read, EOF is reached or an error occurs. */
	ssize_t readAll(FileView, void* buf, size_t count);

	/** Repeatedly reads data from the given FileView, until more than
	 * `least` and less than `count+1` bytes have been read,
	 * EOF is reached or an error occurs. */
	ssize_t readLeast(FileView, void* buf, size_t least, size_t count);


	/** Repeatedly writes data to the given FileView, until `count`
	 * bytes have been written or an error occurs. */
	ssize_t writeAll(FileView, const void* buf, size_t count);

	/** Repeatedly writes data to the given FileView, until more than
	 * `least` and less than `count+1` bytes have been written
	 * or an error occurs. */
	ssize_t writeLeast(FileView, const void* buf, size_t least, size_t count);


	class InputBuffer {
	private:
		FileView file_;
		size_t begin_;
		size_t end_;
		size_t capacity_;
		byte_t* buffer_;

	public:
		InputBuffer() noexcept;
		InputBuffer(const InputBuffer&) = delete;
		InputBuffer(InputBuffer&&) noexcept;
		InputBuffer(FileView, size_t capacity);
		~InputBuffer();

		InputBuffer& operator=(InputBuffer&&) noexcept;

		inline const FileView file() const { return file_; }

		/** Similar to File::read, but may fail after a partial read. */
		ssize_t read(void* buf, size_t count);

		/** Similar to readAll, but may fail after a partial read. */
		ssize_t readAll(void* buf, size_t count);

		/** Similar to readLeast, but may fail after a partial read. */
		ssize_t readLeast(void* buf, size_t least, size_t count);

		/** Try to fill the buffer, if it isn't already full. */
		ssize_t fill();

		/** If the buffer is empty, try to fill it; then discard one byte.
		 * The return value follows File::read semantics. */
		ssize_t fwd();

		/** Returns a pointer to the first ready-to-read byte in the buffer. */
		inline byte_t* data() { return buffer_ + begin_; }

		/** Returns a pointer to the first ready-to-read byte in the buffer. */
		inline const byte_t* data() const { return buffer_ + begin_; }

		/** Returns the number of ready-to-read bytes. */
		inline size_t size() const { return end_ - begin_; }

		/** Discard the entire buffer; the next read will try to fill the buffer. */
		inline void discard() { begin_ = 0;  end_ = 0; }
	};


	class OutputBuffer {
	private:
		FileView file_;
		size_t begin_;
		size_t end_;
		size_t capacity_;
		byte_t* buffer_;

	public:
		OutputBuffer() noexcept;
		OutputBuffer(const OutputBuffer&) = delete;
		OutputBuffer(OutputBuffer&&) noexcept;
		OutputBuffer(FileView, size_t capacity);
		~OutputBuffer();

		OutputBuffer& operator=(OutputBuffer&&) noexcept;

		inline const FileView file() const { return file_; }

		/** Similar to File::write, but may fail after a partial write. */
		ssize_t write(const void* buf, size_t count);

		/** Similar to writeAll, but may fail after a partial write. */
		ssize_t writeAll(const void* buf, size_t count);

		/** Similar to writeLeast, but may fail after a partial write. */
		ssize_t writeLeast(const void* buf, size_t least, size_t count);

		/** Write all the ready-to-write bytes. */
		void flush();
	};


	template<size_t capacity = 4096>
	class ArrayInputBuffer {
		static_assert(capacity > 0);
	private:
		FileView file_;
		size_t bufferBegin_;
		size_t bufferEnd_;
		byte_t buffer_[capacity];

	public:
		ArrayInputBuffer() = default;
		ArrayInputBuffer(const ArrayInputBuffer&) = delete;

		ArrayInputBuffer(ArrayInputBuffer&& mv):
				file_(std::move(mv.file_)),
				bufferBegin_(0),
				bufferEnd_(std::move(mv.bufferEnd_))
		{
			memcpy(buffer_,
				mv.buffer_,
				bufferEnd_ - bufferBegin_ );
		}

		ArrayInputBuffer(FileView file):
				file_(std::move(file)),
				bufferBegin_(0),
				bufferEnd_(0)
		{ }

		ArrayInputBuffer& operator=(ArrayInputBuffer&& mv) {
			this->~ArrayInputBuffer();
			return * new (this) ArrayInputBuffer(std::move(mv));
		}

		const FileView file() const { return file_; }

		/** Similar to File::read, but may fail after a partial read. */
		ssize_t read(void* buf, size_t count) {
			return buffer_op::bfRead(file_, buffer_, &bufferBegin_, &bufferEnd_, buf, count);
		}

		/** Similar to readLeast, but may fail after a partial read. */
		ssize_t readLeast(void* buf, size_t least, size_t count) {
			ssize_t total = 0;
			while(total < least) {
				auto rd = buffer_op::bfRead(file_, buffer_, &bufferBegin_, &bufferEnd_, buf, ssize_t(count) - total);
				if(rd == 0) [[unlikely]] return total;
				if(rd < 0) [[unlikely]] return -1;
				total += rd;
			}
			return total;
		}

		/** Similar to readAll, but may fail after a partial read. */
		ssize_t readAll(void* buf, size_t count) {
			return readLeast(buf, count, count);
		}

		/** Try to fill the buffer, if it isn't already full. */
		ssize_t fill() {
			if(bufferEnd_ < capacity) {
				ssize_t rd = file_.read(buffer_ + bufferEnd_, capacity - bufferEnd_);
				if(rd >= 0) [[likely]]  bufferEnd_ += rd;
				return rd;
			} else {
				return 0;
			}
		}

		/** Discard the entire buffer; the next read will try to fill the buffer. */
		inline void discard() { bufferBegin_ = 0;  bufferEnd_ = 0; }

		/** If the buffer is empty, try to fill it; then discard one byte.
		 * The return value follows File::read semantics. */
		ssize_t fwd() {
			if(bufferBegin_ + 1 >= bufferEnd_) {
				if(bufferEnd_ >= capacity)  discard();
				ssize_t fl = fill();
				if(fl <= 0)  return fl;
			} else {
				++ bufferBegin_;
			}
			return 1;
		}

		/** Returns a pointer to the first ready-to-read byte in the buffer. */
		inline byte_t* data() { return buffer_ + bufferBegin_; }

		/** Returns a pointer to the first ready-to-read byte in the buffer. */
		inline const byte_t* data() const { return buffer_ + bufferBegin_; }

		/** Returns the number of ready-to-read bytes. */
		inline size_t size() const { return bufferEnd_ - bufferBegin_; }
	};


	template<size_t capacity = 4096>
	class ArrayOutputBuffer {
	private:
		FileView file_;
		size_t bufferBegin_;
		size_t bufferEnd_;
		byte_t buffer_[capacity];

	public:
		ArrayOutputBuffer() = default;
		ArrayOutputBuffer(const ArrayOutputBuffer&) = delete;

		ArrayOutputBuffer(ArrayOutputBuffer&& mv):
				file_(std::move(mv.file_)),
				bufferBegin_(0),
				bufferEnd_(std::move(mv.bufferEnd_))
		{
			memcpy(buffer_, mv.buffer_, bufferEnd_);
		}

		ArrayOutputBuffer(FileView file):
				file_(std::move(file)),
				bufferBegin_(0),
				bufferEnd_(0)
		{ }

		~ArrayOutputBuffer() {
			if(file_) {
				if(bufferEnd_ > bufferBegin_) {
					posixfio::writeAll(file_,
						reinterpret_cast<byte_t*>(buffer_) + bufferBegin_,
						bufferEnd_ - bufferBegin_ );
				}
			}
		}

		ArrayOutputBuffer& operator=(ArrayOutputBuffer&& mv) {
			this->~ArrayOutputBuffer();
			return * new (this) ArrayOutputBuffer(std::move(mv));
		}

		const FileView file() const { return file_; }

		/** Similar to File::write, but may fail after a partial write. */
		ssize_t write(const void* buf, size_t count) {
			return buffer_op::bfWrite(file_, buffer_, &bufferBegin_, &bufferEnd_, capacity, buf, count);
		}

		/** Similar to writeLeast, but may fail after a partial write. */
		ssize_t writeLeast(const void* buf, size_t least, size_t count) {
			ssize_t total = 0;
			while(total < least) {
				auto rd = buffer_op::bfWrite(file_, buffer_, &bufferBegin_, &bufferEnd_, capacity, buf, ssize_t(count) - total);
				if(rd == 0) [[unlikely]] return total;
				if(rd < 0) [[unlikely]] return -1;
				total += rd;
			}
			return total;
		}

		/** Similar to writeAll, but may fail after a partial write. */
		ssize_t writeAll(const void* buf, size_t count) {
			return writeLeast(buf, count, count);
		}

		/** Write all the ready-to-write bytes. */
		void flush() {
			writeAll(file_,
				reinterpret_cast<byte_t*>(buffer_) + bufferBegin_,
				bufferEnd_ - bufferBegin_ );
			bufferBegin_ = 0;
			bufferEnd_ = 0;
		}
	};

}
