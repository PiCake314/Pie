#pragma once

#include "utils.hxx"

#include <numeric>

using Operators = std::unordered_map<std::string, Fix*>;


namespace precedence {
  inline constexpr auto LOW         = 0;
  inline constexpr auto ASSIGNMENT  = 100;
//   inline constexpr auto CONDITIONAL = 200;
  inline constexpr auto SUM         = 300;
  inline constexpr auto PROD        = 400;
  inline constexpr auto OP_CALL     = 500;
//   inline constexpr auto EXPONENT    = 600;
  inline constexpr auto PREFIX      = 700;
  inline constexpr auto POSTFIX     = 800;
  inline constexpr auto CALL        = 900;
  inline constexpr auto HIGH        = 1'000;


  int fromToken(const Token& token, const Operators& ops) noexcept {
    switch (token.kind) {
      case TokenKind::PR_LOW:        return LOW;
      case TokenKind::PR_ASSIGNMENT: return ASSIGNMENT;
      case TokenKind::PR_SUM:        return SUM;
      case TokenKind::PR_PROD:       return PROD;
      case TokenKind::PR_OP_CALL:    return OP_CALL;
      case TokenKind::PR_PREFIX:     return PREFIX;
      case TokenKind::PR_POSTFIX:    return POSTFIX;
      case TokenKind::PR_CALL:       return CALL;
      case TokenKind::PR_HIGH:       return HIGH;

      // default: 
      default: {
        if (not ops.contains(token.text)) error("Token does not name any precedende level or operator!");

        const auto& op = ops.at(token.text);
        return std::midpoint(
          fromToken(op->low, ops),
          fromToken(op->high, ops)
        );
      }
    }
  }

  int higher(const Token& token, const Operators& ops) noexcept {
    switch (token.kind) {
      case TokenKind::PR_LOW:        return ASSIGNMENT;
      case TokenKind::PR_ASSIGNMENT: return SUM;
      case TokenKind::PR_SUM:        return PROD;
      case TokenKind::PR_PROD:       return OP_CALL;
      case TokenKind::PR_OP_CALL:    return PREFIX;
      case TokenKind::PR_PREFIX:     return POSTFIX;
      case TokenKind::PR_POSTFIX:    return CALL;
      case TokenKind::PR_CALL:       return HIGH;
      case TokenKind::PR_HIGH:       return 1.5 * HIGH;

      // default: 
      default: {
        // should I assume it already contains?
        // if (not ops.contains(token.text)) error("Token does not name any precedende level or operator!");

        const auto& op = ops.at(token.text);
        return fromToken(op->high, ops);
      }
    }
  }

  int lower(const Token& token, const Operators& ops) noexcept {
    switch (token.kind) {
      case TokenKind::PR_LOW:        return .5 * LOW;
      case TokenKind::PR_ASSIGNMENT: return LOW;
      case TokenKind::PR_SUM:        return ASSIGNMENT;
      case TokenKind::PR_PROD:       return SUM;
      case TokenKind::PR_OP_CALL:    return PROD;
      case TokenKind::PR_PREFIX:     return OP_CALL;
      case TokenKind::PR_POSTFIX:    return PREFIX;
      case TokenKind::PR_CALL:       return POSTFIX;
      case TokenKind::PR_HIGH:       return CALL;

      // default: 
      default: {
        // should I assume it already contains?
        // if (not ops.contains(token.text)) error("Token does not name any precedende level or operator!");

        const auto& op = ops.at(token.text);
        return fromToken(op->high, ops);
      }
    }
  }
}