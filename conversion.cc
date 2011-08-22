#include "common.hpp"
#include "conversion.hpp"


#if defined(_WIN32) || defined(__CYGWIN__)
#include <windows.h>
#else
#include <iconv.h>
#include <boost/scope_exit.hpp>
#endif


NAMESPACE_CIRCLE_BEGIN

#if defined(_WIN32) || defined(__CYGWIN__)

vector<wchar_t> extern_to_wide(io_encoding coding, const string &seq) {
	DWORD code = coding == enc_utf_8 ? CP_UTF8 : CP_ACP;
	vector<wchar_t> w(MultiByteToWideChar(code, 0, seq.data(), static_cast<int>(seq.size()), 0, 0));
	if (w.empty()) return w;
	MultiByteToWideChar(code, 0, &seq[0], static_cast<int>(seq.size()), &w[0], static_cast<int>(w.size()));
	return w;
}

string wide_to_extern(io_encoding coding, const vector<wchar_t> &seq) {
	DWORD code = coding == enc_utf_8 ? CP_UTF8 : CP_ACP;
	vector<char> r(WideCharToMultiByte(code, 0, &seq[0], static_cast<int>(seq.size()), 0, 0, 0, 0));
	if (r.empty()) return string();
	WideCharToMultiByte(code, 0, &seq[0], static_cast<int>(seq.size()), &r[0], static_cast<int>(r.size()), 0, 0);
	return string(r.begin(), r.end());
}

string utf8_to_extern(io_encoding coding, const string &seq) {
	if (coding == enc_utf_8 || seq.empty()) return seq;
	return wide_to_extern(coding, extern_to_wide(enc_utf_8, seq));
}
string extern_to_utf8(io_encoding coding, const string &seq) {
	if (coding == enc_utf_8 || seq.empty()) return seq;
	return wide_to_extern(enc_utf_8, extern_to_wide(coding, seq));
}
#else

inline vector<wchar_t> narrow_to_wide(const string &str, const char *const enc = "") {
	iconv_t cd = iconv_open("wchar_t", enc);
#if defined(_LIBICONV_VERSION) && _LIBICONV_VERSION >= 0x0108
	{
		int c = 0;
		iconvctl(cd, ICONV_SET_DISCARD_ILSEQ, &c);
	}
#endif
	BOOST_SCOPE_EXIT((&cd)) {
		iconv_close(cd);
	} BOOST_SCOPE_EXIT_END;
	char *inbuf = const_cast<char *>(str.c_str());
	size_t inbytesleft = str.length();
	vector<wchar_t> ret;
	do {
		array<char, 1024> buf;
		char *outbuf = buf.data();
		size_t outbytesleft = buf.size();
		const int r = iconv(cd, &inbuf, &inbytesleft, &outbuf, &outbytesleft);
		ret.insert(ret.end(), reinterpret_cast<wchar_t *>(buf.data()), reinterpret_cast<wchar_t *>(outbuf));
		if (r<0) ++inbuf, --inbytesleft;
	} while (inbytesleft > 0);
	return ret;
}
inline string wide_to_narrow(const vector<wchar_t> &seq, const char *const enc = "") {
	iconv_t cd = iconv_open(enc, "wchar_t");
#if defined(_LIBICONV_VERSION) && _LIBICONV_VERSION >= 0x0108
	{
		int c = 0;
		iconvctl(cd, ICONV_SET_DISCARD_ILSEQ, &c);
	}
#endif
	BOOST_SCOPE_EXIT((&cd)) {
		iconv_close(cd);
	} BOOST_SCOPE_EXIT_END;
	char *inbuf = reinterpret_cast<char *>(const_cast<wchar_t *>(&seq.front()));
	size_t inbytesleft = seq.size() * sizeof(wchar_t);
	string ret;
	do {
		array<char, 1024> buf;
		char *outbuf = buf.data();
		size_t outbytesleft = buf.size();
		const int r = iconv(cd, &inbuf, &inbytesleft, &outbuf, &outbytesleft);
		ret.insert(ret.end(), buf.data(), outbuf);
		if (r<0) ++inbuf, --inbytesleft;
	} while (inbytesleft > 0);
	return ret;
}
inline string narrow_to_narrow(const string &str, const char *const from, const char *const to) {
	iconv_t cd = iconv_open(to, from);
#if defined(_LIBICONV_VERSION) && _LIBICONV_VERSION >= 0x0108
	{
		int c = 0;
		iconvctl(cd, ICONV_SET_DISCARD_ILSEQ, &c);
	}
#endif
	BOOST_SCOPE_EXIT((&cd)) {
		iconv_close(cd);
	} BOOST_SCOPE_EXIT_END;
	char *inbuf = const_cast<char *>(str.c_str());
	size_t inbytesleft = str.length();
	string ret;
	do {
		array<char, 1024> buf;
		char *outbuf = buf.data();
		size_t outbytesleft = buf.size();
		const int r = iconv(cd, &inbuf, &inbytesleft, &outbuf, &outbytesleft);
		ret.insert(ret.end(), buf.data(), outbuf);
		if (r<0) ++inbuf, --inbytesleft;
	} while (inbytesleft > 0);
	return ret;
}

vector<wchar_t> extern_to_wide(io_encoding coding, const string &seq) {
	return narrow_to_wide(seq, coding == enc_utf_8 ? "UTF-8" : "");
}

string wide_to_extern(io_encoding coding, const vector<wchar_t> &seq) {
	return wide_to_narrow(seq, coding == enc_utf_8 ? "UTF-8" : "");
}

string utf8_to_extern(io_encoding coding, const string &seq) {
	if (coding == enc_utf_8 || seq.empty()) return seq;
	return narrow_to_narrow(seq, "UTF-8", "");
}
string extern_to_utf8(io_encoding coding, const string &seq) {
	if (coding == enc_utf_8 || seq.empty()) return seq;
	return narrow_to_narrow(seq, "", "UTF-8");
}

#endif

NAMESPACE_CIRCLE_END

