#include <catch2/catch_all.hpp>

#include "StringUtils.h"

#include <iostream>

TEST_CASE("Test StringUtils - int conversion", "[common]")
{
    REQUIRE(hestia::StringUtils::to_int("12") == 12);

    REQUIRE(hestia::StringUtils::to_int("ABC123", true) == 11256099);

    const auto& [first, second] =
        hestia::StringUtils::split_on_first("quick_brown_fox", '_');
    REQUIRE(first == "quick");
    REQUIRE(second == "brown_fox");

    REQUIRE(hestia::StringUtils::to_lower("QuickBrownFox") == "quickbrownfox");

    std::string content = "quick\nbrown\nfox";
    std::vector<std::string> lines, expected_lines;
    hestia::StringUtils::to_lines(content, lines);
    expected_lines = {"quick", "brown", "fox"};
    REQUIRE(lines == expected_lines);
}
TEST_CASE("Test StringUtils - Start/End with prefix", "[common]")
{
    std::string test_string  = "hestia::tiers";
    std::string start_prefix = "hestia::";
    std::string end_prefix   = "tiers";
    REQUIRE(hestia::StringUtils::starts_with(test_string, start_prefix) == 1);
    REQUIRE(hestia::StringUtils::ends_with(test_string, end_prefix) == 1);
}

TEST_CASE("Test StringUtils - Remove prefix", "[common]")
{
    std::string test_string = "hestia::tiers";
    std::string prefix      = "hestia::";
    auto without_prefix =
        hestia::StringUtils::remove_prefix(test_string, prefix);
    REQUIRE(without_prefix == "tiers");
}

TEST_CASE("Test StringUtils - Split on substring", "[common]")
{
    std::string path = "127.0.0.1:8000/api/v1/hsm/objects/12345";
    const auto& [first, second] =
        hestia::StringUtils::split_on_first(path, "/objects");
    REQUIRE(first == "127.0.0.1:8000/api/v1/hsm");
    REQUIRE(second == "/12345");
}

TEST_CASE("Test StringUtils - Split on delimiter string", "[common]")
{
    std::string path = "user::token::value";
    std::vector<std::string> split_string;
    hestia::StringUtils::split(path, "::", split_string);
    REQUIRE(split_string.size() == 3);
    REQUIRE(split_string[0] == "user");
    REQUIRE(split_string[1] == "token");
    REQUIRE(split_string[2] == "value");
}
