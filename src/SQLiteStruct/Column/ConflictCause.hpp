#pragma once
#include "../../TemplateHelper/FixedString.hpp"
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
