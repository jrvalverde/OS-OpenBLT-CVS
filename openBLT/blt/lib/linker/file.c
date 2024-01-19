/* $Id: //depot/blt/lib/linker/file.c#2 $
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

#if defined (LINUX)
#define NULL ((void *) 0)
#elif defined (BLT)
#endif

#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

#define LIB_START 0x200000
#include "file.h"
#include "elf.h"
#include "link.h"
#include "defs.h"

#define MAX_FILES    32

unsigned int num_files = 0;

unsigned int high = 0x200000;
file_info files[MAX_FILES];

/*
 * add a file to the list of files to load, ignoring duplicates. return
 * -1 if too many files or file already added, subscript otherwise.
 */
int _dl_add_file (char *file)
{
	int i;

	if (num_files == MAX_FILES)
		return -1;
	for (i = 0; i < num_files; i++)
		if (!strcmp (files[i].name, file))
			return -1;
	files[num_files].jmprel_off = 0;
	files[num_files].rel_off = 0;
	files[num_files].nopatch = 0;
	files[num_files++].name = file;
	return num_files - 1;
}

void _dl_print_files (void)
{
	int i;

	for (i = 0; i < num_files; i++)
		printf ("%d: %s\n", i, files[i].name);
}

void *_dl_load (int filenum)
{
	char buffer[4096], *file;
	unsigned int i, fd, len, num_pages, part_addr;
	void *addr = NULL, *new_addr, *pgm_base = (void *) 0x1000;
	elf32_hdr_t *new_hdr;
	elf32_pgm_hdr_t *pgm_hdr;
	elf32_sec_hdr_t *sec_hdr;
	extern char *exec_name;

	file = files[filenum].name;
	if (files[filenum].nopatch)
		return;

	if ((fd = open (file, O_RDONLY, 0)) == -1)
		error ("linker: _dl_load: file not found %s\n", file);
	(void) read (fd, buffer, sizeof (buffer));

	/* parse the program headers and map in loadable parts */
	new_hdr = (elf32_hdr_t *) buffer;
	for (i = 0; i < new_hdr->e_phnum; i++)
		{
			pgm_hdr = (elf32_pgm_hdr_t *) ((unsigned int) buffer + new_hdr->e_phoff +
				i * new_hdr->e_phentsize);
			switch (pgm_hdr->p_type)
				{
					case PT_LOAD:
						/* this is a loadable program section */
						if (!pgm_hdr->p_filesz)
							continue;
						num_pages = ((pgm_hdr->p_vaddr + pgm_hdr->p_filesz) >> 12) -
							(pgm_hdr->p_vaddr >> 12) + 1;
						/* printf ("           mapping %4x bytes @ %6x (%4x bytes @ %6x)\n",
							pgm_hdr->p_filesz, pgm_hdr->p_vaddr, pgm_hdr->p_filesz +
							(pgm_hdr->p_vaddr & 0xfff), filenum ? high : pgm_hdr->p_vaddr &
							0xfffff000); */
						if (!(new_addr = mmap ((void *) (filenum ? high :
								pgm_hdr->p_vaddr & 0xfffff000), pgm_hdr->p_filesz +
								(pgm_hdr->p_vaddr & 0xfff), PROT_READ | PROT_WRITE,
								MAP_PRIVATE, fd, pgm_hdr->p_offset & 0xfffff000)))
							; /* error ("mmap failed\n"); */
						else if (!addr)
							addr = new_addr;
						if (filenum)
							high += num_pages * 0x1000;
						else
							pgm_base += num_pages * 0x1000;
						break;
					default:
						break;
				}
		}

	/* load the symbol and string tables after the file */
	files[filenum].symtab_off = (unsigned int) mmap ((void *) (filenum ? high :
		(unsigned int) pgm_base), len = files[filenum].symtab_size +
		((unsigned int) files[filenum].symtab_off & 0xfff), PROT_READ |
		PROT_WRITE, MAP_PRIVATE, fd,
		files[filenum].symtab_off & 0xfffff000) + (files[filenum].symtab_off &
		0xfff);
	if (filenum)
		high += (len / 0x1000 + 1) * 0x1000;
	else
		pgm_base += (len / 0x1000 + 1) * 0x1000;
	files[filenum].strtab_off = (unsigned int) mmap ((void *) (filenum ? high :
		(unsigned int) pgm_base), len = files[filenum].strtab_size +
		((unsigned int) files[filenum].strtab_off & 0xfff), PROT_READ | PROT_WRITE,
		MAP_PRIVATE, fd,
		files[filenum].strtab_off & 0xfffff000) + (files[filenum].strtab_off &
		0xfff);
	if (filenum)
		high += (len / 0x1000 + 1) * 0x1000;
	else
		pgm_base += (len / 0x1000 + 1) * 0x1000;

/*
	printf ("symbol table at %x\n", files[filenum].symtab_off);
	printf ("string table at %x\n", files[filenum].strtab_off);
*/

	/* update accounting information */
	files[filenum].addr = (unsigned int) addr;
	files[filenum].len = lseek (fd, 0, SEEK_END);

	return (void *) addr;
}

