/* $Id: //depot/blt/lib/libc/stdio.c#4 $
**
** Copyright 1999 Sidney Cammeresi
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <blt/fdl.h>
#include <blt/syscall.h>
#include <blt/namer.h>
#include <blt/libsyms.h>

static int input_remote_port, input_local_port;
static fdl_type console_input_fdl_imp = { "console_input", _console_read,
    NULL, NULL, NULL };

FILE *stdin, *stdout, *stderr;

weak_alias (_getc, getc)
weak_alias (_getchar, getchar)

void __libc_init_console_input ()
{
	int nh, fd;

	nh = namer_newhandle ();
	input_remote_port = namer_find (nh, "console_input");
	namer_delhandle (nh);
	input_local_port = port_create (input_remote_port, "input_remote_port");
	fd = _fdl_alloc_descriptor (&console_input_fdl_imp, NULL);
	if (fd)
		_printf ("__libc_init_input: console input not on fd 0\n");
	stdin = malloc (sizeof (FILE));
	stdin->fd = 0;
}

int _console_read (void *cookie, void *buf, size_t count)
{
	char data;
	msg_hdr_t msg;

	data = count;
	msg.src = input_local_port;
	msg.dst = input_remote_port;
	msg.data = &data;
	msg.size = 1;
	port_send (&msg);

	msg.src = input_remote_port;
	msg.dst = input_local_port;
	msg.data = buf;
	msg.size = count;
	port_recv (&msg);
	return msg.size;
}

int _getc (FILE *stream)
{
	char c;

	_read (stream->fd, &c, 1);
	return c;
}

int _getchar (void)
{
	return _getc (stdin);
}

