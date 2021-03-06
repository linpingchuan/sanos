//
// printf.c
//
// Formatted print
//
// Copyright (C) 2011 Michael Ringgaard. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 
// 1. Redistributions of source code must retain the above copyright 
//    notice, this list of conditions and the following disclaimer.  
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.  
// 3. Neither the name of the project nor the names of its contributors
//    may be used to endorse or promote products derived from this software
//    without specific prior written permission. 
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
// OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
// LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
// OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
// SUCH DAMAGE.
// 

#include <stdio.h>
#include <stdarg.h>
#include <limits.h>

int _output(FILE *stream, const char *format, va_list args);
int _stbuf(FILE *stream, char *buf, int bufsiz);
void _ftbuf(FILE *stream);

int vfprintf(FILE *stream, const char *fmt, va_list args) {
  int rc;

  if (stream->flag & _IONBF) {
    char buf[BUFSIZ];

    _stbuf(stream, buf, BUFSIZ);
    rc = _output(stream, fmt, args);
    _ftbuf(stream);
  } else {
    rc = _output(stream, fmt, args);
  }

  return rc;
}

int fprintf(FILE *stream, const char *fmt, ...)
{
  int rc;
  va_list args;

  va_start(args, fmt);

  if (stream->flag & _IONBF) {
    char buf[BUFSIZ];

    _stbuf(stream, buf, BUFSIZ);
    rc = _output(stream, fmt, args);
    _ftbuf(stream);
  } else {
    rc = _output(stream, fmt, args);
  }

  return rc;
}

int vprintf(const char *fmt, va_list args) {
  return vfprintf(stdout, fmt, args);
}

int printf(const char *fmt, ...) {
  va_list args;

  va_start(args, fmt);
  return vfprintf(stdout, fmt, args);
}

int vsprintf(char *buf, const char *fmt, va_list args) {
  FILE str;
  int rc;

  str.cnt = INT_MAX;
  str.flag = _IOWR | _IOSTR;
  str.ptr = str.base = buf;

  rc = _output(&str, fmt, args);
  if (buf != NULL) putc('\0', &str);

  return rc;
}

int sprintf(char *buf, const char *fmt, ...) {
  va_list args;
  FILE str;
  int rc;

  va_start(args, fmt);

  str.cnt = INT_MAX;
  str.flag = _IOWR | _IOSTR;
  str.ptr = str.base = buf;

  rc = _output(&str, fmt, args);
  if (buf != NULL) putc('\0', &str);

  return rc;
}

int vsnprintf(char *buf, size_t count, const char *fmt, va_list args) {
  FILE str;
  int rc;

  str.cnt = (int) count;
  str.flag = _IOWR | _IOSTR;
  str.ptr = str.base = buf;

  rc = _output(&str, fmt, args);
  if (buf != NULL) putc('\0', &str);

  return rc;
}

int snprintf(char *buf, size_t count, const char *fmt, ...) {
  va_list args;
  FILE str;
  int rc;

  va_start(args, fmt);

  str.cnt = (int) count;
  str.flag = _IOWR | _IOSTR;
  str.ptr = str.base = buf;

  rc = _output(&str, fmt, args);
  if (buf != NULL) putc('\0', &str);

  return rc;
}
