#ifndef OAUTH_HPP_
#define OAUTH_HPP_

#include <boost/lexical_cast.hpp>
#include <openssl/hmac.h>
#include <openssl/md5.h>
#include <openssl/rand.h>
#include "util.hpp"

NAMESPACE_CIRCLE_BEGIN

namespace oauth {
	namespace detail {
		template <typename KeyRange, typename DataRange>
		inline vector<unsigned char> sign_hmac_sha1(const KeyRange &key, const DataRange &message) {
			using namespace util;
			using boost::size;
			const typename range_buffer<KeyRange>::type &key_buf = get_buffer(key);
			const typename range_buffer<DataRange>::type &msg_buf = get_buffer(message);
			vector<unsigned char> md(EVP_MAX_MD_SIZE);
			unsigned len = static_cast<unsigned>(md.size());
			if (!HMAC(EVP_sha1(), get_ptr(key_buf), static_cast<int>(size(key)), reinterpret_cast<const unsigned char *>(get_ptr(msg_buf)), size(msg_buf), &md[0], &len)) {
				throw 0;
			}
			md.resize(len);
			return md;
		}
		inline string gen_nonce() {
			static const size_t rand_len = 16;
			vector<unsigned char> a(rand_len);
			RAND_pseudo_bytes(&a[0], static_cast<int>(a.size()));
			const string t = boost::lexical_cast<string>(time(0));
			a.insert(a.end(), t.begin(), t.end());
			array<unsigned char, MD5_DIGEST_LENGTH> d;
			MD5(&a[0], a.size(), d.data());
			return boost::accumulate(d, string(), util::hexanizer(""));
		}

		inline string get_time() { return boost::lexical_cast<string>(time(0)); }
	}

	class token_requester {
	public:
		typedef map<string, string> param_type;
		typedef param_type::value_type param_pair;
		token_requester() {
			params["oauth_signature_method"] = "HMAC-SHA1";
			params["oauth_version"] = "1.0";
		}
		string gen_geturl() const {
			return req_uri + '?' + make_request(combine_as_get_param());
		}
		string gen_getquery() const {
			return make_request(combine_as_get_param());
		}
		string gen_postparam() const {
			return make_request(combine_as_post_param());
		}
		token_requester &gen_request_signature(const string &req_uri, const string &method) {
			using namespace util;
			using namespace detail;
			this->req_uri = req_uri;
			params["oauth_nonce"] = gen_nonce();
			params["oauth_timestamp"] = get_time();
			params.erase("oauth_signature");
			const string message = method + '&' + url_escape(req_uri) + '&' + url_escape(make_request(combine_as_get_param()));
			params["oauth_signature"] = base64_encode(sign_hmac_sha1(consumer_secret + '&' + token_secret, message));
			return *this;
		}
	private:
		struct combine_as_get_param: unary_function<const param_pair &, string> {
			string operator ()(const param_pair &p) const { return p.first + '=' + util::url_escape(p.second); }
			static const char *joiner() { return "&"; }
		};
		struct combine_as_post_param: unary_function<const param_pair &, string> {
			string operator ()(const param_pair &p) const { return p.first + "=\"" + util::url_escape(p.second) + '"'; }
			static const char *joiner() { return ", "; }
		};
		template <typename Combiner>
		struct combine_params: binary_function<const string &, const param_pair &, string> {
			combine_params(const Combiner &cmb): cmb(cmb) { }
			string operator ()(const string &s, const param_pair &p) const {
				return s + cmb.joiner() + cmb(p);
			}
			const Combiner &cmb;
		};
		template <typename Combiner>
		string make_request(const Combiner &cmb) const {
			return std::accumulate(boost::next(params.begin()), params.end(), cmb(*params.begin()), combine_params<Combiner>(cmb));
		}
	public:
		token_requester &set_consumer(const string &key, const string &secret) {
			params["oauth_consumer_key"] = key;
			consumer_secret = secret;
			return *this;
		}
		token_requester &set_token(const string &key, const string &secret) {
			params["oauth_token"] = key;
			token_secret = secret;
			return *this;
		}
		token_requester &set_pin_number(const string &pin) {
			params["oauth_verifier"] = pin;
			return *this;
		}
		token_requester &clear_pin_number() {
			params.erase("oauth_verifier");
			return *this;
		}
	private:
		string req_uri;
		string consumer_secret;
		string token_secret;
	public:
		param_type params;
	};
}
NAMESPACE_CIRCLE_END
#endif
