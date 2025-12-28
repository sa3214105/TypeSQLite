#pragma once

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