#include <test_tools.hpp>

#include "../include/unix/posixfio_tl.hpp"

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

	const std::string tmpFile = "test-tmpfile";

	std::string ioPayload;


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
			assert(result >= 0);
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
			case 0: return "none";
			default: return "unknown_errno";
		}
		#undef ERRNO_CASE1_
		#undef ERRNO_CASE2_
	}

	#define CATCH_ERRNO_(OS_) catch(Errno& errNo) { OS_ << "ERRNO " << errno_str(errNo.errcode) << ' ' << errNo.errcode << std::endl; }


	utest::ResultType requireErrno(std::ostream& out, int expect, int (*fn)(std::ostream& out)) {
		int got;
		try {
			got = fn(out);
		} catch(Errno& errNo) {
			got = errNo.errcode;
		}
		if(got != expect) {
			out << "Expected errno " << errno_str(expect) << ", got " << errno_str(got) << std::endl;
			return eFailure;
		} else {
			return eSuccess;
		}
	}

	utest::ResultType requireFileError(std::ostream& out, int expect, int (*fn)(std::ostream& out)) {
		int got;
		try {
			got = fn(out);
		} catch(FileError& err) {
			got = err.errcode;
		}
		if(got != expect) {
			out << "Expected file error " << errno_str(expect) << ", got " << errno_str(got) << std::endl;
			return eFailure;
		} else {
			return eSuccess;
		}
	}


	std::string mkPayload(size_t payloadSize) {
		static const std::string_view charset = "abcdefghi1234567890\n ";
		static decltype(payloadSize) state = 2;
		std::string r;  r.reserve(payloadSize);
		auto rng = std::minstd_rand(state += payloadSize);
		for(size_t i=0; i < payloadSize; ++i) {
			r.push_back(charset[rng() % charset.size()]);
		}
		return r;
	}


	ssize_t diff(std::string_view s0, std::string_view s1) {
		size_t minSize = std::min(s0.size(), s1.size());
		for(size_t i=0; i < minSize; ++i) {
			if(s0[i] != s1[i]) [[unlikely]] return i;
		}
		return -1;
	}


	template<size_t inputBufferStaticCapacity>
	struct InputBuffer {
		using type = ArrayInputBuffer<inputBufferStaticCapacity>;
		static type ctor(FileView file, size_t) { return type(file); }
	};

	template<>
	struct InputBuffer<0> {
		using type = posixfio::InputBuffer;
		static type ctor(FileView file, size_t varCap) { return type(file, varCap); }
	};


	template<size_t outputBufferStaticCapacity>
	struct OutputBuffer {
		using type = ArrayOutputBuffer<outputBufferStaticCapacity>;
		static type ctor(FileView file, size_t) { return type(file); }
	};

	template<>
	struct OutputBuffer<0> {
		using type = posixfio::OutputBuffer;
		static type ctor(FileView file, size_t varCap) { return type(file, varCap); }
	};


	template<size_t outputBufferStaticCapacity, size_t outputBufferDynamicCapacity>
	utest::ResultType write_file(std::ostream& out) {
		static_assert((outputBufferStaticCapacity == 0) != (outputBufferDynamicCapacity == 0));
		using Buffer = OutputBuffer<outputBufferStaticCapacity>::type;
		try {
			File f = alwaysThrowErr(File::open(tmpFile.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600));
			{
				Buffer buf = OutputBuffer<outputBufferStaticCapacity>::ctor(
					f, outputBufferDynamicCapacity );
				ssize_t wr;
				ssize_t count = ioPayload.size();
				size_t cursor = 0;
				#define WR_ { wr = alwaysThrowErr(buf.write(ioPayload.data() + cursor, count));  assert(wr > 0);  count -= wr;  cursor += wr; }
				WR_
				if(count > 0) {
					out << "Partial write of " << wr << '/' << ioPayload.size() << " bytes" << std::endl;
				}
				while(count > 0) {
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
			auto diffPt = diff(ioPayload, std::string_view(cmpString.get(), cmpString.get() + ioPayload.size()));
			if(0 <= diffPt) {
				out << "File content does not match at char " << diffPt << std::endl;
				return eFailure;
			}
			return eSuccess;
		} CATCH_ERRNO_(out)
		return eFailure;
	}


	template<size_t outputBufferStaticCapacity, size_t outputBufferDynamicCapacity>
	utest::ResultType write_file_inconsistent(std::ostream& out) {
		static_assert((outputBufferStaticCapacity == 0) != (outputBufferDynamicCapacity == 0));
		using Buffer = OutputBuffer<outputBufferStaticCapacity>::type;
		try {
			File f = alwaysThrowErr(File::open(tmpFile.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600));
			{
				Buffer buf = OutputBuffer<outputBufferStaticCapacity>::ctor(
					f, outputBufferDynamicCapacity );
				ssize_t wr;
				ssize_t count = ioPayload.size();
				size_t bigWriteSize = (count - 5*4) / 2;
				size_t cursor = 0;
				#define CHECK_PARTIAL_WR_ { if(wr != ssize_t(n)) { out << "Unexpected partial write of " << wr << '/' << n << " bytes" << std::endl; return eFailure; } }
				#define WR_(N_) { const auto n = N_; wr = alwaysThrowErr(buf.writeAll(ioPayload.data() + cursor, n));  assert(wr > 0);  count -= wr;  cursor += wr; CHECK_PARTIAL_WR_ }
				assert(ioPayload.size() > 5*4);
				WR_(4) WR_(4)
				WR_(bigWriteSize)
				WR_(4)
				WR_(count - 8)
				WR_(4) WR_(4)
				#undef WR_
				#undef CHECK_PARTIAL_RD_
				assert(count == 0);
			}
			return eSuccess;
		} CATCH_ERRNO_(out)
		return eFailure;
	}


	template<size_t inputBufferStaticCapacity, size_t inputBufferDynamicCapacity>
	utest::ResultType read_file_inconsistent(std::ostream& out) {
		static_assert((inputBufferStaticCapacity == 0) != (inputBufferDynamicCapacity == 0));
		using Buffer = InputBuffer<inputBufferStaticCapacity>::type;
		try {
			File f = alwaysThrowErr(File::open(tmpFile.c_str(), O_RDONLY));
			Buffer buf = InputBuffer<inputBufferStaticCapacity>::ctor(
				f, inputBufferDynamicCapacity );
			std::unique_ptr<char[]> cmpString = std::make_unique<char[]>(ioPayload.size());
			ssize_t rd;
			ssize_t count = ioPayload.size();
			size_t bigReadSize = (count - 5*4) / 2;
			size_t cursor = 0;
			#define CHECK_PARTIAL_RD_ { if(rd != ssize_t(n)) { out << "Unexpected partial read of " << rd << '/' << n << " bytes" << std::endl; return eFailure; } }
			#define RD_(N_) { const auto n = N_; rd = alwaysThrowErr(buf.readAll(cmpString.get() + cursor, n));  count -= rd;  cursor += rd; CHECK_PARTIAL_RD_ }
			assert(ioPayload.size() > 5*4);
			RD_(4) RD_(4)
			RD_(bigReadSize)
			RD_(4)
			RD_(count - 8)
			RD_(4) RD_(4)
			#undef RD_
			#undef CHECK_PARTIAL_RD_
			if(count > 0) {
				out << "Unexpected EOF, " << count << " bytes missing" << std::endl;
				return eFailure;
			}
			auto diffPt = diff(ioPayload, std::string_view(cmpString.get(), cmpString.get() + ioPayload.size()));
			if(0 <= diffPt) {
				out << "File content does not match at char " << diffPt << std::endl;
				return eFailure;
			}
			return eSuccess;
		} CATCH_ERRNO_(out)
		return eFailure;
	}


	template<size_t inputBufferStaticCapacity, size_t inputBufferDynamicCapacity>
	utest::ResultType read_buffer(std::ostream& out) {
		static_assert((inputBufferStaticCapacity == 0) != (inputBufferDynamicCapacity == 0));
		using Buffer = InputBuffer<inputBufferStaticCapacity>::type;
		try {
			File f = alwaysThrowErr(File::open(tmpFile.c_str(), O_RDONLY));
			Buffer buf = InputBuffer<inputBufferStaticCapacity>::ctor(
				f, inputBufferDynamicCapacity );
			std::string cmpString;  cmpString.reserve(ioPayload.size());
			ssize_t rd;
			while(1 == (rd = alwaysThrowErr(buf.fwd()))) {
				assert(buf.size() > 0);
				assert(buf.data() != nullptr);
				cmpString.push_back(*buf.data());
			}
			assert(rd < 1);
			if(cmpString.size() != ioPayload.size()) {
				out << "Size mismatch: expected " << ioPayload.size() << ", got " << cmpString.size() << std::endl;
				return eFailure;
			}
			auto diffPt = diff(ioPayload, cmpString);
			if(0 <= diffPt) {
				out << "File content does not match at char " << diffPt << std::endl;
				return eFailure;
			}
			return eSuccess;
		} CATCH_ERRNO_(out)
		return eFailure;
	}


	utest::ResultType fileerror_file_ebadf(std::ostream& out) {
		return requireFileError(out, EBADF, [](std::ostream& out) {
			auto f = File::open(tmpFile.c_str(), O_RDONLY | O_CREAT);
			ssize_t wr = f.write(tmpFile.c_str(), 1); // Can't write to a RDONLY file
			switch(wr) {
				case 0:  out << "CRITICAL: write(..., 1) returned 0" << std::endl;  return -1;
				case 1:  return 0;
				default:  {
					int curErrno = errno;
					errno = 0;
					return curErrno;
				}
			}
		});
	}

	utest::ResultType fileerror_buffer_ebadf(std::ostream& out) {
		return requireFileError(out, EBADF, [](std::ostream& out) {
			auto f = File::open(tmpFile.c_str(), O_WRONLY | O_CREAT | O_TRUNC);
			auto fb = posixfio::InputBuffer(f, 1);
			ssize_t rd = fb.fwd(); // Can't read from a WRONLY file
			switch(rd) {
				case 0:  out << "CRITICAL: fwd() returned 0" << std::endl;  return -1;
				case 1:  return 0;
				default:  {
					int curErrno = errno;
					errno = 0;
					return curErrno;
				}
			}
		});
	}

	utest::ResultType errno_buffer_ebadf(std::ostream& out) {
		return requireErrno(out, EBADF, [](std::ostream& out) {
			auto f = File::open(tmpFile.c_str(), O_WRONLY | O_CREAT | O_TRUNC);
			auto fb = posixfio::InputBuffer(f, 1);
			ssize_t rd = fb.fwd(); // Can't read from a WRONLY file
			switch(rd) {
				case 0:  out << "CRITICAL: fwd() returned 0" << std::endl;  return -1;
				case 1:  return 0;
				default:  {
					int curErrno = errno;
					errno = 0;
					return curErrno;
				}
			}
		});
	}


	void testPayload(
			utest::TestBatch& batch, size_t payloadSize,
			const std::string& wrString, utest::ResultType (*wrFn)(std::ostream&),
			const std::string& rdString, utest::ResultType (*rdFn)(std::ostream&),
			const std::string& bfString, utest::ResultType (*bfFn)(std::ostream&),
			const std::string& wriString, utest::ResultType (*wriFn)(std::ostream&),
			const std::string& rdiString, utest::ResultType (*rdiFn)(std::ostream&)
	) {
		ioPayload = mkPayload(payloadSize);
		batch.run(wrString, wrFn);
		batch.run(rdString, rdFn);
		batch.run(bfString, bfFn);
		batch.run(wriString, wriFn);
		batch.run(rdiString, rdiFn);
	};


	template<size_t n> const std::string constSizeStr = std::to_string(n);

	template<size_t tinyBufSize, size_t smallBufSize, size_t matchBufSize, size_t bigBufSize>
	void testPayloads(
			utest::TestBatch& batch
	) {
		static const std::string wrStackPrefixStr  = "Stack buffer write         " + constSizeStr<matchBufSize> + " / ";
		static const std::string rdStackPrefixStr  = "Stack buffer read          " + constSizeStr<matchBufSize> + " / ";
		static const std::string bfStackPrefixStr  = "Stack buffer read (raw)    " + constSizeStr<matchBufSize> + " / ";
		static const std::string wriStackPrefixStr = "Stack buffer varying write " + constSizeStr<matchBufSize> + " / ";
		static const std::string rdiStackPrefixStr = "Stack buffer varying read  " + constSizeStr<matchBufSize> + " / ";
		static const std::string wrHeapPrefixStr  = "Heap buffer write         " + constSizeStr<matchBufSize> + " / ";
		static const std::string rdHeapPrefixStr  = "Heap buffer read          " + constSizeStr<matchBufSize> + " / ";
		static const std::string bfHeapPrefixStr  = "Heap buffer read (raw)    " + constSizeStr<matchBufSize> + " / ";
		static const std::string wriHeapPrefixStr = "Heap buffer varying write " + constSizeStr<matchBufSize> + " / ";
		static const std::string rdiHeapPrefixStr = "Heap buffer varying read  " + constSizeStr<matchBufSize> + " / ";
		#define TEST_(MEM_, SIZE0_, SIZE1_) { \
			testPayload(batch, matchBufSize, \
				wr  ## MEM_ ## PrefixStr + constSizeStr<SIZE0_ | SIZE1_>, write_file<SIZE0_, SIZE1_>, \
				rd  ## MEM_ ## PrefixStr + constSizeStr<SIZE0_ | SIZE1_>, read_file<SIZE0_, SIZE1_>, \
				bf  ## MEM_ ## PrefixStr + constSizeStr<SIZE0_ | SIZE1_>, read_buffer<SIZE0_, SIZE1_>, \
				wri ## MEM_ ## PrefixStr + constSizeStr<SIZE0_ | SIZE1_>, write_file_inconsistent<SIZE0_, SIZE1_>, \
				rdi ## MEM_ ## PrefixStr + constSizeStr<SIZE0_ | SIZE1_>, read_file_inconsistent<SIZE0_, SIZE1_> ); \
		}
		TEST_(Stack, tinyBufSize,  0)
		TEST_(Stack, smallBufSize, 0)
		TEST_(Stack, matchBufSize, 0)
		TEST_(Stack, bigBufSize,   0)
		TEST_(Heap,  0,  tinyBufSize)
		TEST_(Heap,  0, smallBufSize)
		TEST_(Heap,  0, matchBufSize)
		TEST_(Heap,  0,   bigBufSize)
		#undef TEST_
	};

}



int main(int, char**) {
	auto batch = utest::TestBatch(std::cout);
	testPayloads<220, 2000, 2048, 2500>(batch);
	batch.run("Write read-only file   (EBADF)", fileerror_file_ebadf);
	batch.run("Read write-only buffer (EBADF)", fileerror_buffer_ebadf);
	#ifndef POSIXFIO_NOTHROW
		batch.run("Read write-only buffer (EBADF, legacy)", errno_buffer_ebadf);
	#endif
	return batch.failures() == 0? EXIT_SUCCESS : EXIT_FAILURE;
}
