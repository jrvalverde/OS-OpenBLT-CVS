/* $Id: //depot/blt/lib/linker/elf.c#2 $
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

#include "elf.h"

#if defined (LINUX)
#define NULL ((void *) 0)
#include <stdio.h>
#elif defined (BLT)
#include "blt/types.h"
#endif

#include "defs.h"
#include "file.h"

extern unsigned int num_files;

elf32_sec_hdr_t *_elf_find_section_hdr (elf32_hdr_t *hdr, char *name)
{
	char *section_name;
	int i;
	elf32_sec_hdr_t *sec_hdr;

	sec_hdr = (elf32_sec_hdr_t *) ((unsigned int) hdr + hdr->e_shoff +
		hdr->e_shstrndx * hdr->e_shentsize);
	section_name = (char *) ((unsigned int) hdr + sec_hdr->sh_offset);
	sec_hdr = (elf32_sec_hdr_t *) ((unsigned int) hdr + hdr->e_shoff);
	for (i = 0; i < hdr->e_shnum; i++, sec_hdr = (elf32_sec_hdr_t *)
			((unsigned int) sec_hdr + hdr->e_shentsize))
		if (!strcmp (section_name + sec_hdr->sh_name, name))
			return sec_hdr;
	return NULL;
}

void *_elf_find_section_data (elf32_hdr_t *hdr, char *name)
{
	elf32_sec_hdr_t *sec_hdr;

	sec_hdr = _elf_find_section_hdr (hdr, name);
	return (sec_hdr == NULL) ? NULL : (void *) ((unsigned int) hdr +
		sec_hdr->sh_offset);
}

int _elf_section_offset (elf32_hdr_t *hdr, char *name)
{
	elf32_sec_hdr_t *sec_hdr;

	sec_hdr = _elf_find_section_hdr (hdr, name);
	return (sec_hdr == NULL) ? 0 : sec_hdr->sh_offset;
}

int _elf_section_size (elf32_hdr_t *hdr, char *name)
{
  elf32_sec_hdr_t *sec_hdr;

  sec_hdr = _elf_find_section_hdr (hdr, name);
  return (sec_hdr == NULL) ? 0 : sec_hdr->sh_size;
}

unsigned int _elf_hash (const unsigned char *name)
{
	unsigned int g, h = 0;

	/* spec defined hash function to hash symbols to hash table indices */
	while (*name)
		{
			h = (h << 4) + *name++;
			if (g = h & 0xf0000000)
				h ^= g >> 24;
			h &= ~g;
		}
	return h;
}

void *_elf_lookup_sym (int filenum, const char *name)
{
	char *sym_str;
	unsigned int i, *hash_table, *bucket, num_buckets, *chain, num_chains, index,
		old_index, hash_val;
	elf32_sym_t *symtab;
	file_info *file;

	file = &files[filenum];

	/* look up this file's symbol table and hash table */
	symtab = (elf32_sym_t *) file->symtab_off;
	hash_table = (unsigned int *) file->hash_off;
	sym_str = (char *) file->strtab_off;

#if 0
	/* XXX - this doesn't work */
	/* XXX - it should also be updated following the rewrite */

	/* .hash section is nbucket, nchain, buckets..., chains... */
	num_buckets = *hash_table;
	num_chains = *(hash_table + 1);
	bucket = hash_table + 2;
	chain = hash_table + num_buckets + 2;

	hash_val = _elf_hash (name);
	old_index = index = bucket [hash_val % num_buckets];
	while (strcmp (dyn_str + symtab[index].st_name, name) && index)
		{
			printf (">> %s %x %x\n", sym_str + symtab[index].st_name,
				symtab[index].st_name, index);
			old_index = index;
			index = chain[index];
		}
	printf ("found %s %x\n", sym_str + symtab[old_index].st_name, index);
#else
	/* XXX - at any rate, we do this instead for now */

	/* scan the symbol table sequentially for the one we want */
	for (i = 0; (i * sizeof (elf32_sym_t)) < file->symtab_size; i++)
		if (!strcmp (sym_str + symtab[i].st_name, name) &&
				(symtab[i].st_shndx != SHN_UNDEF))
			{
/*
			if (ELF32_ST_TYPE (symtab[i].st_info) == STT_OBJECT)
				printf ("yahoo\n");
*/
/*
			printf ("found with %x, %x %x %x\n", symtab[i].st_shndx,
				ELF32_ST_BIND (symtab[i].st_info), ELF32_ST_TYPE (symtab[i].st_info),
				0); //ELF32_ST_INFO (symtab[i].st_info));
*/
			return (void *) symtab[i].st_value;
			}
#endif

	return NULL;
}

