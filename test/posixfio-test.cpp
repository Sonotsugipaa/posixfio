#include <test_tools.hpp>

#include "posixfio.hpp"

#include <array>
#include <iostream>
#include <string>
#include <random>
#include <cstring>
#include <cassert>



#pragma message "TODO: test errors, with POSIXFIO_NOTHROW and without"



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
			default: return "unknown_errno";
		}
		#undef ERRNO_CASE1_
		#undef ERRNO_CASE2_
	}


	std::string mkPayload() {
		static constexpr size_t payloadSize = 8192;
		std::string r;  r.reserve(payloadSize);
		auto rng = std::minstd_rand(payloadSize);
		for(size_t i=0; i < payloadSize; ++i) {
			r.push_back(char(rng()));
		}
		return r;
	}


	utest::ResultType create_file(std::ostream& out) {
		File f;
		try {
			f = File::open(tmpFile.c_str(), O_WRONLY | O_CREAT | O_TRUNC);
		} catch(...) { }
		if(! f) {
			out << "ERRNO " << errno << ' ' << errno_str(errno) << '\n';
			return eFailure;
		}
		return eSuccess;
	}


	utest::ResultType write_file(std::ostream& out) {
		File f;
		try {
			f = File::open(tmpFile.c_str(), O_WRONLY);
			ssize_t wr = f.write(ioPayload.data(), ioPayload.size());
			if(wr < 0) throw 0;
			if(wr != (ssize_t) ioPayload.size()) {
				out << "Incomplete write: " << wr << " of " << ioPayload.size() << " bytes" << std::endl;
				return eFailure;
			}
		} catch(...) {
			f = File();
		}
		if(! f) {
			out << "ERRNO " << errno << ' ' << errno_str(errno) << '\n';
			return eFailure;
		}
		return eSuccess;
	}


	utest::ResultType write_fileview(std::ostream& out) {
		File f;
		FileView fv;
		try {
			f = File::open(tmpFile.c_str(), O_WRONLY);
			fv = f.fd();
			ssize_t wr = fv.write(ioPayload.data(), ioPayload.size());
			if(wr < 0) throw 0;
			if(wr != (ssize_t) ioPayload.size()) {
				out << "Incomplete write: " << wr << " of " << ioPayload.size() << " bytes" << std::endl;
				return eFailure;
			}
		} catch(...) {
			f = File();
		}
		if(! fv) {
			out << "ERRNO (FileView) " << errno << ' ' << errno_str(errno) << '\n';
			return eFailure;
		}
		if(! f) {
			out << "ERRNO (File) " << errno << ' ' << errno_str(errno) << '\n';
			return eFailure;
		}
		return eSuccess;
	}


	utest::ResultType read_file(std::ostream& out) {
		File f;
		try {
			f = File::open(tmpFile.c_str(), O_RDONLY);
			IO_PAYLOAD_BUFFER_(buf)
			ssize_t rd = f.read(buf.data(), buf.size()-1);
			if(rd < 0) throw 0;
			if(rd != ssize_t(buf.size()-1)) {
				out << "Incomplete read: " << rd << " of " << (buf.size()-1) << " bytes" << std::endl;
				return eFailure;
			}
			if(0 != strncmp(buf.data(), ioPayload.data(), buf.size()-1)) {
				out << "Payload mismatch: " << buf.data() << " instead of " << ioPayload.data() << " bytes" << std::endl;
				return eFailure;
			}
		} catch(...) {
			f = File();
		}
		if(! f) {
			out << "ERRNO " << errno << ' ' << errno_str(errno) << '\n';
			return eFailure;
		}
		return eSuccess;
	}


	utest::ResultType read_fileview(std::ostream& out) {
		File f;
		FileView fv;
		try {
			f = File::open(tmpFile.c_str(), O_RDONLY);
			fv = f.fd();
			IO_PAYLOAD_BUFFER_(buf)
			ssize_t rd = fv.read(buf.data(), buf.size()-1);
			if(rd < 0) throw 0;
			if(rd != ssize_t(buf.size()-1)) {
				out << "Incomplete read: " << rd << " of " << (buf.size()-1) << " bytes" << std::endl;
				return eFailure;
			}
			if(0 != strncmp(buf.data(), ioPayload.data(), buf.size()-1)) {
				out << "Payload mismatch: " << buf.data() << " instead of " << ioPayload.data() << " bytes" << std::endl;
				return eFailure;
			}
		} catch(...) {
			f = File();
		}
		if(! fv) {
			out << "ERRNO (FileView) " << errno << ' ' << errno_str(errno) << '\n';
			return eFailure;
		}
		if(! f) {
			out << "ERRNO (File) " << errno << ' ' << errno_str(errno) << '\n';
			return eFailure;
		}
		return eSuccess;
	}


	utest::ResultType close_file(std::ostream& out) {
		bool cl = false;
		try {
			File f = File::open(tmpFile.c_str(), O_WRONLY);
			cl = f.close();
			if(! cl) throw 0;
			if(f || (!(!f))) {
				out << "Closed file remains not null" << std::endl;
			}
		} catch(...) {
			out << "ERRNO " << errno << ' ' << errno_str(errno) << '\n';
			return eFailure;
		}
		return eSuccess;
	}


	utest::ResultType copy_file(std::ostream& out) {
		File f0, f1;
		try {
			f0 = File::open(tmpFile.c_str(), O_RDWR);
			f1 = f0;
			IO_PAYLOAD_BUFFER_(buf)
			ssize_t beg0 = 0;
			ssize_t end0 = ioPayload.length() / 2;
			ssize_t beg1 = end0+1;
			ssize_t end1 = ioPayload.length();
			ssize_t retval;
			errno = 0;
			bool result = true;
			#define TRY_(OP_) if(result) { result = (OP_); }
				TRY_(beg0 == (retval = f0.lseek(beg0, SEEK_SET)));
				TRY_((end0-beg0) == (retval = f0.write(ioPayload.data(), end0-beg0)));
				TRY_(beg1 == (retval = f1.lseek(beg1, SEEK_SET)));
				TRY_((end1-beg1) == (retval = f1.write(ioPayload.data()+beg1, end1-beg1)));
				TRY_(0 == (retval = f0.lseek(0, SEEK_SET)));
				TRY_((end1-beg0) == (retval = f0.read(buf.data(), end1-beg0)));
				TRY_(0 == strncmp(buf.data(), ioPayload.data(), ioPayload.length()));
			#undef TRY_
			if(! result) {
				out << "Faulty IO operation: retval = " << retval << '\n';
				throw 0;
			}
		} catch(...) {
			f0 = File();
		}
		if(! (f0 && f1)) {
			out << "ERRNO " << errno << ' ' << errno_str(errno) << '\n';
			return eFailure;
		}
		return eSuccess;
	}


	utest::ResultType copy_fileview(std::ostream& out) {
		File f0, f1;
		FileView fw;
		try {
			f0 = File::open(tmpFile.c_str(), O_RDWR);
			f1 = f0;
			IO_PAYLOAD_BUFFER_(buf)
			ssize_t beg0 = 0;
			ssize_t end0 = ioPayload.length() / 2;
			ssize_t beg1 = end0+1;
			ssize_t end1 = ioPayload.length();
			ssize_t retval;
			errno = 0;
			bool result = true;
			unsigned step = 0;
			#define TRY_(OP_) if(result) { result = (OP_); ++ step; }
				fw = FileView(f0);
				TRY_(beg0 == (retval = fw.lseek(beg0, SEEK_SET)));
				TRY_((end0-beg0) == (retval = fw.write(ioPayload.data(), end0-beg0)));
				fw = FileView(f1);
				TRY_(beg1 == (retval = fw.lseek(beg1, SEEK_SET)));
				TRY_((end1-beg1) == (retval = fw.write(ioPayload.data()+beg1, end1-beg1)));
				fw = FileView(f0);
				TRY_(0 == (retval = fw.lseek(0, SEEK_SET)));
				TRY_((end1-beg0) == (retval = fw.read(buf.data(), end1-beg0)));
				TRY_(0 == strncmp(buf.data(), ioPayload.data(), ioPayload.length()));
			#undef TRY_
			if(! result) {
				out << "Faulty IO operation: step = " << step << " retval = " << retval << '\n';
				throw 0;
			}
		} catch(...) {
			f0 = File();
		}
		if(! (f0 && f1)) {
			out << "ERRNO " << errno << ' ' << errno_str(errno) << '\n';
			return eFailure;
		}
		return eSuccess;
	}

}



int main(int, char**) {
	auto batch = utest::TestBatch(std::cout);
	batch
		.run("Create file", create_file);
	ioPayload = mkPayload();
	batch
		.run("Write file", write_file)
		.run("Read file", read_file);
	ioPayload = mkPayload();
	batch
		.run("Write file view", write_fileview)
		.run("Read file view", read_fileview);
	batch
		.run("Close file", close_file)
		.run("Copy-construct file", copy_file)
		.run("Copy-construct file view", copy_fileview);
	return batch.failures() == 0? EXIT_SUCCESS : EXIT_FAILURE;
}
