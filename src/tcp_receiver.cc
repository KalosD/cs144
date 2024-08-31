#include "tcp_receiver.hh"

using namespace std;

void TCPReceiver::receive( TCPSenderMessage message )
{
  // 接收 TCPSenderMessage，并将其有效的插入到 Reassembler 中。
  // 如果 ByteStream 已经出现错误，则直接返回，不进行处理。
  if ( writer().has_error() ) {
    return;
  }

  // 如果接收到 RST（重置）消息，则设置错误状态并返回。
  if ( message.RST ) {
    reader().set_error();
    return;
  }

  // 如果还未接收到 SYN 消息，即未初始化 zero_point_，则检查当前消息是否为 SYN。
  if ( not zero_point_.has_value() ) {
    if ( not message.SYN ) {
      return; // 如果消息不是 SYN，直接返回。
    }
    // 初始化 zero_point_，将其设置为收到的 SYN 消息的序列号。
    zero_point_.emplace( message.seqno );
  }

  // 计算预期数据的绝对序列号，即下一个要接收的数据的绝对序列号（包括SYN）。
  const uint64_t checkpoint { writer().bytes_pushed() + 1 /* 包含 SYN */ };

  // 使用消息中的相对序列号和 zero_point_，结合 checkpoint 计算得到绝对序列号。
  const uint64_t absolute_seqno { message.seqno.unwrap( zero_point_.value(), checkpoint ) };

  // 计算流中的索引位置：绝对序列号 + 是否为 SYN（1表示存在SYN） - 1（SYN的偏移）
  const uint64_t stream_index { absolute_seqno + static_cast<uint64_t>( message.SYN ) - 1 /* SYN 偏移 */ };

  // 将计算得到的数据片段插入到 Reassembler 中，处理重组数据。
  reassembler_.insert( stream_index, move( message.payload ), message.FIN );
}

TCPReceiverMessage TCPReceiver::send() const
{
  // 计算接收窗口的大小。由于接收窗口的最大值为 65535，因此若容量超过最大值，则取 UINT16_MAX，否则取实际容量。
  const uint16_t window_size { writer().available_capacity() > UINT16_MAX
                                 ? static_cast<uint16_t>( UINT16_MAX )
                                 : static_cast<uint16_t>( writer().available_capacity() ) };

  // 如果 zero_point_ 已经初始化，说明已经接收到 SYN。
  if ( zero_point_.has_value() ) {
    // 计算确认号（即下一个期望接收的字节的绝对序列号），包括SYN，并且如果流已经关闭（即所有数据接收完毕），确认号将加1。
    const uint64_t ack_for_seqno { writer().bytes_pushed() + 1 + static_cast<uint64_t>( writer().is_closed() ) };

    // 返回包含相对序列号（ACK）、接收窗口大小和错误状态的 TCPReceiverMessage 消息。
    return { Wrap32::wrap( ack_for_seqno, zero_point_.value() ), window_size, writer().has_error() };
  }

  // 如果 zero_point_ 未初始化，则返回空的序列号和窗口大小及错误状态。
  return { nullopt, window_size, writer().has_error() };
}
