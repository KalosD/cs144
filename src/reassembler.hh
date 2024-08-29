#pragma once

#include "byte_stream.hh"
#include <map>
#include <optional>

class Reassembler
{
public:
  // Construct Reassembler to write into given ByteStream.
  explicit Reassembler( ByteStream&& output ) : output_( std::move( output ) ) {}

  /*
   * Insert a new substring to be reassembled into a ByteStream.
   *   `first_index`: the index of the first byte of the substring
   *   `data`: the substring itself
   *   `is_last_substring`: this substring represents the end of the stream
   *   `output`: a mutable reference to the Writer
   *
   * The Reassembler's job is to reassemble the indexed substrings (possibly out-of-order
   * and possibly overlapping) back into the original ByteStream. As soon as the Reassembler
   * learns the next byte in the stream, it should write it to the output.
   *
   * If the Reassembler learns about bytes that fit within the stream's available capacity
   * but can't yet be written (because earlier bytes remain unknown), it should store them
   * internally until the gaps are filled in.
   *
   * The Reassembler should discard any bytes that lie beyond the stream's available capacity
   * (i.e., bytes that couldn't be written even if earlier gaps get filled in).
   *
   * The Reassembler should close the stream after writing the last byte.
   */
  /*
   * 插入一个新的子字符串以重新组装成 ByteStream。
   * `first_index`：子字符串第一个字节的索引
   * `data`：子字符串本身
   * `is_last_substring`：此子字符串代表流的结尾
   * `output`：对 Writer 的可变引用
   * 重组器的工作是将索引子字符串（可能无序且可能重叠）重新组合回原始字节流。重组器一旦获悉流中的下一个字节，就应将其写入输出。
   * 如果重组器了解到适合流可用容量但尚未写入的字节（因为较早的字节仍然未知），它应该在内部存储它们，直到填补空白。
   * 重组器应该丢弃超出流可用容量的所有字节（即，即使早期的空白被填补，也无法写入的字节）。
   * 重组器应在写入最后一个字节后关闭流。
   */
  void insert( uint64_t first_index, std::string data, bool is_last_substring );

  // How many bytes are stored in the Reassembler itself?
  uint64_t bytes_pending() const;

  // Access output stream reader
  Reader& reader() { return output_.reader(); }
  const Reader& reader() const { return output_.reader(); }

  // Access output stream writer, but const-only (can't write from outside)
  const Writer& writer() const { return output_.writer(); }

private:
  ByteStream output_; // the Reassembler writes to this ByteStream

  std::map<uint64_t, std::string> buffer_ {}; // map类型缓冲区, 有序存储那些已接收到但还不能立即写入输出的子字符串
  uint64_t total_pending_ {}; // 记录当前缓冲区中待处理字节的总数

  std::optional<uint64_t> end_index_ {};  // 标记流的结尾字节位置。一旦最后一个子字符串到达，end_index_将被设置为字节流的总长度

  auto split( uint64_t pos ) noexcept;
};
