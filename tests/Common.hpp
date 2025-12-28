#pragma once
#include <gtest/gtest.h>
#include "../src/TypeSQlite.hpp"
//#include "single_header.hpp"

using namespace TypeSQLite;
inline Column<"name", DataType::TEXT> NameColumn;
inline Column<"age", DataType::INTEGER> AgeColumn;
inline Column<"score", DataType::REAL> ScoreColumn;
inline Column<"dept", DataType::TEXT> DeptColumn;
inline Column<"city", DataType::TEXT> CityColumn;
inline Column<"country", DataType::TEXT> CountryColumn;

// 定義表類型
inline auto UserTableDefinition = MakeTableDefinition<"name">(std::make_tuple(NameColumn, AgeColumn, ScoreColumn));
inline auto DeptTableDefinition = MakeTableDefinition<"departments">(std::make_tuple(DeptColumn, NameColumn));
inline auto CityTableDefinition = MakeTableDefinition<"cities">(std::make_tuple(NameColumn, CityColumn));
inline auto CountryTableDefinition = MakeTableDefinition<"countries">(std::make_tuple(CityColumn, CountryColumn));
