#include "byte_stream.hh"

using namespace std;

ByteStream::ByteStream( uint64_t capacity ) : capacity_( capacity ) {}

bool Writer::is_closed() const
{
  return closed_;
}

void Writer::push( string data )
{
  // queue实现buffer
  if ( Writer::is_closed() || Writer::available_capacity() == 0 || data.empty() ) {
    return;
  }
  auto const n = min( Writer::available_capacity(), data.size() );
  if ( n < data.size() ) {
    data = data.substr( 0, n );
  }
  total_buffered_ += data.size();
  total_pushed_ += data.size();
  stream_.emplace( data );
}

void Writer::close()
{
  closed_ = true;
}

uint64_t Writer::available_capacity() const
{
  return capacity_ - total_buffered_;
}

uint64_t Writer::bytes_pushed() const
{
  return total_pushed_;
}

bool Reader::is_finished() const
{
  return closed_ and total_buffered_ == 0;
}

uint64_t Reader::bytes_popped() const
{
  return total_popped_;
}

string_view Reader::peek() const
{
  return stream_.empty() ? string_view {} // std::string_view dependents on the initializer through its lifetime.
                         : string_view { stream_.front() }.substr( removed_prefix_ );
}

void Reader::pop( uint64_t len )
{
  total_buffered_ -= len;
  total_popped_ += len;
  while ( len != 0U ) {
    const uint64_t size = stream_.front().size() - removed_prefix_;
    if ( len < size ) {
      removed_prefix_ += len;
      break;
    }
    stream_.pop();
    removed_prefix_ = 0;
    len -= size;
  }
}

uint64_t Reader::bytes_buffered() const
{
  return total_buffered_;
}
