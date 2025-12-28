#pragma once

#include <cmath>

#include <list>

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

#include <string>
#include <tuple>
#include <stdexcept>
#include <type_traits>

namespace TypeSQLite {
    enum class JoinType {
        FULL,
        INNER,
        LEFT,
        RIGHT,
        CROSS
    };

    template<JoinType jt>
    constexpr auto GetJoinTypeString() {
        if constexpr (jt == JoinType::FULL) {
            return " FULL JOIN ";
        } else if constexpr (jt == JoinType::INNER) {
            return " INNER JOIN ";
        } else if constexpr (jt == JoinType::LEFT) {
            return " LEFT JOIN ";
        } else if constexpr (jt == JoinType::RIGHT) {
            return " RIGHT JOIN ";
        } else if constexpr (jt == JoinType::CROSS) {
            return " CROSS JOIN ";
        }
        throw std::runtime_error("Unsupported join type");
    }

    inline std::string GetJoinTypeString(const JoinType &jt) {
        switch (jt) {
            case JoinType::FULL:
                return " FULL JOIN ";
            case JoinType::INNER:
                return " INNER JOIN ";
            case JoinType::LEFT:
                return " LEFT JOIN ";
            case JoinType::RIGHT:
                return " RIGHT JOIN ";
            case JoinType::CROSS:
                return " CROSS JOIN ";
            default:
                throw std::runtime_error("Unsupported join type");
        }
    }

    template<typename Src, typename Cond>
    struct DataSource {
        using Source = Src;
        JoinType type;
        Cond condition;
    };

    template<typename MainSrc, typename... JoinSrcs>
    struct SourceInfo {
        using Source = MainSrc;
        std::tuple<JoinSrcs...> joins;
    };

    template<typename>
    struct IsSourceInfo : std::false_type {
    };

    template<typename MainSrc, typename... JoinSrcs>
    struct IsSourceInfo<SourceInfo<MainSrc, JoinSrcs...> > : std::true_type {
    };

    template<typename T>
    concept SourceInfoConcept = IsSourceInfo<T>::value;

    template<typename MainSrc, typename... JoinSrcs, typename NewJoin>
    auto JoinSource(SourceInfo<MainSrc, JoinSrcs...> src, NewJoin join) {
        using NewType = SourceInfo<MainSrc, JoinSrcs..., NewJoin>;
        return NewType{
            .joins = std::tuple_cat(src.joins, std::make_tuple(std::move(join)))
        };
    }

    template<typename MainSrc, typename... Joins>
    std::string MakeSourceSQL(const SourceInfo<MainSrc, Joins...> &src) {
        std::string sql = std::string(MainSrc::name);
        std::apply([&](const auto &... join) {
            // 展開每個 JoinInfo，使用 expr.sql
            ((sql += GetJoinTypeString(join.type)
              + std::string(std::remove_cvref_t<decltype(join)>::Source::name)
              + " ON " + std::string(join.condition.sql)), ...);
        }, src.joins);
        return sql;
    }

    // 提取 SourceInfo 中所有 JOIN 條件的參數
    template<typename MainSrc, typename... Joins>
    auto GetExtractSourceParams(const SourceInfo<MainSrc, Joins...> &src) {
        return std::apply([](const auto &... join) {
            return std::tuple_cat(join.condition.params...);
        }, src.joins);
    }

    template<typename MainSrc, typename... Joins>
    auto GetExtractSourceCols(const SourceInfo<MainSrc, Joins...> &src) {
        return std::apply([](const auto &... join) {
            return std::tuple_cat(GetCols(join.condition)...);
        }, src.joins);
    }

    // 特化：沒有 JOIN 時返回空 tuple
    template<typename MainSrc>
    auto GetExtractSourceParams(const SourceInfo<MainSrc> &src) {
        return std::tuple<>();
    }

    template<typename MainSrc>
    auto GetExtractSourceCols(const SourceInfo<MainSrc> &src) {
        return std::tuple<>();
    }
}

template<size_t N = 0>
struct FixedString {
    char value[N];

    constexpr FixedString(const char (&str)[N]) : value{} {
        for (size_t i = 0; i < N; ++i)
            value[i] = str[i];
    }

    constexpr operator std::string_view() const {
        return {value, N - 1};
    }
};

template<std::size_t N1, std::size_t N2>
constexpr auto operator+(const FixedString<N1> &a, const FixedString<N2> &b) {
    char result[N1 + N2 - 1]{};
    std::copy_n(a.value, N1 - 1, result);
    std::copy_n(b.value, N2, result + N1 - 1);
    return FixedString<N1 + N2 - 1>(result);
}

template<std::size_t N1, std::size_t N2>
constexpr auto operator+(const FixedString<N1> &a, const char (&b)[N2]) {
    char result[N1 + N2 - 1]{};
    std::copy_n(a.value, N1 - 1, result);
    std::copy_n(b, N2, result + N1 - 1);
    return FixedString<N1 + N2 - 1>(result);
}

template<std::size_t N1, std::size_t N2>
constexpr auto operator+(const char (&a)[N1], const FixedString<N2> &b) {
    char result[N1 + N2 - 1]{};
    std::copy_n(a, N1 - 1, result);
    std::copy_n(b.value, N2, result + N1 - 1);
    return FixedString<N1 + N2 - 1>(result);
}

template<std::size_t N>
std::string operator+(const FixedString<N> &a, const std::string &b) {
    return std::string(a.value) + b;
}

template<std::size_t N>
std::string operator+(const std::string &a, const FixedString<N> &b) {
    return a + std::string(b.value);
}

template<int num>
constexpr auto toFixedString() {
    if constexpr (num < 0) {
        constexpr char arr[2] = {'0' + (num * -1 % 10), '\0'};
        if constexpr (num <= -10) {
            return "-" + toFixedString<-(num / 10)>() + FixedString(arr);
        } else {
            return "-" + FixedString(arr);
        }
    } else {
        char arr[2] = {'0' + (num % 10), '\0'};
        if constexpr (num >= 10) {
            return toFixedString<num / 10>() + FixedString(arr);
        } else {
            return FixedString(arr);
        }
    }
}

template<double Fraction>
constexpr auto FractionToFixedString() {
    // 如果剩餘的小數部分非常小，就停止遞迴
    if constexpr (Fraction < std::numeric_limits<double>::epsilon()) {
        return FixedString("");
    } else {
        constexpr double scaled = Fraction * 10; // 取出下一位小數
        return toFixedString<(int) scaled>() // 加上整數部分
               + FractionToFixedString<scaled - (int) scaled>(); // 繼續處理剩餘小數
    }
}

template<double Number>
constexpr auto toFixedString() {
    constexpr int IntegerPart = Number; // 整數部分
    constexpr double FractionPart = Number - IntegerPart; // 小數部分

    if constexpr (FractionPart < std::numeric_limits<double>::epsilon()) {
        // 沒有小數部分 → 僅輸出整數
        return toFixedString<IntegerPart>();
    } else {
        // 有小數 → 整數 + "." + 小數遞迴轉換
        return toFixedString<IntegerPart>()
               + "."
               + FractionToFixedString<FractionPart>();
    }
}

template<typename T>
    struct IsFixedString : std::false_type {
};

template<size_t N>
struct IsFixedString<FixedString<N> > : std::true_type {
};

namespace TypeSQLite {
    enum class OrderType {
        ASC,
        DESC
    };

    template<OrderType order>
    constexpr auto OrderTypeToString() {
        if constexpr (order == OrderType::ASC) {
            return FixedString(" ASC");
        } else {
            return FixedString(" DESC");
        }
    }

    std::string OrderTypeToString(OrderType order) {
        switch (order) {
            case OrderType::ASC:
                return " ASC";
            case OrderType::DESC:
                return " DESC";
            default:
                throw std::invalid_argument("Invalid OrderType");
        }
    }
}

namespace TypeSQLite {
    enum class ConflictCause {
        ROLLBACK,
        ABORT,
        FAIL,
        IGNORE,
        REPLACE
    };

    template<ConflictCause Cause>
    constexpr auto ConflictCauseToString() {
        if constexpr (Cause == ConflictCause::ROLLBACK) {
            return FixedString(" ON CONFLICT ROLLBACK");
        } else if constexpr (Cause == ConflictCause::ABORT) {
            return FixedString(" ON CONFLICT ABORT");
        } else if constexpr (Cause == ConflictCause::FAIL) {
            return FixedString(" ON CONFLICT FAIL");
        } else if constexpr (Cause == ConflictCause::IGNORE) {
            return FixedString(" ON CONFLICT IGNORE");
        } else if constexpr (Cause == ConflictCause::REPLACE) {
            return FixedString(" ON CONFLICT REPLACE");
        }
    }

    inline std::string ConflictCauseToString(ConflictCause Cause) {
        switch (Cause) {
            case ConflictCause::ROLLBACK:
                return " ON CONFLICT ROLLBACK";
            case ConflictCause::ABORT:
                return " ON CONFLICT ABORT";
            case ConflictCause::FAIL:
                return " ON CONFLICT FAIL";
            case ConflictCause::IGNORE:
                return " ON CONFLICT IGNORE";
            case ConflictCause::REPLACE:
                return " ON CONFLICT REPLACE";
            default:
                throw std::invalid_argument("ConflictCause is invalid");
        }
    }
}

#include <type_traits>

template<typename T, size_t N = 0>
struct FixedType {
    using SaveType = std::conditional_t<std::is_same_v<char, T> && N != 0, FixedString<N>, T>;
    SaveType value;

    constexpr FixedType(T u) : value(u) {
    }

    constexpr FixedType(const T (&str)[N]) : value(str) {
    }
};

template<typename... Ts>
    struct TypeGroup;

template<typename T, typename... Ts>
struct TypeGroup<T, Ts...> {
    using type = T;
    using next = TypeGroup<Ts...>;
};

template<>
struct TypeGroup<> {
    using type = void;
};

template<typename>
struct IsTypeGroup : std::false_type {
};

template<typename... Ts>
struct IsTypeGroup<TypeGroup<Ts...> > : std::true_type {
};

template<>
struct IsTypeGroup<TypeGroup<> > : std::true_type {
};

template<typename T>
concept TypeGroupConcept = IsTypeGroup<T>::value;

template<typename G1, typename G2>
    struct ConcatTypeGroup;

template<typename... Ts1, typename... Ts2>
struct ConcatTypeGroup<TypeGroup<Ts1...>, TypeGroup<Ts2...> > {
    using type = TypeGroup<Ts1..., Ts2...>;
};

