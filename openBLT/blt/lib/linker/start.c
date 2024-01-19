/* $Id: //depot/blt/lib/linker/start.c#2 $
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

#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <fcntl.h>

#include "file.h"
#include "elf.h"
#include "link.h"
#include "defs.h"

char *exec_name;
elf32_sec_hdr_t *dyn_sec;
elf32_sym_t *dyn_sym;

extern unsigned int high;

void _dl_find_files (void);
void _dl_run_init (void);
void _dl_run_fini (void);

void _dl_start (int argc, char **argv, char **environ)
{
	unsigned int i, *pgm_start;
	void (*_start)(int, char **, char **);
	elf32_hdr_t *exec_hdr;
	elf32_sec_hdr_t *elf_sec;
	extern unsigned int num_files;

	/*
	 * figure out what we need to load.  we need to load the executable
	 * to get off of the ground.  note that the linker is already loaded
	 * but it doesn't need anything done to it.
	 */
	_dl_add_file (argv[0]);
	_dl_add_file ("linker");
	files[1].nopatch = 1;
	files[1].addr = (unsigned int) _DL_LIB_START;
	_dl_find_files ();

	/* now load everything */
	for (i = 0; i < num_files; i++)
		_dl_load (i);

	/* print out our present setup */
	for (i = 0; i < num_files; i++)
		printf ("%2d: %6x - %6x %s\n", i, files[i].addr, files[i].addr +
			files[i].len, files[i].name);

	/* patch up external references */
	for (i = 0; i < num_files; i++)
		_dl_patchup (i);

	printf ("we get %x\n", _elf_lookup_sym (2, "__libc_console_port") +
		files[2].addr);

#ifdef BLT
	/* run objects' initialisation code */
	_dl_run_init ();

	/* away we go! */
	exec_hdr = (elf32_hdr_t *) files[0].addr;
	_start = (void (*)(int, char **, char **)) exec_hdr->e_entry;
	(*_start) (argc, argv, environ);

	/* run objects' finalisation code */
	_dl_run_fini ();
#endif
}

#if defined (LINUX)

int main (int argc, char **argv)
{
	int fd, len;

	if (argc < 2)
		{
			printf ("%s: test syntax is `%s filename [args]'\n", *argv, *argv);
			exit (1);
		}

	/* make things a bit more realistic */
	fd = open ("linker", O_RDONLY, 0);
	len = lseek (fd, 0, SEEK_END);
	mmap (_DL_LIB_START, len, PROT_READ, MAP_PRIVATE, fd, 0);
	high += (len / 0x1000 + 1) * 0x1000;

	_dl_start (argc - 1, argv + 1, NULL);
}

#elif defined (BLT)
#endif

void _dl_find_files (void)
{
	unsigned int i, j, fd, len, *magic, filenum;
	char *dynstr, *libname, *temp;
	void *addr;
	elf32_hdr_t *hdr;
	elf32_dyn_t *dyn;
	file_info *file;
	extern unsigned int num_files;

	/*
	 * look through the executable and the library chain to find out what
	 * we need to load.  we need to do this now since the data we need won't
	 * exist in memory when we start constructing the process image.
	 */
	for (i = 0; i < num_files; i++)
		{
			if (files[i].nopatch)
				continue;

			/* mmap the next file */
			temp = files[i].name;
			/* printf ("looking at %s\n", libname); */
			if ((fd = open (temp, O_RDONLY, 0)) == -1)
				error ("linker: _dl_find_files: open failed: %s\n", temp);
			len = lseek (fd, 0, SEEK_END);
			if (!(addr = mmap (_DL_LIB_START, len, PROT_READ, MAP_PRIVATE, fd, 0)))
				error ("linker: _dl_find_files: mmap failed\n");

			hdr = (elf32_hdr_t *) addr;
			magic = (unsigned int *) addr;
			file = &files[i];

			/* perform a few sanity checks */
			if (*magic != ELF_MAGIC)
				error ("linker: bad magic number\n");
			if (hdr->e_machine != EM_386)
				error ("linker: not i386\n");
			if (hdr->e_version != EV_CURRENT)
				error ("linker: wrong ELF version\n");

			/*
			 * grab some useful info while we have .shstrtab around.
			 *
			 * we have to store these as offsets from the beginning
			 * of the file since we don't yet know how the files will
			 * be situated.
			 */
			file = &files[i];
			file->got_off =     _elf_section_offset (hdr, ".got");
			file->plt_off =     _elf_section_offset (hdr, ".plt");
			file->dynstr_off =  _elf_section_offset (hdr, ".dynstr");
			file->dynsym_off =  _elf_section_offset (hdr, ".dynsym");
			file->symtab_off =  _elf_section_offset (hdr, ".symtab");
			file->strtab_off =  _elf_section_offset (hdr, ".strtab");
			file->hash_off =    _elf_section_offset (hdr, ".hash");
			file->got_size =    _elf_section_size (hdr, ".got");
			file->plt_size =    _elf_section_size (hdr, ".plt");
			file->symtab_size = _elf_section_size (hdr, ".symtab");
			file->strtab_size = _elf_section_size (hdr, ".strtab");

			dynstr = (char *) _elf_find_section_data (hdr, ".dynstr");
			dyn = (elf32_dyn_t *) _elf_find_section_data (hdr, ".dynamic");

			for (j = 0; dyn[j].d_tag != DT_NULL; j++)
				switch (dyn[j].d_tag)
					{
						case DT_HASH:
						case DT_STRTAB:
						case DT_SYMTAB:
							/* we got these above already */
							break;
						case DT_NEEDED:
							/* library dependency */
							libname = (char *) (dynstr + dyn[j].d_un.d_ptr);
							temp = (char *) malloc (strlen (libname) + 1);
							(void) strcpy (temp, libname);
							(void) _dl_add_file (temp);
							break;
						case DT_JMPREL:
							/* printf ("DT_JMPREL @ %x\n", dyn[j].d_un.d_ptr); */
							files[i].jmprel_off = dyn[j].d_un.d_ptr;
							break;
						case DT_PLTRELSZ:
							/* printf ("DT_JMPRELSZ %x\n", dyn[j].d_un.d_val); */
							files[i].jmprel_size = dyn[j].d_un.d_val;
							break;
						case DT_REL:
							files[i].rel_off = dyn[j].d_un.d_ptr;
							break;
						case DT_RELSZ:
							/* printf ("DT_RELSZ %x\n", dyn[j].d_un.d_val); */
							files[i].rel_size = dyn[j].d_un.d_val;
							break;
						default:
							/* printf ("dt: %d\n", dyn[j].d_tag); */
							break;
					}
			/* printf ("\n"); */

			/* clean up */
			munmap (addr, len);
		}
}

void _dl_run_init (void)
{
}

void _dl_run_fini (void)
{
}

