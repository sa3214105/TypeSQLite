#pragma once
#include "Common.hpp"

// ============ Column Constraints 測試 ============

class ColumnConstraintsTest : public ::testing::Test {
protected:
    void TearDown() override {
        std::remove("test_database.db");
    }
};

// 測試 PRIMARY KEY 約束
TEST_F(ColumnConstraintsTest, ConstraintPrimaryKey) {
    using IdColumn = Column<"id", DataType::INTEGER, ColumnPrimaryKey<> >;
    auto testTableDef = MakeTableDefinition<"test_pk">(std::make_tuple(IdColumn{}, NameColumn));
    Database<decltype(testTableDef)> db("test_database.db", testTableDef);
    auto &testTable = db.GetTable<decltype(testTableDef)>();

    // 插入第一筆資料
    testTable.Insert<IdColumn, decltype(NameColumn)>(1, "Alice");

    // 嘗試插入重複的 PRIMARY KEY，應該失敗
    EXPECT_THROW(
        (testTable.Insert<IdColumn, decltype(NameColumn)>(1, "Bob")),
        std::runtime_error
    );

    // 驗證只有一筆資料
    auto results = testTable.Select(testTable[IdColumn{}]).Results().ToVector();
    EXPECT_EQ(results.size(), 1);
}

// 測試 PRIMARY KEY 與 DESC 排序
TEST_F(ColumnConstraintsTest, ConstraintPrimaryKeyDesc) {
    using IdColumn = Column<"id", DataType::INTEGER, ColumnPrimaryKey<OrderType::DESC> >;
    auto testTableDef = MakeTableDefinition<"test_pk_desc">(std::make_tuple(IdColumn{}, NameColumn));
    Database<decltype(testTableDef)> db("test_database.db", testTableDef);
    auto &testTable = db.GetTable<decltype(testTableDef)>();

    testTable.Insert<IdColumn, decltype(NameColumn)>(1, "First");
    testTable.Insert<IdColumn, decltype(NameColumn)>(2, "Second");

    auto results = testTable.Select(testTable[IdColumn{}]).Results().ToVector();
    EXPECT_EQ(results.size(), 2);
}

// 測試 NOT NULL 約束
TEST_F(ColumnConstraintsTest, ConstraintNotNull) {
    using EmailColumn = Column<"email", DataType::TEXT, ColumnNotNull<> >;
    auto testTableDef = MakeTableDefinition<"test_not_null">(std::make_tuple(NameColumn, EmailColumn{}));
    Database<decltype(testTableDef)> db("test_database.db", testTableDef);
    auto &testTable = db.GetTable<decltype(testTableDef)>();

    // 插入有效資料
    testTable.Insert<decltype(NameColumn), EmailColumn>("Alice", "alice@example.com");

    auto results = testTable.Select(testTable[EmailColumn{}]).Results().ToVector();
    EXPECT_EQ(results.size(), 1);
    EXPECT_EQ(std::get<0>(results[0]), "alice@example.com");
}

// 測試 UNIQUE 約束
TEST_F(ColumnConstraintsTest, ConstraintUnique) {
    using UsernameColumn = Column<"username", DataType::TEXT, ColumnUnique<> >;
    auto testTableDef = MakeTableDefinition<"test_unique">(std::make_tuple(UsernameColumn{}, AgeColumn));
    Database<decltype(testTableDef)> db("test_database.db", testTableDef);
    auto &testTable = db.GetTable<decltype(testTableDef)>();

    // 插入第一筆資料
    testTable.Insert<UsernameColumn, decltype(AgeColumn)>("alice123", 25);

    // 嘗試插入重複的 UNIQUE 值，應該失敗
    EXPECT_THROW(
        (testTable.Insert<UsernameColumn, decltype(AgeColumn)>("alice123", 30)),
        std::runtime_error
    );

    // 插入不同的值應該成功
    testTable.Insert<UsernameColumn, decltype(AgeColumn)>("bob456", 30);

    auto results = testTable.Select(testTable[UsernameColumn{}]).Results().ToVector();
    EXPECT_EQ(results.size(), 2);
}

// 測試 DEFAULT 約束（整數）
TEST_F(ColumnConstraintsTest, ConstraintDefaultInteger) {
    using StatusColumn = Column<"status", DataType::INTEGER, Default<0> >;
    auto testTableDef = MakeTableDefinition<"test_default_int">(std::make_tuple(NameColumn, StatusColumn{}));
    Database<decltype(testTableDef)> db("test_database.db", testTableDef);
    auto &testTable = db.GetTable<decltype(testTableDef)>();

    // 只插入 name，status 應該使用預設值
    testTable.Insert<decltype(NameColumn)>("Alice");

    auto results = testTable.Select(testTable[NameColumn], testTable[StatusColumn{}]).Results().ToVector();

    EXPECT_EQ(results.size(), 1);
    EXPECT_EQ(std::get<0>(results[0]), "Alice");
    EXPECT_EQ(std::get<1>(results[0]), 0);
}