template<typename T, typename TG>
    constexpr bool FindTypeInTypeGroup() {
    if constexpr (std::is_same_v<T, typename TG::type>) {
        return true;
    } else if constexpr (!std::is_same_v<typename TG::next, TypeGroup<> >) {
        return FindTypeInTypeGroup<T, typename TG::next>();
    } else {
        return false;
    }
}

template<typename TG1, typename TG2>
constexpr bool IsTypeGroupSubset() {
    if constexpr (std::is_void_v<typename TG1::type>) {
        return true; // 空的 TG1 是任何 TG2 的子集
    } else {
        if constexpr (FindTypeInTypeGroup<typename TG1::type, TG2>()) {
            if constexpr (!std::is_same_v<typename TG1::next, TypeGroup<> >) {
                return IsTypeGroupSubset<typename TG1::next, TG2>();
            } else {
                return true; // 已檢查完 TG1 的所有型別
            }
        } else {
            return false; // 找不到 TG1 的型別於 TG2 中
        }
    }
}

namespace TypeSQLite {
    template<OrderType order = OrderType::ASC, ConflictCause conflictCause = ConflictCause::ABORT, bool autoIncrement =
            false>
    struct ColumnPrimaryKey {
        constexpr static FixedString value = [] {
            if constexpr (autoIncrement) {
                return FixedString("PRIMARY KEY") + OrderTypeToString<order>() + ConflictCauseToString<conflictCause>()
                       + FixedString(" AUTOINCREMENT");
            } else {
                return FixedString("PRIMARY KEY") + OrderTypeToString<order>() + ConflictCauseToString<conflictCause>();
            }
        }();
    };

    template<ConflictCause conflictCause = ConflictCause::ABORT>
    struct ColumnNotNull {
        constexpr static FixedString value = "NOT NULL" + ConflictCauseToString<conflictCause>();
    };

    template<ConflictCause conflictCause = ConflictCause::ABORT>
    struct ColumnUnique {
        constexpr static FixedString value = "UNIQUE" + ConflictCauseToString<conflictCause>();
    };

    // Helper to convert FixedString to a quoted string for DEFAULT
    template<size_t N>
    constexpr auto toFixedStringLiteral(const FixedString<N> &fs) {
        return FixedString("'") + fs + FixedString("'");
    }

    template<FixedType DefaultValue>
    struct Default {
        constexpr static auto GetDefaultValueString() {
            using ValueType = decltype(DefaultValue.value);
            if constexpr (IsFixedString<ValueType>::value) {
                // For string types, wrap in quotes
                return FixedString("DEFAULT ") + toFixedStringLiteral(DefaultValue.value);
            } else if constexpr (std::is_integral_v<ValueType>) {
                // For integer types
                return FixedString("DEFAULT ") + toFixedString<DefaultValue.value>();
            } else if constexpr (std::is_floating_point_v<ValueType>) {
                // For floating point types
                return FixedString("DEFAULT ") + toFixedString<DefaultValue.value>();
            } else {
                return FixedString("DEFAULT ") + toFixedString<DefaultValue.value>();
            }
        }

        constexpr static auto value = GetDefaultValueString();
    };

    template<typename>
    struct IsColumnConstraint : std::false_type {
    };

    template<OrderType order, ConflictCause conflictCause, bool autoIncrement>
    struct IsColumnConstraint<ColumnPrimaryKey<order, conflictCause, autoIncrement> > : std::true_type {
    };

    template<ConflictCause conflictCause>
    struct IsColumnConstraint<ColumnNotNull<conflictCause> > : std::true_type {
    };

    template<ConflictCause conflictCause>
    struct IsColumnConstraint<ColumnUnique<conflictCause> > : std::true_type {
    };

    template<FixedType fixType>
    struct IsColumnConstraint<Default<fixType> > : std::true_type {
    };

    template<typename T>
    concept ColumnConstraintConcept = IsColumnConstraint<T>::value;

    template<typename>
    struct IsColumnConstraintGroup : std::false_type {
    };

    template<ColumnConstraintConcept ... Constraints>
    struct IsColumnConstraintGroup<TypeGroup<Constraints...> > : std::true_type {
    };

    template<typename TG>
    concept ColumnConstraintGroupConcept = IsColumnConstraintGroup<TG>::value;

    template<ColumnConstraintGroupConcept TG>
    constexpr auto GetColumnConstraintsSQL() {
        if constexpr (std::is_same_v<TG, TypeGroup<> >) {
            return FixedString(" ");
        } else if constexpr (!std::is_same_v<typename TG::next, TypeGroup<> >) {
            return " " + TG::type::value + GetColumnConstraintsSQL<typename TG::next>();
        } else {
            return " " + TG::type::value;
        }
    }
}

namespace TypeSQLite {
    //TODO 分離ExprType以及colType
    enum class DataType {
        TEXT,
        NUMERIC,
        INTEGER,
        REAL,
        BLOB,
        MULTY_TYPE,
    };
    template<DataType type>
    constexpr auto DataTypeToString() {
        switch (type) {
            case DataType::TEXT:
                return "TEXT";
            case DataType::NUMERIC:
                return "NUMERIC";
            case DataType::INTEGER:
                return "INTEGER";
            case DataType::REAL:
                return "REAL";
            case DataType::BLOB:
                return "BLOB";
            default:
                return "UNKNOWN";
        }
    }
}

namespace TypeSQLite {

    template<FixedString Name, DataType Type, ColumnConstraintConcept... Constraints>
    struct Column {
        constexpr static FixedString name = Name;
        constexpr static DataType type = Type;
        constexpr static DataType resultType = Type;
        const std::string sql = std::string(Name);
        using constraints = TypeGroup<Constraints...>;
    };

    template<typename>
    struct IsColumn : std::false_type {
    };

    template<FixedString Name, DataType Type, typename... Constraints>
    struct IsColumn<Column<Name, Type, Constraints...> > : std::true_type {
    };

    template<typename T>
    concept ColumnConcept = IsColumn<T>::value;

    template<typename T, ColumnConcept U>
    struct TableColumn_Base : U {
        using TableType = T;
        const std::string sql = std::string(T::name) + "." + U::name;
    };

    template<typename>
    struct IsTableColumn : std::false_type {
    };

    template<typename T, ColumnConcept U>
    struct IsTableColumn<TableColumn_Base<T, U> > : std::true_type {
    };

    template<typename T>
    concept TableColumnConcept = IsTableColumn<T>::value;

    template<typename T>
    concept ColumnOrTableColumnConcept = TableColumnConcept<T> || ColumnConcept<T>;

    //TODO 暫時先放寬約束
    template<typename/*ColumnOrTableColumnConcept*/ T>
    constexpr auto GetColumnName(T t) {
        return t.sql;
        // if constexpr (TableColumnConcept<T>) {
        //     return T::TableType::name + FixedString(".") + T::name;
        // } else {
        //     return T::name;
        // }
    }

    //TODO 暫時先放寬約束
    template<typename/*ColumnOrTableColumnConcept*/ T, typename/*ColumnOrTableColumnConcept*/... Ts>
    constexpr auto GetColumnNames(T t, Ts... ts) {
        if constexpr (sizeof...(Ts) == 0) {
            return GetColumnName(t);
        } else {
            return GetColumnName(t) + "," + GetColumnNames(ts...);
        }
    }

    template<ColumnOrTableColumnConcept T, ColumnOrTableColumnConcept... Ts>
    std::string GetColumnNamesWithOutTableName() {
        if constexpr (sizeof...(Ts) == 0) {
            return std::string(T::name);
        } else {
            return std::string(T::name) + "," + GetColumnNamesWithOutTableName<Ts...>();
        }
    }

    template<typename Column>
    constexpr auto GetColumnDefinition() {
        return FixedString(" " + Column::name + " ") +
               DataTypeToString<Column::type>() +
               GetColumnConstraintsSQL<typename Column::constraints>();
    }

    template<ColumnConcept... Columns>
    std::string GetColumnDefinitions() {
        std::string result;
        ((result += GetColumnDefinition<Columns>() + ","), ...);
        if (!result.empty()) result.pop_back(); // 去掉最後一個逗號
        return result;
    }
}

// cpp

#include <tuple>
#include <type_traits>
#include <concepts>
#include <string>

//TODO 由於JsonFunction實作較複雜暫時不實作
namespace TypeSQLite {
    template<typename ReturnType, typename Columns, typename Parameters>
    struct Expressions {
        using returnType = ReturnType;
        const Columns cols;
        const std::string sql;
        const Parameters params;
    };

    template<typename T>
    concept ExpressionsConcept = requires(T t)
    {
        typename T::returnType;
        { t.cols };
        { t.sql } -> std::convertible_to<std::string>;
        { t.params };
    } && (std::derived_from<T, Expressions<typename T::returnType, std::decay_t<decltype(std::declval<T>().cols)>,
        std::decay_t<
            decltype(std::declval<T>().params)> > >);

    template<typename T>
    concept NullAbleExpr = ExpressionsConcept<T> && std::is_same_v<T, std::nullptr_t>;

    template<typename T>
    concept ExprOrColConcept = ExpressionsConcept<T> || ColumnOrTableColumnConcept<T>;

    template<typename T>
    concept NullAbleExprOrColConcept = ExprOrColConcept<T> || std::is_same_v<T, nullptr_t>;

    template<ExprOrColConcept T>
    auto GetCols(const T &t) {
        if constexpr (ColumnOrTableColumnConcept<T>) {
            return std::make_tuple(t);
        } else {
            return t.cols;
        }
    }

    template<ExprOrColConcept T>
    auto GetParms(const T &t) {
        if constexpr (ColumnOrTableColumnConcept<T>) {
            return std::tuple<>{};
        } else {
            return t.params;
        }
    }

    template<typename returnType, ExprOrColConcept ...Exps>
    auto MakeExpr(std::string newSQL, Exps... exps) {
        auto newCols = std::tuple_cat(GetCols(exps)...);
        auto newPara = std::tuple_cat(GetParms(exps)...);
        return Expressions<returnType, decltype(newCols), decltype(newPara)>{
            .cols = newCols,
            .sql = newSQL,
            .params = newPara
        };
    }

    //ExprOrColReturnType
    //TODO 移到Column.hpp
    template<typename expr>
    using ColumnReturnType = std::conditional_t<expr::resultType == DataType::TEXT, std::string,
        std::conditional_t<expr::resultType == DataType::NUMERIC, double,
            std::conditional_t<expr::resultType == DataType::INTEGER, int,
                std::conditional_t<expr::resultType == DataType::REAL, double,
                    std::vector<uint8_t> > > > >;

