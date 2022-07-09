#include <test_tools.hpp>

#include "posixfio_tl.hpp"

#include <array>
#include <iostream>
#include <string>
#include <random>
#include <cstring>
#include <cassert>
#include <memory>



namespace {

	using namespace posixfio;

	constexpr auto eFailure = utest::ResultType::eFailure;
	constexpr auto eNeutral = utest::ResultType::eNeutral;
	constexpr auto eSuccess = utest::ResultType::eSuccess;

	const std::string tmpFile = "tmpfile";

	std::string ioPayload;

	#define IO_PAYLOAD_BUFFER_(NAME_) std::string NAME_; NAME_.resize(ioPayload.size());


	#ifdef POSIXFIO_NOTHROW
		ssize_t alwaysThrowErr(ssize_t result) {
			if(result < 0) {
				auto oldErrno = errno;
				throw posixfio::Errno(oldErrno);
			}
			return result;
		}

		File alwaysThrowErr(File&& file) {
			if(! file) {
				auto oldErrno = errno;
				throw posixfio::Errno(oldErrno);
			}
			return std::move(file);
		}
	#else
		ssize_t alwaysThrowErr(ssize_t result) {
			assert(result > 0);
			return result;
		}

		File alwaysThrowErr(File&& file) {
			assert(file);
			return std::move(file);
		}
	#endif


	std::string_view errno_str(int err_no) {
		#define ERRNO_CASE1_(ERRNO_) case ERRNO_: return #ERRNO_;
		#define ERRNO_CASE2_(ERRNO_, ERRNO0_) case ERRNO_: return #ERRNO_ ;
		switch(err_no) {
			ERRNO_CASE1_(EACCES)
			ERRNO_CASE2_(EAGAIN, EWOULDBLOCK)
			ERRNO_CASE1_(EBADF)
			ERRNO_CASE1_(EBUSY)
			ERRNO_CASE1_(EDESTADDRREQ)
			ERRNO_CASE1_(EEXIST)
			ERRNO_CASE1_(EFAULT)
			ERRNO_CASE1_(EFBIG)
			ERRNO_CASE1_(EINTR)
			ERRNO_CASE1_(EINVAL)
			ERRNO_CASE1_(EIO)
			ERRNO_CASE1_(EISDIR)
			ERRNO_CASE1_(ELOOP)
			ERRNO_CASE1_(EMFILE)
			ERRNO_CASE1_(ENAMETOOLONG)
			ERRNO_CASE1_(ENODEV)
			ERRNO_CASE1_(ENOENT)
			ERRNO_CASE1_(ENOMEM)
			ERRNO_CASE1_(ENOSPC)
			ERRNO_CASE1_(ENOTDIR)
			ERRNO_CASE1_(ENXIO)
			ERRNO_CASE1_(EOPNOTSUPP)
			ERRNO_CASE1_(EOVERFLOW)
			ERRNO_CASE1_(EPERM)
			ERRNO_CASE1_(EPIPE)
			ERRNO_CASE1_(EROFS)
			ERRNO_CASE1_(ETXTBSY)
			default: return "unknown_errno";
		}
		#undef ERRNO_CASE1_
		#undef ERRNO_CASE2_
	}

	#define CATCH_ERRNO_(OS_) catch(Errno& errNo) { OS_ << "ERRNO " << errno_str(errNo.value) << ' ' << errNo.value << std::endl; }


	std::string mkPayload(size_t payloadSize) {
		static decltype(payloadSize) state = 0;
		std::string r;  r.reserve(payloadSize);
		auto rng = std::minstd_rand(state = (payloadSize ^ state));
		for(size_t i=0; i < payloadSize; ++i) {
			r.push_back(char(rng()));
		}
		return r;
	}


	template<size_t inputBufferStaticCapacity>
	struct InputBuffer {
		using type = ArrayInputBuffer<inputBufferStaticCapacity>;
		static type ctor(FileView file, size_t) { return type(file); }
	};

	template<>
	struct InputBuffer<0> {
		using type = HeapInputBuffer;
		static type ctor(FileView file, size_t varCap) { return type(file, varCap); }
	};


	template<size_t outputBufferStaticCapacity>
	struct OutputBuffer {
		using type = ArrayOutputBuffer<outputBufferStaticCapacity>;
		static type ctor(FileView file, size_t) { return type(file); }
	};

	template<>
	struct OutputBuffer<0> {
		using type = HeapOutputBuffer;
		static type ctor(FileView file, size_t varCap) { return type(file, varCap); }
	};


	template<size_t outputBufferStaticCapacity, size_t outputBufferDynamicCapacity>
	utest::ResultType write_file(std::ostream& out) {
		static_assert((outputBufferStaticCapacity == 0) != (outputBufferDynamicCapacity == 0));
		using Buffer = OutputBuffer<outputBufferStaticCapacity>::type;
		try {
			File f = alwaysThrowErr(File::open(tmpFile.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0400));
			{
				Buffer buf = OutputBuffer<outputBufferStaticCapacity>::ctor(
					f, outputBufferDynamicCapacity );
				ssize_t wr;
				ssize_t count = ioPayload.size();
				size_t cursor = 0;
				#define WR_ { wr = alwaysThrowErr(buf.write(ioPayload.data() + cursor, count));  count -= wr;  cursor += wr; }
				WR_
				if(wr != 0 && count > 0) {
					out << "Partial write of " << wr << '/' << ioPayload.size() << " bytes" << std::endl;
				}
				while(wr != 0 && count > 0) {
					WR_
					out << "Partial write of " << wr << '/' << ioPayload.size() << " bytes" << std::endl;
				}
				#undef WR_
				assert(count == 0);
			}
			return eSuccess;
		} CATCH_ERRNO_(out)
		return eFailure;
	}


