#pragma once
#include <list>
#include <vector>
#include <sqlite3.h>
#include <memory>
#include <tuple>
#include <type_traits>

namespace TypeSQLite {
    template<typename T>
    void BindValue(sqlite3_stmt *stmt, int index, const T &value) {
        if constexpr (std::is_same_v<T, std::string> || std::is_same_v<T, const char *> ||
                      std::is_same_v<std::decay_t<T>, char *>) {
            if constexpr (std::is_same_v<T, std::string>) {
                sqlite3_bind_text(stmt, index, value.c_str(), -1, SQLITE_TRANSIENT);
            } else {
                sqlite3_bind_text(stmt, index, value, -1, SQLITE_TRANSIENT);
            }
        } else if constexpr (std::is_integral_v<T>) {
            sqlite3_bind_int(stmt, index, value);
        } else if constexpr (std::is_floating_point_v<T>) {
            sqlite3_bind_double(stmt, index, value);
        } else {
            static_assert([]() { return false; }(), "Unsupported type for bindValue");
        }
    }

    template<typename T>
    auto GetValue(sqlite3_stmt *stmt, int colIndex) {
        T t;
        auto datatype = sqlite3_column_type(stmt, colIndex);

        // 使用 remove_cvref_t 統一處理型別判斷
        using ValueT = std::remove_cvref_t<decltype(std::declval<T>())>;

        constexpr bool is_nullable =
                (std::is_assignable_v<ValueT &, std::nullptr_t> ||
                 std::is_constructible_v<ValueT, std::nullptr_t>) &&
                !std::is_same_v<ValueT, std::string> &&
                !std::is_same_v<ValueT, std::vector<uint8_t> >;

        // 明確先處理 SQLITE_NULL
        if (datatype == SQLITE_NULL) {
            if constexpr (is_nullable) {
                t = nullptr;
            } else if constexpr (std::is_same_v<ValueT, std::string>) {
                t.clear(); // 對於 string，NULL 用空字串表示
            } else if constexpr (std::is_same_v<ValueT, std::vector<uint8_t> >) {
                t.clear(); // 對於 vector，NULL 用空容器表示
            }
            // 其他型別保留預設值
            return t;
        }

        // 處理非 NULL 的情況
        if constexpr (std::is_same_v<T, std::string>) {
            if (datatype == SQLITE_TEXT) {
                t = reinterpret_cast<const char *>(sqlite3_column_text(stmt, colIndex));
            } else {
                // 型別不符，若支援 nullptr 則賦值，否則用預設值
                if constexpr (is_nullable) {
                    t = nullptr;
                } else if constexpr (std::is_same_v<ValueT, std::string>) {
                    t.clear();
                }
            }
        } else if constexpr (std::is_same_v<T, double>) {
            if (datatype == SQLITE_INTEGER || datatype == SQLITE_FLOAT) {
                t = sqlite3_column_double(stmt, colIndex);
            } else {
                if constexpr (is_nullable) {
                    t = nullptr;
                } else {
                    t = ValueT{}; // 預設值
                }
            }
        } else if constexpr (std::is_same_v<T, int>) {
            if (datatype == SQLITE_INTEGER) {
                t = sqlite3_column_int(stmt, colIndex);
            } else {
                if constexpr (is_nullable) {
                    t = nullptr;
                } else {
                    t = ValueT{}; // 預設值
                }
            }
        } else if constexpr (std::is_same_v<T, std::vector<uint8_t> >) {
            if (datatype == SQLITE_BLOB) {
                auto pBytes = static_cast<const uint8_t *>(sqlite3_column_blob(stmt, colIndex));
                auto n = static_cast<size_t>(sqlite3_column_bytes(stmt, colIndex));
                if (pBytes && n > 0) {
                    t = std::vector<uint8_t>(pBytes, pBytes + n);
                } else {
                    t.clear();
                }
            } else {
                if constexpr (is_nullable) {
                    t = nullptr;
                } else if constexpr (std::is_same_v<ValueT, std::vector<uint8_t> >) {
                    t.clear();
                }
            }
        }
        return t;
    }

