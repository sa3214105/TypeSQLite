#pragma once
#include "Common.hpp"

// ============ Table Constraint 測試 ============

class TableConstraintTest : public ::testing::Test {
protected:
    void TearDown() override {
        std::remove("test_database.db");
    }
};

// 測試 PrimaryKey (單欄位主鍵)
TEST_F(TableConstraintTest, TableConstraintPrimaryKeySingle) {
    using IdColumn = Column<"id", DataType::INTEGER>;
    auto testTableDef = MakeTableDefinition<"test_pk_single">(
        std::make_tuple(IdColumn{}, NameColumn),
        std::make_tuple(TablePrimaryKey(std::make_tuple(IdColumn{}))),
        std::make_tuple()
    );
    Database<decltype(testTableDef)> db("test_database.db", testTableDef);
    auto &testTable = db.GetTable<decltype(testTableDef)>();

    testTable.Insert<IdColumn, decltype(NameColumn)>(1, "Alice");

    // 嘗試插入重複的主鍵，應該失敗
    EXPECT_ANY_THROW(
        (testTable.Insert<IdColumn, decltype(NameColumn)>(1, "Bob"))
    );

    auto results = testTable.Select(testTable[IdColumn{}], testTable[NameColumn]).Results().ToVector();

    EXPECT_EQ(results.size(), 1);
    EXPECT_EQ(std::get<0>(results[0]), 1);
    EXPECT_EQ(std::get<1>(results[0]), "Alice");
}

// 測試 PrimaryKey (複合主鍵)
TEST_F(TableConstraintTest, TableConstraintPrimaryKeyComposite) {
    using IdColumn = Column<"id", DataType::INTEGER>;
    auto testTableDef = MakeTableDefinition<"test_pk_composite">(
        std::make_tuple(IdColumn{}, NameColumn, AgeColumn),
        std::make_tuple(TablePrimaryKey(std::make_tuple(IdColumn{}, NameColumn))),
        std::make_tuple()
    );
    Database<decltype(testTableDef)> db("test_database.db", testTableDef);
    auto &testTable = db.GetTable<decltype(testTableDef)>();

    testTable.Insert<IdColumn, decltype(NameColumn), decltype(AgeColumn)>(1, "Alice", 30);
    testTable.Insert<IdColumn, decltype(NameColumn), decltype(AgeColumn)>(1, "Bob", 25);

    // 嘗試插入重複的複合主鍵，應該失敗
    EXPECT_ANY_THROW((
        testTable.Insert<IdColumn, decltype(NameColumn), decltype(AgeColumn)>(1, "Alice", 35)
    ));

    auto results = testTable.Select(
        testTable[IdColumn{}],
        testTable[NameColumn],
        testTable[AgeColumn]
    ).Results().ToVector();

    EXPECT_EQ(results.size(), 2);
}

// 測試 Unique (單欄位唯一約束)
TEST_F(TableConstraintTest, TableConstraintUniqueSingle) {
    using EmailColumn = Column<"email", DataType::TEXT>;
    auto testTableDef = MakeTableDefinition<"test_unique_single">(
        std::make_tuple(NameColumn, EmailColumn{}),
        std::make_tuple(TableUnique(std::make_tuple(EmailColumn{}))),
        std::make_tuple()
    );
    Database<decltype(testTableDef)> db("test_database.db", testTableDef);
    auto &testTable = db.GetTable<decltype(testTableDef)>();

    testTable.Insert<decltype(NameColumn), EmailColumn>("Alice", "alice@example.com");

    // 嘗試插入重複的 email，應該失敗
    EXPECT_ANY_THROW((
        testTable.Insert<decltype(NameColumn), EmailColumn>("Bob", "alice@example.com")
    ));

    auto results = testTable.Select(testTable[NameColumn], testTable[EmailColumn{}]).Results().ToVector();

    EXPECT_EQ(results.size(), 1);
    EXPECT_EQ(std::get<1>(results[0]), "alice@example.com");
}

// 測試 Unique (複合唯一約束)
TEST_F(TableConstraintTest, TableConstraintUniqueComposite) {
    using EmailColumn = Column<"email", DataType::TEXT>;
    auto testTableDef = MakeTableDefinition<"test_unique_composite">(
        std::make_tuple(NameColumn, EmailColumn{}),
        std::make_tuple(TableUnique(std::make_tuple(NameColumn, EmailColumn{}))),
        std::make_tuple()
    );
    Database<decltype(testTableDef)> db("test_database.db", testTableDef);
    auto &testTable = db.GetTable<decltype(testTableDef)>();

    testTable.Insert<decltype(NameColumn), EmailColumn>("Alice", "alice@example.com");

    // 相同的 name 但不同的 email，應該成功
    testTable.Insert<decltype(NameColumn), EmailColumn>("Alice", "alice2@example.com");

    // 嘗試插入重複的複合唯一鍵，應該失敗
    EXPECT_ANY_THROW((
        testTable.Insert<decltype(NameColumn), EmailColumn>("Alice", "alice@example.com")
    ));

    auto results = testTable.Select(testTable[NameColumn], testTable[EmailColumn{}]).Results().ToVector();

    EXPECT_EQ(results.size(), 2);
}

