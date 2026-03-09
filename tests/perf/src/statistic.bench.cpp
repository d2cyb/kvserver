#include <catch2/catch_all.hpp>
#include <catch2/catch_test_macros.hpp>

#include "kvserver_lib_export.h"

import kvserver.statistic;

using kvserver::Statistic;

TEST_CASE("Statistic benchmark", "[statistic][benchmark]")
{
	const int COMMANDS_COUNT { 100 };
	uint32_t totalCommands { 0 };
	uint32_t recentCommands { 0 };
	const uint32_t STATISTIC_OUTOUT_INTERVAL = 100;

	BENCHMARK("recent commands within interval")
	{
		Statistic stats { STATISTIC_OUTOUT_INTERVAL };
		for (int i { 0 }; i < COMMANDS_COUNT; ++i) {
			stats.recordCommand();
		}
		totalCommands  = stats.getTotalCommands();
		recentCommands = stats.getRecentCommands();
	};

	REQUIRE(totalCommands == 100);
	REQUIRE(recentCommands == 100);
}
