#include <limits.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

// Adapted from https://github.com/mit-pdos/xv6-public/blob/master/printf.c
static int int2str(long long xx, int base, int sgn, char* out)
{
	static char digits[] = "0123456789ABCDEF";
	char buf[32];
	int i, neg, j;
	unsigned long long x;

	neg = 0;
	if(sgn && xx < 0){
		neg = 1;
		x = -xx;
	} else {
		x = xx;
	}

	i = 0;
	do{
		buf[i++] = digits[x % base];
	}while((x /= base) != 0);
	if(neg)
		buf[i++] = '-';

	//Reverse
	j = 0;
	while(--i >= 0)
		out[j++] = buf[i];
	
	return j;
}

struct printf_output {
	char* buf;
	size_t size;
	int fd;
	bool (*write)(struct printf_output*, const char* data, size_t length);
};

static int vprintf0(struct printf_output* out, const char* restrict format, va_list parameters) {
	int written = 0;

	while (*format != '\0') {
		size_t maxrem = INT_MAX - written;

		if (format[0] != '%' || (format[0] == '%' && format[1] == '%')) {
			if (format[0] == '%')
				format++;
			size_t amount = 1;
			while (format[amount] && format[amount] != '%')
				amount++;
			if (maxrem < amount) {
				// TODO: Set errno to EOVERFLOW.
				return -1;
			}
			if (!out->write(out, format, amount))
				return -1;
			format += amount;
			written += amount;
			continue;
		}

		const char* format_begun_at = format++; // skip %

		if (*format == 'c') {
			format++;
			char c = (char) va_arg(parameters, int /* char promotes to int */);
			if (!maxrem) {
				// TODO: Set errno to EOVERFLOW.
				return -1;
			}
			if (!out->write(out, &c, sizeof(c)))
				return -1;
			written++;
		} else if (*format == 's') {
			format++;
			const char* str = va_arg(parameters, const char*);
			size_t len = strlen(str);
			if (maxrem < len) {
				// TODO: Set errno to EOVERFLOW.
				return -1;
			}
			if (!out->write(out, str, len))
				return -1;
			written += len;
		} else if (*format == 'd' || *format == 'u' || *format == 'x' || *format == 'p' || *format == 'l' ||  *format == 'h') {
			int length_specifier = 0;
			while(*format == 'l' || *format == 'h') {
				if(*format == 'l') {
					length_specifier++;
				} else {
					length_specifier--;
				}
				format++;
			}
			int sgn = *format == 'd';
			int base = (*format == 'x') ? 16:10;
			format++;
			long long number;
			 if(length_specifier == 1) {
				number = va_arg(parameters, long);
			} else if(length_specifier == 2) {
				number = va_arg(parameters, long long);
			} else {
				number = va_arg(parameters, int);
			}
			length_specifier = 0;
			
			char str[32];
			size_t len = int2str(number, base, sgn, str);
			if (maxrem < len) {
				// TODO: Set errno to EOVERFLOW.
				return -1;
			}
			if (!out->write(out, str, len))
				return -1;
			written += len;
		} else {
			format = format_begun_at;
			size_t len = strlen(format);
			if (maxrem < len) {
				// TODO: Set errno to EOVERFLOW.
				return -1;
			}
			if (!out->write(out, format, len))
				return -1;
			written += len;
			format += len;
		}
	}
	return written;
}

static bool print2serial(struct printf_output* out, const char* data, size_t length) {
	(void) out;
	const unsigned char* bytes = (const unsigned char*) data;
	for (size_t i = 0; i < length; i++)
		if (serial_putchar(bytes[i]) == EOF)
			return false;
	return true;
}

static bool print2tty(struct printf_output* out, const char* data, size_t length) {
	(void) out;
	const unsigned char* bytes = (const unsigned char*) data;
	for (size_t i = 0; i < length; i++)
		if (putchar(bytes[i]) == EOF)
			return false;
	return true;
}

static bool print2str(struct printf_output* out, const char* data, size_t length) {
	if(out->size < length) {
		return false;
	}
	for (size_t i = 0; i < length; i++) {
		out->buf[i] = data[i];
	}
	out->buf += length;
	out->size -= length;
	
	return true;
}

// print to debug/serial output rather than screen
int kprintf(const char* restrict format, ...) {
	va_list parameters;
	struct printf_output out = (struct printf_output) {.write = print2serial};
	va_start(parameters, format);
	int r = vprintf0(&out, format, parameters);
	va_end(parameters);
	return r;
}

int printf(const char* restrict format, ...) {
	va_list parameters;
	struct printf_output out = (struct printf_output) {.write = print2tty};
	va_start(parameters, format);
	int r = vprintf0(&out, format, parameters);
	va_end(parameters);
	return r;
}

int snprintf(char* buf, size_t size, const char* restrict format, ...) {
	va_list parameters;
	struct printf_output out = (struct printf_output) {.write = print2str, .buf = buf, .size = size};
	va_start(parameters, format);
	int r = vprintf0(&out, format, parameters);
	va_end(parameters);
	return r;
}