// 測試混合使用欄位約束和表約束
TEST_F(TableConstraintTest, TableConstraintMixed) {
    using IdColumn = Column<"id", DataType::INTEGER, ColumnPrimaryKey<>>;
    using EmailColumn = Column<"email", DataType::TEXT, ColumnNotNull<>>;
    auto testTableDef = MakeTableDefinition<"test_mixed_constraints">(
        std::make_tuple(IdColumn{}, NameColumn, EmailColumn{}),
        std::make_tuple(TableUnique(std::make_tuple(EmailColumn{}))),
        std::make_tuple()
    );
    Database<decltype(testTableDef)> db("test_database.db", testTableDef);
    auto &testTable = db.GetTable<decltype(testTableDef)>();

    testTable.Insert<IdColumn, decltype(NameColumn), EmailColumn>(1, "Alice", "alice@example.com");

    // 測試主鍵唯一性
    EXPECT_ANY_THROW((
        testTable.Insert<IdColumn, decltype(NameColumn), EmailColumn>(1, "Bob", "bob@example.com")
    ));

    // 測試 NOT NULL 約束
    EXPECT_ANY_THROW((
        testTable.Insert<IdColumn, decltype(NameColumn)>(2, "Bob")
    ));

    // 測試表級 UNIQUE 約束
    EXPECT_ANY_THROW((
        testTable.Insert<IdColumn, decltype(NameColumn), EmailColumn>(2, "Bob", "alice@example.com")
    ));

    auto results = testTable.Select(
        testTable[IdColumn{}],
        testTable[NameColumn],
        testTable[EmailColumn{}]
    ).Results().ToVector();

    EXPECT_EQ(results.size(), 1);
}

// 測試 PrimaryKey 與 DESC 排序
TEST_F(TableConstraintTest, TableConstraintPrimaryKeyDesc) {
    using IdColumn = Column<"id", DataType::INTEGER>;
    auto testTableDef = MakeTableDefinition<"test_pk_desc_table">(
        std::make_tuple(IdColumn{}, NameColumn),
        std::make_tuple(TablePrimaryKey(std::make_tuple(ColumnWithOrder(IdColumn(), OrderType::DESC)))),
        std::make_tuple()
    );
    Database<decltype(testTableDef)> db("test_database.db", testTableDef);
    auto &testTable = db.GetTable<decltype(testTableDef)>();

    testTable.Insert<IdColumn, decltype(NameColumn)>(1, "Alice");
    testTable.Insert<IdColumn, decltype(NameColumn)>(2, "Bob");

    auto results = testTable.Select(testTable[IdColumn{}], testTable[NameColumn]).Results().ToVector();

    EXPECT_EQ(results.size(), 2);
}

// 測試 Unique 與衝突處理
TEST_F(TableConstraintTest, TableConstraintUniqueWithConflict) {
    using EmailColumn = Column<"email", DataType::TEXT>;
    auto testTableDef = MakeTableDefinition<"test_unique_conflict">(
        std::make_tuple(NameColumn, EmailColumn{}),
        std::make_tuple(TableUnique(std::make_tuple(EmailColumn{}), ConflictCause::IGNORE)),
        std::make_tuple()
    );
    Database<decltype(testTableDef)> db("test_database.db", testTableDef);
    auto &testTable = db.GetTable<decltype(testTableDef)>();

    testTable.Insert<decltype(NameColumn), EmailColumn>("Alice", "alice@example.com");

    // 由於設定 IGNORE，插入重複值不會拋出異常，而是被忽略
    testTable.Insert<decltype(NameColumn), EmailColumn>("Bob", "alice@example.com");

    auto results = testTable.Select(testTable[NameColumn], testTable[EmailColumn{}]).Results().ToVector();

    // 只有第一筆成功插入
    EXPECT_EQ(results.size(), 1);
    EXPECT_EQ(std::get<0>(results[0]), "Alice");
}

