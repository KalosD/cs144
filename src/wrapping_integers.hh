#pragma once

#include <cstdint>

/*
 * 序列号 seqno，32位，从一个随机数 ISN 开始（防止被猜到，避免攻击），用于填充TCP头部；
 * 绝对序列号 absolute seqno，64位，从0开始，包含SYN和FIN，64位非循环计数；
 * 流索引 stream index，64位，从0开始，不包含SYN和FIN，用于流重组器；
 */

/*
 * The Wrap32 type represents a 32-bit unsigned integer that:
 *    - starts at an arbitrary "zero point" (initial value), and
 *    - wraps back to zero when it reaches 2^32 - 1.
 */

class Wrap32
{
public:
  explicit Wrap32( uint32_t raw_value ) : raw_value_( raw_value ) {}

  /* Construct a Wrap32 given an absolute sequence number n and the zero point. */
  static Wrap32 wrap( uint64_t n, Wrap32 zero_point );

  /*
   * The unwrap method returns an absolute sequence number that wraps to this Wrap32, given the zero point
   * and a "checkpoint": another absolute sequence number near the desired answer.
   *
   * There are many possible absolute sequence numbers that all wrap to the same Wrap32.
   * The unwrap method should return the one that is closest to the checkpoint.
   */
  uint64_t unwrap( Wrap32 zero_point, uint64_t checkpoint ) const;

  Wrap32 operator+( uint32_t n ) const { return Wrap32 { raw_value_ + n }; }
  bool operator==( const Wrap32& other ) const { return raw_value_ == other.raw_value_; }

protected:
  uint32_t raw_value_ {};
  static constexpr uint64_t MASK_LOW_32 { 0x0000'0000'FFFF'FFFF };
  static constexpr uint64_t MASK_HIGH_32 { 0xFFFF'FFFF'0000'0000 };
  static constexpr uint64_t BASE { MASK_LOW_32 + 1 };
};
