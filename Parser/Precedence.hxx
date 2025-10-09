#pragma once

#include "../Utils/utils.hxx"
#include "../Expr/Expr.hxx"

#include <numeric>


namespace prec {
  inline constexpr auto BASE = 1 << 10;

  inline constexpr auto LOW_VALUE                = 0;
  inline constexpr auto COMMA_VALUE              = BASE * 1;
  inline constexpr auto ASSIGNMENT_VALUE         = BASE * 2;
  inline constexpr auto OR_VALUE                 = BASE * 3;
  inline constexpr auto AND_VALUE                = BASE * 4;
  inline constexpr auto BITOR_VALUE              = BASE * 5;
  inline constexpr auto BITXOR_VALUE             = BASE * 6;
  inline constexpr auto BITAND_VALUE             = BASE * 7;
  inline constexpr auto EQ_VALUE                 = BASE * 8;
  inline constexpr auto CMP_VALUE                = BASE * 9;
  inline constexpr auto SPACESHIP_VALUE          = BASE * 10;
  inline constexpr auto SHIFT_VALUE              = BASE * 11;
  inline constexpr auto SUM_VALUE                = BASE * 12;
  inline constexpr auto PROD_VALUE               = BASE * 13;
  inline constexpr auto PREFIX_VALUE             = BASE * 14;
  inline constexpr auto SUFFIX_VALUE             = BASE * 15;
  inline constexpr auto CALL_VALUE               = BASE * 16;
  inline constexpr auto SCOPE_RESOLUTION_VALUE   = BASE * 17;
  inline constexpr auto HIGH_VALUE               = BASE * 18;


  inline constexpr auto PR_LOW        = "LOW";
  inline constexpr auto PR_ASSIGNMENT = "ASSIGNMENT";
  inline constexpr auto PR_INFIX      = "INFIX";
  inline constexpr auto PR_SUM        = "SUM";
  inline constexpr auto PR_PROD       = "PROD";
  inline constexpr auto PR_PREFIX     = "PREFIX";
  inline constexpr auto PR_POSTFIX    = "POSTFIX";
  inline constexpr auto PR_CALL       = "CALL";
  inline constexpr auto PR_HIGH       = "HIGH";

  inline int precedenceOf(const std::string& p, const Operators& ops) noexcept {
    if (p == "LOW")
      return LOW_VALUE;

    if (p == "=")
      return ASSIGNMENT_VALUE;

    if (p == "||")
      return OR_VALUE;

    if (p == "&&")
      return AND_VALUE;

    if (p == "|")
      return BITOR_VALUE;

    if (p == "^")
      return BITXOR_VALUE;

    if (p == "&")
      return BITAND_VALUE;

    if (p == "!=" or p == "==")
      return EQ_VALUE;

    if (p == ">" or p == ">=" or p == "<" or p == "<=")
      return CMP_VALUE;

    if (p == "<=>")
      return SPACESHIP_VALUE;

    if (p == "<<" or p == ">>")
      return SHIFT_VALUE;

    if (p == "+" or p == "-")
      return SUM_VALUE;

    if (p == "*" or p == "/" or p == "%")
      return PROD_VALUE;

    if (p == "!" or p == "~")
      return PREFIX_VALUE;

    if (p == "[]")
      return SUFFIX_VALUE;

    if (p == "()")
      return CALL_VALUE;

    if (p == "::")
      return SCOPE_RESOLUTION_VALUE;

    if (p == "HIGH")
      return HIGH_VALUE;

    if (not ops.contains(p)) error('\'' + p + "' does not name any operator name or precedende level!");

    const auto& op = ops.at(p);
    return std::midpoint(precedenceOf(op->high, ops), precedenceOf(op->low, ops));
  }

  inline auto calculate(const std::string& high, const std::string& low, const Operators& ops) noexcept {
    return std::midpoint(precedenceOf(high, ops), precedenceOf(low, ops));
  }


  inline std::string higher(const std::string& p, const Operators& ops) noexcept {
    if (p == "LOW")                                     return "=";
    if (p == "=")                                       return "||";
    if (p == "||")                                      return "&&";
    if (p == "&&")                                      return "|";
    if (p == "|")                                       return "^";
    if (p == "^")                                       return "&";
    if (p == "&")                                       return "==";
    if (p == "!=" or p == "==")                         return "<=";
    if (p == ">" or p == ">=" or p == "<" or p == "<=") return "<=>";
    if (p == "<=>")                                     return ">>";
    if (p == "<<" or p == ">>")                         return "-";
    if (p == "+" or p == "-")                           return "%";
    if (p == "*" or p == "/" or p == "%")               return "~";
    if (p == "!" or p == "~")                           return "[]";
    if (p == "[]")                                      return "()";
    if (p == "()")                                      return "::";
    if (p == "::")                                      return "HIGH";
    if (p == "HIGH") error("Can't go higher than HIGH!");

    // should I assume it already contains?
    if (not ops.contains(p)) error('\'' + p + "' not name any precedende level or operator!");

    const auto& op = ops.at(p);
    return op->high == op->low ? higher(op->low /*or op->high*/, ops) : op->high;
  }

  inline std::string lower(const std::string& p, const Operators& ops) noexcept {
    if (p == "LOW") error("Can't go lower than LOW!");
    if (p == "=")                                       return "LOW";
    if (p == "||")                                      return "=";
    if (p == "&&")                                      return "||";
    if (p == "|")                                       return "&&";
    if (p == "^")                                       return "|";
    if (p == "&")                                       return "^";
    if (p == "!=" or p == "==")                         return "&";
    if (p == ">" or p == ">=" or p == "<" or p == "<=") return "==";
    if (p == "<=>")                                     return "<=";
    if (p == "<<" or p == ">>")                         return "<=>";
    if (p == "+" or p == "-")                           return ">>";
    if (p == "*" or p == "/" or p == "%")               return "-";
    if (p == "!" or p == "~")                           return "%";
    if (p == "[]")                                      return "~";
    if (p == "()")                                      return "[]";
    if (p == "::")                                      return "()";
    if (p == "HIGH")                                    return "::";


    //? should I assume it already contains?
    if (not ops.contains(p)) error('\'' + p + "' not name any precedende level or operator!");
    const auto& op = ops.at(p);
    return op->high == op->low ? lower(op->high /*or op->high*/, ops) : op->low;
  }
}