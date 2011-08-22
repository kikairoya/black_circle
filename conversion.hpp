#ifndef CONVERTING_STREAM_HPP_
#define CONVERTING_STREAM_HPP_

NAMESPACE_CIRCLE_BEGIN
enum io_encoding {
	enc_utf_8,
	enc_system
};

vector<wchar_t> extern_to_wide(io_encoding coding, const string &seq);
string wide_to_extern(io_encoding coding, const vector<wchar_t> &seq);
string utf8_to_extern(io_encoding coding, const string &seq);
string extern_to_utf8(io_encoding coding, const string &seq);
NAMESPACE_CIRCLE_END

#endif
