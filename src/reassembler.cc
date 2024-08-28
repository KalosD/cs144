#include "reassembler.hh"

#include <algorithm>
#include <ranges>

using namespace std;

auto Reassembler::split( uint64_t pos ) noexcept
{
  // 在 buffer_ 中找到或创建一个新的分段位置，确保插入的新数据段不会与已有的数据段重叠
  auto it { buffer_.lower_bound( pos ) }; // iterator
  if ( it != buffer_.end() and it->first == pos ) {
    return it;
  }
  if ( it == buffer_.begin() ) {
    return it;
  }
  const auto pit { prev( it ) };
  if ( pit->first + size( pit->second ) > pos ) {
    const auto res = buffer_.emplace_hint( it, pos, pit->second.substr( pos - pit->first ) );
    pit->second.resize( pos - pit->first );
    return res;
  }
  return it;
}

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring )
{
  // 将新接收到的数据段插入到 Reassembler 中，并将完整的、按顺序的数据写入到 ByteStream 中
  const auto try_close = [&]() noexcept -> void {
    if ( end_index_.has_value() and end_index_.value() == writer().bytes_pushed() ) {
      output_.writer().close();
    }
  };

  // 处理空数据段
  if ( data.empty() ) { // No capacity limit
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
  const uint64_t unassembled_index { writer().bytes_pushed() };
  const uint64_t unacceptable_index { unassembled_index + writer().available_capacity() };
  if ( first_index + size( data ) <= unassembled_index or first_index >= unacceptable_index ) {
    return; // Out of ranger
  }
  if ( first_index + size( data ) > unacceptable_index ) { // Remove unacceptable bytes
    data.resize( unacceptable_index - first_index );
    is_last_substring = false;
  }
  if ( first_index < unassembled_index ) { // Remove poped/buffered bytes
    data.erase( 0, unassembled_index - first_index );
    first_index = unassembled_index;
  }

  // 记录结束位置
  if ( not end_index_.has_value() and is_last_substring ) {
    end_index_.emplace( first_index + size( data ) );
  }

  // Can be optimizated !!!
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
