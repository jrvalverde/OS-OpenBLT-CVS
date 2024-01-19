/* $Id: //depot/blt/lib/libc/vfs.c#16 $
**
** Copyright 1999 Sidney Cammeresi.
** All rights reserved.
** 
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
** 1. Redistributions of source code must retain the above copyright notice,
**    this list of conditions and the following disclaimer.
** 
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
** 
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR `AS IS' AND ANY EXPRESS OR IMPLIED
** WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
** MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN
** NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
** TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
** PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
** LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
** NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
** SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <blt/os.h>
#include <blt/syscall.h>
#include <blt/namer.h>
#include <blt/fdl.h>
#include <blt/libsyms.h>
#include <blt/vfs.h>

static int vfs_public_port, vfs_local_port, vfs_remote_port, filename_area;
static char *nameptr;

static void __vfs_openconn (int src_port, int filename_area);
ssize_t _vfs_read (void *cookie, void *buf, size_t count);
int _vfs_close (void *cookie);

static fdl_type vfs_fdl_handler = { "vfs", _vfs_read, NULL, NULL, _vfs_close };

weak_alias (_opendir, opendir)
weak_alias (_readdir, readdir)
weak_alias (_closedir, closedir)
weak_alias (_open, open)
weak_alias (_stat, stat)
weak_alias (_mkdir, mkdir)

void __libc_init_vfs (void)
{
	int nh;

	nh = namer_newhandle ();
	while ((vfs_public_port = namer_find (nh, "vfs")) <= 0)
		os_sleep (10);
	namer_delhandle (nh);
	vfs_local_port = port_create (vfs_public_port,"vfs_public_port");

	filename_area = area_create (0x1000, 0, (void **) &nameptr, 0);
	__vfs_openconn (vfs_local_port, filename_area);
}

void __libc_fini_vfs (void)
{
}

static void __vfs_openconn (int src_port, int filename_area)
{
	msg_hdr_t mh;
	vfs_cmd_t vc;
	vfs_res_t vr;

	vc.cmd = VFS_OPENCONN;
	vc.data[0] = filename_area;

	mh.src = vfs_local_port;
	mh.dst = vfs_public_port;
	mh.data = &vc;
	mh.size = sizeof (vc);
	port_send (&mh);

	mh.src = 0; /* XXX */
	mh.dst = vfs_local_port;
	mh.data = &vr;
	mh.size = sizeof (vr);
	port_recv (&mh);

	if (vr.status != VFS_OK)
	{
		_printf ("libc: couldn't open connection to vfs\n");
		vfs_local_port = vfs_remote_port = -1;
	}
	vfs_remote_port = vr.data[0];
}

DIR *_opendir (const char *name)
{
	int area;
	void *ptr;
	msg_hdr_t msg;
	vfs_cmd_t vc;
	vfs_res_t vr;
	DIR *dirp;

	strlcpy (nameptr, name, BLT_MAX_NAME_LENGTH);
	area = area_create (0x2000, 0, &ptr, 0);

	vc.cmd = VFS_OPENDIR;
	vc.data[0] = 0;
	vc.data[1] = area;
	vc.data[2] = 0;
	vc.data[3] = 0x2000;
	msg.src = vfs_local_port;
	msg.dst = vfs_remote_port;
	msg.data = &vc;
	msg.size = sizeof (vfs_cmd_t);
	port_send (&msg);

	msg.src = vfs_remote_port;
	msg.dst = vfs_local_port;
	msg.data = &vr;
	msg.size = sizeof (vfs_res_t);
	port_recv (&msg);

	if (vr.status != VFS_OK)
	{
		errno = vr.errno;
		return NULL;
	}
	//_printf ("libc: opendir got fd %d\n", vr.data[0]);
	dirp = malloc (sizeof (DIR));
	dirp->fd = vr.data[0];
	dirp->hoffset = 0;
	dirp->head = ptr;
	dirp->current = ptr;
	dirp->more = vr.data[2];
	dirp->len = vr.data[1];
	dirp->left = dirp->len;
	return dirp;
}

struct dirent *_readdir (DIR *dirp)
{
	if (dirp == NULL)
		return NULL;
	else
		return (dirp->left-- > 0) ? dirp->current++ : NULL;
}

int _closedir (DIR *dirp)
{
	msg_hdr_t msg;
	vfs_cmd_t vc;
	vfs_res_t vr;

	if (dirp == NULL)
	{
		errno = EBADF;
		return -1;
	}
	vc.cmd = VFS_CLOSEDIR;
	vc.data[0] = dirp->fd;
	msg.src = vfs_local_port;
	msg.dst = vfs_remote_port;
	msg.data = &vc;
	msg.size = sizeof (vfs_cmd_t);
	port_send (&msg);

	msg.src = vfs_remote_port;
	msg.dst = vfs_local_port;
	msg.data = &vr;
	msg.size = sizeof (vfs_res_t);
	port_recv (&msg);

	errno = vr.errno;
	return (vr.status == VFS_OK) ? 0 : 1;
}

