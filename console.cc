#include "common.hpp"

#include <boost/asio.hpp>
#include <boost/noncopyable.hpp>
#include "console.hpp"
#include "conversion.hpp"

#if defined(_WIN32) && !defined(__CYGWIN__)

#include <io.h>
#include <fcntl.h>
#include <stdio.h>
#include <windows.h>

#else

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>

#endif

NAMESPACE_CIRCLE_BEGIN

#ifndef BOOST_ASIO_HAS_POSIX_STREAM_DESCRIPTOR

class stream_descriptor::stream_descriptor_impl: boost::noncopyable {
	struct object_status: boost::noncopyable {
		object_status(int fd): fd(fd), mtx(), dead(), ev(CreateEvent(0, 0, 0, 0)) { }
		~object_status() { CloseHandle(ev); }
		int fd;
		mutex mtx;
		bool dead;
		HANDLE ev;
	};
public:
	stream_descriptor_impl(asio::io_service &io, int fd): io(io), pst(make_shared<object_status>(fd)) { }
	~stream_descriptor_impl() {
		mutex::scoped_lock lk(pst->mtx);
		SetEvent(pst->ev);
		pst->dead = true;
	}
	void async_read_some(const asio::mutable_buffer &seq, read_handler_type handler) {
		thread(bind(&stream_descriptor_impl::thread_handler, this, asio::buffer_cast<char *>(seq), asio::buffer_size(seq), handler, pst));
	}
	void thread_handler(char *buf, size_t size, read_handler_type handler, shared_ptr<object_status> pst) {
		object_status &st(*pst);
#ifndef __CYGWIN__
		const HANDLE evs[] = { st.ev, reinterpret_cast<HANDLE>(_get_osfhandle(st.fd)) };
		while (GetFileType(evs[1]) == FILE_TYPE_CHAR) {
			if (WaitForMultipleObjects(2, evs, 0, INFINITE) != 1) return;
			INPUT_RECORD ir;
			DWORD d = 0;
			PeekConsoleInput(evs[1], &ir, 1, &d);
			if (d == 1 && (ir.EventType != KEY_EVENT || !ir.Event.KeyEvent.bKeyDown)) {
				ReadConsoleInput(evs[1], &ir, 1, &d);
			} else {
				break;
			}
		}
#endif
		const int r = read(st.fd, buf, static_cast<int>(size));
		mutex::scoped_lock lk(st.mtx);
		if (st.dead) ;
		else if (r < 0) io.post(bind(handler, error_code(GetLastError(), boost::system::system_category()), 0));
		else io.post((bind)(handler, error_code(0, boost::system::system_category()), r));
	}
	asio::io_service &io_service() { return io; }
	asio::io_service &get_io_service() { return io; }
private:
	asio::io_service &io;
	shared_ptr<object_status> pst;
};

stream_descriptor::stream_descriptor(asio::io_service &serv, int fd): pimpl(new stream_descriptor_impl(serv, fd)) { }
void stream_descriptor::async_read_some(const asio::mutable_buffer &seq, read_handler_type handler) { pimpl->async_read_some(seq, handler); }
asio::io_service &stream_descriptor::io_service() { return pimpl->io_service(); }
asio::io_service &stream_descriptor::get_io_service() { return pimpl->get_io_service(); }

#endif







#if defined(_WIN32) && !defined(__CYGWIN__)
// Win32 uses CONIN$ for raw console io

struct raw_termios {
	raw_termios() {
		h = CreateFile("CONIN$", GENERIC_READ|GENERIC_WRITE, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
		GetConsoleMode(h, &mode);
		SetConsoleMode(h, ENABLE_PROCESSED_INPUT);
	}
	~raw_termios() {
		SetConsoleMode(h, mode);
		CloseHandle(h);
	}
	DWORD mode;
	HANDLE h;
};

static const struct set_binmode { // Win32 stdio mode has to set _O_BINARY
	set_binmode(int) {
		_setmode(0, _O_BINARY);
		_setmode(1, _O_BINARY);
		_setmode(2, _O_BINARY);
	}
	~set_binmode() {
	}
} set_binmode_(0);

inline int pipe(int (&pipefd)[2]) { return ::_pipe(pipefd, 0, 0); }

#else
// Cygwin or *nix use /dev/tty for raw console io

struct raw_termios: boost::noncopyable {
	raw_termios() {
		no = open("/dev/tty", O_RDWR);
		tcgetattr(no, &nt); ot = nt;
		nt.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | IXON);
		nt.c_oflag &= ~OPOST;
		nt.c_lflag &= ~(ECHO | ICANON |  IEXTEN);
		nt.c_cflag &= ~(CSIZE | PARENB);
		nt.c_cflag |= CS8;		
		tcsetattr(no, TCSANOW, &nt);
	}
	~raw_termios() {
		tcsetattr(no, TCSANOW, &ot);
		close(no);
	}
	termios ot, nt;
	int no;
};

