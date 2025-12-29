#pragma once
#include <type_traits>
#include "../../TemplateHelper/FixedString.hpp"

namespace TypeSQLite {
    struct CurrentTime {
        constexpr static FixedString value = "CURRENT_TIME";
        constexpr CurrentTime() = default;
    };

    struct CurrentDate {
        constexpr static FixedString value = "CURRENT_DATE";
        constexpr CurrentDate() = default;
    };

    struct CurrentTimestamp {
        constexpr static FixedString value = "CURRENT_TIMESTAMP";
        constexpr CurrentTimestamp() = default;
    };

    template<typename T>
    concept TimeKeyWordConcept = std::is_same_v<T, CurrentTime> ||
                                 std::is_same_v<T, CurrentDate> ||
                                 std::is_same_v<T, CurrentTimestamp>;
}
