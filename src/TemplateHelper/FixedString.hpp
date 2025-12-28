#pragma once
#include <algorithm>

template<size_t N = 0>
struct FixedString {
    char value[N];

    constexpr FixedString(const char (&str)[N]) : value{} {
        for (size_t i = 0; i < N; ++i)
            value[i] = str[i];
    }

    constexpr operator std::string_view() const {
        return {value, N - 1};
    }
};

template<std::size_t N1, std::size_t N2>
constexpr auto operator+(const FixedString<N1> &a, const FixedString<N2> &b) {
    char result[N1 + N2 - 1]{};
    std::copy_n(a.value, N1 - 1, result);
    std::copy_n(b.value, N2, result + N1 - 1);
    return FixedString<N1 + N2 - 1>(result);
}

template<std::size_t N1, std::size_t N2>
constexpr auto operator+(const FixedString<N1> &a, const char (&b)[N2]) {
    char result[N1 + N2 - 1]{};
    std::copy_n(a.value, N1 - 1, result);
    std::copy_n(b, N2, result + N1 - 1);
    return FixedString<N1 + N2 - 1>(result);
}

template<std::size_t N1, std::size_t N2>
constexpr auto operator+(const char (&a)[N1], const FixedString<N2> &b) {
    char result[N1 + N2 - 1]{};
    std::copy_n(a, N1 - 1, result);
    std::copy_n(b.value, N2, result + N1 - 1);
    return FixedString<N1 + N2 - 1>(result);
}

template<std::size_t N>
std::string operator+(const FixedString<N> &a, const std::string &b) {
    return std::string(a.value) + b;
}

template<std::size_t N>
std::string operator+(const std::string &a, const FixedString<N> &b) {
    return a + std::string(b.value);
}

template<int num>
constexpr auto toFixedString() {
    if constexpr (num < 0) {
        constexpr char arr[2] = {'0' + (num * -1 % 10), '\0'};
        if constexpr (num <= -10) {
            return "-" + toFixedString<-(num / 10)>() + FixedString(arr);
        } else {
            return "-" + FixedString(arr);
        }
    } else {
        char arr[2] = {'0' + (num % 10), '\0'};
        if constexpr (num >= 10) {
            return toFixedString<num / 10>() + FixedString(arr);
        } else {
            return FixedString(arr);
        }
    }
}

template<double Fraction>
constexpr auto FractionToFixedString() {
    // 如果剩餘的小數部分非常小，就停止遞迴
    if constexpr (Fraction < std::numeric_limits<double>::epsilon()) {
        return FixedString("");
    } else {
        constexpr double scaled = Fraction * 10; // 取出下一位小數
        return toFixedString<(int) scaled>() // 加上整數部分
               + FractionToFixedString<scaled - (int) scaled>(); // 繼續處理剩餘小數
    }
}

template<double Number>
constexpr auto toFixedString() {
    constexpr int IntegerPart = Number; // 整數部分
    constexpr double FractionPart = Number - IntegerPart; // 小數部分

    if constexpr (FractionPart < std::numeric_limits<double>::epsilon()) {
        // 沒有小數部分 → 僅輸出整數
        return toFixedString<IntegerPart>();
    } else {
        // 有小數 → 整數 + "." + 小數遞迴轉換
        return toFixedString<IntegerPart>()
               + "."
               + FractionToFixedString<FractionPart>();
    }
}

template<typename T>
    struct IsFixedString : std::false_type {
};

template<size_t N>
struct IsFixedString<FixedString<N> > : std::true_type {
};
