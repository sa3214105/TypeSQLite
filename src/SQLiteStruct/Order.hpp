#pragma once
#include "../TemplateHelper/FixedString.hpp"

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

    inline std::string OrderTypeToString(OrderType order) {
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
