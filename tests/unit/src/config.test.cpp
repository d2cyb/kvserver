#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <thread>

#include "kvserver_lib_export.h"

import kvserver.config;

using kvserver::Config, kvserver::ConfigKeyStats;

TEST_CASE("Config basic operations", "[config]")
{
	const std::string testFilePath = "test_config.txt";

	SECTION("must create empty config file, if not exists")
	{
		REQUIRE(std::filesystem::exists(testFilePath) == false);

		Config config(testFilePath);

		REQUIRE(std::filesystem::exists(testFilePath));
		REQUIRE(config.get("nonexistent").has_value() == false);
	}

	SECTION("get must return values, if values set")
	{
		Config config(testFilePath);

		config.set("key1", "value1");
		config.set("key2", "value2");

		REQUIRE(config.get("key1") == "value1");
		REQUIRE(config.get("key2") == "value2");
		REQUIRE(config.get("key3").has_value() == false);
	}

	SECTION("must update value")
	{
		Config config(testFilePath);

		config.set("key1", "value1");
		REQUIRE(config.get("key1") == "value1");

		config.set("key1", "new_value");
		REQUIRE(config.get("key1") == "new_value");
	}

	SECTION("load data, after save in config class destructor")
	{
		{
			Config config(testFilePath);
			config.set("persistent1", "value1");
			config.set("persistent2", "value2");
		}
		REQUIRE(std::filesystem::exists(testFilePath));

		Config config(testFilePath);

		REQUIRE(config.get("persistent1") == "value1");
		REQUIRE(config.get("persistent2") == "value2");
	}

	SECTION("must empty data, if invalid json file")
	{
		std::ofstream file(testFilePath);
		file << "invalid json content";
		file.close();

		Config config(testFilePath);

		config.set("new_key", "new_value");
		REQUIRE(config.get("new_key") == "new_value");
	}

	std::filesystem::remove(testFilePath);
}

// TODO (move to different class)
TEST_CASE("read and write statistic", "[config][statistic]")
{
	const std::string testFilePath = "test_config_statistic.txt";

	SECTION("must empty statistics, for new config")
	{
		Config config(testFilePath);

		auto stats = config.getKeyStats("test_key");
		REQUIRE(stats.reads == 0);
		REQUIRE(stats.writes == 0);
	}

	SECTION("must empty statistics, for get not exists values")
	{
		Config config(testFilePath);

		config.get("test_key");
		auto stats = config.getKeyStats("test_key");

		REQUIRE(stats.reads == 0);
		REQUIRE(stats.writes == 0);
	}

	SECTION("must changed write statistic, if set value")
	{
		Config config(testFilePath);

		config.set("test_key", "test_value");
		auto stats = config.getKeyStats("test_key");

		REQUIRE(stats.reads == 0);
		REQUIRE(stats.writes == 1);
	}

	SECTION("must changed read statistic, if set and get value")
	{
		Config config(testFilePath);

		config.set("test_key", "test_value");
		config.get("test_key");
		auto stats = config.getKeyStats("test_key");

		REQUIRE(stats.reads == 1);
		REQUIRE(stats.writes == 1);
	}

	std::filesystem::remove(testFilePath);
}

TEST_CASE("Config thread safety", "[config][thread]")
{
	const std::string testFilePath = "test_config_thread.txt";

	Config config(testFilePath);

	SECTION("concurrent reads")
	{
		const int numThreads { 10 };
		const int readsPerThread { 100 };
		const int testDataCount { 100 };

		for (int i = 0; i < testDataCount; ++i) {
			config.set("key" + std::to_string(i), "value" + std::to_string(i));
		}

		std::vector<std::thread> threads {};
		threads.reserve(numThreads);

		std::atomic<int> successfulReads { 0 };

		for (int threadIdx = 0; threadIdx < numThreads; ++threadIdx) {
			threads.emplace_back([&config, &successfulReads, readsPerThread]() -> void {
				for (int readIdx = 0; readIdx < readsPerThread; ++readIdx) {
					std::string key { "key" + std::to_string(readIdx) };
					std::string expected { "value" + std::to_string(readIdx) };

					if (config.get(key).value_or("") == expected) {
						successfulReads++;
					}
				}
			});
		}

		for (auto &thread : threads) {
			thread.join();
		}

		REQUIRE(successfulReads == numThreads * readsPerThread);
	}

	SECTION("concurrent reads and writes")
	{
		const int numThreads { 5 };
		const int operationsPerThread { 50 };
		std::atomic<int> operationsCompleted { 0 };

		std::vector<std::thread> threads {};
		threads.reserve(numThreads);

		for (int threadIdx = 0; threadIdx < numThreads; ++threadIdx) {
			threads.emplace_back(
				[&config, &operationsCompleted, operationsPerThread, threadIdx]() -> void {
					for (int readWriteIdx = 0; readWriteIdx < operationsPerThread; ++readWriteIdx) {
						if (readWriteIdx % 2 == 0) {
							config.get("shared_key");
						} else {
							config.set(
								"shared_key", "value_from_thread_" + std::to_string(threadIdx)
							);
						}
						operationsCompleted++;
					}
				}
			);
		}

		for (auto &thread : threads) {
			thread.join();
		}

		REQUIRE(operationsCompleted == numThreads * operationsPerThread);

		auto stats = config.getKeyStats("shared_key");
		REQUIRE(stats.reads + stats.writes > 0);
	}

	std::filesystem::remove(testFilePath);
}
