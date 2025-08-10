#pragma once

#include "utils.hxx"
#include "Expr.hxx"

#include <numeric>

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
      default:
        if (not ops.contains(token.text)) error("Token does not name any precedende level!");

        const auto& op = ops.at(token.text);
        return std::midpoint(fromToken(op->high, ops), fromToken(op->low, ops));
    }
  }
  
  int calculate(const Token& high, const Token& low, const Operators& ops) noexcept {
    return std::midpoint(fromToken(high, ops), fromToken(low, ops));
  }

  Token higher(const Token& token, const Operators& ops) noexcept {
    switch (token.kind) {
      case TokenKind::PR_LOW:        return {TokenKind::PR_ASSIGNMENT, stringify(TokenKind::PR_ASSIGNMENT)};
      case TokenKind::PR_ASSIGNMENT: return {TokenKind::PR_SUM, stringify(TokenKind::PR_SUM)};
      case TokenKind::PR_SUM:        return {TokenKind::PR_PROD, stringify(TokenKind::PR_PROD)};
      case TokenKind::PR_PROD:       return {TokenKind::PR_OP_CALL, stringify(TokenKind::PR_OP_CALL)};
      case TokenKind::PR_OP_CALL:    return {TokenKind::PR_PREFIX, stringify(TokenKind::PR_PREFIX)};
      case TokenKind::PR_PREFIX:     return {TokenKind::PR_POSTFIX, stringify(TokenKind::PR_POSTFIX)};
      case TokenKind::PR_POSTFIX:    return {TokenKind::PR_CALL, stringify(TokenKind::PR_CALL)};
      case TokenKind::PR_CALL:       return {TokenKind::PR_HIGH, stringify(TokenKind::PR_HIGH)};
      case TokenKind::PR_HIGH:       error("Can't go higher than HIGH!");

      // default: 
      default: {
        // should I assume it already contains?
        if (not ops.contains(token.text)) error("Token does not name any precedende level or operator!");

        return {TokenKind::NAME, token.text};
      }
    }
  }

  Token lower(const Token& token, const Operators& ops) noexcept {
    switch (token.kind) {
      case TokenKind::PR_LOW:        error("Can't go lower than LOW!");
      case TokenKind::PR_ASSIGNMENT: return {TokenKind::PR_LOW, stringify(TokenKind::PR_LOW)};
      case TokenKind::PR_SUM:        return {TokenKind::PR_ASSIGNMENT, stringify(TokenKind::PR_ASSIGNMENT)};
      case TokenKind::PR_PROD:       return {TokenKind::PR_SUM, stringify(TokenKind::PR_SUM)};
      case TokenKind::PR_OP_CALL:    return {TokenKind::PR_PROD, stringify(TokenKind::PR_PROD)};
      case TokenKind::PR_PREFIX:     return {TokenKind::PR_OP_CALL, stringify(TokenKind::PR_OP_CALL)};
      case TokenKind::PR_POSTFIX:    return {TokenKind::PR_PREFIX, stringify(TokenKind::PR_PREFIX)};
      case TokenKind::PR_CALL:       return {TokenKind::PR_POSTFIX, stringify(TokenKind::PR_POSTFIX)};
      case TokenKind::PR_HIGH:       return {TokenKind::PR_CALL, stringify(TokenKind::PR_CALL)};

      // default: 
      default: {
        // should I assume it already contains?
        if (not ops.contains(token.text)) error("Token does not name any precedende level or operator!");

        return {TokenKind::NAME, token.text};
      }
    }
  }
}