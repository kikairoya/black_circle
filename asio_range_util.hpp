#ifndef ASIO_RANGE_UTIL_HPP_
#define ASIO_RANGE_UTIL_HPP_

#include <boost/range/iterator_range.hpp>
#include <boost/asio/buffer.hpp>

NAMESPACE_CIRCLE_BEGIN
namespace httpc {
	template <typename T>
	inline boost::iterator_range<T *> make_iterator_range_from_memory(T *head, size_t size) {
		return boost::make_iterator_range(head, head + size);
	}
	template <typename T>
	inline boost::iterator_range<const T *> make_iterator_range_from_memory(const T *head, size_t size) {
		return boost::make_iterator_range(head, head + size);
	}
	template <typename Ptr>
	inline boost::iterator_range<Ptr> make_iterator_range_from_buffer(const asio::const_buffer &b) {
		return make_iterator_range_from_memory(asio::buffer_cast<Ptr>(b), asio::buffer_size(b));
	}
	struct lazy_range_search_t {
		template <typename R, typename S>
		struct result { typedef  typename boost::range_iterator<const R>::type type; };
		template <typename R, typename S>
		typename result<R, S>::type operator ()(const R &r, const S &s) const {
			return std::search(boost::begin(r), boost::end(r), boost::begin(s), boost::end(s));
		}
	};

	template <typename R, typename EndFinder>
	inline R make_partial_range(R r, const EndFinder &fn) { return R(boost::begin(r), fn(r)); }
}
NAMESPACE_CIRCLE_END

#endif
