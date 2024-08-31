#pragma once

#include <cstdint>
#include <queue>
#include <string>
#include <string_view>

// 前向声明 Reader 和 Writer 类，以便在 ByteStream 类中使用它们
class Reader;
class Writer;

/*
 * ByteStream: 基本字节流类，包含了对数据流的管理
 * - 具有固定的容量
 * - 可以检测到错误
 * - 提供对数据流的读取和写入功能的访问
 */
class ByteStream
{
public:
  explicit ByteStream( uint64_t capacity ); // 构造函数，初始化 ByteStream 并设置其容量

  // Helper functions (provided) to access the ByteStream's Reader and Writer interfaces
  // 提供访问 ByteStream 的 Reader 和 Writer 接口的辅助函数
  Reader& reader();             // 返回 Reader 的引用
  const Reader& reader() const; // 返回 Reader 的常量引用
  Writer& writer();             // 返回 Writer 的引用
  const Writer& writer() const; // 返回 Writer 的常量引用

  // 设置错误标志的函数，当流中发生错误时调用
  void set_error() { error_ = true; };

  // 检查流中是否发生错误的函数
  bool has_error() const { return error_; };

protected:
  // ByteStream 的状态和数据存储
  uint64_t capacity_;                 // ByteStream 的容量
  std::queue<std::string> stream_ {}; // 用于存储数据的队列
  uint64_t removed_prefix_ {};        // 已从流中删除的前缀长度

  uint64_t total_popped_ {};   // 累计从流中弹出的总字节数
  uint64_t total_pushed_ {};   // 累计推入流中的总字节数
  uint64_t total_buffered_ {}; // 当前缓冲区中的字节数

  bool closed_ {}; // 表示流是否已关闭的标志

  bool error_ {}; // 表示流中是否发生错误的标志
};

/*
 * Writer: 继承自 ByteStream 的类，用于向流中写入数据
 */
class Writer : public ByteStream
{
public:
  // 向流中推入数据，但只允许在有可用容量的情况下推入
  void push( std::string data );

  // 关闭流，表示不再有数据写入
  void close();

  // 检查流是否已关闭
  bool is_closed() const;

  // 返回当前流中可以推入数据的可用容量
  uint64_t available_capacity() const;

  // 返回累计推入流中的字节数
  uint64_t bytes_pushed() const;
};

/*
 * Reader: 继承自 ByteStream 的类，用于从流中读取数据
 */
class Reader : public ByteStream
{
public:
  // 查看ByteStream队列中第一个字符串的视图，并跳过已移除的前缀部分，但不移除它们
  std::string_view peek() const;

  // 从缓冲区中移除 len 个字节
  void pop( uint64_t len );

  // 检查流是否已完成（关闭并且所有字节已被弹出）
  bool is_finished() const;

  // 返回当前缓冲区中未被弹出的字节数
  uint64_t bytes_buffered() const;

  // 返回累计从流中弹出的字节数
  uint64_t bytes_popped() const;
};

/*
 * read: 一个辅助函数，用于从 ByteStream 的 Reader 中读取最多 len 字节的数据，并将其存储在输出字符串 out 中。
 * 它会不断从 Reader 中获取数据直到满足所需的长度或者 Reader 中没有更多数据。
 */
void read( Reader& reader, uint64_t len, std::string& out );
