#include "test_tools.hpp"

#include <iostream>



namespace {

	std::string stretch_str(const std::string& str, unsigned n) {
		n = std::max(std::string::size_type(0), n - str.size());
		return str + ' ' + std::string(n, '.');
	}


	std::string strSuccess(const std::string& name, unsigned pad) {
		using namespace utest;
		return stretch_str(name, pad) + "... " +
			colorResult<ResultType::eSuccess>("Success");
	}

	std::string strFailure(const std::string& name, unsigned pad) {
		using namespace utest;
		return stretch_str(name, pad) + "... " +
			colorResult<ResultType::eFailure>("Failure");
	}

	std::string strNeutral(const std::string& name, unsigned pad) {
		using namespace utest;
		return stretch_str(name, pad) + "... " +
			colorResult<ResultType::eNeutral>("Review output");
	}

}



namespace utest {


	template<>
	std::string colorResult<ResultType::eSuccess>(const std::string& str) {
		return "\033[1;34m" + str + "\033[m";
	}

	template<>
	std::string colorResult<ResultType::eFailure>(const std::string& str) {
		return "\033[1;31m" + str + "\033[m";
	}

	template<>
	std::string colorResult<ResultType::eNeutral>(const std::string& str) {
		return "\033[1;36m" + str + "\033[m";
	}



	TestBatch::TestBatch(std::ostream& os):
			_os(os),
			_longest(0)
	{ }


	TestBatch::~TestBatch() {
		auto lock = std::lock_guard(_mtx_vec_access);
		for(const std::string& success : _successes) {
			std::cout << strSuccess(success, _longest) << '\n'; }
		for(const std::string& failure : _failures) {
			std::cout << strFailure(failure, _longest) << '\n'; }
		for(const std::string& neutral : _neutrals) {
			std::cout << strNeutral(neutral, _longest) << '\n'; }
	}


	unsigned TestBatch::failures() const {
		return _failures.size();
	}

}
