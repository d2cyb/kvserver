#include <catch2/catch_test_macros.hpp>

#include <variant>

#include "kvserver_lib_export.h"

import kvserver.utilities;

using std::string;

using kvserver::parseCommand, kvserver::CommandGet, kvserver::CommandSet,
    kvserver::CommandUndefined;
using kvserver::trim;

TEST_CASE("trim", "[utilities]")
{
    SECTION("must trim white spaces")
    {
        string testString { "  get key1  " };

        REQUIRE(trim(testString) == "get key1");
    }
}

TEST_CASE("parseCommand", "[utilities]")
{
    SECTION("undefined command if not parsed command string")
    {
        string command { "wrong command 1" };

        auto result = parseCommand(command);

        REQUIRE_NOTHROW(std::get<CommandUndefined>(result));
    }

    SECTION("must parse get command")
    {
        string command { "get key1" };

        auto result = parseCommand(command);

        REQUIRE(std::get<CommandGet>(result).key == "key1");
    }

    SECTION("must parse set command")
    {
        string command { "set key1=value 1" };

        auto result = std::get<CommandSet>(parseCommand(command));

        REQUIRE(result.key == "key1");
        REQUIRE(result.value == "value 1");
    }

    SECTION("must trim set command value")
    {
        string command { "set key1=  value 1 " };

        auto result = std::get<CommandSet>(parseCommand(command));

        REQUIRE(result.key == "key1");
        REQUIRE(result.value == "value 1");
    }
}
