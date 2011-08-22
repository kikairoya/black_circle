#ifndef HTTP_CONNECTION_HPP_
#define HTTP_CONNECTION_HPP_

#include <openssl/opensslconf.h>
#ifdef OPENSSL_NO_SSL2
#include <openssl/ssl.h>
inline const SSL_METHOD *SSLv2_method() { return NULL; }
inline const SSL_METHOD *SSLv2_server_method() { return NULL; }
inline const SSL_METHOD *SSLv2_client_method() { return NULL; }
#endif
#include <boost/scoped_ptr.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/noncopyable.hpp>

NAMESPACE_CIRCLE_BEGIN
namespace httpc {
	struct socket_stream_wrapper: boost::noncopyable {
		typedef asio::mutable_buffers_1 mutable_buffer;
		typedef asio::const_buffers_1 const_buffer;
		typedef asio::io_service io_service;
		typedef function<void (const error_code &)> ConnectHandler;
		typedef function<void (const error_code &)> HandshakeHandler;
		typedef function<void (const error_code &, size_t)> ReadHandler;
		typedef function<void (const error_code &)> ShutdownHandler;
		typedef function<void (const error_code &, size_t)> WriteHandler;
		typedef asio::ip::tcp::socket::lowest_layer_type lowest_layer_type;
		typedef asio::ip::tcp::endpoint endpoint_type;
		typedef asio::ip::tcp::socket tcp_stream_type;
		typedef tcp_stream_type::shutdown_type shutdown_type;
		typedef asio::ssl::stream<asio::ip::tcp::socket> ssl_stream_type;
		typedef ssl_stream_type::handshake_type handshake_type;

