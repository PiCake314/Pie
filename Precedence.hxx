#pragma once

#include "Token.hxx"

// not sure where to put it
// keep it here for now...
[[noreturn]] void error(const std::string& msg) noexcept {
    puts(msg.c_str());
    exit(1);
}


namespace precedence {
  inline constexpr auto LOW         = 1;
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


  auto precFromToken(const TokenKind token) noexcept {
    switch (token) {
      case TokenKind::PR_LOW:        return LOW;
      case TokenKind::PR_ASSIGNMENT: return ASSIGNMENT;
      case TokenKind::PR_SUM:        return SUM;
      case TokenKind::PR_PROD:       return PROD;
      case TokenKind::PR_OP_CALL:    return OP_CALL;
      case TokenKind::PR_PREFIX:     return PREFIX;
      case TokenKind::PR_POSTFIX:    return POSTFIX;
      case TokenKind::PR_CALL:       return CALL;
      case TokenKind::PR_HIGH:       return HIGH;

      default: error("Wrong token for precedende!");
    }
  }
}