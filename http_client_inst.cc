#include "common.hpp"

#include <boost/spirit/include/phoenix.hpp>
#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/karma.hpp>
#include <boost/fusion/include/std_pair.hpp>
#include <boost/range.hpp>
#include <boost/range/adaptor/transformed.hpp>

#include "http_connection.hpp"
#include "asio_range_util.hpp"
#include "http_client.hpp"

NAMESPACE_CIRCLE_BEGIN
namespace httpc {
	namespace {
		template <typename Range>
		inline size_t parse_chunk_size(const Range &r, size_t &s) {
			namespace phx = boost::phoenix;
			namespace qi = boost::spirit::qi;
			typename boost::range_iterator<const Range>::type ite = boost::begin(r);
			qi::parse(ite, boost::end(r), (qi::hex[phx::ref(s) = qi::_1] > *(qi::char_-'\n') > '\n'));
			return std::distance(boost::begin(r), ite);
		}
		template <typename Range>
		size_t parse_header(const Range &r, http_response_data &res) {
			namespace phx = boost::phoenix;
			namespace qi = boost::spirit::qi;
			typedef typename boost::range_iterator<const Range>::type iterator;
			iterator ite = boost::begin(r);
			const iterator end = boost::end(r);
			typedef pair<string, string> pair_type;
			
			qi::parse(ite, end, 
					  ("HTTP/" > qi::digit > '.' > qi::digit > ' ' > qi::int_[phx::ref(res.status.first) = qi::_1]
					   > ' ' > qi::as_string[+(qi::char_-'\r')][phx::ref(res.status.second) = qi::_1] > "\r\n"));
			qi::parse(ite, end,
					  *((qi::as_string[+(qi::char_-'\r'-':')] > ": " > qi::as_string[+(qi::char_-'\r')] > "\r\n")
						[phx::insert(phx::ref(res.header), phx::construct<pair_type>(qi::_1, qi::_2))]) > "\r\n");
			return std::distance(boost::begin(r), ite);
		}
		const string eol1 = "\r\n";
		const string eol2 = "\r\n\r\n";
	}

	map<string, string> split_params(const string &data) {
		namespace phx = boost::phoenix;
		namespace qi = boost::spirit::qi;
		string::const_iterator ite = data.begin();
		map<string, string> r;
		qi::parse(ite, data.end(), *((qi::as_string[+(qi::char_-'=')] >> '=' >> qi::as_string[+(qi::char_-'&')] >> '&')
									 [phx::insert(phx::ref(r), phx::construct<pair<string, string> >(qi::_1, qi::_2))]));
		return r;
	}

	http_client::http_client(asio::io_service &serv, http_handler handler, const string &host)
		 : host(host), path(), query(), header(),
		   con(serv, *resolver_type(serv).resolve(resolver_type::query(host, "http"))), sb(), handler(handler) { }
	http_client::http_client(asio::io_service &serv, http_handler handler, const resolver_type::endpoint_type &host)
		 : host(host.address().to_string()), path(), query(), header(), con(serv, host), sb(), handler(handler) { }
	http_client::http_client(asio::io_service &serv, http_handler handler, const string &host, asio::ssl::context &ctx)
		: host(host), path(), query(), header(),
		  con(serv, *resolver_type(serv).resolve(resolver_type::query(host, "https")), ctx), sb(), handler(handler) { }
	http_client::http_client(asio::io_service &serv, http_handler handler, const resolver_type::endpoint_type &host, asio::ssl::context &ctx)
		: host(host.address().to_string()), path(), query(), header(), con(serv, host, ctx), sb(), handler(handler) { }

	void http_client::start_read() {
		shared_ptr<http_response_data> d(make_shared<http_response_data>());
		asio::async_read_until(con.get_stream(), sb, "\r\n\r\n", bind(&this_type::header_read_complete, shared_from_this(), d, _1, _2));
	}
	