// 測試 DEFAULT 約束（字串）
TEST_F(ColumnConstraintsTest, ConstraintDefaultString) {
    using CountryColumn = Column<"country", DataType::TEXT, Default<"Taiwan"> >;
    auto testTableDef = MakeTableDefinition<"test_default_str">(std::make_tuple(NameColumn, CountryColumn{}));
    Database<decltype(testTableDef)> db("test_database.db", testTableDef);
    auto &testTable = db.GetTable<decltype(testTableDef)>();

    // 只插入 name
    testTable.Insert<decltype(NameColumn)>("Alice");

    auto results = testTable.Select(testTable[NameColumn], testTable[CountryColumn{}]).Results().ToVector();

    EXPECT_EQ(results.size(), 1);
    EXPECT_EQ(std::get<0>(results[0]), "Alice");
    EXPECT_EQ(std::get<1>(results[0]), "Taiwan");
}

// 測試組合約束：PRIMARY KEY + NOT NULL
TEST_F(ColumnConstraintsTest, ConstraintCombinationPrimaryKeyNotNull) {
    using IdColumn = Column<"id", DataType::INTEGER, ColumnPrimaryKey<>, ColumnNotNull<> >;
    auto testTableDef = MakeTableDefinition<"test_pk_nn">(std::make_tuple(IdColumn{}, NameColumn));
    Database<decltype(testTableDef)> db("test_database.db", testTableDef);
    auto &testTable = db.GetTable<decltype(testTableDef)>();

    testTable.Insert<IdColumn, decltype(NameColumn)>(1, "Alice");

    // 嘗試插入重複 ID 應該失敗
    EXPECT_THROW(
        (testTable.Insert<IdColumn, decltype(NameColumn)>(1, "Bob")),
        std::runtime_error
    );

    auto results = testTable.Select(testTable[IdColumn{}]).Results().ToVector();
    EXPECT_EQ(results.size(), 1);
}

// 測試組合約束：UNIQUE + NOT NULL
TEST_F(ColumnConstraintsTest, ConstraintCombinationUniqueNotNull) {
    using EmailColumn = Column<"email", DataType::TEXT, ColumnUnique<>, ColumnNotNull<> >;
    auto testTableDef = MakeTableDefinition<"test_uniq_nn">(std::make_tuple(NameColumn, EmailColumn{}));
    Database<decltype(testTableDef)> db("test_database.db", testTableDef);
    auto &testTable = db.GetTable<decltype(testTableDef)>();

    testTable.Insert<decltype(NameColumn), EmailColumn>("Alice", "alice@test.com");

    // 嘗試插入重複的 email
    EXPECT_THROW(
        (testTable.Insert<decltype(NameColumn), EmailColumn>("Bob", "alice@test.com")),
        std::runtime_error
    );

    // 插入不同的 email 應該成功
    testTable.Insert<decltype(NameColumn), EmailColumn>("Bob", "bob@test.com");

    auto results = testTable.Select(testTable[EmailColumn{}]).Results().ToVector();
    EXPECT_EQ(results.size(), 2);
}

// 測試 PRIMARY KEY 與 ON CONFLICT REPLACE
TEST_F(ColumnConstraintsTest, ConstraintPrimaryKeyConflictReplace) {
    using IdColumn = Column<"id", DataType::INTEGER, ColumnPrimaryKey<OrderType::ASC, ConflictCause::REPLACE> >;
    auto testTableDef = MakeTableDefinition<"test_pk_replace">(std::make_tuple(IdColumn{}, NameColumn));
    Database<decltype(testTableDef)> db("test_database.db", testTableDef);
    auto &testTable = db.GetTable<decltype(testTableDef)>();

    testTable.Insert<IdColumn, decltype(NameColumn)>(1, "Alice");

    // 插入重複 ID，應該替換舊資料
    testTable.Insert<IdColumn, decltype(NameColumn)>(1, "Bob");

    auto results = testTable.Select(testTable[IdColumn{}], testTable[NameColumn]).Results().ToVector();

    EXPECT_EQ(results.size(), 1);
    EXPECT_EQ(std::get<1>(results[0]), "Bob");
}

