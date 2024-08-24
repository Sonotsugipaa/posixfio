#pragma once

#include <vector>
#include <string>
#include <string_view>
#include <functional>
#include <atomic>
#include <mutex>
#include <ostream>
#include <sstream>



namespace utest {

	enum class ResultType {
		eSuccess, eFailure, eNeutral
	};

	template<ResultType result> std::string colorResult(const std::string&);

	template<> std::string colorResult<ResultType::eSuccess>(const std::string&);
	template<> std::string colorResult<ResultType::eFailure>(const std::string&);
	template<> std::string colorResult<ResultType::eNeutral>(const std::string&);


	class TestBatch {
	private:
		std::vector<std::string> _successes;
		std::vector<std::string> _failures;
		std::vector<std::string> _neutrals;
		std::ostream& _os;
		std::mutex _mtx_vec_access;
		std::atomic_uint _longest;

	public:
		TestBatch(std::ostream& os);

		~TestBatch();


		unsigned failures() const;


		TestBatch& run(
				const std::string& name,
				std::function<ResultType (std::ostream&)> fn
		) {
			std::stringstream ss;
			ResultType result = fn(ss);
			{
				auto lock = std::lock_guard(_mtx_vec_access);
				std::string coloredName = "[[ " + name + " ]]\n";
				if(name.size() > _longest) {
					_longest = name.size(); }
				switch(result) {
					case ResultType::eFailure:
						coloredName = colorResult<ResultType::eFailure>(coloredName);
						_failures.push_back(name);  break;
					case ResultType::eNeutral:
						coloredName = colorResult<ResultType::eNeutral>(coloredName);
						_neutrals.push_back(name);  break;
					case ResultType::eSuccess:
						coloredName = colorResult<ResultType::eSuccess>(coloredName);
						_successes.push_back(name);  break;
				}
				if(ss.peek() != std::stringstream::traits_type::eof()) {
					_os << coloredName << ss.rdbuf() << std::endl;
				}
			}
			return *this;
		}
	};

}