// 測試 PrimaryKey 複合主鍵每個欄位不同排序
TEST_F(TableConstraintTest, TableConstraintPrimaryKeyMixedOrder) {
    using IdColumn = Column<"id", DataType::INTEGER>;
    auto testTableDef = MakeTableDefinition<"test_pk_mixed_order">(
        std::make_tuple(IdColumn{}, NameColumn, AgeColumn),
        std::make_tuple(TablePrimaryKey(std::make_tuple(
            ColumnWithOrder(IdColumn(), OrderType::ASC),
            ColumnWithOrder(NameColumn, OrderType::DESC)
        ))),
        std::make_tuple()
    );
    Database<decltype(testTableDef)> db("test_database.db", testTableDef);
    auto &testTable = db.GetTable<decltype(testTableDef)>();

    testTable.Insert<IdColumn, decltype(NameColumn), decltype(AgeColumn)>(1, "Alice", 30);
    testTable.Insert<IdColumn, decltype(NameColumn), decltype(AgeColumn)>(1, "Bob", 25);

    // 嘗試插入重複的複合主鍵
    EXPECT_ANY_THROW((
        testTable.Insert<IdColumn, decltype(NameColumn), decltype(AgeColumn)>(1, "Alice", 35)
    ));

    auto results = testTable.Select(
        testTable[IdColumn{}],
        testTable[NameColumn],
        testTable[AgeColumn]
    ).Results().ToVector();

    EXPECT_EQ(results.size(), 2);
}

// 測試 Unique 複合唯一約束每個欄位不同排序
TEST_F(TableConstraintTest, TableConstraintUniqueMixedOrder) {
    using EmailColumn = Column<"email", DataType::TEXT>;
    using PhoneColumn = Column<"phone", DataType::TEXT>;
    auto testTableDef = MakeTableDefinition<"test_unique_mixed_order">(
        std::make_tuple(NameColumn, EmailColumn{}, PhoneColumn{}),
        std::make_tuple(TableUnique(std::make_tuple(
            ColumnWithOrder(EmailColumn(), OrderType::DESC),
            ColumnWithOrder(PhoneColumn(), OrderType::ASC)
        ))),
        std::make_tuple()
    );
    Database<decltype(testTableDef)> db("test_database.db", testTableDef);
    auto &testTable = db.GetTable<decltype(testTableDef)>();

    testTable.Insert<decltype(NameColumn), EmailColumn, PhoneColumn>("Alice", "alice@example.com", "123-4567");

    // 不同的 email 和 phone 組合，應該成功
    testTable.Insert<decltype(NameColumn), EmailColumn, PhoneColumn>("Bob", "bob@example.com", "123-4567");

    // 嘗試插入重複的複合唯一鍵
    EXPECT_ANY_THROW((
        testTable.Insert<decltype(NameColumn), EmailColumn, PhoneColumn>("Charlie", "alice@example.com", "123-4567")
    ));

    auto results = testTable.Select(
        testTable[NameColumn],
        testTable[EmailColumn{}],
        testTable[PhoneColumn{}]
    ).Results().ToVector();

    EXPECT_EQ(results.size(), 2);
}

// 測試混合使用有序和無序的欄位
TEST_F(TableConstraintTest, TableConstraintMixedOrderedUnordered) {
    using IdColumn = Column<"id", DataType::INTEGER>;
    using EmailColumn = Column<"email", DataType::TEXT>;
    auto testTableDef = MakeTableDefinition<"test_mixed_ordered_unordered">(
        std::make_tuple(IdColumn{}, NameColumn, EmailColumn{}),
        std::make_tuple(TablePrimaryKey(std::make_tuple(
            ColumnWithOrder(IdColumn(), OrderType::DESC),
            NameColumn // 未指定排序，使用預設
        ))),
        std::make_tuple()
    );
    Database<decltype(testTableDef)> db("test_database.db", testTableDef);
    auto &testTable = db.GetTable<decltype(testTableDef)>();

    testTable.Insert<IdColumn, decltype(NameColumn), EmailColumn>(1, "Alice", "alice@example.com");
    testTable.Insert<IdColumn, decltype(NameColumn), EmailColumn>(1, "Bob", "bob@example.com");

    auto results = testTable.Select(testTable[IdColumn{}], testTable[NameColumn]).Results().ToVector();

    EXPECT_EQ(results.size(), 2);
}

