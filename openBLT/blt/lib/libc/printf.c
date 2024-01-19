/* $Id: //depot/blt/lib/libc/printf.c#6 $
**
** Copyright 1998 Sidney Cammeresi
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions, and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions, and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <stdarg.h>
#include <string.h>
#include <blt/namer.h>
#include <blt/syscall.h>
#include <blt/libsyms.h>

static int __libc_console_public_port, __libc_console_port;

void va_snprintf (char *s, int len, const char *fmt, ...);

void __libc_init_console (void)
{
	int nh;

	nh = namer_newhandle ();
	while ((__libc_console_public_port = namer_find (nh, "console")) < 1)
		os_sleep (10);
	__libc_console_port = port_create (__libc_console_public_port,"console_public_port");
}

weak_alias (_printf, __libc_printf)
weak_alias (_printf, printf)
link_warning (__libc_printf, "warning: __libc_printf is deprecated; use printf instead.")

int _printf(char *fmt,...)
{   
	msg_hdr_t kmsg;
	int l;
	char buf[256];

	va_list pvar;
	va_start(pvar,fmt);
	va_snprintf(buf,256,fmt,pvar);
	va_end(pvar);
	kmsg.size = strlen(buf);
	kmsg.data = buf;
	kmsg.src = __libc_console_port;
	kmsg.dst = __libc_console_public_port;
	l = port_send(&kmsg);
	return -1; /* FIXME */
}

