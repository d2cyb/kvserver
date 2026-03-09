#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <string>
#include <thread>

#include "kvserver_lib_export.h"

import kvserver.config;

using kvserver::Config, kvserver::ConfigData;

TEST_CASE("Config basic operations", "[config]")
{
	const std::string testFilePath = "test_config.txt";

	SECTION("get must return values, if values set")
	{
		Config config(testFilePath);

		config.set("key1", "value1");
		config.set("key2", "value2");

		REQUIRE((*config.get("key1")).value == "value1");
		REQUIRE((*config.get("key2")).value == "value2");
		REQUIRE(config.get("key3").has_value() == false);
	}

	SECTION("add new value")
	{
		Config config(testFilePath);

		REQUIRE(config.get("new_key").has_value() == false);

		config.set("new_key", "new_value");
		REQUIRE((*config.get("new_key")).value == "new_value");
	}

	SECTION("must update value")
	{
		Config config(testFilePath);

		config.set("key1", "value1");
		REQUIRE((*config.get("key1")).value == "value1");

		config.set("key1", "new_value");
		REQUIRE((*config.get("key1")).value == "new_value");
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

		REQUIRE((*config.get("persistent1")).value == "value1");
		REQUIRE((*config.get("persistent2")).value == "value2");
	}

	SECTION("must empty data, if invalid json file")
	{
		std::ofstream file(testFilePath);
		file << "invalid json content";
		file.close();

		Config config(testFilePath);

		config.set("new_key", "new_value");
		REQUIRE((*config.get("new_key")).value == "new_value");
	}

	std::filesystem::remove(testFilePath);
}

TEST_CASE("read and write statistic", "[config][statistic]")
{
	const std::string testFilePath = "test_config_statistic.txt";

	SECTION("must empty statistics, for get not exists values")
	{
		Config config(testFilePath);

		REQUIRE(not config.get("test_key").has_value());
	}

	SECTION("must changed write statistic, if set value")
	{
		Config config(testFilePath);

		auto stats = config.set("test_key", "test_value");

		REQUIRE(stats.reads == 0);
		REQUIRE(stats.writes == 1);
	}

	SECTION("must changed read statistic, if set and get value")
	{
		Config config(testFilePath);

		config.set("test_key", "test_value");
		auto stats = *config.get("test_key");

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

					if (config.get(key).value_or({ .value = "", .reads = 0, .writes = 0 }).value
						== expected) {
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

	SECTION("should calculate, concurrent reads and writes")
	{
		const int numThreads		  = 5;
		const int operationsPerThread = 50;
		const std::string sharedKey	  = "shared_key";

		std::vector<std::thread> threads;
		threads.reserve(numThreads);
		ConfigData lastSharedState;

		for (int threadIdx = 0; threadIdx < numThreads; ++threadIdx) {
			threads.emplace_back([&]() -> void {
				for (int i = 1; i <= operationsPerThread; ++i) {
					if (i % 2 == 0) {
						lastSharedState = *config.get(sharedKey);
					} else {
						lastSharedState = config.set(sharedKey, std::to_string(i));
					}

					std::string nonExistentKey { "thread_" + std::to_string(threadIdx) + "_key_"
												 + std::to_string(i) };
					config.get(nonExistentKey);
				}
			});
		}

		for (auto &thread : threads) {
			thread.join();
		}

		REQUIRE(lastSharedState.reads == numThreads * (operationsPerThread / 2));
		REQUIRE(lastSharedState.writes == numThreads * (operationsPerThread / 2));
	}

	std::filesystem::remove(testFilePath);
}