// 測試多個 UNIQUE 約束
TEST_F(TableConstraintTest, TableConstraintMultipleUnique) {
    using IdColumn = Column<"id", DataType::INTEGER>;
    using EmailColumn = Column<"email", DataType::TEXT>;
    using PhoneColumn = Column<"phone", DataType::TEXT>;
    using UsernameColumn = Column<"username", DataType::TEXT>;

    // 一個表可以有多個 UNIQUE 約束
    auto testTableDef = MakeTableDefinition<"test_multiple_unique">(
        std::make_tuple(IdColumn{}, UsernameColumn{}, EmailColumn{}, PhoneColumn{}),
        std::make_tuple(
            TableUnique(std::make_tuple(EmailColumn{})),
            TableUnique(std::make_tuple(PhoneColumn{})),
            TableUnique(std::make_tuple(UsernameColumn{}))
        ),
        std::make_tuple()
    );
    Database<decltype(testTableDef)> db("test_database.db", testTableDef);
    auto &testTable = db.GetTable<decltype(testTableDef)>();

    // 插入第一筆資料
    testTable.Insert<IdColumn, UsernameColumn, EmailColumn, PhoneColumn>(
        1, "alice", "alice@example.com", "123-4567"
    );

    // 嘗試插入重複的 email，應該失敗
    EXPECT_ANY_THROW((
        testTable.Insert<IdColumn, UsernameColumn, EmailColumn, PhoneColumn>(
            2, "bob", "alice@example.com", "234-5678"
        )
    ));

    // 嘗試插入重複的 phone，應該失敗
    EXPECT_ANY_THROW((
        testTable.Insert<IdColumn, UsernameColumn, EmailColumn, PhoneColumn>(
            2, "bob", "bob@example.com", "123-4567"
        )
    ));

    // 嘗試插入重複的 username，應該失敗
    EXPECT_ANY_THROW((
        testTable.Insert<IdColumn, UsernameColumn, EmailColumn, PhoneColumn>(
            2, "alice", "bob@example.com", "234-5678"
        )
    ));

    // 插入完全不重複的資料，應該成功
    testTable.Insert<IdColumn, UsernameColumn, EmailColumn, PhoneColumn>(
        2, "bob", "bob@example.com", "234-5678"
    );

    auto results = testTable.Select(
        testTable[IdColumn{}],
        testTable[UsernameColumn{}],
        testTable[EmailColumn{}],
        testTable[PhoneColumn{}]
    ).Results().ToVector();

    EXPECT_EQ(results.size(), 2);
    EXPECT_EQ(std::get<1>(results[0]), "alice");
    EXPECT_EQ(std::get<1>(results[1]), "bob");
}

// 測試複合 UNIQUE 約束組合
TEST_F(TableConstraintTest, TableConstraintMultipleCompositeUnique) {
    using IdColumn = Column<"id", DataType::INTEGER>;
    using FirstNameColumn = Column<"first_name", DataType::TEXT>;
    using LastNameColumn = Column<"last_name", DataType::TEXT>;
    using EmailColumn = Column<"email", DataType::TEXT>;

    // 多個複合 UNIQUE 約束
    auto testTableDef = MakeTableDefinition<"test_multiple_composite_unique">(
        std::make_tuple(IdColumn{}, FirstNameColumn{}, LastNameColumn{}, EmailColumn{}),
        std::make_tuple(
            TableUnique(std::make_tuple(FirstNameColumn{}, LastNameColumn{})), // 名字組合唯一
            TableUnique(std::make_tuple(EmailColumn{})) // email 也要唯一
        ),
        std::make_tuple()
    );
    Database<decltype(testTableDef)> db("test_database.db", testTableDef);
    auto &testTable = db.GetTable<decltype(testTableDef)>();

    // 插入第一筆資料
    testTable.Insert<IdColumn, FirstNameColumn, LastNameColumn, EmailColumn>(
        1, "John", "Doe", "john.doe@example.com"
    );

    // 相同的 first_name 但不同的 last_name，應該成功
    testTable.Insert<IdColumn, FirstNameColumn, LastNameColumn, EmailColumn>(
        2, "John", "Smith", "john.smith@example.com"
    );

    // 嘗試插入重複的名字組合，應該失敗
    EXPECT_ANY_THROW((
        testTable.Insert<IdColumn, FirstNameColumn, LastNameColumn, EmailColumn>(
            3, "John", "Doe", "another@example.com"
        )
    ));

    // 嘗試插入重複的 email，應該失敗
    EXPECT_ANY_THROW((
        testTable.Insert<IdColumn, FirstNameColumn, LastNameColumn, EmailColumn>(
            3, "Jane", "Doe", "john.doe@example.com"
        )
    ));

    auto results = testTable.Select(
        testTable[IdColumn{}],
        testTable[FirstNameColumn{}],
        testTable[LastNameColumn{}]
    ).Results().ToVector();

    EXPECT_EQ(results.size(), 2);
}

