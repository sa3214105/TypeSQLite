#pragma once
#include "../../SQLiteWrapper.hpp"

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
