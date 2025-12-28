#pragma once
#include <cmath>
#include <optional>

#include "../../SQLiteWrapper.hpp"
#include "../Query/DataSource.hpp"
#include "../Column/Column.hpp"
#include "../Expressions/Expressions.hpp"

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