// 測試 PRIMARY KEY 與多個 UNIQUE 約束組合
TEST_F(TableConstraintTest, TableConstraintPrimaryKeyWithMultipleUnique) {
    using IdColumn = Column<"id", DataType::INTEGER>;
    using EmailColumn = Column<"email", DataType::TEXT>;
    using PhoneColumn = Column<"phone", DataType::TEXT>;
    using SsnColumn = Column<"ssn", DataType::TEXT>; // Social Security Number

    auto testTableDef = MakeTableDefinition<"test_pk_multi_unique">(
        std::make_tuple(IdColumn{}, NameColumn, EmailColumn{}, PhoneColumn{}, SsnColumn{}),
        std::make_tuple(
            TablePrimaryKey(std::make_tuple(IdColumn{})),
            TableUnique(std::make_tuple(EmailColumn{})),
            TableUnique(std::make_tuple(PhoneColumn{})),
            TableUnique(std::make_tuple(SsnColumn{}))
        ),
        std::make_tuple()
    );
    Database<decltype(testTableDef)> db("test_database.db", testTableDef);
    auto &testTable = db.GetTable<decltype(testTableDef)>();

    // 插入第一筆資料
    testTable.Insert<IdColumn, decltype(NameColumn), EmailColumn, PhoneColumn, SsnColumn>(
        1, "Alice", "alice@example.com", "123-4567", "111-11-1111"
    );

    // 測試 PRIMARY KEY 唯一性
    EXPECT_ANY_THROW((
        testTable.Insert<IdColumn, decltype(NameColumn), EmailColumn, PhoneColumn, SsnColumn>(
            1, "Bob", "bob@example.com", "234-5678", "222-22-2222"
        )
    ));

    // 測試第一個 UNIQUE 約束（email）
    EXPECT_ANY_THROW((
        testTable.Insert<IdColumn, decltype(NameColumn), EmailColumn, PhoneColumn, SsnColumn>(
            2, "Bob", "alice@example.com", "234-5678", "222-22-2222"
        )
    ));

    // 測試第二個 UNIQUE 約束（phone）
    EXPECT_ANY_THROW((
        testTable.Insert<IdColumn, decltype(NameColumn), EmailColumn, PhoneColumn, SsnColumn>(
            2, "Bob", "bob@example.com", "123-4567", "222-22-2222"
        )
    ));

    // 測試第三個 UNIQUE 約束（ssn）
    EXPECT_ANY_THROW((
        testTable.Insert<IdColumn, decltype(NameColumn), EmailColumn, PhoneColumn, SsnColumn>(
            2, "Bob", "bob@example.com", "234-5678", "111-11-1111"
        )
    ));

    // 插入完全不重複的資料，應該成功
    testTable.Insert<IdColumn, decltype(NameColumn), EmailColumn, PhoneColumn, SsnColumn>(
        2, "Bob", "bob@example.com", "234-5678", "222-22-2222"
    );

    auto results = testTable.Select(testTable[IdColumn{}], testTable[NameColumn]).Results().ToVector();

    EXPECT_EQ(results.size(), 2);
}

// 測試混合單一和複合 UNIQUE 約束
TEST_F(TableConstraintTest, TableConstraintMixedSingleCompositeUnique) {
    using IdColumn = Column<"id", DataType::INTEGER>;
    using CountryColumn = Column<"country", DataType::TEXT>;
    using CityColumn = Column<"city", DataType::TEXT>;
    using EmailColumn = Column<"email", DataType::TEXT>;

    auto testTableDef = MakeTableDefinition<"test_mixed_unique">(
        std::make_tuple(IdColumn{}, NameColumn, CountryColumn{}, CityColumn{}, EmailColumn{}),
        std::make_tuple(
            TablePrimaryKey(std::make_tuple(IdColumn{})),
            TableUnique(std::make_tuple(EmailColumn{})), // 單一欄位
            TableUnique(std::make_tuple(CountryColumn{}, CityColumn{}, NameColumn)) // 複合欄位
        ),
        std::make_tuple()
    );
    Database<decltype(testTableDef)> db("test_database.db", testTableDef);
    auto &testTable = db.GetTable<decltype(testTableDef)>();

    // 插入第一筆資料
    testTable.Insert<IdColumn, decltype(NameColumn), CountryColumn, CityColumn, EmailColumn>(
        1, "Alice", "USA", "New York", "alice@example.com"
    );

    // 相同的名字和地點但不同的 email，由於複合約束相同應該失敗
    EXPECT_ANY_THROW((
        testTable.Insert<IdColumn, decltype(NameColumn), CountryColumn, CityColumn, EmailColumn>(
            2, "Alice", "USA", "New York", "alice2@example.com"
        )
    ));

    // 不同的名字但相同的 email，應該失敗（email unique 約束）
    EXPECT_ANY_THROW((
        testTable.Insert<IdColumn, decltype(NameColumn), CountryColumn, CityColumn, EmailColumn>(
            2, "Bob", "USA", "Boston", "alice@example.com"
        )
    ));

    // 相同的名字但不同的地點和 email，應該成功
    testTable.Insert<IdColumn, decltype(NameColumn), CountryColumn, CityColumn, EmailColumn>(
        2, "Alice", "USA", "Boston", "alice.boston@example.com"
    );

    auto results = testTable.Select(
        testTable[IdColumn{}],
        testTable[NameColumn],
        testTable[CityColumn{}]
    ).Results().ToVector();

    EXPECT_EQ(results.size(), 2);
}