int _open (const char *path, int flags, mode_t mode)
{
	int i, area;
	void *ptr;
	msg_hdr_t msg;
	vfs_cmd_t vc;
	vfs_res_t vr;
	vfs_fd *fd;

	strlcpy (nameptr, path, BLT_MAX_NAME_LENGTH);
	area = area_create (0x2000, 0, &ptr, 0);

	vc.cmd = VFS_OPEN;
	vc.data[0] = 0;
	vc.data[1] = area;
	vc.data[2] = 0;
	vc.data[3] = 0x2000;
	msg.src = vfs_local_port;
	msg.dst = vfs_remote_port;
	msg.data = &vc;
	msg.size = sizeof (vfs_cmd_t);
	port_send (&msg);

	msg.src = vfs_remote_port;
	msg.dst = vfs_local_port;
	msg.data = &vr;
	msg.size = sizeof (vfs_res_t);
	port_recv (&msg);

	if (vr.status != VFS_OK)
	{
		errno = vr.errno;
		return -1;
	}
	//_printf ("libc: open got %d %d %d\n", vr.data[0], vr.data[1], vr.data[2]);
	fd = malloc (sizeof (vfs_fd));
	i = _fdl_alloc_descriptor (&vfs_fdl_handler, fd);
	fd->buf = ptr;
	fd->offset = 0;
	fd->srv_fd = vr.data[0];
	fd->length = vr.data[1];
	fd->more = vr.data[2];
	return i;
}

int _vfs_close (void *cookie)
{
	msg_hdr_t mh;
	vfs_cmd_t vc;
	vfs_res_t vr;
	vfs_fd *vfd;

	vfd = cookie;
	vc.cmd = VFS_CLOSE;
	vc.data[0] = vfd->srv_fd;
	mh.src = vfs_local_port;
	mh.dst = vfs_remote_port;
	mh.data = &vc;
	mh.size = sizeof (vfs_cmd_t);
	port_send (&mh);

	mh.src = vfs_remote_port;
	mh.dst = vfs_local_port;
	mh.data = &vr;
	mh.size = sizeof (vfs_res_t);
	port_recv (&mh);

	if (vr.status != VFS_OK)
	{
		errno = vr.errno;
		return -1;
	}
	return 0;
}

ssize_t _vfs_read (void *cookie, void *buf, size_t count)
{
	int i, len;
	msg_hdr_t mh;
	vfs_cmd_t vc;
	vfs_res_t vr;
	register vfs_fd *vfd;

	vfd = cookie;
	vc.cmd = VFS_READ;
	vc.data[0] = vfd->srv_fd;
	vc.data[1] = count;
	mh.src = vfs_local_port;
	mh.dst = vfs_remote_port;
	mh.data = &vc;
	mh.size = sizeof (vfs_cmd_t);
	port_send (&mh);

	mh.src = 0;
	mh.dst = vfs_local_port;
	mh.data = &vr;
	mh.size = sizeof (vfs_res_t);
	port_recv (&mh);
	len = vr.data[0];
	if (!len)
		return errno = 0;

	if (len < 0x1000)
	{
		mh.data = buf;
		mh.size = count;
		port_recv (&mh);
		errno = vr.errno;
		return len;
	}
	else
	{
		for (i = 0; len > 0x1000; i += 0x1000, len -= 0x1000)
		{
			mh.data = (char *) buf + i;
			mh.size = 0x1000;
			port_recv (&mh);
		}
		mh.data = (char *) buf + i;
		mh.size = len;
		port_recv (&mh);
		errno = vr.errno;
		return i + len;
	}
}

int _stat (const char *filename, struct stat *buf)
{
	msg_hdr_t msg;
	vfs_cmd_t vc;
	vfs_res_t *vr;

	strlcpy (nameptr, filename, BLT_MAX_NAME_LENGTH);
	vc.cmd = VFS_RSTAT;
	vc.data[0] = 0;
	msg.src = vfs_local_port;
	msg.dst = vfs_remote_port;
	msg.data = &vc;
	msg.size = sizeof (vfs_cmd_t);
	port_send (&msg);

	vr = malloc (sizeof (vfs_res_t) + sizeof (struct stat));
	msg.dst = vfs_local_port;
	msg.data = vr;
	msg.size = sizeof (vfs_res_t) + sizeof (struct stat);
	port_recv (&msg);

	if (vr->status == VFS_OK)
	{
		memcpy (buf, (void *) vr + sizeof (vfs_res_t), sizeof (struct stat));
		return 0;
	}
	else
	{
		errno = vr->errno;
		return -1;
	}
}

int _mkdir (const char *path, mode_t mode)
{
	msg_hdr_t mh;
	vfs_cmd_t vc;
	vfs_res_t vr;

	strlcpy (nameptr, path, BLT_MAX_NAME_LENGTH);
	vc.cmd = VFS_MKDIR;
	vc.data[0] = 0;
	vc.data[1] = mode;
	mh.src = vfs_local_port;
	mh.dst = vfs_remote_port;
	mh.data = &vc;
	mh.size = sizeof (vfs_cmd_t);
	port_send (&mh);

	mh.src = vfs_remote_port;
	mh.dst = vfs_local_port;
	mh.data = &vr;
	mh.size = sizeof (vfs_res_t);
	port_recv (&mh);

	if (vr.status != VFS_OK)
	{
		errno = vr.errno;
		return -1;
	}
	return 0;
}

