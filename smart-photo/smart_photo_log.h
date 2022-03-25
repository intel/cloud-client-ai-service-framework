#ifndef CCAI_SYSLOG_H
#define CCAI_SYSLOG_H

#include <string>
#include <iostream>
#include <streambuf>
namespace csyslog {
	#include <syslog.h>
}

using namespace std;

namespace syslog
{
	enum priority {
		emerg   = LOG_EMERG,   alert   = LOG_ALERT,
		critical= LOG_CRIT,    error   = LOG_ERR,
		warning = LOG_WARNING, notice  = LOG_NOTICE,
		info    = LOG_INFO,    debug   = LOG_DEBUG
	};

	class streambuf : public std::streambuf
	{
		std::string _buffer;
		int _log_level;
	public:
		streambuf() : _log_level(priority::debug) {}
		void log_level(int l) {
			_log_level = l;
		}
	protected:
		int sync() {
			if (_buffer.size()) {
				csyslog::syslog(_log_level, "%s", _buffer.c_str());
				_buffer.erase();
			}
			return 0;
		}

		int_type overflow(int_type i) {
			if (i == traits_type::eof()) {
				sync();
			} else {
				_buffer += static_cast<char>(i);
			}
			return i;
		}
	};

	class ostream : public std::ostream
	{
		streambuf _logbuf;
	public:
		ostream() : std::ostream(&_logbuf) {}
		void log_level(int l) {
			_logbuf.log_level(l);
		}
	};

	inline ostream& operator<<(ostream& os, const priority p) {
		os.log_level(p);
		return os;
	}
}

extern int __ccai_sp_debug_log_enabled;
extern syslog::ostream * __ccai_sp_syslog;
extern int __ccai_sp_log_target;

#define LOG_TARGET_SYSLOG	0
#define LOG_TARGET_COUT		1

#define DEBUG
#ifdef DEBUG
#define D(x) \
	do { if (__ccai_sp_debug_log_enabled) { \
		if (__ccai_sp_log_target == LOG_TARGET_SYSLOG) { \
			(*__ccai_sp_syslog) << syslog::priority::debug \
				<< "[SPLIB][" << __func__ << "] " x << std::endl; \
		} else { \
			std::cout << "[SPLIB][" << __func__ << "] " x << std::endl;  \
		} \
	}} while(0)
#else
#define D(x) do {} while(0)
#endif // DEBUG

#define E(x) do { \
		if (__ccai_sp_log_target == LOG_TARGET_SYSLOG) { \
			(*__ccai_sp_syslog)  << syslog::priority::error \
				<< "[SPLIB][ERROR] " x << std::endl; \
		} else { \
			std::cerr << "[SPLIB][ERROR] " x << std::endl; \
		} \
	} while(0)

#endif