    template<typename T, typename... Ts>
    std::tuple<T, Ts...> GetRowData(sqlite3_stmt *stmt, int colIndex = 0) {
        auto tuple = std::make_tuple(GetValue<T>(stmt, colIndex));
        if constexpr (sizeof...(Ts) == 0) {
            return tuple;
        } else {
            return std::tuple_cat(
                tuple,
                GetRowData<Ts...>(stmt, colIndex + 1)
            );
        }
    }

    using AutoStmtPtr = std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)>;

    static AutoStmtPtr MakeAutoStmtPtr() {
        return {nullptr, sqlite3_finalize};
    }

    static AutoStmtPtr MakeAutoStmtPtr(sqlite3_stmt *stmt) {
        return {stmt, sqlite3_finalize};
    }

    template<typename... Ts>
    class RowIterator {
        std::optional<std::reference_wrapper<AutoStmtPtr> > _pStmt;

    public:
        // 标准迭代器所需的类型别名
        using iterator_category = std::input_iterator_tag;
        using value_type = std::tuple<Ts...>;
        using difference_type = std::ptrdiff_t;
        using pointer = value_type *;
        using reference = value_type;

        explicit RowIterator(std::optional<std::reference_wrapper<AutoStmtPtr> > pStmt) : _pStmt(pStmt) {
        }

        std::tuple<Ts...> GetData() const {
            if (!_pStmt) {
                throw std::runtime_error("RowIterator does not have a valid AutoStmtPtr");
            }
            return GetRowData<Ts...>(_pStmt->get().get());
        }

        std::tuple<Ts...> operator*() {
            if (!_pStmt) {
                throw std::runtime_error("RowIterator does not have a valid AutoStmtPtr");
            }
            return GetRowData<Ts...>(_pStmt->get().get());
        }

        RowIterator &operator++() {
            if (!_pStmt) {
                throw std::runtime_error("RowIterator does not have a valid AutoStmtPtr");
            }
            if (sqlite3_step(_pStmt->get().get()) != SQLITE_ROW) {
                _pStmt.reset();
            }
            return *this;
        }

        bool operator==(const RowIterator &other) const {
            return _pStmt == std::nullopt ? other._pStmt == std::nullopt : other._pStmt != std::nullopt;
        }

        bool operator!=(const RowIterator &other) const {
            return !(*this == other);
        }

        template<size_t N>
        auto get() const {
            return GetValue<std::tuple_element_t<N, std::tuple<Ts...> > >(_pStmt->get().get(), N);
        }

        template<size_t N>
        friend auto get(RowIterator &it) {
            return it.get<N>();
        }
    };

    template<typename... Ts>
    class QueryResult {
        AutoStmtPtr _pStmt;
        bool isEmpty = false;
        bool _hasBeenIterated = false;

    public:
        explicit QueryResult(AutoStmtPtr &&pStmt) : _pStmt(std::move(pStmt)) {
            isEmpty = sqlite3_step(_pStmt.get()) != SQLITE_ROW;
        }

        RowIterator<Ts...> begin() {
            if (_hasBeenIterated) {
                throw std::runtime_error(
                    "QueryResult has already been iterated. Cannot call begin() or ToVector() multiple times.");
            }
            _hasBeenIterated = true;
            return RowIterator<Ts...>(isEmpty ? std::nullopt : std::make_optional(std::ref(_pStmt)));
        }

        RowIterator<Ts...> end() {
            return RowIterator<Ts...>(std::nullopt);
        }

        std::vector<std::tuple<Ts...> > ToVector() {
            return std::vector<std::tuple<Ts...> >(begin(), end());
        }

        std::list<std::tuple<Ts...> > ToList() {
            return std::list<std::tuple<Ts...> >(begin(), end());
        }
    };

    class SQLiteWrapper final {
    public:
        // Transaction 類別，使用 RAII 模式管理交易
        class Transaction {
        private:
            SQLiteWrapper &_sqlite;
            bool _isCommittedOrRolledBack = false;
            int _exceptionCount;

        public:
            explicit Transaction(SQLiteWrapper &sqlite)
                : _sqlite(sqlite), _exceptionCount(std::uncaught_exceptions()) {
                _sqlite.Execute("BEGIN TRANSACTION;");
            }

            ~Transaction() noexcept {
                if (_isCommittedOrRolledBack) {
                    return;
                }

                if (std::uncaught_exceptions() > _exceptionCount) {
                    try {
                        Rollback();
                    } catch (const std::exception &exception) {
                        std::cerr << exception.what() << std::endl;
                    }
                } else {
                    try {
                        Commit();
                    } catch (const std::exception &exception) {
                        std::cerr << exception.what() << std::endl;
                    }
                }
            }

            void Rollback() {
                if (_isCommittedOrRolledBack) {
                    throw std::runtime_error("Multiple commit/rollback calls on the same transaction");
                }
                _isCommittedOrRolledBack = true;
                _sqlite.Execute("ROLLBACK;");
            }

            void Commit() {
                if (_isCommittedOrRolledBack) {
                    throw std::runtime_error("Multiple commit/rollback calls on the same transaction");
                }
                _isCommittedOrRolledBack = true;
                _sqlite.Execute("COMMIT;");
            }
        };

    public:
        std::string _db_path;
        std::unique_ptr<sqlite3, decltype(&sqlite3_close)> _dbPtr = {nullptr, sqlite3_close};

        explicit SQLiteWrapper(const std::string &dbPath,
                               const int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE
        ) : _db_path(std::move(dbPath)) {
            sqlite3 *pDb = nullptr;
            if (sqlite3_open_v2(_db_path.c_str(), &pDb, flags, nullptr) != SQLITE_OK) {
                std::string errMsg = pDb ? sqlite3_errmsg(pDb) : "Unknown error";
                throw std::runtime_error("Can't open database: " + errMsg + "\nPath: " + _db_path);
            }
            _dbPtr = {pDb, sqlite3_close};
        }

        template<typename... ResultColumns, typename... Parameters>
        QueryResult<ResultColumns...> Query(const std::string &sql, Parameters... parameters) const {
            auto pAutoStmt = MakeAutoStmtPtr();
            {
                sqlite3_stmt *pStmt = nullptr;
                if (sqlite3_prepare_v2(_dbPtr.get(), sql.c_str(), -1, &pStmt, nullptr) != SQLITE_OK) {
                    throw std::runtime_error(
                        "Failed to prepare statement: " + std::string(sqlite3_errmsg(_dbPtr.get())) +
                        "\nSQL: " + sql);
                }
                pAutoStmt = MakeAutoStmtPtr(pStmt);
            }
            int index = 1;
            (BindValue(pAutoStmt.get(), index++, parameters), ...);
            return QueryResult<ResultColumns...>(std::move(pAutoStmt));
        }

        template<typename... Parameter>
        void Execute(const std::string &sql, const Parameter &... values) const {
            auto pAutoStmt = MakeAutoStmtPtr();
            {
                sqlite3_stmt *pStmt = nullptr;
                if (sqlite3_prepare_v2(_dbPtr.get(), sql.c_str(), -1, &pStmt, nullptr) != SQLITE_OK) {
                    throw std::runtime_error(
                        "Failed to prepare statement: " + std::string(sqlite3_errmsg(_dbPtr.get())) +
                        "\nSQL: " + sql);
                }
                pAutoStmt = MakeAutoStmtPtr(pStmt);
            }
            int index = 1;
            (BindValue(pAutoStmt.get(), index++, values), ...);
            if (sqlite3_step(pAutoStmt.get()) != SQLITE_DONE) {
                throw std::runtime_error(
                    "Failed to execute statement: " + std::string(sqlite3_errmsg(_dbPtr.get())) +
                    "\nSQL: " + sql);
            }
        }
    };
}

// std 命名空間特化，支援結構化綁定
namespace std {
    template<typename... Ts>
    struct tuple_size<TypeSQLite::RowIterator<Ts...> > : integral_constant<size_t, sizeof...(Ts)> {
    };

    template<size_t N, typename... Ts>
    struct tuple_element<N, TypeSQLite::RowIterator<Ts...> > {
        using type = typename tuple_element<N, tuple<Ts...> >::type;
    };
}