    template<typename expr>
    struct ExprOrColReturnTypeImpl{
        using type = expr::returnType;
    };

    template<ColumnOrTableColumnConcept col>
    struct ExprOrColReturnTypeImpl<col> {
        using type = ColumnReturnType<col>;
    };
    template<typename T>
    using ExprOrColReturnType = typename ExprOrColReturnTypeImpl<T>::type;

    // sqlite literal
    inline auto operator""_expr(const char *str, size_t) {
        return Expressions<std::string, std::tuple<>, std::tuple<std::string> >{
            .cols = std::tuple<>{},
            .sql = "?",
            .params = std::make_tuple(std::string(str))
        };
    }

    inline auto operator""_expr(const long double value) {
        return Expressions<double, std::tuple<>, std::tuple<double> >{
            .cols = std::tuple<>{},
            .sql = "?",
            .params = std::make_tuple(static_cast<double>(value))
        };
    }

    inline auto operator""_expr(const unsigned long long value) {
        return Expressions<int, std::tuple<>, std::tuple<double> >{
            .cols = std::tuple<>{},
            .sql = "?",
            .params = std::make_tuple(static_cast<double>(value))
        };
    }

    template<ExprOrColConcept expr>
    auto operator-(const expr &e) {
        return MakeExpr<double>("-(" + e.sql + ")", e);
    }

    template<ExprOrColConcept expr>
    auto operator+(const expr &e) {
        return MakeExpr<double>("+(" + e.sql + ")", e);
    }

    template<ExprOrColConcept expr>
    auto operator!(const expr &e) {
        return MakeExpr<double>("NOT (" + e.sql + ")", e);
    }

    template<ExprOrColConcept expr>
    auto operator~(const expr &e) {
        return MakeExpr<double>("~ (" + e.sql + ")", e);
    }

    template<ExprOrColConcept expr>
    auto Brackets(const expr &e) {
        return MakeExpr<double>("(" + e.sql + ")", e);
    }

    template<ExprOrColConcept lhs, ExprOrColConcept rhs>
    auto operator+(const lhs &left, const rhs &right) {
        return MakeExpr<double>(left.sql + " + " + right.sql, left, right);
    }

    template<ExprOrColConcept lhs, ExprOrColConcept rhs>
    auto operator-(const lhs &left, const rhs &right) {
        return MakeExpr<double>(left.sql + " - " + right.sql, left, right);
    }

    template<ExprOrColConcept lhs, ExprOrColConcept rhs>
    auto operator*(const lhs &left, const rhs &right) {
        return MakeExpr<double>(left.sql + " * " + right.sql, left, right);
    }

    template<ExprOrColConcept lhs, ExprOrColConcept rhs>
    auto operator/(const lhs &left, const rhs &right) {
        return MakeExpr<double>(left.sql + " / " + right.sql, left, right);
    }

    template<ExprOrColConcept lhs, ExprOrColConcept rhs>
    auto operator%(const lhs &left, const rhs &right) {
        return MakeExpr<double>(left.sql + " % " + right.sql, left, right);
    }

    template<ExprOrColConcept lhs, ExprOrColConcept rhs>
    auto operator^(const lhs &left, const rhs &right) {
        return MakeExpr<double>(left.sql + " ^ " + right.sql, left, right);
    }

    template<ExprOrColConcept lhs, ExprOrColConcept rhs>
    auto operator&&(const lhs &left, const rhs &right) {
        return MakeExpr<double>("(" + left.sql + ") AND (" + right.sql + ")", left, right);
    }

    template<ExprOrColConcept lhs, ExprOrColConcept rhs>
    auto operator||(const lhs &left, const rhs &right) {
        return MakeExpr<double>("(" + left.sql + ") OR (" + right.sql + ")", left, right);
    }

    template<ExprOrColConcept lhs, ExprOrColConcept rhs>
    auto operator&(const lhs &left, const rhs &right) {
        return MakeExpr<double>(left.sql + " & " + right.sql, left, right);
    }

    template<ExprOrColConcept lhs, ExprOrColConcept rhs>
    auto operator|(const lhs &left, const rhs &right) {
        return MakeExpr<double>(left.sql + " | " + right.sql, left, right);
    }

    template<ExprOrColConcept lhs, ExprOrColConcept rhs>
    auto operator==(const lhs &left, const rhs &right) {
        return MakeExpr<double>(left.sql + " = " + right.sql, left, right);
    }

    template<ExprOrColConcept lhs, ExprOrColConcept rhs>
    auto operator!=(const lhs &left, const rhs &right) {
        return MakeExpr<double>(left.sql + " <> " + right.sql, left, right);
    }

    template<ExprOrColConcept lhs, ExprOrColConcept rhs>
    auto operator<(const lhs &left, const rhs &right) {
        return MakeExpr<double>(left.sql + " < " + right.sql, left, right);
    }

    template<ExprOrColConcept lhs, ExprOrColConcept rhs>
    auto operator<=(const lhs &left, const rhs &right) {
        return MakeExpr<double>(left.sql + " <= " + right.sql, left, right);
    }

    template<ExprOrColConcept lhs, ExprOrColConcept rhs>
    auto operator>(const lhs &left, const rhs &right) {
        return MakeExpr<double>(left.sql + " > " + right.sql, left, right);
    }

    template<ExprOrColConcept lhs, ExprOrColConcept rhs>
    auto operator>=(const lhs &left, const rhs &right) {
        return MakeExpr<double>(left.sql + " >= " + right.sql, left, right);
    }

    template<ExprOrColConcept lhs, ExprOrColConcept rhs>
    auto operator<<(const lhs &left, const rhs &right) {
        return MakeExpr<double>(left.sql + " << " + right.sql, left, right);
    }

    template<ExprOrColConcept lhs, ExprOrColConcept rhs>
    auto operator>>(const lhs &left, const rhs &right) {
        return MakeExpr<double>(left.sql + " >> " + right.sql, left, right);
    }

    // Note: REGEXP and MATCH are pattern matching operators
    template<ExprOrColConcept lhs, ExprOrColConcept rhs>
    auto Regexp(const lhs &left, const rhs &right) {
        return MakeExpr<double>(left.sql + " REGEXP " + right.sql, left, right);
    }

    template<ExprOrColConcept lhs, ExprOrColConcept rhs>
    auto Match(const lhs &left, const rhs &right) {
        return MakeExpr<double>(left.sql + " MATCH " + right.sql, left, right);
    }

    //TODO 確認In功能

    template<ExprOrColConcept Lhs, ExprOrColConcept Mid, ExprOrColConcept Rhs>
    auto Between(const Lhs &left, const Mid &mid, const Rhs &right) {
        constexpr auto newSQL = left.sql + " BETWEEN " + mid.sql + " AND " + right.sql;
        auto newCols = std::tuple_cat(left.cols, mid.cols, right.cols);
        auto newPara = std::tuple_cat(left.params, mid.params, right.params);
        return Expressions<int, decltype(newCols), decltype(newPara)>{
            .cols = newCols,
            .sql = newSQL,
            .params = newPara
        };
    }

    // CAST - Type conversion
    template<DataType targetType, ExprOrColConcept T>
    auto Cast(const T &expr) {
        std::string typeStr;
        if constexpr (targetType == DataType::INTEGER) {
            return MakeExpr<int>("CAST(" + expr.sql + " AS INTEGER)", expr);
        } else if constexpr (targetType == DataType::REAL) {
            return MakeExpr<double>("CAST(" + expr.sql + " AS REAL)", expr);
        } else if constexpr (targetType == DataType::TEXT) {
            return MakeExpr<std::string>("CAST(" + expr.sql + " AS TEXT)", expr);
        } else if constexpr (targetType == DataType::BLOB) {
            return MakeExpr<std::vector<uint8_t> >("CAST(" + expr.sql + " AS BLOB)", expr);
        } else if constexpr (targetType == DataType::NUMERIC) {
            return MakeExpr<double>("CAST(" + expr.sql + " AS NUMERIC)", expr);
        } else {
            static_assert(targetType != targetType, "Unsupported target type for Cast");
        }
    }

    template<typename /*ExprOrColConcept*/ expr, typename/*ExprOrColConcept*/... exprs>
    auto GetExprSqls(const expr &first, const exprs &... rest) {
        if constexpr (sizeof...(rest) == 0) {
            return first.sql;
        } else {
            return first.sql + ", " + GetExprSqls(rest...);
        }
    }

    template<typename/*NullAbleExprOrColConcept*/ expr, typename/*ExprOrColConcept*/... exprs>
    auto GetExprsColTuple(const expr &first, const exprs &... rest) {
        if constexpr (sizeof...(rest) == 0) {
            if constexpr (std::is_same_v<expr, std::nullptr_t>) {
                return std::tuple();
            } else if constexpr (ColumnOrTableColumnConcept<expr>) {
                return std::make_tuple(first);
            } else {
                return first.cols;
            }
        } else {
            if constexpr (ColumnOrTableColumnConcept<expr>) {
                return std::tuple_cat(std::make_tuple(first), GetExprsColTuple(rest...));
            } else {
                return std::tuple_cat(first.cols, GetExprsColTuple(rest...));
            }
        }
    }

    template<typename ExprTuple>
    auto GetExprsTupleColTuple(const ExprTuple &exprs) {
        if constexpr (std::is_same_v<ExprTuple, nullptr_t>) {
            return std::tuple();
        } else {
            return std::apply(
                [](auto... expr) {
                    return GetExprsColTuple(expr...);
                },
                exprs
            );
        }
    }

    template<typename/*NullAbleExprOrColConcept*/ expr, typename/*ExprOrColConcept*/... exprs>
    auto GetExprsParamTuple(const expr &first, const exprs &... rest) {
        if constexpr (sizeof...(rest) == 0) {
            if constexpr (std::is_same_v<expr, std::nullptr_t>) {
                return std::tuple();
            } else if constexpr (ColumnOrTableColumnConcept<expr>) {
                return std::tuple<>{};
            } else {
                return first.params;
            }
        } else {
            if constexpr (ColumnOrTableColumnConcept<expr>) {
                return GetExprsParamTuple(rest...);
            } else {
                return std::tuple_cat(first.params, GetExprsParamTuple(rest...));
            }
        }
    }

    template<typename ExprTuple>
    auto GetExprsTupleParamTuple(const ExprTuple &exprs) {
        if constexpr (std::is_same_v<ExprTuple, nullptr_t>) {
            return std::tuple();
        } else {
            return std::apply(
                [](auto... expr) {
                    return GetExprsParamTuple(expr...);
                },
                exprs
            );
        }
    }
}

