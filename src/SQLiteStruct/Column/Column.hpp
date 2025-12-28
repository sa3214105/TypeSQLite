#pragma once
#include "./ColumnConstraints.hpp"
#include "../../TemplateHelper/FixedString.hpp"
#include "../DataType.hpp"

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
