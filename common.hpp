#ifndef COMMON_HPP_
#define COMMON_HPP_

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#endif

#ifndef __USE_W32_SOCKETS
#define __USE_W32_SOCKETS
#endif

//#ifndef BOOST_ASIO_SEPARATE_COMPILATION
//#define BOOST_ASIO_SEPARATE_COMPILATION
//#endif

#include <stdlib.h>
#include <stddef.h>
#include <string>
#include <vector>
#include <map>
#include <vector>
#include <deque>
#include <algorithm>
#include <memory>
#include <functional>
#include <locale>
#include <boost/cstdint.hpp>
#if defined(_MSC_VER) && _MSC_VER == 1600
#else
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/make_shared.hpp>
#include <boost/function.hpp>
#include <boost/bind.hpp>
#include <boost/ref.hpp>
#endif
#include <boost/version.hpp>
#include <boost/array.hpp>
#include <boost/system/error_code.hpp>
#include <boost/thread.hpp>

namespace boost {
	namespace asio {
#if defined(_WIN32) && defined(__MINGW32__) && defined(_W64) 
		typedef long SOCKET;
#endif
	}
}

#define NAMESPACE_CIRCLE_BEGIN namespace circle {
#define NAMESPACE_CIRCLE_END }

NAMESPACE_CIRCLE_BEGIN
using std::string;
using std::vector;
using std::map;
using std::pair;
using std::deque;
using std::unary_function;
using std::binary_function;
using std::locale;
using boost::uint64_t;
using boost::uint32_t;
using boost::int32_t;
using boost::int64_t;
using boost::thread;
using boost::mutex;
#if defined(_MSC_VER) && _MSC_VER == 1600
using std::tr1::shared_ptr;
using std::tr1::weak_ptr;
using std::tr1::make_shared;
using std::tr1::enable_shared_from_this;
using std::tr1::function;
using std::tr1::bind;
using std::tr1::placeholders::_1;
using std::tr1::placeholders::_2;
using std::tr1::placeholders::_3;
using std::tr1::ref;
#else
using boost::shared_ptr;
using boost::weak_ptr;
using boost::make_shared;
using boost::enable_shared_from_this;
using boost::function;
using boost::bind;
using ::_1;
using ::_2;
using ::_3;
using boost::ref;
#endif
using boost::array;
namespace asio = boost::asio;
using boost::system::error_code;
NAMESPACE_CIRCLE_END

#if defined(__CYGWIN__) && defined(__GNUC__) && __GNUC__ < 4
namespace std { typedef basic_string<wchar_t> wstring; }
#endif

#endif