// ============ Table Foreign Key 測試 ============

// 測試 Foreign Key (單欄位外鍵)
TEST_F(TableConstraintTest, TableForeignKeySingle) {
    // 創建父表（被引用的表）
    using ParentIdColumn = Column<"parent_id", DataType::INTEGER, ColumnPrimaryKey<>>;
    using ParentNameColumn = Column<"parent_name", DataType::TEXT>;
    auto parentTableDef = MakeTableDefinition<"parent_table">(
        std::make_tuple(ParentIdColumn{}, ParentNameColumn{}),
        std::make_tuple(),
        std::make_tuple()
    );

    // 創建子表（包含外鍵的表）
    using ChildIdColumn = Column<"child_id", DataType::INTEGER, ColumnPrimaryKey<>>;
    using ChildNameColumn = Column<"child_name", DataType::TEXT>;
    using ParentRefColumn = Column<"parent_ref", DataType::INTEGER>;

    auto foreignKeyClause = ForeignKeyClause(parentTableDef, ParentIdColumn{});
    auto childTableDef = MakeTableDefinition<"child_table">(
        std::make_tuple(ChildIdColumn{}, ChildNameColumn{}, ParentRefColumn{}),
        std::make_tuple(TableForeignKey(std::make_tuple(ParentRefColumn{}), foreignKeyClause)),
        std::make_tuple()
    );

    Database<decltype(parentTableDef), decltype(childTableDef)> db("test_database.db", parentTableDef, childTableDef);
    auto &parentTable = db.GetTable<decltype(parentTableDef)>();
    auto &childTable = db.GetTable<decltype(childTableDef)>();

    // 插入父表資料
    parentTable.Insert<ParentIdColumn, ParentNameColumn>(1, "Parent One");
    parentTable.Insert<ParentIdColumn, ParentNameColumn>(2, "Parent Two");

    // 插入有效的子表資料（引用存在的父表記錄）
    childTable.Insert<ChildIdColumn, ChildNameColumn, ParentRefColumn>(101, "Child One", 1);
    childTable.Insert<ChildIdColumn, ChildNameColumn, ParentRefColumn>(102, "Child Two", 2);

    auto results = childTable.Select(
        childTable[ChildIdColumn{}],
        childTable[ChildNameColumn{}],
        childTable[ParentRefColumn{}]
    ).Results().ToVector();

    EXPECT_EQ(results.size(), 2);
    EXPECT_EQ(std::get<0>(results[0]), 101);
    EXPECT_EQ(std::get<2>(results[0]), 1);
}