namespace TypeSQLite {
    template<typename Cols, SourceInfoConcept Src>
    class SelectAble;

    template<typename>
    struct IsQueryAble : std::false_type {
    };

    template<typename Cols, SourceInfoConcept Src>
    struct IsQueryAble<SelectAble<Cols, Src> > : std::true_type {
    };

    template<typename T>
    concept ConvertToQueryAbleConcept = requires(T *p)
    {
        []<typename... Args>(const SelectAble<Args...> *) {
        }(p);
    } || IsQueryAble<T>::value;

    template<typename Cols>
    constexpr static DataType GetSelectResultDataType() {
        if constexpr (std::tuple_size_v<Cols> == 1) {
            return std::tuple_element_t<0, Cols>::resultType;
        } else {
            return DataType::MULTY_TYPE;
        }
    }

    template<typename>
    struct ReturnTypesImpl;

    template<typename ... Cols>
    struct ReturnTypesImpl<std::tuple<Cols...> > {
        using type = std::conditional_t<std::tuple_size_v<std::tuple<Cols...>> == 1, ExprOrColReturnType<std::tuple_element_t<0, std::tuple<Cols...>> >,
        std::tuple<ExprOrColReturnType<Cols>...> >;
    };

    template<typename Cols>
    using ReturnTypes = typename ReturnTypesImpl<Cols>::type;

    //TODO GroupBy是ExpressionsConcept tuple
    template<
        typename Source,
        typename Where,
        typename GroupBy,
        typename OrderExpr,
        typename... ResultColumns>
    struct SelectStatementInfo {
        Source source;
        Where where;
        GroupBy groupBy;
        OrderExpr orderExpr;
        std::tuple<ResultColumns...> resultColumns;
        OrderType orderType = OrderType::ASC;
        //TODO 支援express limit offset
        std::optional<std::pair<int, int> > limitOffset;
        bool isDistinct;
    };

    template<
        typename Source,
        typename Where,
        typename GroupBy,
        typename OrderExpr,
        typename... ResultColumns>
    auto MakeSelectStatementInfo(
        Source source,
        Where where,
        GroupBy groupBy,
        OrderExpr orderExpr,
        OrderType orderType,
        const std::optional<std::pair<int, int> > &limitOffset,
        bool isDistinct,
        ResultColumns... columns
    ) {
        return SelectStatementInfo<Source, Where, GroupBy, OrderExpr, ResultColumns...>{
            .source = source,
            .where = where,
            .groupBy = groupBy,
            .orderExpr = orderExpr,
            .resultColumns = std::make_tuple(columns...),
            .orderType = orderType,
            .limitOffset = limitOffset,
            .isDistinct = isDistinct
        };
    }

    template<typename Info>
    std::string GetInfoSql(const Info &info) {
        auto sql = std::string("SELECT ") + (info.isDistinct ? "DISTINCT " : "") +
                   std::apply([](auto &&... results) { return GetExprSqls(results...); }, info.resultColumns)
                   + " FROM "
                   + MakeSourceSQL(info.source);
        if constexpr (!std::is_null_pointer_v<decltype(info.where)>) {
            sql += " WHERE " + info.where.sql;
        }
        if constexpr (!std::is_null_pointer_v<decltype(info.groupBy)>) {
            sql += " GROUP BY " + std::apply([](auto &&... expr) { return GetExprSqls(expr...); }, info.groupBy);
        }
        if constexpr (!std::is_null_pointer_v<decltype(info.orderExpr)>) {
            sql += " ORDER BY " + info.orderExpr.sql + " " + OrderTypeToString(info.orderType);
        }
        if (info.limitOffset.has_value()) {
            sql += " LIMIT " + std::to_string(info.limitOffset->first);
            if (info.limitOffset->second > 0) {
                sql += " OFFSET " + std::to_string(info.limitOffset->second);
            }
        }
        return sql;
    };

    template<typename Info>
    auto GetSelectInfoCols(const Info &info) {
        return std::tuple_cat(
            GetExprsTupleColTuple(info.resultColumns),
            GetExtractSourceCols(info.source),
            GetExprsColTuple(info.where),
            GetExprsTupleColTuple(info.groupBy),
            GetExprsColTuple(info.orderExpr)
        );
    };

    template<typename Info>
    auto GetSelectInfoParams(const Info &info) {
        return std::tuple_cat(
            GetExprsTupleParamTuple(info.resultColumns),
            GetExtractSourceParams(info.source),
            GetExprsParamTuple(info.where),
            GetExprsTupleParamTuple(info.groupBy),
            GetExprsParamTuple(info.orderExpr)
        );
    };

    template<typename Cols, SourceInfoConcept Source>
    class SelectAble {
    public:
        const Cols columns;

    protected:
        SQLiteWrapper &_sqlite;
        const Source _source;

        template<typename Info>
        class [[nodiscard("You must call Result() for the query to run.")]]
                SelectStatement
                : public Expressions<
                    ReturnTypes<decltype(std::declval<Info>().resultColumns)>,
                    decltype(GetSelectInfoCols(std::declval<Info>())),
                    decltype(GetSelectInfoParams(std::declval<Info>()))
                > {
            const SQLiteWrapper &_sqlite;
            Info _info;

        public:
            explicit SelectStatement(const SQLiteWrapper &sqlite, Info info)
                : Expressions<
                      ReturnTypes<decltype(std::declval<Info>().resultColumns)>,
                      decltype(GetSelectInfoCols(std::declval<Info>())),
                      decltype(GetSelectInfoParams(std::declval<Info>()))
                  >{
                      .cols = GetSelectInfoCols(info),
                      .sql = "(" + GetInfoSql(info) + ")",
                      .params = GetSelectInfoParams(info)
                  },
                  _sqlite(sqlite),
                  _info(info) {
            }

            template<ExprOrColConcept Expr>
            auto Where(const Expr &expr) {
                auto info = std::apply([this,&expr](auto... results) {
                    return MakeSelectStatementInfo(
                        _info.source,
                        expr,
                        _info.groupBy,
                        _info.orderExpr,
                        _info.orderType,
                        _info.limitOffset,
                        _info.isDistinct,
                        results...
                    );
                }, _info.resultColumns);
                return SelectStatement<decltype(info)>(_sqlite, info);
            }

            SelectStatement &LimitOffset(int limit, int offset = 0) {
                _info.limitOffset = std::make_pair(limit, offset);
                return *this;
            }

            SelectStatement &Distinct() {
                _info.isDistinct = true;
                return *this;
            }

            template<ExprOrColConcept... Exprs>
            auto GroupBy(Exprs... exprs) {
                auto info = std::apply([this,&exprs...](auto... results) {
                    return MakeSelectStatementInfo(
                        _info.source,
                        _info.where,
                        std::make_tuple(exprs...),
                        _info.orderExpr,
                        _info.orderType,
                        _info.limitOffset,
                        _info.isDistinct,
                        results...
                    );
                }, _info.resultColumns);
                return SelectStatement<decltype(info)>(_sqlite, info);
            }

            template<ExprOrColConcept Expr>
            auto OrderBy(Expr expr, OrderType order = OrderType::ASC) {
                auto info = std::apply([this,&expr,&order](auto... results) {
                    return MakeSelectStatementInfo(
                        _info.source,
                        _info.where,
                        _info.groupBy,
                        expr,
                        order,
                        _info.limitOffset,
                        _info.isDistinct,
                        results...
                    );
                }, _info.resultColumns);
                return SelectStatement<decltype(info)>(_sqlite, info);
            }

            auto Results() {
                return std::apply([this](auto... params) {
                    return std::apply([this,&params...](auto... results) {
                        return _sqlite.Query<ExprOrColReturnType<decltype(results)>
                            ...>(GetInfoSql(_info) + ";", params...);
                    }, _info.resultColumns);
                }, this->params);
            }

            //TODO 支援Size查詢
        };

    public:
        explicit SelectAble(SQLiteWrapper &sqlite, Cols columns, Source source) : columns(columns), _sqlite(sqlite),
            _source(source) {
        }

        virtual ~SelectAble() = default;

        template<typename... ResultCol>
        auto Select(ResultCol... resultCols) const {
            //TODO 支援聚合暫時關閉檢查
            //static_assert(IsTypeGroupSubset<TypeGroup<ResultCol...>, columns>(),"ResultCol must be subset of table columns");
            auto info = MakeSelectStatementInfo(
                _source,
                nullptr,
                nullptr,
                nullptr,
                OrderType::ASC,
                std::nullopt,
                false,
                resultCols...
            );
            return SelectStatement(_sqlite, info);
        }

        template<ConvertToQueryAbleConcept Table2, ExprOrColConcept Expr>
        auto FullJoin(const Table2 &table2, const Expr &expr) const {
            auto newSource = JoinSource(this->_source, DataSource<Table2, Expr>{
                                            .type = JoinType::FULL,
                                            .condition = expr
                                        });
            auto newColumns = std::tuple_cat(columns, table2.columns);
            using NewCols = decltype(newColumns);
            using NewSource = decltype(newSource);
            return SelectAble<NewCols, NewSource>(this->_sqlite, newColumns, newSource);
        }

        template<ConvertToQueryAbleConcept Table2, ExprOrColConcept Expr>
        auto InnerJoin(const Table2 &table2, const Expr &expr) const {
            auto newSource = JoinSource(this->_source, DataSource<Table2, Expr>{
                                            .type = JoinType::INNER,
                                            .condition = expr
                                        });
            auto newColumns = std::tuple_cat(columns, table2.columns);
            using NewCols = decltype(newColumns);
            using NewSource = decltype(newSource);
            return SelectAble<NewCols, NewSource>(this->_sqlite, newColumns, newSource);
        }

        template<ConvertToQueryAbleConcept Table2, ExprOrColConcept Expr>
        auto LeftJoin(const Table2 &table2, const Expr &expr) const {
            auto newSource = JoinSource(this->_source, DataSource<Table2, Expr>{
                                            .type = JoinType::LEFT,
                                            .condition = expr
                                        });
            auto newColumns = std::tuple_cat(columns, table2.columns);
            using NewCols = decltype(newColumns);
            using NewSource = decltype(newSource);
            return SelectAble<NewCols, NewSource>(this->_sqlite, newColumns, newSource);
        }

        template<ConvertToQueryAbleConcept Table2, ExprOrColConcept Expr>
        auto RightJoin(const Table2 &table2, const Expr &expr) const {
            auto newSource = JoinSource(this->_source, DataSource<Table2, Expr>{
                                            .type = JoinType::RIGHT,
                                            .condition = expr
                                        });
            auto newColumns = std::tuple_cat(columns, table2.columns);
            using NewCols = decltype(newColumns);
            using NewSource = decltype(newSource);
            return SelectAble<NewCols, NewSource>(this->_sqlite, newColumns, newSource);
        }

