#include "common.hpp"

#include "twitter.hpp"

NAMESPACE_CIRCLE_BEGIN
namespace twitter {
	namespace detail {
		const datetime_convert_locale_t datetime_convert_locale;
	}
	user_store_t stores::user_store;
	tweet_store_t stores::tweet_store;

	template <typename T> struct expand_repl_type { typedef T type; };
	template <> struct expand_repl_type<int> { typedef int64_t type; };
	template <> struct expand_repl_type<uint64_t> { typedef int64_t type; };
	template <> struct expand_repl_type<color_t> { typedef string type; };
	template <> struct expand_repl_type<datetime_t> { typedef string type; };
	template <> struct expand_repl_type<twitter_user_object> { typedef json::json_object_map type; };
	template <> struct expand_repl_type<tweet_object> { typedef json::json_object_map type; };
	template <typename T> struct expand_repl_type< boost::optional<T> > { typedef typename expand_repl_type<T>::type type; };

	template <typename T>
	inline bool set_if_found(T &v, const json::json_object_map &o, const string &name) {
		const json::json_object_map::const_iterator ite = o.find(name);
		if (ite == o.end()) return false;
		if (boost::get<json::null_t>(&ite->second)) return false; // if null
		typedef typename expand_repl_type<T>::type replT;
		if (const replT *const p = boost::get<replT>(&ite->second)) {
			v = *p;
			return true;
		}
		return false;
	}
#define TW_SET_IF_FOUND(name, object) set_if_found(name##_, object, #name)

	twitter_user_object::twitter_user_object(const json::json_object_map &o) {
		TW_SET_IF_FOUND(url, o);
		TW_SET_IF_FOUND(description, o);
		TW_SET_IF_FOUND(followers_count, o);
		TW_SET_IF_FOUND(time_zone, o);
		TW_SET_IF_FOUND(location, o);
		TW_SET_IF_FOUND(notifications, o);
		TW_SET_IF_FOUND(friends_count, o);
		TW_SET_IF_FOUND(profile_sidebar_border_color, o);
		TW_SET_IF_FOUND(profile_image_url, o);
		TW_SET_IF_FOUND(statuses_count, o);
		TW_SET_IF_FOUND(profile_link_color, o);
		TW_SET_IF_FOUND(profile_background_color, o);
		TW_SET_IF_FOUND(lang, o);
		TW_SET_IF_FOUND(profile_background_image_url, o);
		TW_SET_IF_FOUND(favourites_count, o);
		TW_SET_IF_FOUND(profile_text_color, o);
		TW_SET_IF_FOUND(screen_name, o);
		TW_SET_IF_FOUND(contributors_enabled, o);
		TW_SET_IF_FOUND(geo_enabled, o);
		TW_SET_IF_FOUND(profile_background_tile, o);
		TW_SET_IF_FOUND(protected, o);
		TW_SET_IF_FOUND(following, o);
		TW_SET_IF_FOUND(created_at, o);
		TW_SET_IF_FOUND(profile_sidebar_fill_color, o);
		TW_SET_IF_FOUND(name, o);
		TW_SET_IF_FOUND(verified, o);
		TW_SET_IF_FOUND(id, o);
		TW_SET_IF_FOUND(utc_offset, o);
	}

	tweet_object::tweet_object(const json::json_object_map &o)  {
		bool b = true;
		twitter_user_object user_;
		if (b &= TW_SET_IF_FOUND(user, o)) {
			stores::user_store.insert(user_);
			this->user_.id_ = user_.id_;
		}
		tweet_object retweeted_status_;
		if (TW_SET_IF_FOUND(retweeted_status, o)) {
			stores::tweet_store.insert(retweeted_status_);
			retweeted_status_id_ = retweeted_status_.id_;
		}
		b &= TW_SET_IF_FOUND(text, o);
		b &= TW_SET_IF_FOUND(id, o);
		b &= TW_SET_IF_FOUND(created_at, o);
		TW_SET_IF_FOUND(in_reply_to_status_id, o);
		TW_SET_IF_FOUND(retweeted_status_id, o);
		TW_SET_IF_FOUND(truncated, o);
		TW_SET_IF_FOUND(favorited, o);
		TW_SET_IF_FOUND(source, o);
		TW_SET_IF_FOUND(in_reply_to_user_id, o);
		if (!b) throw std::runtime_error("properties not found");
		std::replace(text_.begin(), text_.end(), '\r', '\n');
	}

	uint64_t insert_new_tweet(const json::json_object_map &o) {
		tweet_object t(o);
		stores::tweet_store.insert(t);
		return t.id_;
	}
}
NAMESPACE_CIRCLE_END
