#pragma once

#include "byte_stream.hh"
#include "tcp_receiver_message.hh"
#include "tcp_sender_message.hh"

#include <cstdint>
#include <functional>
#include <queue>

// 重传计时器类，用于管理TCP重传超时（RTO）
class RetransmissionTimer
{
public:
  // 构造函数，初始化重传超时（RTO）计时器
  explicit RetransmissionTimer( uint64_t initial_RTO_ms ) : RTO_ms_( initial_RTO_ms ) {}

  // 检查计时器是否激活
  [[nodiscard]] constexpr auto is_active() const noexcept -> bool { return is_active_; }

  // 检查计时器是否过期
  [[nodiscard]] constexpr auto is_expired() const noexcept -> bool { return is_active_ and timer_ >= RTO_ms_; }

  // 重置计时器
  constexpr auto reset() noexcept -> void { timer_ = 0; }

  // 发生指数退避时，成倍增加RTO
  constexpr auto exponential_backoff() noexcept -> void { RTO_ms_ *= 2; }

  // 重新加载RTO并重置计时器
  constexpr auto reload( uint64_t initial_RTO_ms ) noexcept -> void { RTO_ms_ = initial_RTO_ms, reset(); };

  // 启动计时器并重置
  constexpr auto start() noexcept -> void { is_active_ = true, reset(); }

  // 停止计时器并重置
  constexpr auto stop() noexcept -> void { is_active_ = false, reset(); }

  // 每次时间流逝时更新计时器
  constexpr auto tick( uint64_t ms_since_last_tick ) noexcept -> RetransmissionTimer&
  {
    timer_ += is_active_ ? ms_since_last_tick : 0;
    return *this;
  }

private:
  bool is_active_ {}; // 计时器是否激活
  uint64_t RTO_ms_;   // 重传超时时间
  uint64_t timer_ {}; // 当前计时器值
};

// TCP发送器类，用于管理TCP发送逻辑
class TCPSender
{
public:
  /* Construct TCP sender with given default Retransmission Timeout and possible ISN */
  TCPSender( ByteStream&& input, Wrap32 isn, uint64_t initial_RTO_ms )
    : input_( std::move( input ) ), isn_( isn ), initial_RTO_ms_( initial_RTO_ms ), timer_( initial_RTO_ms )
  {}

  /* Generate an empty TCPSenderMessage */
  [[nodiscard]] TCPSenderMessage make_empty_message() const;

  /* Receive and process a TCPReceiverMessage from the peer's receiver */
  void receive( const TCPReceiverMessage& msg );

  /* Type of the `transmit` function that the push and tick methods can use to send messages */
  using TransmitFunction = std::function<void( const TCPSenderMessage& )>;

  /* Push bytes from the outbound stream */
  void push( const TransmitFunction& transmit );

  /* Time has passed by the given # of milliseconds since the last time the tick() method was called */
  void tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit );

  // Accessors
  [[nodiscard]] uint64_t sequence_numbers_in_flight() const; // How many sequence numbers are outstanding?
  [[nodiscard]] uint64_t consecutive_retransmissions()
    const; // How many consecutive *re*transmissions have happened?
  Writer& writer() { return input_.writer(); }
  [[nodiscard]] const Writer& writer() const { return input_.writer(); }

  // Access input stream reader, but const-only (can't read from outside)
  [[nodiscard]] const Reader& reader() const { return input_.reader(); }

private:
  // 在构造函数中初始化的变量
  ByteStream input_;        // 输入字节流
  Wrap32 isn_;              // 初始序列号
  uint64_t initial_RTO_ms_; // 初始重传超时时间

  RetransmissionTimer timer_; // 重传计时器

  bool SYN_sent_ {}; // 是否发送了SYN
  bool FIN_sent_ {}; // 是否发送了FIN

  uint64_t next_abs_seqno_ {};                          // 下一个绝对序列号
  uint64_t ack_abs_seqno_ {};                           // 已确认的绝对序列号
  uint16_t window_size_ { 1 };                          // 当前窗口大小
  std::queue<TCPSenderMessage> outstanding_message_ {}; // 未确认的消息队列

  uint64_t total_outstanding_ {};    // 总未确认的字节数
  uint64_t total_retransmission_ {}; // 总重传次数
};
