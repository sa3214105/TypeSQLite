#pragma once
#include <tuple>
#include <map>
#include <stdexcept>
#include <string>
#include "../Column/Column.hpp"

namespace TypeSQLite {
    enum class ForeignKeyAction {
        NO_ACTION,
        RESTRICT,
        SET_NULL,
        SET_DEFAULT,
        CASCADE
    };

    inline std::string ForeignKeyActionToString(ForeignKeyAction action) {
        switch (action) {
            case ForeignKeyAction::NO_ACTION:
                return "NO ACTION";
            case ForeignKeyAction::RESTRICT:
                return "RESTRICT";
            case ForeignKeyAction::SET_NULL:
                return "SET NULL";
            case ForeignKeyAction::SET_DEFAULT:
                return "SET DEFAULT";
            case ForeignKeyAction::CASCADE:
                return "CASCADE";
            default:
                throw std::invalid_argument("Invalid ForeignKeyAction");
        }
    }

    enum class ForeignTableAction {
        UPDATE,
        DELETE
    };

    inline std::string ForeignTableActionToString(const ForeignTableAction action) {
        switch (action) {
            case ForeignTableAction::UPDATE:
                return "UPDATE";
            case ForeignTableAction::DELETE:
                return "DELETE";
            default:
                throw std::invalid_argument("Invalid Action");
        }
    }

    enum class ForeignDeferrable {
        DEFERRABLE,
        DEFERRABLE_INITIALLY_IMMEDIATE,
        NOT_DEFERRABLE,
        NOT_DEFERRABLE_INITIALLY_IMMEDIATE,
        NOT_DEFERRABLE_INITIALLY_DEFERRED
    };

    inline std::string ForeignDeferrableToString(const ForeignDeferrable deferrable) {
        switch (deferrable) {
            case ForeignDeferrable::DEFERRABLE:
                return "DEFERRABLE";
            case ForeignDeferrable::DEFERRABLE_INITIALLY_IMMEDIATE:
                return "DEFERRABLE INITIALLY IMMEDIATE";
            case ForeignDeferrable::NOT_DEFERRABLE:
                return "NOT DEFERRABLE";
            case ForeignDeferrable::NOT_DEFERRABLE_INITIALLY_IMMEDIATE:
                return "NOT DEFERRABLE INITIALLY IMMEDIATE";
            case ForeignDeferrable::NOT_DEFERRABLE_INITIALLY_DEFERRED:
                return "NOT DEFERRABLE INITIALLY DEFERRED";
            default:
                throw std::invalid_argument("Invalid Deferrable");
        }
    }

    template<typename ForeignTable, typename... ForeignColumns>
    class ForeignKeyClause {
        ForeignTable foreignTable;
        std::tuple<ForeignColumns...> foreignColumns;
        std::map<ForeignTableAction, ForeignKeyAction> foreignTableActions;
        ForeignDeferrable deferrable = ForeignDeferrable::DEFERRABLE_INITIALLY_IMMEDIATE;

        void updateValue() {
            value = std::string("REFERENCES ") + std::string(ForeignTable::name) + " (";
            value += GetColumnNamesWithOutTableName<ForeignColumns...>();
            value += ")";
            for (const auto &[action, fkAction]: foreignTableActions) {
                value += " ON " + ForeignTableActionToString(action) + " " + ForeignKeyActionToString(fkAction);
            }
            value += " " + ForeignDeferrableToString(deferrable);
        }

    public:
        std::string value;

        explicit ForeignKeyClause(ForeignTable foreignTable, ForeignColumns... foreignColumns)
            : foreignTable(foreignTable), foreignColumns{foreignColumns...} {
            updateValue();
        }

        ForeignKeyClause &On(const ForeignTableAction action, const ForeignKeyAction fkAction) {
            foreignTableActions[action] = fkAction;
            updateValue();
            return *this;
        }

        ForeignKeyClause &Deferrable(const ForeignDeferrable foreignDeferrable) {
            deferrable = foreignDeferrable;
            updateValue();
            return *this;
        }
    };
}
