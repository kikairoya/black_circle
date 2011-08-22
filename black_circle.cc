#include "common.hpp"

#include <iostream>
#include <fstream>
#include <boost/program_options.hpp>
#include <boost/exception/all.hpp>
#include <boost/asio.hpp>
#include <boost/range/algorithm.hpp>
#include <boost/range/adaptor/filtered.hpp>
#include <boost/range/adaptor/reversed.hpp>
#include <boost/variant.hpp>
#include <boost/optional.hpp>
#include <boost/scope_exit.hpp>
#include <boost/parameter.hpp>
#include <boost/utility/value_init.hpp>

#include "util.hpp"
#include "oauth.hpp"
#include "http_client.hpp"
#include "json_parser.hpp"
#include "twitter.hpp"
#include "console.hpp"

NAMESPACE_CIRCLE_BEGIN
struct process_single_entity: unary_function<json::json_object_value, bool> {
	bool operator ()(json::json_object_value t) {
		json::json_object_map o = boost::get<json::json_object_map>(t);
		if (o.find("text") != o.end()) {
			uint64_t id = twitter::insert_new_tweet(o);
			std::cout << *twitter::stores::tweet_store.find(id) << '\n';
		} else if (o.find("delete") != o.end()) {
			uint64_t id = static_cast<uint64_t>(boost::get<int64_t>(boost::get<json::json_object_map>(boost::get<json::json_object_map>(o["delete"])["status"])["id"]));
			twitter::tweet_store_t::iterator ite = twitter::stores::tweet_store.find(id);
			if (ite != twitter::stores::tweet_store.end()) {
				std::cout << "DELETE: " << *ite << '\n';
			}
			twitter::stores::tweet_store.erase(id);
		} else {
			return false;
		}
		std::cout << std::flush;
		return true;
	}
};