	template<size_t inputBufferStaticCapacity, size_t inputBufferDynamicCapacity>
	utest::ResultType read_file(std::ostream& out) {
		static_assert((inputBufferStaticCapacity == 0) != (inputBufferDynamicCapacity == 0));
		using Buffer = InputBuffer<inputBufferStaticCapacity>::type;
		try {
			File f = alwaysThrowErr(File::open(tmpFile.c_str(), O_RDONLY));
			Buffer buf = InputBuffer<inputBufferStaticCapacity>::ctor(
				f, inputBufferDynamicCapacity );
			std::unique_ptr<char[]> cmpString = std::make_unique<char[]>(ioPayload.size());
			ssize_t rd;
			ssize_t count = ioPayload.size();
			size_t cursor = 0;
			#define RD_ { rd = alwaysThrowErr(buf.read(cmpString.get() + cursor, count));  count -= rd;  cursor += rd; }
			RD_
			if(rd != 0 && count > 0) {
				out << "Partial read of " << rd << '/' << ioPayload.size() << " bytes" << std::endl;
			}
			while(rd != 0 && count > 0) {
				RD_
				out << "Partial read of " << rd << '/' << ioPayload.size() << " bytes" << std::endl;
			}
			#undef RD_
			if(count > 0) {
				out << "Unexpected EOF, " << count << " bytes missing" << std::endl;
				return eFailure;
			}
			if(0 != strncmp(ioPayload.data(), cmpString.get(), ioPayload.size())) {
				out << "File content does not match" << std::endl;
				return eFailure;
			}
			return eSuccess;
		} CATCH_ERRNO_(out)
		return eFailure;
	}


	void testPayload(
			utest::TestBatch& batch, size_t payloadSize,
			const std::string& wrString, utest::ResultType (*wrFn)(std::ostream&),
			const std::string& rdString, utest::ResultType (*rdFn)(std::ostream&)
	) {
		ioPayload = mkPayload(payloadSize);
		batch.run(wrString, wrFn);
		batch.run(rdString, rdFn);
	};


	template<size_t n> const std::string constSizeStr = std::to_string(n);

	template<size_t tinyBufSize, size_t smallBufSize, size_t matchBufSize, size_t bigBufSize>
	void testPayloads(
			utest::TestBatch& batch
	) {
		static const std::string wrStackPrefixStr = "Stack buffer write  " + constSizeStr<matchBufSize> + " / ";
		static const std::string rdStackPrefixStr = "Stack buffer read   " + constSizeStr<matchBufSize> + " / ";
		static const std::string wrHeapPrefixStr = "Heap buffer write  " + constSizeStr<matchBufSize> + " / ";
		static const std::string rdHeapPrefixStr = "Heap buffer read   " + constSizeStr<matchBufSize> + " / ";
		testPayload(batch, matchBufSize,
			wrStackPrefixStr + constSizeStr<tinyBufSize>, write_file<tinyBufSize, 0>,
			rdStackPrefixStr + constSizeStr<tinyBufSize>, read_file<tinyBufSize, 0> );
		testPayload(batch, matchBufSize,
			wrStackPrefixStr + constSizeStr<smallBufSize>, write_file<smallBufSize, 0>,
			rdStackPrefixStr + constSizeStr<smallBufSize>, read_file<smallBufSize, 0> );
		testPayload(batch, matchBufSize,
			wrStackPrefixStr + constSizeStr<matchBufSize>, write_file<matchBufSize, 0>,
			rdStackPrefixStr + constSizeStr<matchBufSize>, read_file<matchBufSize, 0> );
		testPayload(batch, matchBufSize,
			wrStackPrefixStr + constSizeStr<bigBufSize>, write_file<bigBufSize, 0>,
			rdStackPrefixStr + constSizeStr<bigBufSize>, read_file<bigBufSize, 0> );
		testPayload(batch, matchBufSize,
			wrHeapPrefixStr + constSizeStr<tinyBufSize>, write_file<0, tinyBufSize>,
			rdHeapPrefixStr + constSizeStr<tinyBufSize>, read_file<0, tinyBufSize> );
		testPayload(batch, matchBufSize,
			wrHeapPrefixStr + constSizeStr<smallBufSize>, write_file<0, smallBufSize>,
			rdHeapPrefixStr + constSizeStr<smallBufSize>, read_file<0, smallBufSize> );
		testPayload(batch, matchBufSize,
			wrHeapPrefixStr + constSizeStr<matchBufSize>, write_file<0, matchBufSize>,
			rdHeapPrefixStr + constSizeStr<matchBufSize>, read_file<0, matchBufSize> );
		testPayload(batch, matchBufSize,
			wrHeapPrefixStr + constSizeStr<bigBufSize>, write_file<0, bigBufSize>,
			rdHeapPrefixStr + constSizeStr<bigBufSize>, read_file<0, bigBufSize> );
	};

}



int main(int, char**) {
	auto batch = utest::TestBatch(std::cout);

	testPayloads<220, 2000, 2048, 2500>(batch);
	testPayloads<220, 2048, 2500, 2800>(batch);

	return batch.failures() == 0? EXIT_SUCCESS : EXIT_FAILURE;
}