        template<ConvertToQueryAbleConcept Table2, ExprOrColConcept Expr>
        auto CrossJoin(const Table2 &table2, const Expr &expr) const {
            auto newSource = JoinSource(this->_source, DataSource<Table2, Expr>{
                                            .type = JoinType::CROSS,
                                            .condition = expr
                                        });
            auto newColumns = std::tuple_cat(columns, table2.columns);
            using NewCols = decltype(newColumns);
            using NewSource = decltype(newSource);
            return SelectAble<NewCols, NewSource>(this->_sqlite, newColumns, newSource);
        }
    };
}

//TODO Aggregate Functions有WindowFunction的特性
namespace TypeSQLite {
    // AVG - Average value
    template<ExprOrColConcept T>
    auto Avg(const T &expr) {
        return MakeExpr<double>("AVG(" + expr.sql + ")",expr);
    }

    // COUNT - Count rows
    template<ExprOrColConcept T>
    auto Count(const T &expr) {
        return MakeExpr<double>("COUNT(" + expr.sql + ")",expr);
    }

    // MAX - Maximum value
    template<ExprOrColConcept T>
    auto Max(const T &expr) {
        return MakeExpr<double>("MAX(" + expr.sql + ")",expr);
    }

    // MIN - Minimum value
    template<ExprOrColConcept T>
    auto Min(const T &expr) {
        return MakeExpr<double>("MIN(" + expr.sql + ")",expr);
    }

    // SUM - Sum of values
    template<ExprOrColConcept T>
    auto Sum(const T &expr) {
        return MakeExpr<double>("SUM(" + expr.sql + ")",expr);
    }

    // TOTAL - Total of values (returns 0.0 for empty set instead of NULL)
    template<ExprOrColConcept T>
    auto Total(const T &expr) {
        return MakeExpr<double>("TOTAL(" + expr.sql + ")",expr);
    }

    // GROUP_CONCAT - Concatenate strings with separator
    template<ExprOrColConcept T1, ExprOrColConcept T2>
    auto GroupConcat(const T1 &expr1, const T2 &expr2) {
        return MakeExpr<"GROUP_CONCAT(" + T1::sql + ", " + T2::sql + ")">(expr1, expr2);
    }

    // MEDIAN - Median value
    template<ExprOrColConcept T>
    auto Median(const T &expr) {
        return MakeExpr<double>("MEDIAN(" + expr.sql + ")",expr);
    }

    // PERCENTILE - Percentile value
    template<double percent, ExprOrColConcept T>
    auto Percentile(const T &expr) {
        return MakeExpr<double>("PERCENTILE(" + expr.sql + ", " + toFixedString<percent>() + ")",expr);
    }

    // PERCENTILE_CONT - Continuous percentile value
    template<double percent, ExprOrColConcept T>
    auto PercentileCont(const T &expr) {
        return MakeExpr<double>("PERCENTILE_CONT(" + expr.sql + ", " + toFixedString<percent>() + ")",expr);
    }
}

#include <cstddef>
#include <string>
#include <tuple>

namespace TypeSQLite {
    template<typename ReturnType, typename WindowFuncCols, typename WindowFuncParams, typename PartitionBy, typename
        OrderExpr>
    struct WindowFuncInfo {
        using returnType = ReturnType;
        const std::string windowFuncSql;
        const WindowFuncCols windowFuncCols;
        const WindowFuncParams windowFuncParams;
        PartitionBy partitionBy;
        OrderExpr orderExpr;
        OrderType orderType = OrderType::ASC;
    };

    template<typename ReturnType, typename WindowFuncCols, typename WindowFuncParams, typename PartitionBy, typename
        OrderExpr>
    auto MakeInfo(std::string sql, WindowFuncCols cols, WindowFuncParams params, PartitionBy partitionBy,
                  OrderExpr orderExpr, OrderType orderType = OrderType::ASC) {
        return WindowFuncInfo<ReturnType, WindowFuncCols, WindowFuncParams, PartitionBy, OrderExpr>{
            .windowFuncSql = sql,
            .windowFuncCols = cols,
            .windowFuncParams = params,
            .partitionBy = partitionBy,
            .orderExpr = orderExpr,
            .orderType = orderType
        };
    }

    template<typename NewInfo>
    std::string CreateSQLPartitionBy(const NewInfo &_info) {
        if constexpr (std::is_same_v<decltype(_info.partitionBy), nullptr_t>) {
            return "";
        } else {
            return std::apply([](auto... exprs) {
                return " PARTITION BY " + GetExprSqls(exprs...);
            }, _info.partitionBy);
        }
    }

    template<typename NewInfo>
    std::string CreateSQLOrderBy(const NewInfo &_info) {
        if constexpr (std::is_same_v<decltype(_info.orderExpr), nullptr_t>) {
            return "";
        } else {
            return " ORDER BY " + _info.orderExpr.sql + " " + OrderTypeToString(_info.orderType);
        }
    }

    template<typename NewInfo>
    auto GetSql(const NewInfo &_info) {
        return _info.windowFuncSql + " OVER(" +
               CreateSQLPartitionBy(_info) +
               CreateSQLOrderBy(_info) +
               ")";
    }

    template<typename NewInfo>
    auto GetInfoCols(const NewInfo &_info) {
        return std::tuple_cat(_info.windowFuncCols, GetExprsTupleColTuple(_info.partitionBy),
                              GetExprsColTuple(_info.orderExpr));
    }

    template<typename NewInfo>
    auto GetInfoParams(const NewInfo &_info) {
        return std::tuple_cat(_info.windowFuncParams, GetExprsTupleParamTuple(_info.partitionBy),
                              GetExprsParamTuple(_info.orderExpr));
    }

    //TODO frame_clause 未實作
    template<typename Columns, typename Parameters, typename Info>
    class WindowFunctions {
    public:
        using returnType = Info::returnType;
        const Info info;
        const Columns cols;
        const Parameters params;
        const std::string sql;

        template<ExprOrColConcept... Exprs>
        auto PartitionedBy(Exprs... exprs) {
            auto _info = MakeInfo<returnType>(
                info.windowFuncSql,
                info.windowFuncCols,
                info.windowFuncParams,
                std::make_tuple(exprs...),
                info.orderExpr,
                info.orderType
            );
            auto newCols = GetInfoCols(_info);
            auto newParams = GetInfoParams(_info);
            return WindowFunctions<decltype(newCols), decltype(newParams), decltype(_info)>{
                .info = _info,
                .cols = newCols,
                .params = newParams,
                .sql = GetSql(_info)
            };
        }

        template<ExprOrColConcept Expr>
        auto OrderBy(Expr expr, const OrderType order = OrderType::ASC) {
            auto _info = MakeInfo<returnType>(
                info.windowFuncSql,
                info.windowFuncCols,
                info.windowFuncParams,
                info.partitionBy,
                expr,
                order
            );
            auto newCols = GetInfoCols(_info);
            auto newParams = GetInfoParams(_info);
            return WindowFunctions<decltype(newCols), decltype(newParams), decltype(_info)>{
                .info = _info,
                .cols = newCols,
                .params = newParams,
                .sql = GetSql(_info)
            };
        }
    };

    template<typename>
    struct IsWindowFunctions : std::false_type {
    };

    template<typename Columns, typename Parameters, typename Info>
    struct IsWindowFunctions<WindowFunctions<Columns, Parameters, Info> > : std::true_type {
    };

    template<typename T>
    concept WindowFunctionsConcept = IsWindowFunctions<T>::value;

    template<typename ReturnType, ExprOrColConcept ... Exprs>
    auto MakeWindowFunction(std::string newSQL, Exprs... exprs) {
        auto newCols = std::tuple_cat(GetCols(exprs)...);
        auto newPara = std::tuple_cat(GetParms(exprs)...);
        auto info = MakeInfo<ReturnType>(
            newSQL,
            newCols,
            newPara,
            nullptr,
            nullptr
        );
        return WindowFunctions{
            .info = info,
            .cols = GetInfoCols(info),
            .params = GetInfoParams(info),
            .sql = GetSql(info)
        };
    }

    inline auto RowNumber() {
        return MakeWindowFunction<int>(" ROW_NUMBER()");
    }

    inline auto Rank() {
        return MakeWindowFunction<int>(" RANK()");
    }

    inline auto DenseRank() {
        return MakeWindowFunction<int>(" DENSE_RANK()");
    }

    inline auto PercentRank() {
        return MakeWindowFunction<double>(" PERCENT_RANK()");
    }

    inline auto CumeDist() {
        return MakeWindowFunction<double>(" CUME_DIST()");
    }

    //TODO 限定int Expr
    template<ExprOrColConcept Expr>
    auto NTile(Expr n) {
        return MakeWindowFunction<int>(" NTILE(" + n.sql + ")", n);
    }

    template<ExprOrColConcept Expr>
    auto Lag(Expr expr) {
        return MakeWindowFunction<ExprOrColReturnType<Expr>>(" LAG(" + expr.sql + ")", expr);
    }

    template<ExprOrColConcept Expr, ExprOrColConcept Offset>
    auto Lag(Expr expr, Offset offset) {
        return MakeWindowFunction<ExprOrColReturnType<Expr>>(" LAG(" + expr.sql + ", " + offset.sql + ")", expr, offset);
    }

    template<ExprOrColConcept Expr, ExprOrColConcept Offset, ExprOrColConcept Default>
    auto Lag(Expr expr, Offset offset, Default defaultValue) {
        return MakeWindowFunction<ExprOrColReturnType<Expr>>(
            " LAG(" + expr.sql + ", " + offset.sql + ", " + defaultValue.sql + ")", expr, offset,
            defaultValue);
    }

    template<ExprOrColConcept Expr>
    auto Lead(Expr expr) {
        return MakeWindowFunction<ExprOrColReturnType<Expr>>(" LEAD(" + expr.sql + ")", expr);
    }

    template<ExprOrColConcept Expr, ExprOrColConcept Offset>
    auto Lead(Expr expr, Offset offset) {
        return MakeWindowFunction<ExprOrColReturnType<Expr>>(" LEAD(" + expr.sql + ", " + offset.sql + ")", expr, offset);
    }

    template<ExprOrColConcept Expr, ExprOrColConcept Offset, ExprOrColConcept Default>
    auto Lead(Expr expr, Offset offset, Default defaultValue) {
        return MakeWindowFunction<ExprOrColReturnType<Expr>>(
            " LEAD(" + expr.sql + ", " + offset.sql + ", " + defaultValue.sql + ")", expr, offset,
            defaultValue);
    }

