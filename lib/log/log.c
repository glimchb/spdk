/*   SPDX-License-Identifier: BSD-3-Clause
 *   Copyright (C) 2016 Intel Corporation.
 *   All rights reserved.
 */

#include "spdk/stdinc.h"
#include "spdk/util.h"

#include "spdk/log.h"

static const char *const spdk_level_names[] = {
	[SPDK_LOG_ERROR]	= "ERROR",
	[SPDK_LOG_WARN]		= "WARNING",
	[SPDK_LOG_NOTICE]	= "NOTICE",
	[SPDK_LOG_INFO]		= "INFO",
	[SPDK_LOG_DEBUG]	= "DEBUG",
};

#define MAX_TMPBUF 1024

static struct spdk_log_opts g_log_opts = {
	.log = NULL,
	.open = NULL,
	.close = NULL,
	.user_ctx = NULL,
};
static bool g_log_timestamps = true;

enum spdk_log_level g_spdk_log_level;
enum spdk_log_level g_spdk_log_print_level;

void
spdk_log_set_level(enum spdk_log_level level)
{
	assert(level >= SPDK_LOG_DISABLED);
	assert(level <= SPDK_LOG_DEBUG);
	g_spdk_log_level = level;
}

enum spdk_log_level
spdk_log_get_level(void) {
	return g_spdk_log_level;
}

void
spdk_log_set_print_level(enum spdk_log_level level)
{
	assert(level >= SPDK_LOG_DISABLED);
	assert(level <= SPDK_LOG_DEBUG);
	g_spdk_log_print_level = level;
}

enum spdk_log_level
spdk_log_get_print_level(void) {
	return g_spdk_log_print_level;
}

static void
log_open(void *ctx)
{
	openlog("spdk", LOG_PID, LOG_LOCAL7);
}

static void
log_close(void *ctx)
{
	closelog();
}

void
spdk_log_open(spdk_log_cb *log)
{
	if (log) {
		struct spdk_log_opts opts = {.log = log};
		opts.size = SPDK_SIZEOF(&opts, log);
		spdk_log_open_ext(&opts);
	} else {
		spdk_log_open_ext(NULL);
	}
}

void
spdk_log_open_ext(struct spdk_log_opts *opts)
{
	if (!opts) {
		g_log_opts.open = log_open;
		g_log_opts.close = log_close;
		goto out;
	}

	g_log_opts.log = SPDK_GET_FIELD(opts, log, NULL);
	g_log_opts.open = SPDK_GET_FIELD(opts, open, NULL);
	g_log_opts.close = SPDK_GET_FIELD(opts, close, NULL);
	g_log_opts.user_ctx = SPDK_GET_FIELD(opts, user_ctx, NULL);

out:
	if (g_log_opts.open) {
		g_log_opts.open(g_log_opts.user_ctx);
	}
}

void
spdk_log_close(void)
{
	if (g_log_opts.close) {
		g_log_opts.close(g_log_opts.user_ctx);
	}
	memset(&g_log_opts, 0, sizeof(g_log_opts));
}

void
spdk_log_enable_timestamps(bool value)
{
	g_log_timestamps = value;
}

static void
get_timestamp_prefix(char *buf, int buf_size)
{
	struct tm *info;
	char date[24];
	struct timespec ts;
	long usec;

	if (!g_log_timestamps) {
		buf[0] = '\0';
		return;
	}

	clock_gettime(CLOCK_REALTIME, &ts);
	info = localtime(&ts.tv_sec);
	usec = ts.tv_nsec / 1000;
	if (info == NULL) {
		snprintf(buf, buf_size, "[%s.%06ld] ", "unknown date", usec);
		return;
	}

	strftime(date, sizeof(date), "%Y-%m-%d %H:%M:%S", info);
	snprintf(buf, buf_size, "[%s.%06ld] ", date, usec);
}

void
spdk_log(enum spdk_log_level level, const char *file, const int line, const char *func,
	 const char *format, ...)
{
	va_list ap;

	va_start(ap, format);
	spdk_vlog(level, file, line, func, format, ap);
	va_end(ap);
}

int
spdk_log_to_syslog_level(enum spdk_log_level level)
{
	switch (level) {
	case SPDK_LOG_DEBUG:
	case SPDK_LOG_INFO:
		return LOG_INFO;
	case SPDK_LOG_NOTICE:
		return LOG_NOTICE;
	case SPDK_LOG_WARN:
		return LOG_WARNING;
	case SPDK_LOG_ERROR:
		return LOG_ERR;
	case SPDK_LOG_DISABLED:
		return -1;
	default:
		break;
	}

	return LOG_INFO;
}