	void http_client::header_read_complete(shared_ptr<http_response_data> res, const error_code &ec, size_t len) {
		if (sb.size() == 0) return;

		sb.consume(parse_header(make_iterator_range_from_buffer<const char *>(sb.data()), *res));
		const string trn_enc = util::map_get_or(res->header, "Transfer-Encoding", "");
		if (trn_enc == "chunked") {
			asio::async_read_until(con.get_stream(), sb, eol1, bind(&this_type::chunk_size_read_complete, shared_from_this(), res, _1, _2));
		} else {
			const size_t length = boost::lexical_cast<size_t>(util::map_get_or(res->header, "Content-Length", "0"));
			asio::async_read(con.get_stream(), sb, asio::transfer_at_least(length - asio::buffer_size(sb.data())),
							 bind(&this_type::plain_body_read_complete, shared_from_this(), res, length, _1, _2));
		}
	}
	void http_client::plain_body_read_complete(shared_ptr<http_response_data> res, size_t length, const error_code &ec, size_t len) {
		if (sb.size() == 0) return;
		res->data.assign(asio::buffer_cast<const char *>(sb.data()), length);
		sb.consume(res->data.length());
		handler(shared_from_this(), res);
		start_read();
	}
	void http_client::chunk_size_read_complete(shared_ptr<http_response_data> res, const error_code &ec, size_t len) {
		if (sb.size() == 0) return;
		if (sb.size() <= 2) {
			asio::async_read_until(con.get_stream(), sb, eol1, bind(&this_type::chunk_size_read_complete, shared_from_this(), res, _1, _2));
		} else {
			size_t chunk_size = 0;
			sb.consume(parse_chunk_size(make_iterator_range_from_buffer<const char *>(sb.data()), chunk_size));
			const size_t bufsize = asio::buffer_size(sb.data());
			if (chunk_size+2 > bufsize) {
				asio::async_read(con.get_stream(), sb, asio::transfer_at_least(chunk_size+2-bufsize), 
								 bind(&this_type::chunk_body_read_complete, shared_from_this(), res, chunk_size, _1, _2));
			} else {
				chunk_body_read_complete(res, chunk_size, ec, 0);
			}
		}
	}
	void http_client::chunk_body_read_complete(shared_ptr<http_response_data> res, size_t chunk_size, const error_code &ec, size_t len) {
		if (sb.size() == 0) return;
		if (chunk_size == 0) {
			asio::async_read_until(con.get_stream(), sb, eol2, bind(&this_type::lastchunk_read_complete, shared_from_this(), _1, _2));
		} else {
			assert(asio::buffer_size(sb.data()) >= chunk_size + 2);
			res->data.assign(asio::buffer_cast<const char *>(sb.data()), chunk_size+2);
			sb.consume(res->data.length());
			handler(shared_from_this(), res);
			asio::async_read_until(con.get_stream(), sb, eol1, bind(&this_type::chunk_size_read_complete, shared_from_this(), res, _1, _2));
		}
	}
	void http_client::lastchunk_read_complete(const error_code &ec, size_t) {
		if (sb.size() == 0) return;
		start_read();
	}
	void http_client::write_complete(const shared_ptr<string> &, const error_code &, size_t) {
	}
	void http_client::send_request(const string &data) {
		shared_ptr<string> pdata(make_shared<string>(generate_http_header(data) + data));
		asio::async_write(con.get_stream(), asio::buffer(*pdata), bind(&this_type::write_complete, shared_from_this(), pdata, _1, _2));
	}
	string http_client::generate_http_header(const string &data) const {
		namespace karma = boost::spirit::karma;
		string s = (data.length() ? "POST " : "GET ") + get_uri() + (query.empty() ? "" : "?") + query + " HTTP/1.1\r\nHost: " + host + "\r\n";
		karma::generate(
			back_inserter(s),
			*(karma::string << ':' << karma::string << "\r\n") << (&karma::uint_(0) | ("Content-Length: " << karma::uint_ << "\r\n")) << "\r\n",
			header, data.length());
		return s;
	}
}
NAMESPACE_CIRCLE_END
