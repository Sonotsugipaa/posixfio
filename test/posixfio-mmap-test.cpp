#include "test_tools.hpp"

#if defined POSIXFIO_UNIX
	#include "../include/unix/posixfio.hpp"
#elif defined POSIXFIO_WIN32
	#include "../include/win32/posixfio.hpp"
#endif

#include <array>
#include <iostream>
#include <string>
#include <random>
#include <cstring>
#include <cassert>



namespace {

	using namespace posixfio;

	constexpr auto eFailure = utest::ResultType::eFailure;
	constexpr auto eNeutral = utest::ResultType::eNeutral;
	constexpr auto eSuccess = utest::ResultType::eSuccess;

	const std::string tmpFile = "tmpfile";

	std::string ioPayload;

	#define IO_PAYLOAD_BUFFER_(NAME_) std::string NAME_; NAME_.resize(ioPayload.size());


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


	std::string mkPayload() {
		static constexpr size_t payloadSize = 8192;
		static size_t state = 3;
		std::string r;  r.reserve(payloadSize);
		auto rng = std::minstd_rand(state = (payloadSize ^ state));
		for(size_t i=0; i < payloadSize; ++i) {
			r.push_back(char(rng()));
		}
		return r;
	}


	utest::ResultType write_file(std::ostream& out) {
		File f;
		try {
			f = File::open(tmpFile.c_str(), O_RDWR | O_CREAT | O_TRUNC, 0600);
			if(f) {
				f.ftruncate(ioPayload.size());
				MemMapping map = f.mmap(ioPayload.size(), MemProtFlags::eWrite, MemMapFlags::eShared);
				auto addr = map.get<char>();
				auto len = map.size();
				if(addr == nullptr) {
					out << "Null address returned by `mmap`\n";
					return eFailure;
				}
				if(len != ioPayload.size()) {
					out << "Mapping size mismatch by `mmap`\n";
					return eFailure;
				}
				memcpy(addr, ioPayload.data(), ioPayload.size());
			}
		} catch(...) { f = { }; }
		if(! f) {
			out << "ERRNO " << errno << ' ' << errno_str(errno) << '\n';
			return eFailure;
		}
		return eSuccess;
	};


	utest::ResultType read_file(std::ostream& out) {
		File f;
		try {
			f = File::open(tmpFile.c_str(), O_RDONLY);
			if(f) {
				MemMapping map = f.mmap(ioPayload.size(), MemProtFlags::eRead, MemMapFlags::eShared);
				auto addr = map.get<char>();
				auto len = map.size();
				if(addr == nullptr) {
					out << "Null address returned by `mmap`\n";
					return eFailure;
				}
				if(len != ioPayload.size()) {
					out << "Mapping size mismatch by `mmap`\n";
					return eFailure;
				}
				if(0 != memcmp(addr, ioPayload.data(), ioPayload.size())) {
					out << "File != IO payload\n";
					return eFailure;
				}
			}
		} catch(...) { f = { }; }
		if(! f) {
			out << "ERRNO " << errno << ' ' << errno_str(errno) << '\n';
			return eFailure;
		}
		return eSuccess;
	};

}



int main(int, char**) {
	auto batch = utest::TestBatch(std::cout);
	ioPayload = mkPayload();
	batch
		.run("Write mapped file", write_file)
		.run("Read mapped file", read_file);
	ioPayload = mkPayload();
	return batch.failures() == 0? EXIT_SUCCESS : EXIT_FAILURE;
}
