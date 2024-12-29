#include <test_tools.hpp>

#define POSIXFIO_STL_STRINGVIEW
#if defined POSIXFIO_UNIX
	#include "../include/unix/posixfio_tl.hpp"
#elif defined POSIXFIO_WIN32
	#include "../include/win32/posixfio_tl.hpp"
#endif

#include <array>
#include <iostream>
#include <string>
#include <random>
#include <cstring>
#include <cassert>
#include <memory>
#include <concepts>
#include <bit>
#include <limits>
#include <chrono>
#include <sstream>



namespace {

	using namespace posixfio;

	constexpr auto eFailure = utest::ResultType::eFailure;
	constexpr auto eNeutral = utest::ResultType::eNeutral;

	constexpr auto eCreat = OpenFlags::eCreat;
	constexpr auto eRdwr  = OpenFlags::eRdwr;
	constexpr auto eTrunc = OpenFlags::eTrunc;


	template <std::integral Dst, std::integral T>
	constexpr Dst fastHash(T x) noexcept { return x; }

	template <std::integral Dst, std::integral T0, std::integral T1, std::integral... T>
	constexpr Dst fastHash(T0 x0, T1 x1, T... x) noexcept {
		using UnsignedDst = std::make_unsigned_t<Dst>;

		// Subtract a prime number proportionate to the max value,
		// so that the result is less likely to be a multiple of
		// an uncomfortable number (such as the size of an unordered_set)
		// ((this reasoning might not hold up to someone who knows math))
		constexpr auto zeroMinusLargePrime = UnsignedDst(0) - ([]() -> UnsignedDst {
			using D = UnsignedDst;
			constexpr D max = std::numeric_limits<D>::max();
			if constexpr (max <  (D(1) << D( 8))) return D(127);
			if constexpr (max <= (D(1) << D( 8))) return D(251);
			if constexpr (max <  (D(1) << D(16))) return D(32749);
			if constexpr (max <= (D(1) << D(16))) return D(65521);
			return D(0x0291'D8A1) /* Mersenne prime taken from Wikipedia, Python was about to awaken my OOMK and I couldn't be bothered to implement Erastothenes in C */;
		} ());

		auto r = Dst(
			zeroMinusLargePrime + (
				UnsignedDst(  std::rotl<UnsignedDst>(x0, 4)) ^
				UnsignedDst(~ std::rotr<UnsignedDst>(fastHash<Dst>(x1, x...), 7)) ) );
		return r;
	}


	template <typename T> concept TimeRepType = std::is_arithmetic_v<T>;

	template <typename T> concept TimePeriodType = requires {
		{ T::num } -> std::convertible_to<intmax_t>;
		{ T::den } -> std::convertible_to<intmax_t>; };

	template <typename T> concept TimeDurationType = requires {
		requires TimeRepType<typename T::rep>;
		requires TimePeriodType<typename T::period>; };


	template <typename clock_tp = std::chrono::steady_clock, TimeDurationType duration_tp = clock_tp::duration>
	requires std::chrono::is_clock_v<clock_tp>
	class Timer {
	public:
		using Clock = clock_tp;
		using Dur   = duration_tp;

		Timer(): t_begin(Clock::now()) { }

		template <TimeDurationType Dur = Timer::Dur>
		auto count() const noexcept { return std::chrono::duration_cast<Dur>(Clock::now() - t_begin).count(); }

		template <TimeRepType Rep, TimePeriodType Period = Dur::period>
		auto count() const noexcept { return count<std::chrono::duration<Rep, Period>>(); }

		template <TimePeriodType Period, TimeRepType Rep = Dur::rep>
		auto count() const noexcept { return count<std::chrono::duration<Rep, Period>>(); }

	private:
		Clock::time_point t_begin;
	};


	template <typename duration_tp = std::chrono::steady_clock::duration>
	requires requires(duration_tp t) { { std::chrono::duration_cast<std::chrono::seconds>(t) } -> std::same_as<std::chrono::seconds>; }
	using SteadyTimer = Timer<std::chrono::steady_clock, duration_tp>;


	template <typename T> concept TimerType = requires (T t, T* tp) {
		typename T::Clock;
		typename T::Dur;
		T();
		{ t.template count<int, std::milli>() } -> std::integral;
	};


	struct Env {
		static constexpr byte_t initialSeed = fastHash<byte_t>('s', 'e', 'e', 'd');
		static constexpr size_t fileSize = 128*1024*1024;
		File file;
		bool initialized = false;

		void init() {
			if(initialized) {
				file.ftruncate(0);
			} else {
				initialized = true;
			}
			try {
				file = File::open("perf-tmpfile.txt", OpenFlags::eRdwr | OpenFlags::eCreat | OpenFlags::eTrunc);
			} catch(Errcode& err) {
				initialized = false;
				file.close();
				std::stringstream msg;
				msg << "Failed to open temporary file: errno ";
				msg << err.errcode;
				msg << '\n';
				std::cerr << std::move(msg).str() << std::endl;
				std::rethrow_exception(std::current_exception());
			}
		}

		static byte_t genBlock(byte_t* dst, size_t byteCount, byte_t seed) {
			if(dst != nullptr) {
				for(size_t i = 0; i < byteCount; ++i) {
					dst[i] = seed;
					seed = fastHash<byte_t>(seed, initialSeed);
				}
			} else {
				for(size_t i = 0; i < byteCount; ++i) {
					seed = fastHash<byte_t>(seed, initialSeed);
				}
			}
			return seed;
		}

		static byte_t hashBlock(const byte_t* src, size_t byteCount) {
			return fastHash<byte_t>(src[byteCount-1], initialSeed);
		}

		void reset() {
			file.lseek(0, Whence::eSet);
			auto adv = posix_fadvise(file.fd(), 0, 0, MADV_DONTNEED);
			(void) adv;
			assert(adv == 0);
		}

		~Env() {
			if(initialized) file.ftruncate(0);
			file.close();
			initialized = false;
		}
	};
	static Env env;


	auto rwPerftest(size_t segmSize, size_t bufferSize) {
		std::pair<uintmax_t, uintmax_t> r;
		auto segm = std::make_unique_for_overwrite<byte_t[]>(segmSize);
		auto obuf = OutputBuffer(env.file, bufferSize);
		byte_t hash = Env::initialSeed;
		SteadyTimer timer;
		for(size_t i = 0; i < Env::fileSize; i += segmSize) {
			hash = Env::genBlock(segm.get(), segmSize, hash);
			obuf.writeAll(segm.get(), segmSize);
		}
		obuf = { };
		r.first = timer.count<uintmax_t, std::milli>();
		env.reset();
		auto ibuf = InputBuffer(env.file, bufferSize);
		hash = Env::initialSeed;
		timer = { };
		for(size_t i = 1; i < Env::fileSize; i += segmSize) {
			hash = Env::genBlock(nullptr, segmSize, hash);
			ibuf.readAll(segm.get(), segmSize);
			auto hashCmp = Env::hashBlock(segm.get(), segmSize); (void) hashCmp;
			assert(hash == hashCmp);
		}
		ibuf = { };
		r.second = timer.count<uintmax_t, std::milli>();
		env.reset();
		return r;
	}


	auto runRwPerftest(utest::TestBatch& batch, std::string nm, size_t segmSize, size_t bufferSize) {
		auto fn = [&](std::ostream& os) {
			try {
				auto timings = rwPerftest(segmSize, bufferSize);
				os
				<< (timings.second/1000) << '.' << (timings.second%1000) << "s,  w "
				<< (timings.first /1000) << '.' << (timings.first %1000) << 's' << std::endl;
				return eNeutral;
			} catch(Errcode& err) {
				os << "Errno " << err.errcode << std::endl;
				return eFailure;
			}
		};
		batch.run(std::move(nm), fn);
	}

}



int main(int, char**) {
	try {
		auto batch = utest::TestBatch(std::cout);
		std::tuple<std::string_view, size_t, size_t> paramList[] = {
			{ "4096 | 1024*1024", 4096, 1024*1024 },
			{ "   1 | 1024*1024",    1, 1024*1024 },
			{ "4096 |      4100", 4096,      4100 },
			{ "4096 |      4096", 4096,      4096 },
			{ "2300 |      4096", 2300,      4096 },
			{ "   1 |      4096",    1,      4096 },
			{ "4096 |         1", 4096,         1 } };
		env.init();
		for(auto& params : paramList) {
			runRwPerftest(batch, std::string(std::get<0>(params)), std::get<1>(params), std::get<2>(params));
		}
		return batch.failures() == 0? EXIT_SUCCESS : EXIT_FAILURE;
	} catch(Errcode& err) {
		std::stringstream msg;
		msg << "Unknown error: errno ";
		msg << err.errcode;
		msg << '\n';
		std::cerr << "A\n" << std::move(msg).str() << std::endl;
		return EXIT_FAILURE;
	}
}