void
spdk_vlog(enum spdk_log_level level, const char *file, const int line, const char *func,
	  const char *format, va_list ap)
{
	int severity = LOG_INFO;
	char *buf, _buf[MAX_TMPBUF], *ext_buf = NULL;
	char timestamp[64];
	va_list ap_copy;
	int rc;

	if (g_log_opts.log) {
		g_log_opts.log(level, file, line, func, format, ap);
		return;
	}

	if (level > g_spdk_log_print_level && level > g_spdk_log_level) {
		return;
	}

	severity = spdk_log_to_syslog_level(level);
	if (severity < 0) {
		return;
	}

	buf = _buf;

	va_copy(ap_copy, ap);
	rc = vsnprintf(_buf, sizeof(_buf), format, ap);
	if (rc > MAX_TMPBUF) {
		/* The output including the terminating was more than MAX_TMPBUF bytes.
		 * Try allocating memory large enough to hold the output.
		 */
		rc = vasprintf(&ext_buf, format, ap_copy);
		if (rc < 0) {
			/* Failed to allocate memory. Allow output to be truncated. */
		} else {
			buf = ext_buf;
		}
	}
	va_end(ap_copy);

	if (level <= g_spdk_log_print_level) {
		get_timestamp_prefix(timestamp, sizeof(timestamp));
		if (file) {
			fprintf(stderr, "%s%s:%4d:%s: *%s*: %s", timestamp, file, line, func, spdk_level_names[level], buf);
		} else {
			fprintf(stderr, "%s%s", timestamp, buf);
		}
	}

	if (level <= g_spdk_log_level) {
		if (file) {
			syslog(severity, "%s:%4d:%s: *%s*: %s", file, line, func, spdk_level_names[level], buf);
		} else {
			syslog(severity, "%s", buf);
		}
	}

	free(ext_buf);
}

void
spdk_vflog(FILE *fp, const char *file, const int line, const char *func,
	   const char *format, va_list ap)
{
	char buf[MAX_TMPBUF];
	char timestamp[64];

	vsnprintf(buf, sizeof(buf), format, ap);

	get_timestamp_prefix(timestamp, sizeof(timestamp));

	if (file) {
		fprintf(fp, "%s%s:%4d:%s: %s", timestamp, file, line, func, buf);
	} else {
		fprintf(fp, "%s%s", timestamp, buf);
	}

	fflush(fp);
}

void
spdk_flog(FILE *fp, const char *file, const int line, const char *func,
	  const char *format, ...)
{
	va_list ap;

	va_start(ap, format);
	spdk_vflog(fp, file, line, func, format, ap);
	va_end(ap);
}

static void
fdump(FILE *fp, const char *label, const uint8_t *buf, size_t len)
{
	char tmpbuf[MAX_TMPBUF];
	char buf16[16 + 1];
	size_t total;
	unsigned int idx;

	fprintf(fp, "%s\n", label);

	memset(buf16, 0, sizeof buf16);
	total = 0;
	for (idx = 0; idx < len; idx++) {
		if (idx != 0 && idx % 16 == 0) {
			snprintf(tmpbuf + total, sizeof tmpbuf - total,
				 " %s", buf16);
			memset(buf16, 0, sizeof buf16);
			fprintf(fp, "%s\n", tmpbuf);
			total = 0;
		}
		if (idx % 16 == 0) {
			total += snprintf(tmpbuf + total, sizeof tmpbuf - total,
					  "%08x ", idx);
		}
		if (idx % 8 == 0) {
			total += snprintf(tmpbuf + total, sizeof tmpbuf - total,
					  "%s", " ");
		}
		total += snprintf(tmpbuf + total, sizeof tmpbuf - total,
				  "%2.2x ", buf[idx] & 0xff);
		buf16[idx % 16] = isprint(buf[idx]) ? buf[idx] : '.';
	}
	for (; idx % 16 != 0; idx++) {
		if (idx == 8) {
			total += snprintf(tmpbuf + total, sizeof tmpbuf - total,
					  " ");
		}

		total += snprintf(tmpbuf + total, sizeof tmpbuf - total, "   ");
	}
	snprintf(tmpbuf + total, sizeof tmpbuf - total, "  %s", buf16);
	fprintf(fp, "%s\n", tmpbuf);
	fflush(fp);
}

void
spdk_log_dump(FILE *fp, const char *label, const void *buf, size_t len)
{
	fdump(fp, label, buf, len);
}