    template<ExprOrColConcept Expr>
    auto FirstValue(Expr expr) {
        return MakeWindowFunction<ExprOrColReturnType<Expr>>(" FIRST_VALUE(" + expr.sql + ")", expr);
    }

    template<ExprOrColConcept Expr>
    auto LastValue(Expr expr) {
        return MakeWindowFunction<ExprOrColReturnType<Expr>>(" LAST_VALUE(" + expr.sql + ")", expr);
    }

    template<ExprOrColConcept Expr, ExprOrColConcept N>
    auto NthValue(Expr expr, N n) {
        return MakeWindowFunction<ExprOrColReturnType<Expr>>(" NTH_VALUE(" + expr.sql + ", " + n.sql + ")", expr, n);
    }
}

namespace TypeSQLite {
    // Trigonometric functions
    template<ExprOrColConcept T>
    auto Acos(const T &expr) {
        return MakeExpr<double>("ACOS(" + expr.sql + ")", expr);
    }

    template<ExprOrColConcept T>
    auto Acosh(const T &expr) {
        return MakeExpr<double>("ACOSH(" + expr.sql + ")", expr);
    }

    template<ExprOrColConcept T>
    auto Asin(const T &expr) {
        return MakeExpr<double>("ASIN(" + expr.sql + ")", expr);
    }

    template<ExprOrColConcept T>
    auto Asinh(const T &expr) {
        return MakeExpr<double>("ASINH(" + expr.sql + ")", expr);
    }

    template<ExprOrColConcept T>
    auto Atan(const T &expr) {
        return MakeExpr<double>("ATAN(" + expr.sql + ")", expr);
    }

    template<ExprOrColConcept T1, ExprOrColConcept T2>
    auto Atan2(const T1 &y, const T2 &x) {
        return MakeExpr<double>("ATAN2(" + y.sql + ", " + x.sql + ")", y, x);
    }

    template<ExprOrColConcept T>
    auto Atanh(const T &expr) {
        return MakeExpr<double>("ATANH(" + expr.sql + ")", expr);
    }

    template<ExprOrColConcept T>
    auto Cos(const T &expr) {
        return MakeExpr<double>("COS(" + expr.sql + ")", expr);
    }

    template<ExprOrColConcept T>
    auto Cosh(const T &expr) {
        return MakeExpr<double>("COSH(" + expr.sql + ")", expr);
    }

    template<ExprOrColConcept T>
    auto Sin(const T &expr) {
        return MakeExpr<double>("SIN(" + expr.sql + ")", expr);
    }

    template<ExprOrColConcept T>
    auto Sinh(const T &expr) {
        return MakeExpr<double>("SINH(" + expr.sql + ")", expr);
    }

    template<ExprOrColConcept T>
    auto Tan(const T &expr) {
        return MakeExpr<double>("TAN(" + expr.sql + ")", expr);
    }

    template<ExprOrColConcept T>
    auto Tanh(const T &expr) {
        return MakeExpr<double>("TANH(" + expr.sql + ")", expr);
    }

    // Exponential and logarithmic functions
    template<ExprOrColConcept T>
    auto Exp(const T &expr) {
        return MakeExpr<double>("EXP(" + expr.sql + ")", expr);
    }

    template<ExprOrColConcept T>
    auto Ln(const T &expr) {
        return MakeExpr<double>("LN(" + expr.sql + ")", expr);
    }

    template<ExprOrColConcept T>
    auto Log(const T &expr) {
        return MakeExpr<double>("LOG(" + expr.sql + ")", expr);
    }

    template<ExprOrColConcept T1, ExprOrColConcept T2>
    auto Log(const T1 &base, const T2 &x) {
        return MakeExpr<double>("LOG(" + base.sql + ", " + x.sql + ")", base, x);
    }

    template<ExprOrColConcept T>
    auto Log10(const T &expr) {
        return MakeExpr<double>("LOG10(" + expr.sql + ")", expr);
    }

    template<ExprOrColConcept T>
    auto Log2(const T &expr) {
        return MakeExpr<double>("LOG2(" + expr.sql + ")", expr);
    }

    // Power and root functions
    template<ExprOrColConcept T1, ExprOrColConcept T2>
    auto Power(const T1 &base, const T2 &exponent) {
        return MakeExpr<double>("POWER(" + base.sql + ", " + exponent.sql + ")", base, exponent);
    }

    template<ExprOrColConcept T>
    auto Sqrt(const T &expr) {
        return MakeExpr<double>("SQRT(" + expr.sql + ")", expr);
    }

    template<ExprOrColConcept T>
    auto Ceil(const T &expr) {
        return MakeExpr<double>("CEIL(" + expr.sql + ")", expr);
    }

    template<ExprOrColConcept T>
    auto Ceiling(const T &expr) {
        return MakeExpr<double>("CEILING(" + expr.sql + ")", expr);
    }

    template<ExprOrColConcept T>
    auto Floor(const T &expr) {
        return MakeExpr<double>("FLOOR(" + expr.sql + ")", expr);
    }

    template<ExprOrColConcept T>
    auto Trunc(const T &expr) {
        return MakeExpr<double>("TRUNC(" + expr.sql + ")", expr);
    }

    // Note: ROUND and SIGN moved to ScalarFunctions.hpp

    // Other math functions
    template<ExprOrColConcept T>
    auto Degrees(const T &expr) {
        return MakeExpr<double>("DEGREES(" + expr.sql + ")", expr);
    }

    template<ExprOrColConcept T>
    auto Radians(const T &expr) {
        return MakeExpr<double>("RADIANS(" + expr.sql + ")", expr);
    }

    // Pi constant
    inline auto Pi() {
        return MakeExpr<double>("PI()");
    }

    // Modulo function
    template<ExprOrColConcept T1, ExprOrColConcept T2>
    auto Mod(const T1 &x, const T2 &y) {
        return MakeExpr<double>("MOD(" + x.sql + ", " + y.sql + ")", x, y);
    }

} // namespace TypeSQLite

namespace TypeSQLite {
    // ============ SQLite Core Scalar Functions (Alphabetical Order) ============

    // ABS - Absolute value
    template<ExprOrColConcept T>
    auto Abs(const T &expr) {
        return MakeExpr<double>("ABS(" + expr.sql + ")", expr);
    }

    // CHANGES - Number of rows modified by recent INSERT, UPDATE or DELETE
    inline auto Changes() {
        return MakeExpr<int>("CHANGES()");
    }

    // CHAR - Convert integers to characters
    template<ExprOrColConcept... Args>
    auto Char(const Args &... args) {
        std::string sql = "CHAR(";
        std::vector<std::string> parts;
        (parts.push_back(args.sql), ...);
        for (size_t i = 0; i < parts.size(); ++i) {
            sql += parts[i];
            if (i < parts.size() - 1) sql += ", ";
        }
        sql += ")";
        return MakeExpr<std::string>(sql, args...);
    }

    // COALESCE - Return first non-NULL value
    template<ExprOrColConcept... Args>
    auto Coalesce(const Args &... args) {
        std::string sql = "COALESCE(";
        std::vector<std::string> parts;
        (parts.push_back(args.sql), ...);
        for (size_t i = 0; i < parts.size(); ++i) {
            sql += parts[i];
            if (i < parts.size() - 1) sql += ", ";
        }
        sql += ")";
        return MakeExpr<std::string>(sql, args...);
    }

    // CONCAT - Concatenate strings
    template<ExprOrColConcept... Args>
    auto Concat(const Args &... args) {
        std::string sql = "CONCAT(";
        std::vector<std::string> parts;
        (parts.push_back(args.sql), ...);
        for (size_t i = 0; i < parts.size(); ++i) {
            sql += parts[i];
            if (i < parts.size() - 1) sql += ", ";
        }
        sql += ")";
        return MakeExpr<std::string>(sql, args...);
    }

    // CONCAT_WS - Concatenate with separator
    template<ExprOrColConcept Sep, ExprOrColConcept... Args>
    auto ConcatWs(const Sep &separator, const Args &... args) {
        std::string sql = "CONCAT_WS(" + separator.sql;
        ((sql += ", " + args.sql), ...);
        sql += ")";
        return MakeExpr<std::string>(sql, separator, args...);
    }

    // FORMAT - Format string
    template<ExprOrColConcept FormatStr, ExprOrColConcept... Args>
    auto Format(const FormatStr &format, const Args &... args) {
        std::string sql = "FORMAT(" + format.sql;
        ((sql += ", " + args.sql), ...);
        sql += ")";
        return MakeExpr<std::string>(sql, format, args...);
    }

    // GLOB - Pattern matching (case-sensitive)
    template<ExprOrColConcept T1, ExprOrColConcept T2>
    auto Glob(const T1 &str, const T2 &pattern) {
        return MakeExpr<int>(str.sql + " GLOB " + pattern.sql, str, pattern);
    }

    // HEX - Convert to hexadecimal
    template<ExprOrColConcept T>
    auto Hex(const T &expr) {
        return MakeExpr<std::string>("HEX(" + expr.sql + ")", expr);
    }

    // IF - Conditional expression
    template<ExprOrColConcept... Args>
    auto If(const Args&... args) {
        std::string sql = "IF(";
        std::vector<std::string> parts;
        (parts.push_back(args.sql), ...);
        for (size_t i = 0; i < parts.size(); ++i) {
            sql += parts[i];
            if (i < parts.size() - 1) sql += ", ";
        }
        sql += ")";
        return MakeExpr<std::string>(sql, args...);
    }

    // IFNULL - Return replacement if NULL
    template<ExprOrColConcept T1, ExprOrColConcept T2>
    auto IfNull(const T1 &expr, const T2 &replacement) {
        return MakeExpr<std::string>("IFNULL(" + expr.sql + ", " + replacement.sql + ")", expr, replacement);
    }

    // IIF - Inline IF
    template<ExprOrColConcept Cond, ExprOrColConcept TrueVal, ExprOrColConcept FalseVal>
    auto Iif(const Cond &condition, const TrueVal &trueValue, const FalseVal &falseValue) {
        return MakeExpr<std::string>("IIF(" + condition.sql + ", " + trueValue.sql + ", " + falseValue.sql + ")",
                                        condition, trueValue, falseValue);
    }

    // INSTR - Find substring position
    template<ExprOrColConcept T1, ExprOrColConcept T2>
    auto Instr(const T1 &str, const T2 &substr) {
        return MakeExpr<int>("INSTR(" + str.sql + ", " + substr.sql + ")", str, substr);
    }

