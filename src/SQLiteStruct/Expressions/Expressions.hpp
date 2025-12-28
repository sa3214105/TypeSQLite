// cpp
#pragma once
#include <tuple>
#include <type_traits>
#include <concepts>
#include <string>
#include <vector>
#include "../Column/Column.hpp"
#include "../DataType.hpp"

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
