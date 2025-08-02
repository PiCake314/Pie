#pragma once

namespace precedence {
  inline constexpr auto ASSIGNMENT  = 1;
//   inline constexpr auto CONDITIONAL = 2;
  inline constexpr auto SUM         = 3;
  inline constexpr auto PRODUCT     = 4;
//   inline constexpr auto EXPONENT    = 5;
  inline constexpr auto PREFIX      = 6;
  // inline constexpr auto POSTFIX     = 7;
  inline constexpr auto OP_CALL     = 8;
  inline constexpr auto CALL        = 9;
}