// 測試 UNIQUE 與 ON CONFLICT IGNORE
TEST_F(ColumnConstraintsTest, ConstraintUniqueConflictIgnore) {
    using EmailColumn = Column<"email", DataType::TEXT, ColumnUnique<ConflictCause::IGNORE> >;
    auto testTableDef = MakeTableDefinition<"test_uniq_ignore">(std::make_tuple(NameColumn, EmailColumn{}));
    Database<decltype(testTableDef)> db("test_database.db", testTableDef);
    auto &testTable = db.GetTable<decltype(testTableDef)>();

    testTable.Insert<decltype(NameColumn), EmailColumn>("Alice", "same@test.com");

    // 插入重複值，應該被忽略（不拋出異常）
    testTable.Insert<decltype(NameColumn), EmailColumn>("Bob", "same@test.com");

    auto results = testTable.Select(testTable[NameColumn]).Results().ToVector();
    EXPECT_EQ(results.size(), 1);
    EXPECT_EQ(std::get<0>(results[0]), "Alice");
}

// 測試 NOT NULL 與 ON CONFLICT FAIL
TEST_F(ColumnConstraintsTest, ConstraintNotNullConflictFail) {
    using RequiredColumn = Column<"required_field", DataType::TEXT, ColumnNotNull<ConflictCause::FAIL> >;
    auto testTableDef = MakeTableDefinition<"test_nn_fail">(std::make_tuple(NameColumn, RequiredColumn{}));
    Database<decltype(testTableDef)> db("test_database.db", testTableDef);
    auto &testTable = db.GetTable<decltype(testTableDef)>();

    // 插入有效資料
    testTable.Insert<decltype(NameColumn), RequiredColumn>("Alice", "value");

    auto results = testTable.Select(testTable[RequiredColumn{}]).Results().ToVector();
    EXPECT_EQ(results.size(), 1);
}

// 測試複雜的組合約束
TEST_F(ColumnConstraintsTest, ConstraintComplexCombination) {
    using IdColumn = Column<"id", DataType::INTEGER, ColumnPrimaryKey<> >;
    using EmailColumn = Column<"email", DataType::TEXT, ColumnUnique<>, ColumnNotNull<> >;
    using StatusColumn = Column<"status", DataType::INTEGER, Default<1> >;
    auto testTableDef = MakeTableDefinition<"test_complex">(std::make_tuple(IdColumn{}, NameColumn, EmailColumn{}, StatusColumn{}));
    Database<decltype(testTableDef)> db("test_database.db", testTableDef);
    auto &testTable = db.GetTable<decltype(testTableDef)>();

    // 插入完整資料
    testTable.Insert<
        IdColumn,
        decltype(NameColumn),
        EmailColumn,
        StatusColumn
    >(1, "Alice", "alice@test.com", 5);

    // 插入部分資料，status 使用預設值
    testTable.Insert<
        IdColumn,
        decltype(NameColumn),
        EmailColumn
    >(2, "Bob", "bob@test.com");

    auto results = testTable.Select(
        testTable[IdColumn{}],
        testTable[NameColumn],
        testTable[EmailColumn{}],
        testTable[StatusColumn{}]
    ).Results().ToVector();

    EXPECT_EQ(results.size(), 2);

    // 驗證第一筆
    EXPECT_EQ(std::get<0>(results[0]), 1);
    EXPECT_EQ(std::get<3>(results[0]), 5);

    // 驗證第二筆的預設值
    EXPECT_EQ(std::get<0>(results[1]), 2);
    EXPECT_EQ(std::get<3>(results[1]), 1);
}

// 測試實數型的 DEFAULT 約束
TEST_F(ColumnConstraintsTest, ConstraintDefaultReal) {
    using RatingColumn = Column<"rating", DataType::REAL, Default<5.0> >;
    auto testTableDef = MakeTableDefinition<"test_default_real">(std::make_tuple(NameColumn, RatingColumn{}));
    Database<decltype(testTableDef)> db("test_database.db", testTableDef);
    auto &testTable = db.GetTable<decltype(testTableDef)>();

    testTable.Insert<decltype(NameColumn)>("Product1");

    auto results = testTable.Select(testTable[NameColumn], testTable[RatingColumn{}]).Results().ToVector();

    EXPECT_EQ(results.size(), 1);
    EXPECT_DOUBLE_EQ(std::get<1>(results[0]), 5.0);
}

// 測試 DEFAULT 約束（CURRENT_TIMESTAMP）
TEST_F(ColumnConstraintsTest, ConstraintDefaultCurrentTimestamp) {
    using CreatedAtColumn = Column<"created_at", DataType::TEXT, Default<CurrentTimestamp{}> >;
    auto testTableDef = MakeTableDefinition<"test_default_timestamp">(std::make_tuple(NameColumn, CreatedAtColumn{}));
    Database<decltype(testTableDef)> db("test_database.db", testTableDef);
    auto &testTable = db.GetTable<decltype(testTableDef)>();

    // 只插入 name，created_at 應該使用 CURRENT_TIMESTAMP
    testTable.Insert<decltype(NameColumn)>("Alice");

    auto results = testTable.Select(testTable[NameColumn], testTable[CreatedAtColumn{}]).Results().ToVector();

    EXPECT_EQ(results.size(), 1);
    EXPECT_EQ(std::get<0>(results[0]), "Alice");
    // 驗證時間戳不為空且格式正確（包含日期和時間）
    auto timestamp = std::get<1>(results[0]);
    EXPECT_FALSE(timestamp.empty());
    EXPECT_TRUE(timestamp.find('-') != std::string::npos); // 應該包含日期分隔符
    EXPECT_TRUE(timestamp.find(':') != std::string::npos); // 應該包含時間分隔符
}

