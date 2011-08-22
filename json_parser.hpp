#ifndef JSON_PARSER_
#define JSON_PARSER_

#include <boost/variant.hpp>

#if BOOST_VERSION <= 104601
namespace boost { struct recursive_variant_ { }; }
#endif

NAMESPACE_CIRCLE_BEGIN
namespace json {
	struct null_t {
		bool operator ==(const null_t &) const { return true; }
		bool operator !=(const null_t &) const { return false; }
	};

	typedef boost::make_recursive_variant<
		vector<boost::recursive_variant_>,
		string,
		int64_t,
		double,
		map<string, boost::recursive_variant_>,
		bool,
		null_t
	>::type json_object_value;
	typedef pair<string, json_object_value> json_object;
	typedef map<string, json_object_value> json_object_map;
	typedef vector<json_object_value> json_array_type;

	json_object_value parse_json(const string &s);
}
NAMESPACE_CIRCLE_END

#endif
