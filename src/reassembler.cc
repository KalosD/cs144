#include "reassembler.hh"

#include <algorithm>
#include <ranges>

using namespace std;

auto Reassembler::split( uint64_t pos ) noexcept
{
  // 在 buffer_ 中找到或创建一个新的分段位置，确保插入的新数据段不会与已有的数据段重叠
  // 尝试找到在给定位置 pos 处或之后的第一个段。
  auto it { buffer_.lower_bound( pos ) }; // 迭代器

  // 如果找到的段的起始位置与 pos 完全匹配，则返回该迭代器（不需要拆分）。
  if ( it != buffer_.end() and it->first == pos ) {
    return it;
  }

  // 如果 pos 位于所有现有段之前（即位于开头），则返回该迭代器。
  if ( it == buffer_.begin() ) {
    return it;
  }

  // 检查位于找到的位置之前的段。
  const auto pit { prev( it ) };

  // 如果前一个段与 pos 重叠，则对该段进行拆分。
  if ( pit->first + pit->second.size() > pos ) {
    // 在找到的位置插入一个新的段，该段包含从 pos 开始的子字符串。
    const auto res = buffer_.emplace_hint( it, pos, pit->second.substr( pos - pit->first ) );

    // 调整前一个段的大小，使其在 pos 处结束。
    pit->second.resize( pos - pit->first );

    // 返回新插入段的迭代器。
    return res;
  }

  // 如果没有重叠，返回原始位置的迭代器。
  return it;
}

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring )
{
  // 将新接收到的数据段插入到 Reassembler 中，并将完整的、按顺序的数据写入到 ByteStream 中
  const auto try_close = [&]() noexcept -> void {
    // 当end_index_=push入字节流中字节的总数，正常关闭
    if ( end_index_.has_value() and end_index_.value() == writer().bytes_pushed() ) {
      output_.writer().close();
    }
  };

  // 处理空数据段: 若空数据段是最后一个子串，end_index_将被设置为字节流的总长度(即最后一个空字串的起始位置)
  if ( data.empty() ) {
    if ( not end_index_.has_value() and is_last_substring ) {
      end_index_.emplace( first_index );
    }
    return try_close();
  }

  // 检查字节流的状态和容量：若写关闭或无容量
  if ( writer().is_closed() or writer().available_capacity() == 0U ) {
    return;
  }

  // Reassembler's internal storage: [unassembled_index, unacceptable_index)
  // 调整数据段以适应流的容量
  // 未经重组器整理的字节起始位置
  const uint64_t unassembled_index { writer().bytes_pushed() };
  // 超出字节流容量的字节起始位置(重组器容量=字节流容量)
  const uint64_t unacceptable_index { unassembled_index + writer().available_capacity() };
  // 整个丢弃掉全部超出容量的或全部重复出现的子串
  if ( first_index + size( data ) <= unassembled_index or first_index >= unacceptable_index ) {
    return; // Out of ranger
  }
  // 丢弃掉子串超出容量的部分，保留容量内的部分
  if ( first_index + size( data ) > unacceptable_index ) {
    data.resize( unacceptable_index - first_index );
    // 保证流的完整性s
    is_last_substring = false;
  }
  // 丢弃掉子串重复出现的部分
  if ( first_index < unassembled_index ) {
    data.erase( 0, unassembled_index - first_index );
    first_index = unassembled_index;
  }

  // 记录结束位置
  if ( not end_index_.has_value() and is_last_substring ) {
    end_index_.emplace( first_index + size( data ) );
  }

  // Can be optimizated
  // 在 buffer_ 中插入和合并数据段
  const auto upper { split( first_index + size( data ) ) };
  const auto lower { split( first_index ) };
  ranges::for_each( ranges::subrange( lower, upper ) | views::values,
                    [&]( const auto& str ) { total_pending_ -= str.size(); } );
  total_pending_ += size( data );
  buffer_.emplace_hint( buffer_.erase( lower, upper ), first_index, move( data ) );

  // 写入连续的数据段到 ByteStream
  while ( not buffer_.empty() ) {
    auto&& [index, payload] { *buffer_.begin() };
    if ( index != writer().bytes_pushed() ) {
      break;
    }

    total_pending_ -= size( payload );
    output_.writer().push( move( payload ) );
    buffer_.erase( buffer_.begin() );
  }
  return try_close();
}

uint64_t Reassembler::bytes_pending() const
{
  return total_pending_;
}
