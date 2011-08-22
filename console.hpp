#ifndef CONSOLE_HPP_
#define CONSOLE_HPP_

#include <stdio.h>
#if defined(__GNUC__) && defined(__STRICT_ANSI__) && (defined(__CYGWIN__) || !defined(_WIN32))
extern "C" int fileno(FILE *);
#endif

NAMESPACE_CIRCLE_BEGIN

#ifndef BOOST_ASIO_HAS_POSIX_STREAM_DESCRIPTOR

struct stream_descriptor {
	typedef function<void (const error_code &, size_t bytes_transferred)> read_handler_type;
	stream_descriptor(asio::io_service &serv, int fd);
	void async_read_some(const asio::mutable_buffer &seq, read_handler_type handler);
	asio::io_service &io_service();
	asio::io_service &get_io_service();
private:
	class stream_descriptor_impl;
	shared_ptr<stream_descriptor_impl> pimpl;
};

#else
using asio::posix::stream_descriptor;
#endif

struct output_handler {
	output_handler(asio::io_service &serv, bool use_locale);
private:
	class output_handler_impl;
	shared_ptr<output_handler_impl> pimpl;
};

NAMESPACE_CIRCLE_END

#endif
