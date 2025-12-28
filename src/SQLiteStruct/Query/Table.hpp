#pragma once
#include <vector>
#include "./TableConstraint.hpp"

namespace TypeSQLite {
    template<ColumnOrTableColumnConcept T, ColumnOrTableColumnConcept... Ts>
    std::string GetUpdateField() {
        if constexpr (sizeof...(Ts) == 0) {
            return std::string(std::string(T::name) + " = ?");
        } else {
            return std::string(std::string(T::name) + " = ?, " + GetUpdateField<Ts...>());
        }
    }

    template<typename T, typename... Ts>
    std::string GetTableConstraintSQLFromPack(T t, Ts... ts) {
        if constexpr (sizeof...(Ts) == 0) {
            return ", " + std::string(t.value);
        } else {
            return ", " + std::string(t.value) + GetTableConstraintSQLFromPack(ts...);
        }
    }

    template<typename T, typename... Ts>
    std::string GetTableOptionsSQLFromPack() {
        if constexpr (sizeof...(Ts) == 0) {
            return std::string(T::value);
        } else {
            return std::string(T::value) + "," + GetTableOptionsSQLFromPack<Ts...>();
        }
    }

    template<typename T, typename... Ts>
    std::string GetColumnDefinitionFromPack() {
        if constexpr (sizeof...(Ts) == 0) {
            return GetColumnDefinition<T>();
        } else {
            return GetColumnDefinition<T>() + "," + GetColumnDefinitionFromPack<Ts...>();
        }
    }

    template<
        FixedString Name,
        typename ColumTypes= std::tuple<>,
        typename TableConstraints= std::tuple<>,
        typename TableOptions=std::tuple<> >
    struct TableDefinition {
        constexpr static FixedString name = Name;
        ColumTypes columns;
        TableConstraints tableConstraints;
        TableOptions tableOptions;
    };

    template<FixedString Name, typename ColumTypes, typename TableConstraints = std::tuple<>, typename TableOptions =
        std::tuple<> >
    constexpr auto MakeTableDefinition(ColumTypes columns,
                                       TableConstraints tableConstraints = std::make_tuple(),
                                       TableOptions tableOptions = std::make_tuple()) {
        return TableDefinition<Name, ColumTypes, TableConstraints, TableOptions>{
            .columns = columns, .tableConstraints = tableConstraints, .tableOptions = tableOptions
        };
    }

    template<typename>
    struct IsTableDefinition : std::false_type {
    };

    template<FixedString Name, typename ColumTypes, typename TableConstraints, typename TableOptions>
    struct IsTableDefinition<TableDefinition<Name, ColumTypes, TableConstraints, TableOptions> > : std::true_type {
    };

    template<typename T>
    concept TableDefinitionConcept = IsTableDefinition<T>::value;

    template<typename TableDef>
    class Table final : public SelectAble<decltype(std::declval<TableDef>().columns), SourceInfo<Table<TableDef> > > {
    public:
        template<ColumnConcept Col>
        using TableColumn = TableColumn_Base<Table, Col>;
        constexpr static FixedString name = TableDef::name;
        TableDef _tableDef;
        const decltype(_tableDef.columns) columns;

    private:
        SQLiteWrapper &_sqlite;

        template<typename _Where, bool AllowEmptyWhere, ColumnOrTableColumnConcept... Ts>
        class [[nodiscard("You must call Execute() for the query to run.")]] UpdateStatement {
            const Table &_table;
            std::tuple<ExprOrColReturnType<Ts>...> datas;
            _Where _where;

        public:
            explicit UpdateStatement(_Where where, const Table &table, ExprOrColReturnType<Ts>... ts) : _table(table),
                datas(ts...),
                _where(where) {
            }

            template<ExprOrColConcept Expr>
            auto Where(const Expr &expr) {
                return std::apply([&](auto &&... params) {
                    return UpdateStatement<Expr, AllowEmptyWhere, Ts...>(expr, _table, params...);
                }, datas);
            }

            auto WhereAll() {
                return std::apply([&](auto &&... params) {
                    return UpdateStatement<_Where, true, Ts...>(_where, _table, params...);
                }, datas);
            }

            void Execute() {
                static_assert(!std::is_same_v<_Where, nullptr_t> || AllowEmptyWhere,
                              "Where clause is required for UpdateStatement.Execute()");
                auto sql = std::string("UPDATE ") + std::string(name) + " SET " + GetUpdateField<Ts
                               ...>();
                if constexpr (!std::is_null_pointer_v<_Where>) {
                    sql += " WHERE " + _where.sql;
                }
                sql += ";";
                auto all_params = [&]() {
                    if constexpr (std::is_null_pointer_v<_Where>) {
                        return datas;
                    } else {
                        return std::tuple_cat(datas, _where.params);
                    }
                }();
                return std::apply([this, &sql](auto &&... params) {
                    return _table._sqlite.Execute(sql, params...);
                }, all_params);
            }
        };

        template<typename _Where, bool AllowEmptyWhere>
        class [[nodiscard("You must call Execute() for the query to run.")]] DeleteStatement {
            const Table &_table;
            _Where _where;

        public:
            explicit DeleteStatement(_Where where, const Table &table) : _table(table), _where(where) {
            }

            template<ExprOrColConcept Expr>
            auto Where(const Expr &expr) {
                return DeleteStatement<Expr, AllowEmptyWhere>(expr, _table);
            }

            auto WhereAll() {
                return DeleteStatement<_Where, true>(_where, _table);
            }

