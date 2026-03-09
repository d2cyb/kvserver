module;

#include <atomic>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <shared_mutex>
#include <stop_token>
#include <string>
#include <syncstream>
#include <thread>

export module kvserver.statistic;

using std::string;

namespace kvserver {

const uint32_t DEFAULT_OUTPUT_INTERVAL_SECONDS = 5 * 1000;

export class Statistic {
private:
	uint32_t intervalMilliseconds;
	double intervalSeconds;
	std::atomic<uint32_t> totalCommands { 0 };
	std::atomic<uint32_t> recentCommands { 0 };
	std::jthread outputThread;
	mutable std::shared_timed_mutex recentCommandsMutex;

public:
	explicit Statistic(uint32_t printIntervalSeconds)
		: intervalMilliseconds(printIntervalSeconds)
		, intervalSeconds(
			  std::ceil(static_cast<double>(intervalMilliseconds) / 1000.0 * 10.0) / 10.0
		  )
	{
		outputThread = std::jthread(&Statistic::periodicOutputWorker, this, intervalMilliseconds);
	}
	explicit Statistic()
		: Statistic(DEFAULT_OUTPUT_INTERVAL_SECONDS)
	{
	}
	Statistic(const Statistic &)					  = delete;
	Statistic(const Statistic &&)					  = delete;
	auto operator=(const Statistic &) -> Statistic &  = delete;
	auto operator=(const Statistic &&) -> Statistic & = delete;
	~Statistic() { }

	void recordCommand()
	{
		totalCommands++;

		std::lock_guard<std::shared_timed_mutex> recentCommandWriterLock(recentCommandsMutex);
		recentCommands++;
	}

	[[nodiscard]] auto getTotalCommands() const -> uint32_t
	{
		return totalCommands.load();
	}

	[[nodiscard]] auto getRecentCommands() const -> uint32_t
	{
		std::shared_lock<std::shared_timed_mutex> recentCommandReaderLock(recentCommandsMutex);
		return recentCommands.load();
	}

private:
	void periodicOutputWorker(std::stop_token stoken, int intervalMilliseconds)
	{
		while (true) {
			std::this_thread::sleep_for(std::chrono::milliseconds(intervalMilliseconds));
			if (stoken.stop_requested()) {
				break;
			}

			auto now	   = std::chrono::system_clock::now();
			auto timePoint = std::chrono::system_clock::to_time_t(now);
			{
				std::lock_guard<std::shared_timed_mutex> recentCommandWriterLock(
					recentCommandsMutex
				);

				std::osyncstream(std::cout)
					<< "\n=== Statistic at "
					<< std::put_time(std::localtime(&timePoint), "%H:%M:%S") << " ===\n"
					<< "Total commands : " << totalCommands.load() << "\n"
					<< "Commands in last " << intervalSeconds
					<< " seconds: " << recentCommands.load() << '\n';

				recentCommands = 0;
			}
		}
	}
};

} // namespace kvserver
