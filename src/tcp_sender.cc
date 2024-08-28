#include "tcp_sender.hh"
#include "tcp_config.hh"

using namespace std;

uint64_t TCPSender::sequence_numbers_in_flight() const
{
  // Your code here.
  return total_outstanding_;
}

uint64_t TCPSender::consecutive_retransmissions() const
{
  // Your code here.
  return total_retransmission_;
}

void TCPSender::push( const TransmitFunction& transmit )
{
  // Your code here.
  // 当窗口大小大于当前未确认的字节数时，继续发送数据
  while ( ( window_size_ == 0 ? 1 : window_size_ ) > total_outstanding_ ) {
    // 如果FIN标志已发送，表示传输结束，直接退出循环
    if ( FIN_sent_ ) {
      break; // 传输完成。
    }

    // 生成一个空的TCP消息
    auto msg = make_empty_message();

    // 如果SYN标志尚未发送，添加SYN标志并标记为已发送
    if ( not SYN_sent_ ) {
      msg.SYN = true;
      SYN_sent_ = true;
    }

    // 计算剩余的可用窗口大小
    const uint64_t remaining { ( window_size_ == 0 ? 1 : window_size_ ) - total_outstanding_ };

    // 确定最大可以发送的负载大小
    const size_t len { min( TCPConfig::MAX_PAYLOAD_SIZE, remaining - msg.sequence_length() ) };

    // 获取消息的负载部分
    auto&& payload { msg.payload };

    // 将数据从字节流中读取到消息的负载部分，直到填满或字节流为空
    while ( reader().bytes_buffered() != 0U and payload.size() < len ) {
      string_view view { reader().peek() };
      view = view.substr( 0, len - payload.size() ); // 限制读取长度以适应剩余空间
      payload += view; // 将数据添加到消息的负载部分
      input_.reader().pop( view.size() ); // 从字节流中移除已读取的数据
    }

    // 如果数据流已结束且未发送FIN标志，则发送FIN标志
    if ( not FIN_sent_ and remaining > msg.sequence_length() and reader().is_finished() ) {
      msg.FIN = true;
      FIN_sent_ = true;
    }

    // 如果消息长度为0，则退出循环
    if ( msg.sequence_length() == 0 ) {
      break;
    }

    // 发送消息
    transmit( msg );

    // 如果计时器未激活，启动计时器
    if ( not timer_.is_active() ) {
      timer_.start();
    }

    // 更新下一个绝对序列号和未确认的字节数
    next_abs_seqno_ += msg.sequence_length();
    total_outstanding_ += msg.sequence_length();

    // 将消息放入未确认的消息队列中
    outstanding_message_.emplace( move( msg ) );
  }
}

TCPSenderMessage TCPSender::make_empty_message() const
{
  // Your code here.
  return { Wrap32::wrap( next_abs_seqno_, isn_ ), false, {}, false, input_.has_error() };
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  // Your code here.
  // 检查输入流是否有错误，如果有，直接返回
  if ( input_.has_error() ) {
    return;
  }

  // 如果接收到的消息包含RST标志，则设置输入流错误并返回
  if ( msg.RST ) {
    input_.set_error();
    return;
  }

  // 更新接收到的窗口大小
  window_size_ = msg.window_size;

  // 如果消息中没有acknowledgment号，则不需要进一步处理，直接返回
  if ( not msg.ackno.has_value() ) {
    return;
  }

  // 解包接收到的acknowledgment号，获取绝对序列号
  const uint64_t recv_ack_abs_seqno { msg.ackno->unwrap( isn_, next_abs_seqno_ ) };

  // 如果接收到的acknowledgment号超出了预期的序列号范围，则返回
  if ( recv_ack_abs_seqno > next_abs_seqno_ ) {
    return;
  }

  bool has_acknowledgment { false };

  // 遍历未确认的消息队列，确认消息
  while ( not outstanding_message_.empty() ) {
    const auto& message { outstanding_message_.front() };

    // 如果当前消息的序列号范围未完全被acknowledgment覆盖，则退出循环
    if ( ack_abs_seqno_ + message.sequence_length() > recv_ack_abs_seqno ) {
      break; // 消息必须完全被TCP接收方确认。
    }

    // 标记至少有一个消息被acknowledgment确认
    has_acknowledgment = true;

    // 更新已确认的绝对序列号和未确认的字节数
    ack_abs_seqno_ += message.sequence_length();
    total_outstanding_ -= message.sequence_length();

    // 从未确认的消息队列中移除该消息
    outstanding_message_.pop();
  }

  // 如果有消息被acknowledgment确认
  if ( has_acknowledgment ) {
    // 重置重传计数
    total_retransmission_ = 0;

    // 重新加载初始的重传超时
    timer_.reload( initial_RTO_ms_ );

    // 如果队列为空，则停止计时器，否则重新启动计时器
    outstanding_message_.empty() ? timer_.stop() : timer_.start();
  }
}

void TCPSender::tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit )
{
  // Your code here.
  // 让计时器前进指定的毫秒数，并检查它是否已过期
  if ( timer_.tick( ms_since_last_tick ).is_expired() ) {
    // 如果没有未确认的消息，直接返回
    if ( outstanding_message_.empty() ) {
      return;
    }

    // 重新传输队列中的第一个未确认消息
    transmit( outstanding_message_.front() );

    // 如果窗口大小不为0，增加重传计数并执行指数退避
    if ( window_size_ != 0 ) {
      total_retransmission_ += 1;
      timer_.exponential_backoff();
    }

    // 重置计时器
    timer_.reset();
  }
}