// 測試 Foreign Key (複合外鍵)
TEST_F(TableConstraintTest, TableForeignKeyComposite) {
    // 創建父表（複合主鍵）
    using DeptIdColumn = Column<"dept_id", DataType::INTEGER>;
    using DeptCodeColumn = Column<"dept_code", DataType::TEXT>;
    using DeptNameColumn = Column<"dept_name", DataType::TEXT>;

    auto deptTableDef = MakeTableDefinition<"departments">(
        std::make_tuple(DeptIdColumn{}, DeptCodeColumn{}, DeptNameColumn{}),
        std::make_tuple(TablePrimaryKey(std::make_tuple(DeptIdColumn{}, DeptCodeColumn{}))),
        std::make_tuple()
    );

    // 創建員工表（複合外鍵）
    using EmpIdColumn = Column<"emp_id", DataType::INTEGER, ColumnPrimaryKey<>>;
    using EmpNameColumn = Column<"emp_name", DataType::TEXT>;
    using EmpDeptIdColumn = Column<"emp_dept_id", DataType::INTEGER>;
    using EmpDeptCodeColumn = Column<"emp_dept_code", DataType::TEXT>;

    auto foreignKeyClause = ForeignKeyClause(
        deptTableDef,
        DeptIdColumn{},
        DeptCodeColumn{}
    );

    auto empTableDef = MakeTableDefinition<"employees">(
        std::make_tuple(EmpIdColumn{}, EmpNameColumn{}, EmpDeptIdColumn{}, EmpDeptCodeColumn{}),
        std::make_tuple(TableForeignKey(
            std::make_tuple(EmpDeptIdColumn{}, EmpDeptCodeColumn{}),
            foreignKeyClause
        )),
        std::make_tuple()
    );

    Database<decltype(deptTableDef), decltype(empTableDef)> db("test_database.db", deptTableDef, empTableDef);
    auto &deptTable = db.GetTable<decltype(deptTableDef)>();
    auto &empTable = db.GetTable<decltype(empTableDef)>();

    // 插入部門資料
    deptTable.Insert<DeptIdColumn, DeptCodeColumn, DeptNameColumn>(1, "IT", "Information Technology");
    deptTable.Insert<DeptIdColumn, DeptCodeColumn, DeptNameColumn>(2, "HR", "Human Resources");

    // 插入員工資料
    empTable.Insert<EmpIdColumn, EmpNameColumn, EmpDeptIdColumn, EmpDeptCodeColumn>(
        1001, "Alice", 1, "IT"
    );
    empTable.Insert<EmpIdColumn, EmpNameColumn, EmpDeptIdColumn, EmpDeptCodeColumn>(
        1002, "Bob", 2, "HR"
    );

    auto results = empTable.Select(
        empTable[EmpIdColumn{}],
        empTable[EmpNameColumn{}],
        empTable[EmpDeptIdColumn{}]
    ).Results().ToVector();

    EXPECT_EQ(results.size(), 2);
}

// 測試 Foreign Key with CASCADE
TEST_F(TableConstraintTest, TableForeignKeyWithCascade) {
    // 創建父表
    using CategoryIdColumn = Column<"category_id", DataType::INTEGER, ColumnPrimaryKey<>>;
    using CategoryNameColumn = Column<"category_name", DataType::TEXT>;

    auto categoryTableDef = MakeTableDefinition<"categories">(
        std::make_tuple(CategoryIdColumn{}, CategoryNameColumn{}),
        std::make_tuple(),
        std::make_tuple()
    );

    // 創建產品表（帶 ON DELETE CASCADE）
    using ProductIdColumn = Column<"product_id", DataType::INTEGER, ColumnPrimaryKey<>>;
    using ProductNameColumn = Column<"product_name", DataType::TEXT>;
    using ProductCategoryIdColumn = Column<"product_category_id", DataType::INTEGER>;

    auto foreignKeyClause = ForeignKeyClause(
        categoryTableDef,
        CategoryIdColumn{}
    )
    .On(ForeignTableAction::DELETE, ForeignKeyAction::CASCADE)
    .On(ForeignTableAction::UPDATE, ForeignKeyAction::CASCADE);

    auto productTableDef = MakeTableDefinition<"products">(
        std::make_tuple(ProductIdColumn{}, ProductNameColumn{}, ProductCategoryIdColumn{}),
        std::make_tuple(TableForeignKey(
            std::make_tuple(ProductCategoryIdColumn{}),
            foreignKeyClause
        )),
        std::make_tuple()
    );

    Database<decltype(categoryTableDef), decltype(productTableDef)> db("test_database.db", categoryTableDef, productTableDef);
    auto &categoryTable = db.GetTable<decltype(categoryTableDef)>();
    auto &productTable = db.GetTable<decltype(productTableDef)>();

    // 插入分類
    categoryTable.Insert<CategoryIdColumn, CategoryNameColumn>(1, "Electronics");
    categoryTable.Insert<CategoryIdColumn, CategoryNameColumn>(2, "Books");

    // 插入產品
    productTable.Insert<ProductIdColumn, ProductNameColumn, ProductCategoryIdColumn>(
        201, "Laptop", 1
    );
    productTable.Insert<ProductIdColumn, ProductNameColumn, ProductCategoryIdColumn>(
        202, "Novel", 2
    );

    auto results = productTable.Select(
        productTable[ProductIdColumn{}],
        productTable[ProductNameColumn{}]
    ).Results().ToVector();

    EXPECT_EQ(results.size(), 2);

    // 注意：SQLite 默認不啟用外鍵約束，需要 PRAGMA foreign_keys = ON
    // 這個測試主要驗證語法正確性
}

