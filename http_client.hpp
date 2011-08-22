#ifndef HTTP_CLIENT_HPP_
#define HTTP_CLIENT_HPP_

#include <boost/lexical_cast.hpp>

#include "util.hpp"
#include "http_connection.hpp"

NAMESPACE_CIRCLE_BEGIN
namespace httpc {
	struct http_response_data {
		pair<int, string> status;
		map<string, string> header;
		string data;
	};
	class http_client;
	typedef function<void (const shared_ptr<http_client> &cli, const shared_ptr<http_response_data> &data)> http_handler;

	map<string, string> split_params(const string &data);

	class http_client: public enable_shared_from_this<http_client> {
		typedef http_client this_type;
	public:
		typedef asio::ip::tcp::resolver resolver_type;
	public:
		http_client(asio::io_service &serv, http_handler handler, const string &host);
		http_client(asio::io_service &serv, http_handler handler, const resolver_type::endpoint_type &host);
		http_client(asio::io_service &serv, http_handler handler, const string &host, asio::ssl::context &ctx);
		http_client(asio::io_service &serv, http_handler handler, const resolver_type::endpoint_type &host, asio::ssl::context &ctx);
		string get_uri() const { return get_schema_string() + "://" + host + path; }
		void request() { send_request(string()); }
		void request(const string &data) { send_request(data); }
		void start_read();
		shared_ptr<http_client> get_shared() { return shared_from_this(); }
		void abort() { error_code ec; con.get_stream().close(ec); }
		bool connected() const { return con.get_stream().is_open(); }
	public:
		const string host;
		string path;
		string query;
		map<string, string> header;
	private:
		string generate_http_header(const string &data) const;
		void header_read_complete(shared_ptr<http_response_data> res, const error_code &ec, size_t);
		void plain_body_read_complete(shared_ptr<http_response_data> res, size_t length, const error_code &ec, size_t);
		void chunk_size_read_complete(shared_ptr<http_response_data> res, const error_code &ec, size_t);
		void chunk_body_read_complete(shared_ptr<http_response_data> res, size_t chunk_size, const error_code &ec, size_t);
		void lastchunk_read_complete(const error_code &ec, size_t);
		void write_complete(const shared_ptr<string> &data, const error_code &ec, size_t);
		void send_request(const string &data);
		string get_schema_string() const { return con.get_context_ptr() ? "https" : "http"; }
	private:
		connection con;
		asio::streambuf sb;
		http_handler handler;
	};
	inline shared_ptr<http_client> make_http_client(asio::io_service &serv, const http_handler &handler, const string &host) {
		return make_shared<http_client>(ref(serv), handler, host);
	}
	inline shared_ptr<http_client> make_http_client(asio::io_service &serv, const http_handler &handler, const http_client::resolver_type::endpoint_type &host) {
		return make_shared<http_client>(ref(serv), handler, host);
	}
	inline shared_ptr<http_client> make_http_client(asio::io_service &serv, const http_handler &handler, const string &host, asio::ssl::context &ctx) {
		return make_shared<http_client>(ref(serv), handler, host, ref(ctx));
	}
	inline shared_ptr<http_client> make_http_client(asio::io_service &serv, const http_handler &handler, const http_client::resolver_type::endpoint_type &host, boost::asio::ssl::context &ctx) {
		return make_shared<http_client>(ref(serv), handler, host, ref(ctx));
	}
}
NAMESPACE_CIRCLE_END

#endif