            void Execute() {
                static_assert(!std::is_same_v<_Where, nullptr_t> || AllowEmptyWhere,
                              "Where clause is required for DeleteStatement.Execute()");
                auto sql = std::string("DELETE FROM ") + std::string(name);
                if constexpr (!std::is_null_pointer_v<_Where>) {
                    sql += " WHERE " + _where.sql;
                }
                sql += ";";
                auto all_params = [&]() {
                    if constexpr (std::is_null_pointer_v<_Where>) {
                        return std::make_tuple();
                    } else {
                        return _where.params;
                    }
                }();
                return std::apply([this, &sql](auto &&... params) {
                    return _table._sqlite.Execute(sql, params...);
                }, all_params);
            }
        };

    public:
        explicit Table(SQLiteWrapper &sqlite, TableDef table_def) : SelectAble<decltype(table_def.columns), SourceInfo<
                                                                        Table> >(sqlite, table_def.columns,
                                                                        SourceInfo<Table>()), _tableDef(table_def),
                                                                    columns(table_def.columns),
                                                                    _sqlite(sqlite) {
            std::string sql = std::string("CREATE TABLE IF NOT EXISTS ") + std::string(name) + " (";

            // 添加列定義
            sql += std::apply([](auto... cols) {
                                  if constexpr (sizeof ...(cols) == 0) {
                                      return "";
                                  } else {
                                      return GetColumnDefinitionFromPack<decltype(cols)...>();
                                  }
                              },
                              _tableDef.columns);

            // 添加表約束（GetTableConstraintSQLFromPack 已經包含前導逗號）
            sql += std::apply([](auto... tableConstraints) {
                                  if constexpr (sizeof...(tableConstraints) == 0) {
                                      return "";
                                  } else {
                                      return GetTableConstraintSQLFromPack(tableConstraints...);
                                  }
                              },
                              _tableDef.tableConstraints
            );

            sql += ")";

            // 添加表選項（在括號外面）
            sql += std::apply([](auto... tableOptions) {
                                  if constexpr (sizeof...(tableOptions) == 0) {
                                      return "";
                                  } else {
                                      return " " + GetTableOptionsSQLFromPack<decltype(tableOptions)...>();
                                  }
                              },
                              _tableDef.tableOptions);
            sql += ";";
            sqlite.Execute(sql);
        }

        template<typename... U>
        void Insert(ExprOrColReturnType<U>... values) {
            //TODO 重啟檢查

            // static_assert(IsTypeGroupSubset<TypeGroup<U...>, columns>(),
            //               "Insert values must be subset of table columns");
            if (sizeof...(U) == 0) {
                throw std::runtime_error("Insert values cannot be empty");
            }
            std::string sql = std::string("INSERT INTO ") + std::string(name) + " (";
            sql += GetColumnNamesWithOutTableName<U...>();
            sql += ") VALUES (?";
            for (auto i = 0; i < sizeof...(U) - 1; ++i) {
                sql += ",?";
            }
            sql += ");";

            _sqlite.Execute(sql, values...);
        }

        // 批量插入支援
        template<typename... U>
        void InsertMany(const std::vector<std::tuple<ExprOrColReturnType<U>...> > &rows) {
            //TODO 重啟檢查

            // static_assert(IsTypeGroupSubset<TypeGroup<U...>, columns>(),
            //               "Insert values must be subset of table columns");
            if (sizeof...(U) == 0) {
                throw std::runtime_error("Insert values cannot be empty");
            }
            if (rows.empty()) {
                return;
            }

            // 使用 SQLiteWrapper::Transaction 來提高批量插入效能
            SQLiteWrapper::Transaction transaction(_sqlite);

            // 準備 SQL 語句
            std::string sql = std::string("INSERT INTO ") + std::string(name) + " (";
            sql += GetColumnNamesWithOutTableName<U...>();
            sql += ") VALUES (?";
            for (auto i = 0; i < sizeof...(U) - 1; ++i) {
                sql += ",?";
            }
            sql += ");";

            // 對每一行執行插入
            for (const auto &row: rows) {
                std::apply([this, &sql](auto &&... values) {
                    _sqlite.Execute(sql, values...);
                }, row);
            }

            // Transaction 解構子會自動 Commit（如果沒有異常）或 Rollback（如果有異常）
        }

        template<typename... U>
        void Upsert(ExprOrColReturnType<U>... values) {
            //TODO 重啟檢查

            // static_assert(IsTypeGroupSubset<TypeGroup<U...>, columns>(),
            //               "Upsert values must be subset of table columns");
            if (sizeof...(U) == 0) {
                throw std::runtime_error("Upsert values cannot be empty");
            }
            std::string sql = std::string("INSERT INTO ") + std::string(name) + " (";
            sql += GetColumnNamesWithOutTableName<U...>();
            sql += ") VALUES (?";
            for (auto i = 0; i < sizeof...(U) - 1; ++i) {
                sql += ",?";
            }
            sql += ") ON CONFLICT DO UPDATE SET ";
            sql += GetUpdateField<U...>();
            sql += ";";

            _sqlite.Execute(sql, values..., values...);
        }

        template<ColumnOrTableColumnConcept... U>
        auto Update(ExprOrColReturnType<U>... values) {
            //TODO 重啟檢查

            // static_assert(IsTypeGroupSubset<TypeGroup<U...>, columns>(),
            //               "Update values must be subset of table columns");
            if (sizeof...(U) == 0) {
                throw std::runtime_error("Update values cannot be empty");
            }
            return UpdateStatement<nullptr_t, false, U...>(nullptr, *this, values...);
        }

        auto Delete() {
            return DeleteStatement<nullptr_t, false>(nullptr, *this);
        }

        template<typename Column>
        auto operator[](Column column) {
            return TableColumn<Column>();
        }
    };

    template<typename>
    struct IsTable : std::false_type {
    };

    template<TableDefinitionConcept TableDef>
    struct IsTable<Table<TableDef> > : std::true_type {
    };

    template<typename T>
    concept TableConcept = IsTable<T>::value;
}
