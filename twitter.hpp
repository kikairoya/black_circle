#ifndef TWITTER_H_
#define TWITTER_H_

#include <sstream>
#include <stdlib.h>
#include <boost/date_time.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/c_local_time_adjustor.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/optional.hpp>

#include "json_parser.hpp"

NAMESPACE_CIRCLE_BEGIN
namespace twitter {
	struct color_t {
		color_t(): value() { }
		color_t(const string &s): value(strtoul(s.c_str(), 0, 16)) { }
		color_t &operator =(const string &s) { value = strtoul(s.c_str(), 0, 16); return *this; }
		uint32_t value;
	};

	namespace detail {
		struct datetime_convert_locale_t {
			typedef boost::posix_time::time_input_facet time_input_facet;
			typedef boost::posix_time::time_facet time_facet;
			datetime_convert_locale_t(): in(locale::classic(), make_in_facet()), out(locale::classic(), make_out_facet()) { }
			static time_input_facet *make_in_facet() {
				time_input_facet *p = new time_input_facet();
				p->format("%a %b %d %H:%M:%S +0000 %Y");
				return p;
			}
			static time_facet *make_out_facet() {
				time_facet *p = new time_facet();
				p->format("%Y-%m-%d %H:%M:%S %z");
				return p;
			}
			locale in;
			locale out;
		};
		extern const datetime_convert_locale_t datetime_convert_locale;
	}

	struct datetime_t {
		datetime_t(): time(boost::posix_time::second_clock::universal_time()) { }
		datetime_t(const string &rfcdate) {
			std::stringstream ss(rfcdate);
			ss.imbue(detail::datetime_convert_locale.in);
			ss >> time;
		}
		string to_string() const {
			std::stringstream ss;
			ss.imbue(detail::datetime_convert_locale.out);
			ss << boost::date_time::c_local_adjustor<boost::posix_time::ptime>::utc_to_local(time);
			return ss.str();
		}
		boost::posix_time::ptime time;
		bool operator ==(const datetime_t &x) const { return time == x.time; }
		bool operator <(const datetime_t &x) const { return time < x.time; }
	};

	namespace midx = boost::multi_index;

	struct id_tag { };
	struct name_tag { };
	struct date_tag { };

	struct twitter_user_object {
		string url_;
		string description_;
		uint64_t followers_count_;
		string time_zone_;
		string location_;
		bool notifications_;
		uint64_t friends_count_;
		color_t profile_sidebar_border_color_;
		string profile_image_url_;
		uint64_t statuses_count_;
		color_t profile_link_color_;
		color_t profile_background_color_;
		string lang_;
		string profile_background_image_url_;
		uint64_t favourites_count_;
		color_t profile_text_color_;
		string screen_name_;
		bool contributors_enabled_;
		bool geo_enabled_;
		bool profile_background_tile_;
		bool protected_;
		bool following_;
		datetime_t created_at_;
		color_t profile_sidebar_fill_color_;
		string name_;
		bool verified_;
		uint64_t id_;
		int64_t utc_offset_;
		twitter_user_object() { }
		twitter_user_object(const json::json_object_map &o);
	};
	typedef midx::multi_index_container<
		twitter_user_object,
		midx::indexed_by<
			midx::ordered_unique< midx::tag<id_tag>,   midx::member<twitter_user_object, uint64_t, &twitter_user_object::id_> >,
			midx::ordered_unique< midx::tag<name_tag>, midx::member<twitter_user_object, string,   &twitter_user_object::screen_name_> >
			>
		> user_store_t;

	struct user_proxy {
		uint64_t id_;
		const twitter_user_object *operator ->() const { return get_ptr(); }
		const twitter_user_object &operator *() const { return get(); }
		const twitter_user_object *get_ptr() const;
		const twitter_user_object &get() const;
	};

	struct tweet_object {
		user_proxy user_;
		string text_;
		boost::optional<uint64_t> in_reply_to_status_id_;
		boost::optional<uint64_t> retweeted_status_id_;
		uint64_t id_;
		bool truncated_;
		bool favorited_;
		datetime_t created_at_;
		string source_;
		boost::optional<uint64_t> in_reply_to_user_id_;
		tweet_object() { }
		tweet_object(const json::json_object_map &o);
	};

	typedef midx::multi_index_container<
		tweet_object,
		midx::indexed_by<
			midx::ordered_unique    < midx::tag<id_tag>,   midx::member<tweet_object, uint64_t,   &tweet_object::id_> >,
			midx::ordered_non_unique< midx::tag<date_tag>, midx::member<tweet_object, datetime_t, &tweet_object::created_at_> >
			>
		> tweet_store_t;

	struct stores {
		static user_store_t user_store;
		static tweet_store_t tweet_store;
	};

	inline const twitter_user_object *user_proxy::get_ptr() const { return &*stores::user_store.get<id_tag>().find(id_); }
	inline const twitter_user_object &user_proxy::get() const { return *stores::user_store.get<id_tag>().find(id_); }
	uint64_t insert_new_tweet(const json::json_object_map &o);

	struct tweet_event {
		enum event_type_t {
			tw_tweeted,
			tw_removed,
			dm_sent,
			dm_removed,
			followed,
			blocked,
			unblocked,
			favorited,
			unfavorited,
			retweeted,
			unretweeted,
			list_member_added,
			list_member_removed,
			list_created,
			list_updated,
			list_destroyed,
			subscription_added,
			subscription_removed,
			profile_updated,
			unknown
		} event_type;
	};

	inline std::ostream &operator <<(std::ostream &os, const twitter::twitter_user_object &user) {
		return os << '@' << user.screen_name_;
	}

	inline std::ostream &operator <<(std::ostream &os, const twitter::tweet_object &tw) {
		return os << tw.created_at_.to_string() << ' ' << *tw.user_ << ": " << tw.text_;
	}

}
NAMESPACE_CIRCLE_END

#endif