    // LAST_INSERT_ROWID - Last inserted rowid
    inline auto LastInsertRowid() {
        return MakeExpr<int>("LAST_INSERT_ROWID()");
    }

    // LENGTH - String length
    template<ExprOrColConcept T>
    auto Length(const T &expr) {
        return MakeExpr<int>("LENGTH(" + expr.sql + ")", expr);
    }

    // LIKE - Pattern matching
    template<ExprOrColConcept T1, ExprOrColConcept T2>
    auto Like(const T1 &str, const T2 &pattern) {
        return MakeExpr<int>(str.sql + " LIKE " + pattern.sql, str, pattern);
    }

    // LIKE with ESCAPE
    template<ExprOrColConcept T1, ExprOrColConcept T2, ExprOrColConcept T3>
    auto Like(const T1 &str, const T2 &pattern, const T3 &escape) {
        return MakeExpr<int>(str.sql + " LIKE " + pattern.sql + " ESCAPE " + escape.sql, str, pattern,
                                           escape);
    }

    // LIKELIHOOD - Provide hint to query planner
    template<ExprOrColConcept T>
    auto Likelihood(const T& expr, double probability) {
        return MakeExpr<int>("LIKELIHOOD(" + expr.sql + ", " + std::to_string(probability) + ")", expr);
    }

    // LIKELY - Mark expression as likely to be true
    template<ExprOrColConcept T>
    auto Likely(const T& expr) {
        return MakeExpr<int>("LIKELY(" + expr.sql + ")", expr);
    }

    // LOAD_EXTENSION - Load extension
    template<ExprOrColConcept T>
    auto LoadExtension(const T& path) {
        return MakeExpr<std::string>("LOAD_EXTENSION(" + path.sql + ")", path);
    }

    template<ExprOrColConcept T1, ExprOrColConcept T2>
    auto LoadExtension(const T1& path, const T2& entryPoint) {
        return MakeExpr<std::string>("LOAD_EXTENSION(" + path.sql + ", " + entryPoint.sql + ")", path, entryPoint);
    }

    // LOWER - Convert to lowercase
    template<ExprOrColConcept T>
    auto Lower(const T &expr) {
        return MakeExpr<std::string>("LOWER(" + expr.sql + ")", expr);
    }

    // LTRIM - Trim left whitespace
    template<ExprOrColConcept T>
    auto Ltrim(const T &expr) {
        return MakeExpr<std::string>("LTRIM(" + expr.sql + ")", expr);
    }

    // LTRIM with specific characters
    template<ExprOrColConcept T1, ExprOrColConcept T2>
    auto Ltrim(const T1 &expr, const T2 &chars) {
        return MakeExpr<std::string>("LTRIM(" + expr.sql + ", " + chars.sql + ")", expr, chars);
    }

    // MAX - Maximum value (scalar version with multiple arguments)
    template<ExprOrColConcept... Args>
    auto Max(const Args&... args) {
        std::string sql = "MAX(";
        std::vector<std::string> parts;
        (parts.push_back(args.sql), ...);
        for (size_t i = 0; i < parts.size(); ++i) {
            sql += parts[i];
            if (i < parts.size() - 1) sql += ", ";
        }
        sql += ")";
        return MakeExpr<double>(sql, args...);
    }

    // MIN - Minimum value (scalar version with multiple arguments)
    template<ExprOrColConcept... Args>
    auto Min(const Args&... args) {
        std::string sql = "MIN(";
        std::vector<std::string> parts;
        (parts.push_back(args.sql), ...);
        for (size_t i = 0; i < parts.size(); ++i) {
            sql += parts[i];
            if (i < parts.size() - 1) sql += ", ";
        }
        sql += ")";
        return MakeExpr<double>(sql, args...);
    }

    // NULLIF - Return NULL if equal
    template<ExprOrColConcept T1, ExprOrColConcept T2>
    auto NullIf(const T1 &expr1, const T2 &expr2) {
        return MakeExpr<std::string>("NULLIF(" + expr1.sql + ", " + expr2.sql + ")", expr1, expr2);
    }

    // OCTET_LENGTH - Length in bytes
    template<ExprOrColConcept T>
    auto OctetLength(const T& expr) {
        return MakeExpr<int>("OCTET_LENGTH(" + expr.sql + ")", expr);
    }

    // PRINTF - Formatted output
    template<ExprOrColConcept Format, ExprOrColConcept... Args>
    auto Printf(const Format &format, const Args &... args) {
        std::string sql = "PRINTF(" + format.sql;
        ((sql += ", " + args.sql), ...);
        sql += ")";
        return MakeExpr<std::string>(sql, format, args...);
    }

    // QUOTE - Quote string for SQL
    template<ExprOrColConcept T>
    auto Quote(const T &expr) {
        return MakeExpr<std::string>("QUOTE(" + expr.sql + ")", expr);
    }

    // RANDOM - Random integer
    inline auto Random() {
        return MakeExpr<int>("RANDOM()");
    }

    // RANDOMBLOB - Random blob
    template<ExprOrColConcept T>
    auto RandomBlob(const T &n) {
        return MakeExpr<std::vector<uint8_t>>("RANDOMBLOB(" + n.sql + ")", n);
    }

    // REPLACE - Replace substring
    template<ExprOrColConcept T1, ExprOrColConcept T2, ExprOrColConcept T3>
    auto Replace(const T1 &str, const T2 &old, const T3 &newStr) {
        return MakeExpr<std::string>("REPLACE(" + str.sql + ", " + old.sql + ", " + newStr.sql + ")", str, old,
                                        newStr);
    }

    // ROUND - Round to nearest integer
    template<ExprOrColConcept T>
    auto Round(const T &expr) {
        return MakeExpr<double>("ROUND(" + expr.sql + ")", expr);
    }

    // ROUND with digits
    template<ExprOrColConcept T1, ExprOrColConcept T2>
    auto Round(const T1 &expr, const T2 &digits) {
        return MakeExpr<double>("ROUND(" + expr.sql + ", " + digits.sql + ")", expr, digits);
    }

    // RTRIM - Trim right whitespace
    template<ExprOrColConcept T>
    auto Rtrim(const T &expr) {
        return MakeExpr<std::string>("RTRIM(" + expr.sql + ")", expr);
    }

    // RTRIM with specific characters
    template<ExprOrColConcept T1, ExprOrColConcept T2>
    auto Rtrim(const T1 &expr, const T2 &chars) {
        return MakeExpr<std::string>("RTRIM(" + expr.sql + ", " + chars.sql + ")", expr, chars);
    }

    // SIGN - Sign of number
    template<ExprOrColConcept T>
    auto Sign(const T &expr) {
        return MakeExpr<double>("SIGN(" + expr.sql + ")", expr);
    }

    // SOUNDEX - Soundex encoding
#ifdef SQLITE_SOUNDEX
    template<ExprOrColConcept T>
    auto Soundex(const T &expr) {
        return MakeExpr<std::string>("SOUNDEX(" + expr.sql + ")", expr);
    }
#endif

    // SQLITE_COMPILEOPTION_GET - Get compile-time option
    template<ExprOrColConcept T>
    auto SqliteCompileoptionGet(const T& n) {
        return MakeExpr<std::string>("SQLITE_COMPILEOPTION_GET(" + n.sql + ")", n);
    }

    // SQLITE_COMPILEOPTION_USED - Check if compile-time option was used
    template<ExprOrColConcept T>
    auto SqliteCompileoptionUsed(const T& name) {
        return MakeExpr<int>("SQLITE_COMPILEOPTION_USED(" + name.sql + ")", name);
    }

    // SQLITE_OFFSET - Byte offset in database file
    template<ExprOrColConcept T>
    auto SqliteOffset(const T& expr) {
        return MakeExpr<int>("SQLITE_OFFSET(" + expr.sql + ")", expr);
    }

    // SQLITE_SOURCE_ID - Source ID of SQLite library
    inline auto SqliteSourceId() {
        return MakeExpr<std::string>("SQLITE_SOURCE_ID()");
    }

    // SQLITE_VERSION - SQLite version
    inline auto SqliteVersion() {
        return MakeExpr<std::string>("SQLITE_VERSION()");
    }

    // SUBSTR - Substring
    template<ExprOrColConcept T1, ExprOrColConcept T2>
    auto Substr(const T1 &str, const T2 &start) {
        return MakeExpr<std::string>("SUBSTR(" + str.sql + ", " + start.sql + ")", str, start);
    }

    // SUBSTR with length
    template<ExprOrColConcept T1, ExprOrColConcept T2, ExprOrColConcept T3>
    auto Substr(const T1 &str, const T2 &start, const T3 &length) {
        return MakeExpr<std::string>("SUBSTR(" + str.sql + ", " + start.sql + ", " + length.sql + ")", str, start,
                                        length);
    }

    // SUBSTRING - Alias for SUBSTR
    template<ExprOrColConcept T1, ExprOrColConcept T2>
    auto Substring(const T1 &str, const T2 &start) {
        return Substr(str, start);
    }

    template<ExprOrColConcept T1, ExprOrColConcept T2, ExprOrColConcept T3>
    auto Substring(const T1 &str, const T2 &start, const T3 &length) {
        return Substr(str, start, length);
    }

    // TOTAL_CHANGES - Total number of row changes
    inline auto TotalChanges() {
        return MakeExpr<int>("TOTAL_CHANGES()");
    }

    // TRIM - Trim whitespace
    template<ExprOrColConcept T>
    auto Trim(const T &expr) {
        return MakeExpr<std::string>("TRIM(" + expr.sql + ")", expr);
    }

    // TRIM with specific characters
    template<ExprOrColConcept T1, ExprOrColConcept T2>
    auto Trim(const T1 &expr, const T2 &chars) {
        return MakeExpr<std::string>("TRIM(" + expr.sql + ", " + chars.sql + ")", expr, chars);
    }

    // TYPEOF - Return type name
    template<ExprOrColConcept T>
    auto TypeOf(const T &expr) {
        return MakeExpr<std::string>("TYPEOF(" + expr.sql + ")", expr);
    }

    // UNHEX - Convert hexadecimal to BLOB
    template<ExprOrColConcept T>
    auto Unhex(const T& expr) {
        return MakeExpr<std::vector<uint8_t>>("UNHEX(" + expr.sql + ")", expr);
    }

    template<ExprOrColConcept T1, ExprOrColConcept T2>
    auto Unhex(const T1& expr, const T2& ignoreChars) {
        return MakeExpr<std::vector<uint8_t>>("UNHEX(" + expr.sql + ", " + ignoreChars.sql + ")", expr, ignoreChars);
    }

