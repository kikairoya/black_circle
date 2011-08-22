#ifndef UTIL_HPP_
#define UTIL_HPP_

#include <boost/mpl/bool.hpp>
#include <boost/mpl/if.hpp>
#include <boost/range.hpp>
#include <boost/range/numeric.hpp>

NAMESPACE_CIRCLE_BEGIN
namespace util {
	namespace detail {
		template <typename Range>
		struct has_range_buffer: boost::mpl::false_ { };
		template <typename charT, typename Traits, typename Alloc>
		struct has_range_buffer< std::basic_string<charT, Traits, Alloc> >: boost::mpl::true_ { };
		template <typename T, typename Alloc>
		struct has_range_buffer< vector<T, Alloc> >: boost::mpl::true_ { };
		template <typename T, size_t N>
		struct has_range_buffer< T [N] >: boost::mpl::true_ { };
		template <typename T, size_t N>
		struct has_range_buffer< array<T, N> >: boost::mpl::true_ { };
	}
	template <typename Range>
	struct range_buffer: boost::mpl::if_< detail::has_range_buffer<Range>, Range, std::vector<typename boost::range_value<Range>::type> > { };

	template <typename Range>
	inline typename boost::enable_if<typename detail::has_range_buffer<Range>::type, typename boost::range_pointer<Range>::type>::type
		get_ptr(Range &r) { return &*boost::begin(r); }
	template <typename Range>
	inline typename boost::enable_if<typename detail::has_range_buffer<Range>::type, typename boost::range_pointer<const Range>::type>::type
		get_ptr(const Range &r) { return &*boost::begin(r); }

	template <typename Range>
	inline typename boost::enable_if<typename detail::has_range_buffer<Range>::type, Range &>::type
		get_buffer(Range &r) { return r; }
	template <typename Range>
	inline typename boost::enable_if<typename detail::has_range_buffer<Range>::type, const Range &>::type
		get_buffer(const Range &r) { return r; }
	template <typename Range>
	inline typename boost::disable_if<typename detail::has_range_buffer<Range>::type, typename range_buffer<Range>::type>::type
		get_buffer(const Range &r) { return typename range_buffer<Range>::type(boost::begin(r), boost::end(r)); }

	struct hexanizer: binary_function<const string &, char, string> {
		hexanizer(const string &head): head(head) { }
		string operator ()(const string &s, char c) const {
			static const char hex[] = "0123456789ABCDEF";
			return s + head + hex[(c>>4)&0xF] + hex[c&0xF];
		}
		string head;
	};
	namespace detail {
		inline bool isalnum(int c) { return ('0'<=c && c<='9') || ('a'<=c && c<='z') || ('A'<=c && c<='Z'); }
		struct do_escape: binary_function<const string &, char, string> {
			do_escape(): hexz("%") { }
			string operator ()(const string &str, char c) {
				if (isalnum(c) || c=='-' || c=='.' || c=='_' || c=='~') return str + c;
				return hexz(str, c);
			}
			hexanizer hexz;
		};
	}
	template <typename Range>
	inline string url_escape(const Range &s) {
		return boost::accumulate(s, string(), detail::do_escape());
	}
	template <typename Range>
	struct base64_encoder: unary_function<const Range &, string> {
		static char base64_convert_single(char c) {
			if (c < 0) return '=';
			else if (c < 26) return 'A' + c;
			else if (c < 52) return 'a' + c - 26;
			else if (c < 62) return '0' + c - 52;
			else if (c == 62) return '+';
			else if (c == 63) return '/';
			else return '=';
		}
		string operator ()(const Range &src) const {
			typedef typename boost::range_iterator<const Range>::type iterator;
			const iterator end = boost::end(src);
			iterator ite = boost::begin(src);
			string ret;
			while (ite != end) {
				uint32_t tmp = 0;
				int pad = 0;
				for (int n = 0; n < 3; ++n) {
					tmp <<= 8;
					if (ite != end) tmp |= *ite++;
					else ++pad;
				}
				ret += base64_convert_single((tmp>>18) & 0x3F);
				ret += base64_convert_single((tmp>>12) & 0x3F);
				ret += pad > 1 ? '=' : base64_convert_single((tmp>> 6) & 0x3F);
				ret += pad > 0 ? '=' : base64_convert_single((tmp>> 0) & 0x3F);
			}
			return ret;
		}
	};
	template <typename Range>
	inline string base64_encode(const Range &src) {
		return base64_encoder<Range>()(src);
	}

	template <typename Map>
	inline const typename Map::mapped_type &map_get_or(
		const Map &m,
		const typename Map::key_type &k,
		const typename Map::mapped_type &v
	) {
		const typename Map::const_iterator ite = m.find(k);
		if (ite == m.end()) return v;
		return ite->second;
	}
}
NAMESPACE_CIRCLE_END

#endif