// 測試 DEFAULT 約束（CURRENT_DATE）
TEST_F(ColumnConstraintsTest, ConstraintDefaultCurrentDate) {
    using DateColumn = Column<"date", DataType::TEXT, Default<CurrentDate{}> >;
    auto testTableDef = MakeTableDefinition<"test_default_date">(std::make_tuple(NameColumn, DateColumn{}));
    Database<decltype(testTableDef)> db("test_database.db", testTableDef);
    auto &testTable = db.GetTable<decltype(testTableDef)>();

    testTable.Insert<decltype(NameColumn)>("Bob");

    auto results = testTable.Select(testTable[NameColumn], testTable[DateColumn{}]).Results().ToVector();

    EXPECT_EQ(results.size(), 1);
    EXPECT_EQ(std::get<0>(results[0]), "Bob");
    // 驗證日期不為空且格式正確（YYYY-MM-DD）
    auto date = std::get<1>(results[0]);
    EXPECT_FALSE(date.empty());
    EXPECT_TRUE(date.find('-') != std::string::npos); // 應該包含日期分隔符
    EXPECT_EQ(date.length(), 10); // YYYY-MM-DD 格式長度為 10
}

// 測試 DEFAULT 約束（CURRENT_TIME）
TEST_F(ColumnConstraintsTest, ConstraintDefaultCurrentTime) {
    using TimeColumn = Column<"time", DataType::TEXT, Default<CurrentTime{}> >;
    auto testTableDef = MakeTableDefinition<"test_default_time">(std::make_tuple(NameColumn, TimeColumn{}));
    Database<decltype(testTableDef)> db("test_database.db", testTableDef);
    auto &testTable = db.GetTable<decltype(testTableDef)>();

    testTable.Insert<decltype(NameColumn)>("Charlie");

    auto results = testTable.Select(testTable[NameColumn], testTable[TimeColumn{}]).Results().ToVector();

    EXPECT_EQ(results.size(), 1);
    EXPECT_EQ(std::get<0>(results[0]), "Charlie");
    // 驗證時間不為空且格式正確（HH:MM:SS）
    auto time = std::get<1>(results[0]);
    EXPECT_FALSE(time.empty());
    EXPECT_TRUE(time.find(':') != std::string::npos); // 應該包含時間分隔符
    EXPECT_EQ(time.length(), 8); // HH:MM:SS 格式長度為 8
}

// 測試組合 TimeKeyWord 和其他約束
TEST_F(ColumnConstraintsTest, ConstraintDefaultTimeKeyWordWithNotNull) {
    using IdColumn = Column<"id", DataType::INTEGER, ColumnPrimaryKey<> >;
    using CreatedAtColumn = Column<"created_at", DataType::TEXT, Default<CurrentTimestamp{}>, ColumnNotNull<> >;
    using UpdatedAtColumn = Column<"updated_at", DataType::TEXT, Default<CurrentTimestamp{}> >;

    auto testTableDef = MakeTableDefinition<"test_time_combo">(
        std::make_tuple(IdColumn{}, NameColumn, CreatedAtColumn{}, UpdatedAtColumn{})
    );
    Database<decltype(testTableDef)> db("test_database.db", testTableDef);
    auto &testTable = db.GetTable<decltype(testTableDef)>();

    // 插入資料，時間欄位使用預設值
    testTable.Insert<IdColumn, decltype(NameColumn)>(1, "David");

    auto results = testTable.Select(
        testTable[IdColumn{}],
        testTable[NameColumn],
        testTable[CreatedAtColumn{}],
        testTable[UpdatedAtColumn{}]
    ).Results().ToVector();

    EXPECT_EQ(results.size(), 1);
    EXPECT_EQ(std::get<0>(results[0]), 1);
    EXPECT_EQ(std::get<1>(results[0]), "David");

    // 驗證兩個時間戳都有值
    auto createdAt = std::get<2>(results[0]);
    auto updatedAt = std::get<3>(results[0]);
    EXPECT_FALSE(createdAt.empty());
    EXPECT_FALSE(updatedAt.empty());
    EXPECT_TRUE(createdAt.find(':') != std::string::npos);
    EXPECT_TRUE(updatedAt.find(':') != std::string::npos);
}