    // UNICODE - Unicode code point
    template<ExprOrColConcept T>
    auto Unicode(const T &expr) {
        return MakeExpr<int>("UNICODE(" + expr.sql + ")", expr);
    }

    // UNISTR - Create string from Unicode code points
    template<ExprOrColConcept... Args>
    auto Unistr(const Args&... args) {
        std::string sql = "UNISTR(";
        std::vector<std::string> parts;
        (parts.push_back(args.sql), ...);
        for (size_t i = 0; i < parts.size(); ++i) {
            sql += parts[i];
            if (i < parts.size() - 1) sql += ", ";
        }
        sql += ")";
        return MakeExpr<std::string>(sql, args...);
    }

    // UNISTR_QUOTE - Quote string using Unicode escapes
    template<ExprOrColConcept T>
    auto UnistrQuote(const T& expr) {
        return MakeExpr<std::string>("UNISTR_QUOTE(" + expr.sql + ")", expr);
    }

    // UNLIKELY - Mark expression as unlikely to be true
    template<ExprOrColConcept T>
    auto Unlikely(const T& expr) {
        return MakeExpr<int>("UNLIKELY(" + expr.sql + ")", expr);
    }

    // UPPER - Convert to uppercase
    template<ExprOrColConcept T>
    auto Upper(const T &expr) {
        return MakeExpr<std::string>("UPPER(" + expr.sql + ")", expr);
    }

    // ZEROBLOB - Create zero-filled blob
    template<ExprOrColConcept T>
    auto ZeroBlob(const T &n) {
        return MakeExpr<std::vector<uint8_t>>("ZEROBLOB(" + n.sql + ")", n);
    }

} // namespace TypeSQLite

namespace TypeSQLite {
    // ============ SQLite Date and Time Functions ============

    // DATE - Extract date (requires at least one time-value)
    template<ExprOrColConcept TimeValue, ExprOrColConcept... Modifiers>
    auto Date(const TimeValue &timeValue, const Modifiers &... modifiers) {
        std::string sql = "DATE(" + timeValue.sql;
        ((sql += ", " + modifiers.sql), ...);
        sql += ")";
        return MakeExpr<std::string>(sql, timeValue, modifiers...);
    }

    // DATETIME - Extract datetime (requires at least one time-value)
    template<ExprOrColConcept TimeValue, ExprOrColConcept... Modifiers>
    auto DateTime(const TimeValue &timeValue, const Modifiers &... modifiers) {
        std::string sql = "DATETIME(" + timeValue.sql;
        ((sql += ", " + modifiers.sql), ...);
        sql += ")";
        return MakeExpr<std::string>(sql, timeValue, modifiers...);
    }

    // JULIANDAY - Julian day number (requires at least one time-value)
    template<ExprOrColConcept TimeValue, ExprOrColConcept... Modifiers>
    auto JulianDay(const TimeValue &timeValue, const Modifiers &... modifiers) {
        std::string sql = "JULIANDAY(" + timeValue.sql;
        ((sql += ", " + modifiers.sql), ...);
        sql += ")";
        return MakeExpr<double>(sql, timeValue, modifiers...);
    }

    // STRFTIME - Format time (requires format and at least one time-value)
    template<ExprOrColConcept Format, ExprOrColConcept TimeValue, ExprOrColConcept... Modifiers>
    auto Strftime(const Format &format, const TimeValue &timeValue, const Modifiers &... modifiers) {
        std::string sql = "STRFTIME(" + format.sql + ", " + timeValue.sql;
        ((sql += ", " + modifiers.sql), ...);
        sql += ")";
        return MakeExpr<std::string>(sql, format, timeValue, modifiers...);
    }

    // TIME - Extract time (requires at least one time-value)
    template<ExprOrColConcept TimeValue, ExprOrColConcept... Modifiers>
    auto Time(const TimeValue &timeValue, const Modifiers &... modifiers) {
        std::string sql = "TIME(" + timeValue.sql;
        ((sql += ", " + modifiers.sql), ...);
        sql += ")";
        return MakeExpr<std::string>(sql, timeValue, modifiers...);
    }

    // TIMEDIFF - Difference between two times
    template<ExprOrColConcept Time1, ExprOrColConcept Time2>
    auto TimeDiff(const Time1 &time1, const Time2 &time2) {
        std::string sql = "TIMEDIFF(" + time1.sql + ", " + time2.sql + ")";
        return MakeExpr<std::string>(sql, time1, time2);
    }

    template<ExprOrColConcept TimeValue, ExprOrColConcept... Modifiers>
    auto UnixEpoch(const TimeValue &timeValue, const Modifiers &... modifiers) {
        std::string sql = "UNIXEPOCH(" + timeValue.sql;
        ((sql += ", " + modifiers.sql), ...);
        sql += ")";
        return MakeExpr<int>(sql, timeValue, modifiers...);
    }

} // namespace TypeSQLite

namespace TypeSQLite {
    // Helper structure to pair a column with its order
    template<ColumnOrTableColumnConcept Column>
    struct ColumnWithOrder {
        Column column;
        OrderType order;
        ColumnWithOrder(Column column, OrderType order)
            : column(column), order(order) {
        }
    };

    // Helper to check if type is ColumnWithOrder
    template<typename>
    struct IsColumnWithOrder : std::false_type {
    };

    template<ColumnOrTableColumnConcept Col>
    struct IsColumnWithOrder<ColumnWithOrder<Col> > : std::true_type {
    };

    // Helper to convert a single column/ColumnWithOrder to its name with order
    template<typename T>
    auto GetColumnNameWithOrder(T t) {
        if constexpr (IsColumnWithOrder<T>::value) {
            return GetColumnName(t.column) + OrderTypeToString(t.order);
        } else if constexpr (ColumnOrTableColumnConcept<T>) {
            return GetColumnName(t); // No order specified, default behavior
        } else {
            static_assert(ColumnOrTableColumnConcept<T> || IsColumnWithOrder<T>::value,
                          "Type must be Column or ColumnWithOrder");
        }
    }

    template<typename T, typename... Ts>
    constexpr auto GetColumnsNameWithOrder(T t, Ts... ts) {
        if constexpr (sizeof...(ts) == 0) {
            return GetColumnNameWithOrder(t);
        } else {
            return GetColumnNameWithOrder(t) + FixedString(",") + GetColumnsNameWithOrder(ts...);
        }
    }

    //TODO COLLATE 暫時不實作
    template<typename Columns>
    struct TablePrimaryKey {
        const std::string value;

        explicit TablePrimaryKey(Columns columns, ConflictCause conflictCause = ConflictCause::ABORT)
            : value(std::string("PRIMARY KEY(") +
                    std::apply([](auto... columns) { return GetColumnsNameWithOrder(columns...); }, columns) +
                    std::string(")") +
                    ConflictCauseToString(conflictCause)) {
        }
    };

    template<typename Columns>
    struct TableUnique {
        const std::string value;

        explicit TableUnique(Columns columns, ConflictCause conflictCause = ConflictCause::ABORT)
            : value(std::string("UNIQUE(") +
                    std::apply([](auto... columns) { return GetColumnsNameWithOrder(columns...); }, columns) +
                    std::string(")") +
                    ConflictCauseToString(conflictCause)) {
        }
    };

    //TODO CHECK暫時不實作
    //TODO FOREIGN KEY暫時不實作

    // Table options (applied after table definition)
    struct WithoutRowId {
        constexpr static FixedString value = FixedString(" WITHOUT ROWID");
    };

    struct Strict {
        constexpr static FixedString value = FixedString(" STRICT");
    };

    // Table constraint concept
    template<typename>
    struct IsTableConstraint : std::false_type {
    };

    template<typename Columns>
    struct IsTableConstraint<TablePrimaryKey<Columns> > : std::true_type {
    };

    template<typename Columns>
    struct IsTableConstraint<TableUnique<Columns> > : std::true_type {
    };

    template<typename T>
    concept TableConstraintConcept = IsTableConstraint<T>::value;

    // Table option concept
    template<typename>
    struct IsTableOption : std::false_type {
    };

    template<>
    struct IsTableOption<WithoutRowId> : std::true_type {
    };

    template<>
    struct IsTableOption<Strict> : std::true_type {
    };

    template<typename T>
    concept TableOptionConcept = IsTableOption<T>::value;

    template<typename T>
    concept ColumnOrTableConstraintConcept = ColumnConcept<T> || TableConstraintConcept<T>;

    template<typename T>
    concept ColumnOrTableConstraintOrOptionConcept =
            ColumnConcept<T> || TableConstraintConcept<T> || TableOptionConcept<T>;
}

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

namespace TypeSQLite {
    //TODO 改成ColumnWithOrder
    //TODO 支援Where
    template<FixedString Name, typename Table, typename ColumTypes, bool Unique = true>
    struct IndexDefinition {
        constexpr static FixedString name = Name;
        constexpr static bool isUnique = Unique;
        using table = Table;
        ColumTypes columns;
    };

    template<typename>
    struct IsIndexDefinition : std::false_type {
    };

    template<FixedString Name, typename Table, typename ColumTypes, bool Unique>
    struct IsIndexDefinition<IndexDefinition<Name, Table, ColumTypes, Unique> > : std::true_type {
    };

    template<FixedString Name, typename Table, typename ColumTypes, bool Unique = true>
    constexpr auto MakeIndexDefinition(ColumTypes columns) {
        return IndexDefinition<Name, Table, ColumTypes, Unique>{.columns = columns};
    }

    template<typename T>
    concept IndexDefinitionConcept = IsIndexDefinition<T>::value;

    template<IndexDefinitionConcept IndexDef>
    class Index {
        SQLiteWrapper &_sqlite;
        IndexDef indexDef;

    public:
        explicit Index(SQLiteWrapper &sqlite, IndexDef indexDef) : _sqlite(sqlite), indexDef(indexDef) {
            std::string sql = "CREATE ";
            if constexpr (IndexDef::isUnique) {
                sql += "UNIQUE ";
            }
            sql += "INDEX IF NOT EXISTS " + std::string(IndexDef::name) + " ON " +
                    std::string(IndexDef::table::name) + " (";
            sql += std::apply([](auto... cols) {
                                  if constexpr (sizeof ...(cols) == 0) {
                                      return "";
                                  } else {
                                      return GetColumnNamesWithOutTableName<decltype(cols)...>();
                                  }
                              }
                              , indexDef.columns);
            sql += ");";
            _sqlite.Execute(sql);
        }
    };
}

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