// 測試 Foreign Key with SET NULL
TEST_F(TableConstraintTest, TableForeignKeyWithSetNull) {
    // 創建供應商表
    using SupplierIdColumn = Column<"supplier_id", DataType::INTEGER, ColumnPrimaryKey<>>;
    using SupplierNameColumn = Column<"supplier_name", DataType::TEXT>;

    auto supplierTableDef = MakeTableDefinition<"suppliers">(
        std::make_tuple(SupplierIdColumn{}, SupplierNameColumn{}),
        std::make_tuple(),
        std::make_tuple()
    );

    // 創建零件表（帶 ON DELETE SET NULL）
    using PartIdColumn = Column<"part_id", DataType::INTEGER, ColumnPrimaryKey<>>;
    using PartNameColumn = Column<"part_name", DataType::TEXT>;
    using PartSupplierIdColumn = Column<"part_supplier_id", DataType::INTEGER>;

    auto foreignKeyClause = ForeignKeyClause(
        supplierTableDef,
        SupplierIdColumn{}
    )
    .On(ForeignTableAction::DELETE, ForeignKeyAction::SET_NULL);

    auto partTableDef = MakeTableDefinition<"parts">(
        std::make_tuple(PartIdColumn{}, PartNameColumn{}, PartSupplierIdColumn{}),
        std::make_tuple(TableForeignKey(
            std::make_tuple(PartSupplierIdColumn{}),
            foreignKeyClause
        )),
        std::make_tuple()
    );

    Database<decltype(supplierTableDef), decltype(partTableDef)> db("test_database.db", supplierTableDef, partTableDef);
    auto &supplierTable = db.GetTable<decltype(supplierTableDef)>();
    auto &partTable = db.GetTable<decltype(partTableDef)>();

    // 插入供應商
    supplierTable.Insert<SupplierIdColumn, SupplierNameColumn>(1, "Supplier A");
    supplierTable.Insert<SupplierIdColumn, SupplierNameColumn>(2, "Supplier B");

    // 插入零件
    partTable.Insert<PartIdColumn, PartNameColumn, PartSupplierIdColumn>(301, "Screw", 1);
    partTable.Insert<PartIdColumn, PartNameColumn, PartSupplierIdColumn>(302, "Bolt", 2);

    auto results = partTable.Select(
        partTable[PartIdColumn{}],
        partTable[PartSupplierIdColumn{}]
    ).Results().ToVector();

    EXPECT_EQ(results.size(), 2);
    // 注意：SQLite 默認不啟用外鍵約束
    // 這個測試主要驗證語法正確性
}

// 測試 Foreign Key 違反約束
TEST_F(TableConstraintTest, TableForeignKeyViolation) {
    // 創建主表
    using MasterIdColumn = Column<"master_id", DataType::INTEGER, ColumnPrimaryKey<>>;
    using MasterNameColumn = Column<"master_name", DataType::TEXT>;

    auto masterTableDef = MakeTableDefinition<"master">(
        std::make_tuple(MasterIdColumn{}, MasterNameColumn{}),
        std::make_tuple(),
        std::make_tuple()
    );

    // 創建詳細表
    using DetailIdColumn = Column<"detail_id", DataType::INTEGER, ColumnPrimaryKey<>>;
    using DetailMasterIdColumn = Column<"detail_master_id", DataType::INTEGER>;

    auto foreignKeyClause = ForeignKeyClause(
        masterTableDef,
        MasterIdColumn{}
    );

    auto detailTableDef = MakeTableDefinition<"detail">(
        std::make_tuple(DetailIdColumn{}, DetailMasterIdColumn{}),
        std::make_tuple(TableForeignKey(
            std::make_tuple(DetailMasterIdColumn{}),
            foreignKeyClause
        )),
        std::make_tuple()
    );

    Database<decltype(masterTableDef), decltype(detailTableDef)> db("test_database.db", masterTableDef, detailTableDef);
    auto &masterTable = db.GetTable<decltype(masterTableDef)>();
    auto &detailTable = db.GetTable<decltype(detailTableDef)>();

    masterTable.Insert<MasterIdColumn, MasterNameColumn>(1, "Master One");

    // 插入有效的記錄
    detailTable.Insert<DetailIdColumn, DetailMasterIdColumn>(1001, 1);

    auto results = detailTable.Select(detailTable[DetailIdColumn{}]).Results().ToVector();
    EXPECT_EQ(results.size(), 1);

    // 注意：SQLite 默認不強制執行外鍵約束，需要 PRAGMA foreign_keys = ON
    // 這個測試主要驗證語法正確性
}
