#include "wrapping_integers.hh"

using namespace std;

// 绝对序列号 -> 序列号
// 将64位的绝对序列号 n 转换为相对于 zero_point 的32位回绕值（相对值）。
Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point ) {
    // 通过将 n 的低32位与 zero_point 相加得到回绕后的32位序列号
    return zero_point + static_cast<uint32_t>(n);
}

// 序列号 -> 绝对序列号
// 根据给定的 zero_point 和 checkpoint（最近一次转换的绝对序列号）
// 返回当前序列号对应的绝对序列号，该绝对序列号应尽可能接近 checkpoint
uint64_t Wrap32::unwrap(Wrap32 zero_point, uint64_t checkpoint) const {
    // 计算当前 Wrap32 对象（this）的相对序列号（n_low32）
    // 即 this 的 raw_value_ 相对于 zero_point 的偏移量
    const uint64_t n_low32 { this->raw_value_ - zero_point.raw_value_ };

    // 提取 checkpoint 的低32位部分，用于后续比较
    const uint64_t c_low32 { checkpoint & MASK_LOW_32 };

    // 将 checkpoint 的高32位与 n_low32 组合，生成一个初步的绝对序列号 res
    const uint64_t res { ( checkpoint & MASK_HIGH_32 ) | n_low32 };

    // 检查是否需要向下调整 res，以生成一个与 checkpoint 更接近的绝对序列号
    // 如果 res 较大，并且 n_low32 比 c_low32 大很多（超过了半个32位的范围），则向下调整
    if ( res >= BASE and n_low32 > c_low32 and ( n_low32 - c_low32 ) > ( BASE / 2 ) ) {
        return res - BASE;
    }

    // 检查是否需要向上调整 res，以生成一个与 checkpoint 更接近的绝对序列号
    // 如果 res 较小，并且 c_low32 比 n_low32 大很多（超过了半个32位的范围），则向上调整
    if ( res < MASK_HIGH_32 and c_low32 > n_low32 and ( c_low32 - n_low32 ) > ( BASE / 2 ) ) {
        return res + BASE;
    }

    // 如果不需要调整，则直接返回计算得到的 res
    return res;
}
