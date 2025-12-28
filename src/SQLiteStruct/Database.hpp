#pragma once
#include <functional>
#include "Query/Table.hpp"
#include "Query/Index.hpp"
#include "../SQLiteWrapper.hpp"

namespace TypeSQLite {
    template<typename T>
    concept TableOrIndexConcept = TableDefinitionConcept<T> || IndexDefinitionConcept<T>;

    template<TableOrIndexConcept Def, TableOrIndexConcept... Defs>
    auto CreateTables(SQLiteWrapper &sqlite, Def def, Defs... defs) {
        if constexpr (TableDefinitionConcept<Def>) {
            auto ret = std::tuple{Table<Def>(sqlite, def)};
            if constexpr (sizeof...(Defs) == 0) {
                return ret;
            } else {
                return std::tuple_cat(ret, CreateTables(sqlite, defs...));
            }
        } else {
            if constexpr (sizeof...(Defs) == 0) {
                return std::tuple{};
            } else {
                return CreateTables(sqlite, defs...);
            }
        }
    }

    template<TableOrIndexConcept Def, TableOrIndexConcept... Defs>
    auto CreateIndexes(SQLiteWrapper &sqlite, Def def, Defs... defs) {
        if constexpr (IndexDefinitionConcept<Def>) {
            auto ret = std::tuple{Index<Def>(sqlite, def)};
            if constexpr (sizeof...(Defs) == 0) {
                return ret;
            } else {
                return std::tuple_cat(ret, CreateIndexes<Defs...>(sqlite, defs...));
            }
        } else {
            if constexpr (sizeof...(Defs) == 0) {
                return std::tuple{};
            } else {
                return CreateIndexes<Defs...>(sqlite, defs...);
            }
        }
    }

    template<TableOrIndexConcept... TableOrIndexDefs>
    class Database {
    public:
        // 使用 SQLiteWrapper 的 Transaction
        using Transaction = SQLiteWrapper::Transaction;

    private:
        SQLiteWrapper _sqlite;
        decltype(CreateTables<TableOrIndexDefs...>(std::declval<SQLiteWrapper &>(), std::declval<TableOrIndexDefs>()...)
        ) _tables;
        decltype(CreateIndexes<TableOrIndexDefs...>(std::declval<SQLiteWrapper &>(),
                                                    std::declval<TableOrIndexDefs>()...)) _indexes;

    public:
        explicit Database(const std::string &db_path, TableOrIndexDefs... table_defs)
            : _sqlite(db_path, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE),
              _tables(CreateTables(_sqlite, table_defs...)),
              _indexes(CreateIndexes(_sqlite, table_defs...)) {
        }

        template<typename T>
        auto &GetTable() {
            return std::get<Table<T> >(_tables);
        }

        template<typename T>
        auto &GetIndex() {
            return std::get<Index<T> >(_indexes);
        }

        void CreateTransaction(const std::function<void(Transaction &)> &callback) {
            Transaction transaction(_sqlite);
            callback(transaction);
            // 解構子會自動根據錯誤狀態決定 Commit 或 Rollback
        }

        void CreateTransaction(const std::function<void()> &callback) {
            Transaction transaction(_sqlite);
            callback();
            // 解構子會自動根據錯誤狀態決定 Commit 或 Rollback
        }
    };
}
