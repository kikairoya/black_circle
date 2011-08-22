#include "common.hpp"

#include <boost/range.hpp>
#include <boost/range/numeric.hpp>
#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/phoenix.hpp>
#include "json_parser.hpp"

NAMESPACE_CIRCLE_BEGIN
namespace json {
	namespace spirit = boost::spirit;
	namespace qi = boost::spirit::qi;
	namespace ascii = boost::spirit::ascii;
	namespace phx = boost::phoenix;

	struct ucn_to_utf8_t {
		template <typename, typename>
		struct result { typedef bool type; };
		bool operator ()(uint32_t code, string &res) const {
			deque<char> seq;
			unsigned char head;
			if (0xD800 <= code && code < 0xE000) return false;
			else if (code >= 0x04000000) head = 0xFC | ((code>>30) & 0x01);
			else if (code >= 0x00200000) head = 0xF8 | ((code>>24) & 0x03);
			else if (code >= 0x00010000) head = 0xF0 | ((code>>18) & 0x07);
			else if (code >= 0x00000800) head = 0xE0 | ((code>>12) & 0x0F);
			else if (code >= 0x00000080) head = 0xC0 | ((code>> 6) & 0x1F);
			else head = code;
			while (code >= 0x80) {
				seq.push_front(0x80 | (code&0x3F));
				code >>= 6;
			}
			seq.push_front(head);
			res.assign(seq.begin(), seq.end());
			return true;
		}
	};

	template <typename Iter>
	struct json_parser: qi::grammar<Iter, json_object_value(), ascii::space_type> {
		json_parser(): json_parser::base_type(json_text) {
			json_text %= object_ | array_;
			value_ %= array_ | string_ | (qi::int_parser<int64_t>() >> !qi::lit('.')) | qi::double_ | object_ | qi::bool_ | null_;
			null_ = qi::lit("null")[qi::_val = phx::construct<null_t>()];
			ucn_code = "\\u" > qi::uint_parser<uint32_t, 16, 4, 4>()[qi::_val = qi::_1];
			escaped =
				(qi::lit('&') >>
				 (qi::lit("gt;")[qi::_val = '>'] |
				  qi::lit("lt;")[qi::_val = '<'] | 
				  qi::lit("amp;")[qi::_val = '&'] |
				  qi::eps[qi::_val = '&'])) |
				(qi::lit('\\') >>
				 (qi::char_("bfnrt")[qi::_val = qi::_1 - 'a'] |
				  qi::char_("/\\\"")[qi::_val = qi::_1])) |
				(ucn_code[qi::_pass = ucn_to_utf8(qi::_1, qi::_val)] |
				 (ucn_code > ucn_code)[qi::_pass = ucn_to_utf8(((qi::_1-0xD800)<<10)+(qi::_2-0xDC00)+0x10000, qi::_val)]);
			string_ = qi::lexeme['"' > *(escaped[qi::_val += qi::_1] | (ascii::char_-'"')[qi::_val += qi::_1]) > '"'];
			value_pair = (string_ > ':' > value_)[qi::_val = phx::construct<json_object>(qi::_1, qi::_2)];
			object_ %= '{' > -(value_pair % ',') > '}';
			array_ %= '[' > -(value_ % ',') > ']';
		}
		qi::rule<Iter, json_object_value(), ascii::space_type> json_text;
		qi::rule<Iter, json_object_value(), ascii::space_type> value_;
		qi::rule<Iter, json_array_type(), ascii::space_type> array_;
		qi::rule<Iter, string(), ascii::space_type> string_;
		qi::rule<Iter, json_object_map(), ascii::space_type> object_;
		qi::rule<Iter, json_object(), ascii::space_type> value_pair;
		qi::rule<Iter, null_t(), ascii::space_type> null_;
		qi::rule<Iter, string()> escaped;
		qi::rule<Iter, uint32_t()> ucn_code;
		phx::function<ucn_to_utf8_t> ucn_to_utf8;
	};

	json_object_value parse_json(const string &s) {
		string::const_iterator ite = s.begin();
		json_object_value v;
		json_parser<string::const_iterator> jsonp;
		qi::phrase_parse(ite, s.end(), jsonp, ascii::space, v);
		return v;
	}
}
NAMESPACE_CIRCLE_END
