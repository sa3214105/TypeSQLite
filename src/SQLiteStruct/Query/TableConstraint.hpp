#pragma once
#include "../Column/Column.hpp"
#include "../../TemplateHelper/FixedString.hpp"

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
    template<typename LocalColumns, typename ForeignKeyClause>
    struct TableForeignKey {
        const std::string value;

        explicit TableForeignKey(LocalColumns localColumns, ForeignKeyClause clause)
            : value(std::string("FOREIGN KEY(") +
                    std::apply([](auto... columns) { return GetColumnsNameWithOrder(columns...); }, localColumns) +
                    std::string(") ") +
                    clause.value) {
        }
    };

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

    template<typename LocalColumns, typename ForeignKeyClause>
    struct IsTableConstraint<TableForeignKey<LocalColumns, ForeignKeyClause> > : std::true_type {
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