		void async_connect(const endpoint_type &peer_endpoint, ConnectHandler handler) { do_async_connect(peer_endpoint, handler); }
		void async_handshake(HandshakeHandler handler) { do_async_handshake(ssl_stream_type::client, handler); }
		void async_handshake(handshake_type type, HandshakeHandler handler) { do_async_handshake(type, handler); }
		void async_read_some(const mutable_buffer &buffers, ReadHandler handler) { do_async_read_some(buffers, handler); }
		void async_shutdown(ShutdownHandler handler) { do_async_shutdown(handler); }
		void async_write_some(const const_buffer &buffers, WriteHandler handler) { do_async_write_some(buffers, handler); }
		void close() { lowest_layer().close(); }
		error_code close(error_code &ec) { return lowest_layer().close(ec); }
		void connect(const endpoint_type &peer_endpoint) { do_connect(peer_endpoint); }
		error_code connect(const endpoint_type &peer_endpoint, error_code &ec) { return do_connect(peer_endpoint, ec); }
		io_service &get_io_service() { return do_get_io_service(); }
		void handshake() { do_handshake(ssl_stream_type::client); };
		void handshake(handshake_type type) { do_handshake(type); }
		error_code handshake(error_code &ec) { return do_handshake(ssl_stream_type::client, ec); }
		error_code handshake(handshake_type type, error_code &ec) { return do_handshake(type, ec); }
		bool is_open() const { return const_cast<socket_stream_wrapper *>(this)->lowest_layer().is_open(); }
		endpoint_type local_endpoint() const { return lowest_layer().local_endpoint(); }
		endpoint_type local_endpoint(error_code &ec) const { return lowest_layer().local_endpoint(ec); }
		lowest_layer_type &lowest_layer()  { return do_lowest_layer(); }
		const lowest_layer_type &lowest_layer() const { return do_lowest_layer(); }
		size_t read_some(const mutable_buffer &buffers) { return do_read_some(buffers); }
		size_t read_some(const mutable_buffer &buffers, error_code &ec) { return do_read_some(buffers, ec); }
		endpoint_type remote_endpoint() { return lowest_layer().remote_endpoint(); }
		endpoint_type remote_endpoint(error_code &ec) { return lowest_layer().remote_endpoint(); }
		void shutdown() { do_shutdown(tcp_stream_type::shutdown_both); }
		void shutdown(shutdown_type what) { do_shutdown(what); }
		error_code shutdown(error_code &ec) { return do_shutdown(tcp_stream_type::shutdown_both, ec); }
		error_code shutdown(shutdown_type what, error_code &ec) { return do_shutdown(what, ec); }
		size_t write_some(const const_buffer &buffers) { return do_write_some(buffers); }
		size_t write_some(const const_buffer &buffers, error_code &ec) { return do_write_some(buffers); }
		virtual ~socket_stream_wrapper() { }
	private:
		virtual void do_async_connect(const endpoint_type &peer_endpoint, ConnectHandler handler) = 0;
		virtual void do_async_handshake(handshake_type type, HandshakeHandler handler) = 0;
		virtual void do_async_read_some(const mutable_buffer &buffers, ReadHandler handler) = 0;
		virtual void do_async_shutdown(ShutdownHandler handler) = 0;
		virtual void do_async_write_some(const const_buffer &buffers, WriteHandler handler) = 0;
		virtual void do_connect(const endpoint_type &peer_endpoint) = 0;
		virtual error_code do_connect(const endpoint_type &peer_endpoint, error_code &ec) = 0;
		virtual io_service &do_get_io_service() = 0;
		virtual void do_handshake(handshake_type type) = 0;
		virtual error_code do_handshake(handshake_type type, error_code &ec) = 0;
		virtual lowest_layer_type &do_lowest_layer() = 0;
		virtual const lowest_layer_type &do_lowest_layer() const = 0;
		virtual size_t do_read_some(const mutable_buffer &buffers) = 0;
		virtual size_t do_read_some(const mutable_buffer &buffers, error_code &ec) = 0;
		virtual void do_shutdown(shutdown_type what) = 0;
		virtual error_code do_shutdown(shutdown_type what, error_code &ec) = 0;
		virtual size_t do_write_some(const const_buffer &buffers) = 0;
		virtual size_t do_write_some(const const_buffer &buffers, error_code &ec) = 0;
	};
	template <typename Socket>
	struct basic_socket_stream_wrapper: socket_stream_wrapper {
		typedef Socket socket_type;
		socket_type socket;
		template <typename Arg1>
		basic_socket_stream_wrapper(Arg1 &arg1): socket(arg1) { }
		template <typename Arg1, typename Arg2>
		basic_socket_stream_wrapper(Arg1 &arg1, Arg2 &arg2): socket(arg1, arg2) { }
		void do_async_read_some(const mutable_buffer &buffers, ReadHandler handler) { socket.async_read_some(buffers, handler); }
		void do_async_write_some(const const_buffer &buffers, WriteHandler handler) { socket.async_write_some(buffers, handler); }
		io_service &do_get_io_service() { return socket.get_io_service(); }
		lowest_layer_type &do_lowest_layer() { return socket.lowest_layer(); }
		const lowest_layer_type &do_lowest_layer() const { return socket.lowest_layer(); }
		size_t do_read_some(const mutable_buffer &buffers) { return socket.read_some(buffers); }
		size_t do_read_some(const mutable_buffer &buffers, error_code &ec) { return socket.read_some(buffers, ec); }
		size_t do_write_some(const const_buffer &buffers) { return socket.write_some(buffers); }
		size_t do_write_some(const const_buffer &buffers, error_code &ec) { return socket.write_some(buffers, ec); }
	};
	struct tcp_socket_stream: basic_socket_stream_wrapper<asio::ip::tcp::socket> {
		typedef basic_socket_stream_wrapper<asio::ip::tcp::socket> base_type;
		tcp_socket_stream(io_service &service): base_type(service) { }
		void do_async_connect(const endpoint_type &peer_endpoint, ConnectHandler handler) { lowest_layer().async_connect(peer_endpoint, handler); }
		void do_async_handshake(handshake_type, HandshakeHandler handler) { get_io_service().post(bind(handler, error_code())); }
		void do_async_shutdown(ShutdownHandler handler) {
			error_code ec;
			socket.shutdown(asio::ip::tcp::socket::shutdown_both, ec);
			get_io_service().post(bind(handler, ec));
		}
		void do_connect(const endpoint_type &peer_endpoint) { lowest_layer().connect(peer_endpoint); }
		error_code do_connect(const endpoint_type &peer_endpoint, error_code &ec) { return lowest_layer().connect(peer_endpoint, ec); }
		void do_handshake(handshake_type) { }
		error_code do_handshake(handshake_type, error_code &ec) { return ec = error_code(); }
		void do_shutdown() { socket.shutdown(asio::ip::tcp::socket::shutdown_both); }
		void do_shutdown(shutdown_type what) { socket.shutdown(what); }
		error_code do_shutdown(shutdown_type what, error_code &ec) { return socket.shutdown(what, ec); }
	};
	struct ssl_socket_stream: basic_socket_stream_wrapper<asio::ssl::stream<asio::ip::tcp::socket> > {
		typedef basic_socket_stream_wrapper<asio::ssl::stream<asio::ip::tcp::socket> > base_type;
		ssl_socket_stream(io_service &service, asio::ssl::context &ctx): base_type(service, ctx) { }
		void do_async_connect(const endpoint_type &peer_endpoint, ConnectHandler handler) { lowest_layer().async_connect(peer_endpoint, handler); }
		void do_async_handshake(handshake_type type, ConnectHandler handler) { socket.async_handshake(type, handler); }
		void do_async_shutdown(ShutdownHandler handler) { socket.async_shutdown(handler); }
		void do_connect(const endpoint_type &peer_endpoint) { lowest_layer().connect(peer_endpoint); }
		error_code do_connect(const endpoint_type &peer_endpoint, error_code &ec) { return lowest_layer().connect(peer_endpoint, ec); }
		void do_handshake(handshake_type type) { socket.handshake(type); }
		error_code do_handshake(handshake_type type, error_code &ec) { return socket.handshake(type, ec); }
		void do_shutdown(shutdown_type) { socket.shutdown(); }
		error_code do_shutdown(shutdown_type, error_code &ec) { return socket.shutdown(ec); }
	};

	class connection: boost::noncopyable {
	public:
		typedef socket_stream_wrapper stream_type;
		typedef stream_type::endpoint_type endpoint_type;
	public:
		connection(asio::io_service &serv, const endpoint_type &ep): ctx(), pstream(new tcp_socket_stream(serv)) {
			pstream->connect(ep);
		}
		connection(asio::io_service &serv, const endpoint_type &ep, asio::ssl::context &ctx): ctx(&ctx), pstream(new ssl_socket_stream(serv, ctx)) {
			pstream->connect(ep);
			pstream->handshake();
		}
		~connection() try {
			error_code ec;
			pstream->lowest_layer().cancel(ec);
			pstream->close(ec);
		} catch (...) {
		}
		stream_type &get_stream() { return *pstream; }
		const stream_type &get_stream() const { return *pstream; }
		asio::ssl::context *get_context_ptr() const { return ctx; }
	private:
		asio::ssl::context *ctx;
		boost::scoped_ptr<stream_type> pstream;
	};
}
NAMESPACE_CIRCLE_END

#endif