#endif

struct piped_stdio: boost::noncopyable {
	piped_stdio() {
		int pipefd[2];

		pipe(pipefd);
		piped_stdout = pipefd[0];
		true_stdout = dup(fileno(stdout));
		dup2(pipefd[1], fileno(stdout));
		close(pipefd[1]);

		pipe(pipefd);
		piped_stdin = pipefd[1];
		true_stdin = dup(fileno(stdin));
		dup2(pipefd[0], fileno(stdin));
		close(pipefd[0]);
	}
	~piped_stdio() {
		dup2(true_stdout, fileno(stdout));
		close(true_stdout);
		close(piped_stdout);

		dup2(true_stdin, fileno(stdin));
		close(true_stdin);
		close(piped_stdin);
	}
	int piped_stdout;
	int true_stdout;
	int piped_stdin;
	int true_stdin;
};


class output_handler::output_handler_impl {
public:
	output_handler_impl(asio::io_service &serv, bool use_locale): enc(use_locale ? enc_system : enc_utf_8), ti(), ps(), ost(serv, ps.piped_stdout), ist(serv, ps.true_stdin), osb(), isb(), lastline() {
		asio::async_read_until(ost, osb, '\n', bind(&output_handler_impl::out_handler, this, _1, _2));
		asio::async_read(ist, isb, asio::transfer_at_least(1), bind(&output_handler_impl::in_handler, this, _1, _2));
	}
private:
	void out_handler(const boost::system::error_code &, size_t len) {
		std::istream is(&osb);
		string s;
		getline(is, s);
		for (unsigned n = 0; n < s.length(); ++n) {
			if (s[n] == '\n') s.insert(n++, 1, '\r');
			if (s[n] == '\r') ++n;
		}
		const string d(utf8_to_extern(enc, s));
		s = '\r' + string(lastline.length(), ' ') + '\r' + string(d.begin(), d.end()) + "\r\n" + lastline;
		write(ps.true_stdout, s.data(), s.length());
		asio::async_read_until(ost, osb, '\n', bind(&output_handler_impl::out_handler, this, _1, _2));
	}
	void in_handler(const boost::system::error_code &, size_t len) {
		lastline.append(asio::buffer_cast<const char *>(isb.data()), isb.size());
		std::replace(lastline.end() - isb.size(), lastline.end(), '\r', '\n');
		size_t pos;
		while ((pos = lastline.find(0x08)) != lastline.npos) {
			vector<wchar_t> ws(extern_to_wide(enc, lastline.substr(0, pos)));
			ws.pop_back(); // TODO: should handle surrogate pair
			lastline = wide_to_extern(enc, ws) + lastline.substr(pos+1);
		}
		isb.consume(isb.size());
		const size_t eolpos = lastline.find('\n');
		if (eolpos!=lastline.npos)	{
			write(ps.true_stdout, ('\r' + string(lastline.length(), ' ') + '\r').data(), lastline.length() + 1);
			const string d(extern_to_utf8(enc, lastline.substr(0, eolpos+1)));
			write(ps.piped_stdin, d.data(), d.size());
			lastline.erase(0, eolpos+1);
		} else {
			string s = '\r' + lastline + "  \r" + lastline;
			for (unsigned n = 0; n < s.length(); ++n) {
				if (s[n] == '\n') s.insert(n++, 1, '\r');
				if (s[n] == '\r') ++n;
			}
			write(ps.true_stdout, s.data(), s.length());
		}
		asio::async_read(ist, isb, asio::transfer_at_least(1), bind(&output_handler_impl::in_handler, this, _1, _2));
	}
	io_encoding enc;
	raw_termios ti;
	piped_stdio ps;
	stream_descriptor ost;
	stream_descriptor ist;
	asio::streambuf osb;
	asio::streambuf isb;
	string lastline;
};

output_handler::output_handler(asio::io_service &serv, bool use_locale): pimpl(new output_handler_impl(serv, use_locale)) { }

NAMESPACE_CIRCLE_END
