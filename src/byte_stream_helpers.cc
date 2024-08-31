#include "byte_stream.hh"

#include <cstdint>
#include <stdexcept>

/*
 * read: 一个辅助函数，用于从 ByteStream 的 Reader 中读取最多 len 字节到一个字符串中；
 * 它会首先清空输出字符串 out，然后尝试从 Reader 中提取字节并添加到 out 中，直到满足所需的长度 len 或者 Reader 中没有更多数据。
 */
void read( Reader& reader, uint64_t len, std::string& out )
{
  out.clear();  // 清空输出字符串。

  // 循环读取，直到读取到的字节数达到len或Reader中没有更多数据
  while ( reader.bytes_buffered() and out.size() < len ) {
    auto view = reader.peek();  // 获取Reader中当前可用的字节块（不移除它们）

    // 如果获取到的视图为空，抛出异常
    if ( view.empty() ) {
      throw std::runtime_error( "Reader::peek() returned empty string_view" );
    }

    // 只获取所需的字节数，防止超过len
    view = view.substr( 0, len - out.size() );
    out += view;               // 将获取到的字节块添加到输出字符串
    reader.pop( view.size() ); // 从Reader中移除已获取的字节数
  }
}

// 以下函数是 ByteStream 类中的方法，用于返回 ByteStream 中的 Reader 或 Writer 引用。
// 它们使用了静态断言（static_assert）来确保 Reader 或 Writer 的大小与 ByteStream 相同，以确保它们能够安全地进行类型转换。
Reader& ByteStream::reader()
{
  static_assert( sizeof( Reader ) == sizeof( ByteStream ),
                 "Please add member variables to the ByteStream base, not the ByteStream Reader." );

  return static_cast<Reader&>( *this ); // 将 ByteStream 对象强制转换为 Reader 类型并返回引用。
}

const Reader& ByteStream::reader() const
{
  static_assert( sizeof( Reader ) == sizeof( ByteStream ),
                 "Please add member variables to the ByteStream base, not the ByteStream Reader." );

  return static_cast<const Reader&>( *this ); // 将 ByteStream 对象强制转换为 Reader 类型并返回常量引用。
}

Writer& ByteStream::writer()
{
  static_assert( sizeof( Writer ) == sizeof( ByteStream ),
                 "Please add member variables to the ByteStream base, not the ByteStream Writer." );

  return static_cast<Writer&>( *this ); // 将 ByteStream 对象强制转换为 Writer 类型并返回引用。
}

const Writer& ByteStream::writer() const
{
  static_assert( sizeof( Writer ) == sizeof( ByteStream ),
                 "Please add member variables to the ByteStream base, not the ByteStream Writer." );

  return static_cast<const Writer&>( *this ); // 将 ByteStream 对象强制转换为 Writer 类型并返回常量引用。
}
