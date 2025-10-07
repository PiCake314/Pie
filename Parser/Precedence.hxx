#pragma once

#include "../Utils/utils.hxx"
#include "../Expr/expr.hxx"

#include <numeric>


namespace prec {
  inline constexpr auto LOW_VALUE         = 0;
  inline constexpr auto ASSIGNMENT_VALUE  = 1'000;
  inline constexpr auto INFIX_VALUE       = 2'000;
  inline constexpr auto SUM_VALUE         = 5'000;
  inline constexpr auto PROD_VALUE        = 6'000;
  inline constexpr auto PREFIX_VALUE      = 7'000;
  inline constexpr auto POSTFIX_VALUE     = 8'000;
  inline constexpr auto CALL_VALUE        = 9'000;
  inline constexpr auto HIGH_VALUE        = 10'000;

  inline constexpr auto PR_LOW =        "LOW";
  inline constexpr auto PR_ASSIGNMENT = "ASSIGNMENT";
  inline constexpr auto PR_INFIX =      "INFIX";
  inline constexpr auto PR_SUM =        "SUM";
  inline constexpr auto PR_PROD =       "PROD";
  inline constexpr auto PR_PREFIX =     "PREFIX";
  inline constexpr auto PR_POSTFIX =    "POSTFIX";
  inline constexpr auto PR_CALL =       "CALL";
  inline constexpr auto PR_HIGH =       "HIGH";


  inline int precedenceOf(const std::string& p, const Operators& ops) noexcept {
    if (p == "LOW")        return LOW_VALUE;
    if (p == "ASSIGNMENT") return ASSIGNMENT_VALUE;
    if (p == "INFIX")      return INFIX_VALUE;
    if (p == "SUM")        return SUM_VALUE;
    if (p == "PROD")       return PROD_VALUE;
    if (p == "PREFIX")     return PREFIX_VALUE;
    if (p == "POSTFIX")    return POSTFIX_VALUE;
    if (p == "CALL")       return CALL_VALUE;
    if (p == "HIGH")       return HIGH_VALUE;

    if (not ops.contains(p)) error('\'' + p + "' does not name any operator name or precedende level!");

    const auto& op = ops.at(p);
    return std::midpoint(precedenceOf(op->high, ops), precedenceOf(op->low, ops));
  }

  inline auto calculate(const std::string& high, const std::string& low, const Operators& ops) noexcept {
    return std::midpoint(precedenceOf(high, ops), precedenceOf(low, ops));
  }

  inline std::string higher(const std::string& p, const Operators& ops) noexcept {
    if (p == "LOW")        return "ASSIGNMENT";
    if (p == "ASSIGNMENT") return "INFIX";
    if (p == "INFIX")      return "SUM";
    if (p == "SUM")        return "PROD";
    if (p == "PROD")       return "PREFIX";
    if (p == "PREFIX")     return "POSTFIX";
    if (p == "POSTFIX")    return "CALL";
    if (p == "CALL")       return "HIGH";
    if (p == "HIGH")       error("Can't go higher than HIGH!");

    // should I assume it already contains?
    if (not ops.contains(p)) error('\'' + p + "' not name any precedende level or operator!");

    const auto& op = ops.at(p);
    return op->high == op->low ? higher(op->low /*or op->high*/, ops) : op->high;
  }

  inline std::string lower(const std::string& p, const Operators& ops) noexcept {
    if (p == "LOW")        error("Can't go lower than LOW!");
    if (p == "ASSIGNMENT") return "LOW";
    if (p == "INFIX")      return "ASSIGNMENT";
    if (p == "SUM")        return "INFIX";
    if (p == "PROD")       return "SUM";
    if (p == "PREFIX")     return "PROD";
    if (p == "POSTFIX")    return "PREFIX";
    if (p == "CALL")       return "POSTFIX";
    if (p == "HIGH")       return "CALL";

    // should I assume it already contains?
    if (not ops.contains(p)) error('\'' + p + "' not name any precedende level or operator!");

    const auto& op = ops.at(p);
    return op->high == op->low ? lower(op->high /*or op->high*/, ops) : op->low;
  }
}