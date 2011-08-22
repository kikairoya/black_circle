#include <common.hpp>
#include "json_parser.hpp"
#define BOOST_TEST_MAIN
#include <boost/test/unit_test.hpp>
#include <boost/lexical_cast.hpp>

using namespace circle;

template <typename T>
inline T clone(const T &v) { return v; }

template <typename T>
struct object_type { typedef T type; };
template <> struct object_type<int> { typedef circle::int64_t type; };
template <> struct object_type<unsigned> { typedef circle::int64_t type; };
template <> struct object_type<char *> { typedef string type; };
template <> struct object_type<const char *> { typedef string type; };
template <size_t N> struct object_type<char [N]> { typedef string type; };
template <size_t N> struct object_type<const char [N]> { typedef string type; };

struct insert_object_t {
	insert_object_t(json::json_object_map &obj, string &jstr): obj(obj), jstr(jstr), n() { }
	template <typename T>
	void operator()(const T &val, const string &vstr) {
		const string key = "val" + boost::lexical_cast<string>(++n);
		obj[key] = static_cast<typename object_type<T>::type>(val);
		jstr.insert(jstr.length()-1, ",\""+key+"\":"+vstr);
	}
	json::json_object_map &obj;
	string &jstr;
	unsigned n;
};

BOOST_AUTO_TEST_CASE(json_parse_test) {
	BOOST_CHECK(json::parse_json("[]") == json::json_object_value(json::json_array_type()));
	json::json_object_value obj((json::json_object_map()));
	json::json_object_map &om = boost::get<json::json_object_map>(obj);

	BOOST_CHECK(json::parse_json("{}") == obj);
	string jstr = "{\"null\":null}";
	insert_object_t ins(om, jstr);
	om["null"] = json::null_t();
	BOOST_CHECK(json::parse_json(jstr) == obj);

	ins(10, "10");
	BOOST_CHECK(json::parse_json(jstr) == obj);
	ins(10., "10.");
	BOOST_CHECK(json::parse_json(jstr) == obj);
	ins("str", "\"str\"");
	BOOST_CHECK(json::parse_json(jstr) == obj);
	ins("", "\"\"");
	BOOST_CHECK(json::parse_json(jstr) == obj);
	ins(true, "true");
	BOOST_CHECK(json::parse_json(jstr) == obj);
	ins(false, "false");
	BOOST_CHECK(json::parse_json(jstr) == obj);
	ins(0, "0");
	BOOST_CHECK(json::parse_json(jstr) == obj);
	ins(clone(obj), clone(jstr));
	BOOST_CHECK(json::parse_json(jstr) == obj);
	ins(-5, "-5");
	ins(-3., "-3.");
	ins("3.3", "\"3.3\"");
	ins("null", "\"null\"");
	BOOST_CHECK(json::parse_json(jstr) == obj);
	ins(clone(obj), clone(jstr));
	BOOST_CHECK(json::parse_json(jstr) == obj);
}