#define PARAM_EXPAND(x) x
#define PARAM_MEMBER_OPTIONAL(type, name) \
	const boost::optional<PARAM_EXPAND type> &name() const { return name##_; } \
	boost::optional<PARAM_EXPAND type> &name() { return name##_; } \
	this_type &name(PARAM_EXPAND type val) { name##_ = val; return *this; } \
	boost::optional<PARAM_EXPAND type> name##_
#define PARAM_MEMBER_INITIALIZED(type, name) \
	const PARAM_EXPAND type &name() const { return name##_.data(); } \
	PARAM_EXPAND type &name() { return name##_.data(); } \
	this_type &name(PARAM_EXPAND type val) { name##_.data() = val; return *this; } \
	boost::value_initialized<PARAM_EXPAND type> name##_

struct statuses_update_params {
	typedef statuses_update_params this_type;
	PARAM_MEMBER_INITIALIZED((uint64_t), in_reply_to_status_id);
	PARAM_MEMBER_OPTIONAL((double), latitude);
	PARAM_MEMBER_OPTIONAL((double), longititude);
	PARAM_MEMBER_INITIALIZED((string), place_id);
	PARAM_MEMBER_INITIALIZED((bool), display_coordinates);
	PARAM_MEMBER_INITIALIZED((bool), trim_user);
	PARAM_MEMBER_INITIALIZED((bool), include_entities);
};
struct friends_timeline_params {
	typedef friends_timeline_params this_type;
	PARAM_MEMBER_INITIALIZED((uint64_t), since_id);
	PARAM_MEMBER_INITIALIZED((uint64_t), max_id);
	PARAM_MEMBER_INITIALIZED((unsigned), count);
	PARAM_MEMBER_INITIALIZED((unsigned), page);
	PARAM_MEMBER_INITIALIZED((bool), trim_user);
	PARAM_MEMBER_INITIALIZED((bool), include_rts);
	PARAM_MEMBER_INITIALIZED((bool), include_entities);
};

class twitter_client {
public:
	enum api_type {
		api_user_stream,
		api_friends_timeline,
		api_statuses_update
	};
public:
	twitter_client(asio::io_service &service, asio::ssl::context &ctx, const oauth::token_requester &req): service(service), ctx(ctx), req(req) {
		make_userstream();
	}
	void request_friends_timeline(const friends_timeline_params &) {
		shared_ptr<httpc::http_client> c(httpc::make_http_client(service, bind(&twitter_client::http_handler, this, api_friends_timeline, _1, _2), "api.twitter.com", ctx));
		c->start_read();
		c->header["Connection"] = "close";
		c->path = "/1/statuses/friends_timeline.json";
		c->query = req.gen_request_signature(c->get_uri(), "GET").gen_getquery();
		c->request();
	}
	void statuses_update(const string &status, const statuses_update_params &) {
		shared_ptr<httpc::http_client> c(httpc::make_http_client(service, bind(&twitter_client::http_handler, this, api_statuses_update, _1, _2), "api.twitter.com", ctx));
		c->start_read();
		c->header["Connection"] = "close";
		c->path = "/1/statuses/update.json";
		c->query = c->get_uri();
		req.params["status"] = status;
		req.gen_request_signature(c->get_uri(), "POST");
		req.params.erase("status");
		c->query = "";
		c->header["Authorization"] = "OAuth " + req.gen_postparam();
		c->request("status=" + util::url_escape(status));
	}
	void shutdown() {
		ust.reset();
		service.post(bind(&asio::io_service::stop, &service));
	}
private:
	void make_userstream() {
		ust = httpc::make_http_client(service, bind(&twitter_client::http_handler, this, api_user_stream, _1, _2), "userstream.twitter.com", ctx);
		ust->start_read();
		ust->path = "/2/user.json";
		ust->header["User-Agent"] = "Grimoiretter/0.1";
		ust->header["Accept"] = "*/*";
		ust->query = req.gen_request_signature(ust->get_uri(), "GET").gen_getquery();
		ust->request();
	}
	void http_handler(api_type type, const shared_ptr<httpc::http_client> &p, const shared_ptr<httpc::http_response_data> &data) { 
		if (!data) {
			if (p == ust) make_userstream();
			return;
		}
		if (ust && !ust->connected()) make_userstream();
		if (data->status.first!=200) {
			std::cout << "request rejected: " << data->status.second << '\n';
			std::cout << data->data << std::endl;
			return;
		}
		const string &s = data->data;
		try {
			json::json_object_value t = json::parse_json(s);
			switch (type) {
			case api_user_stream:
				{
					if (s.find('{')==s.npos) break;
					process_single_entity()(t);
					break;
				}
			case api_friends_timeline:
				boost::for_each(boost::get<json::json_array_type>(t) | boost::adaptors::reversed, process_single_entity());
				break;
			case api_statuses_update:
				break;
			}
		} catch (boost::exception &e) {
			std::cerr << "Warning: Execption Thrown in append_data_body:\n";
			std::cerr << diagnostic_information(e) << '\n';
			std::cerr << "When parsing this: \n";
			std::cerr << s << '\n';
		} catch (const std::exception &e) {
			std::cerr << "Warning: Execption Thrown in append_data_body:\n";
			std::cerr << e.what() << '\n';
			std::cerr << "When parsing this: \n";
			std::cerr << s << '\n';
		} catch (...) {
			std::cerr << "Warning: Exception Thrown in append_data_body" << std::flush;
			std::cerr << "When parsing this: \n";
			std::cerr << s << '\n';
		}
	}
	asio::io_service &service;
	asio::ssl::context &ctx;
	oauth::token_requester req;
	shared_ptr<httpc::http_client> ust;
};

static const char request_uri[] = "https://twitter.com/oauth/request_token";
static const char access_uri[] = "https://twitter.com/oauth/access_token";
static const char consumer_key[] = "rsMOC9H8ktdcmyipPMO9g";
static const char consumer_secret[] = "U6EiksBEvtQyZH54LVwEUPNpzvvnf3kJxAWBfKmUo0g";
// Icon from http://www.pixiv.net/member_illust.php?mode=medium&illust_id=11730508

oauth::token_requester req;


struct by_user: unary_function<const twitter::tweet_object &, bool> {
	by_user(const string &name): name(name) { }
	bool operator ()(const twitter::tweet_object &tw) const { return tw.user_->screen_name_ == name; }
	const string name;
};

struct console_handler {
	typedef console_handler this_type;
	console_handler(asio::io_service &serv, twitter_client &cli): st(serv, get_stdin()), sb(), cli(cli) {
		asio::async_read_until(st, sb, '\n', bind(&this_type::readline_handler, this, _1, _2));
	}
	stream_descriptor st;
	int get_stdin() { return fileno(stdin); }
	asio::streambuf sb;
	twitter_client &cli;
	void readline_handler(const error_code &ec, size_t) {
		asio::detail::throw_error(ec);
		std::istream is(&sb);
		string cmd, args;
		is >> cmd;
		(void)std::istream::sentry(is);
		getline(is, args);
		if (cmd == "get") {
			cli.request_friends_timeline(friends_timeline_params());
		}
		if (cmd == "print") {
			const twitter::tweet_store_t::index<twitter::date_tag>::type &date_index = twitter::stores::tweet_store.get<twitter::date_tag>();
			std::ostream_iterator<twitter::tweet_object> out(std::cout, "\n");
			if (args.empty()) boost::copy(date_index, out);
			else if (args[0] == '@') boost::copy(date_index | boost::adaptors::filtered(by_user(args.substr(1))), out);
			else if (args == "users") boost::copy(twitter::stores::user_store.get<twitter::id_tag>(), std::ostream_iterator<twitter::twitter_user_object>(std::cout, "\n"));
			std::cout << std::flush;
		}
		if (cmd == "post") {
			cli.statuses_update(args, statuses_update_params());
		}
		if (cmd == "exit") {
			cli.shutdown();
			return;
		}
		asio::async_read_until(st, sb, "\n", bind(&this_type::readline_handler, this, _1, _2));
	}
};


void do_initial_auth(asio::io_service &service, asio::ssl::context &ctx, oauth::token_requester &req);
int main(int argc, char **argv) try {

	setlocale(LC_ALL, "C");
	setlocale(LC_CTYPE, "");
	try {
		locale::global(locale(locale::classic(), "", locale::ctype));
	} catch (...) {
	}

	namespace po = boost::program_options;
	using asio::ip::tcp;

	string access_token;
	string token_secret;

	po::options_description desc("");
	desc.add_options()
		("help", "display help message")
		("auth", "get access token")
		("locale", "use system locale for I/O")
		("token", po::value<string>(&access_token), "access token")
		("secret", po::value<string>(&token_secret), "token secret")
	;
	po::variables_map vm;
	po::store(po::parse_command_line(argc, argv, desc), vm);
	po::notify(vm);

	req.set_consumer(consumer_key, consumer_secret);

	const bool use_locale = vm.count("locale") != 0;
	
	if (vm.count("help")) {
		std::cout << desc << std::endl;
		return 0;
	}

	asio::io_service service;
	asio::ssl::context context(service, asio::ssl::context::sslv23);

	if (vm.count("auth")) {
		do_initial_auth(service, context, req);
		return 0;
	}
	if (access_token.empty() || token_secret.empty()) {
		std::cout << "token and secret must be specified\n";
		return 1;
	}
	{
		req.set_token(access_token, token_secret);
		twitter_client cli(service, context, req);
		output_handler out(service, use_locale);
		console_handler ch(service, cli);
		while (1) {
			try {
				service.run();
				break;
			} catch (boost::exception &e) {
				std::cerr << "Execption Thrown:\n";
				std::cerr << diagnostic_information(e) << '\n';
			} catch (std::exception &e) {
				std::cerr << "Execption Thrown:\n";
				std::cerr << e.what() << '\n';
			} catch (...) {
				std::cerr << "Execption Thrown\n";
			}
		}
	}
	return 0;
} catch (boost::exception &e) {
	std::cerr << "Execption Thrown:\n";
	std::cerr << diagnostic_information(e) << '\n';
	return 200;
} catch (const std::exception &e) {
	std::cerr << "Execption Thrown:\n";
	std::cerr << e.what() << '\n';
	return 200;
} catch (...) {
	std::cerr << "Execption Thrown\n";
	return 200;
}

void auth_pin_handler(oauth::token_requester &req, const shared_ptr<httpc::http_client> &p, const shared_ptr<httpc::http_response_data> &data) {
	if (!data) return;
	if (data->status.first != 200) throw std::runtime_error("Authorization Failed");
	map<string, string> par = httpc::split_params(data->data);
	std::cout << "Authorization Succeed. Re-Launch with these parameters:\n";
	std::cout << " --token=" << par["oauth_token"] << " --secret=" << par["oauth_token_secret"] << std::endl;
}
void auth_req_handler(asio::io_service &service, asio::ssl::context &ctx, oauth::token_requester &req, const shared_ptr<httpc::http_client> &p, const shared_ptr<httpc::http_response_data> &data) {
	if (!data) return;
	if (data->status.first != 200) throw std::runtime_error("Authorization Failed");
	map<string, string> par = httpc::split_params(data->data);
	std::cout << "Access to https://twitter.com/oauth/authorize?oauth_token=" << par["oauth_token"] << " and input PIN number." << std::endl;

	unsigned pin_number;
	std::cin >> pin_number;
	req.set_token(par["oauth_token"], par["oauth_token_secret"]).set_pin_number(boost::lexical_cast<string>(pin_number));
	{
		shared_ptr<httpc::http_client> c(httpc::make_http_client(service, bind(&auth_pin_handler, ref(req), _1, _2), "twitter.com", ctx));
		c->start_read();
		c->header["Connection"] = "close";
		c->path = "/oauth/access_token";
		c->query = req.gen_request_signature(c->get_uri(), "GET").gen_getquery();
		c->request();
	}
}

void do_initial_auth(asio::io_service &service, asio::ssl::context &ctx, oauth::token_requester &req) {
	const string request_url = req.gen_request_signature(request_uri, "GET").gen_geturl();
	{
		shared_ptr<httpc::http_client> c(httpc::make_http_client(service, bind(&auth_req_handler, ref(service), ref(ctx), ref(req), _1, _2), "twitter.com", ctx));
		c->start_read();
		c->header["Connection"] = "close";
		c->path = "/oauth/request_token";
		c->query = req.gen_request_signature(c->get_uri(), "GET").gen_getquery();
		c->request();
	}
	service.run();
}

NAMESPACE_CIRCLE_END

int main(int argc, char **argv) { return circle::main(argc, argv); }
