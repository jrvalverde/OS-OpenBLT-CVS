/* $Id: //depot/blt/lib/linker/link.c#2 $
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
#include <stdio.h>
#endif

#include "file.h"
#include "elf.h"
#include "link.h"
#include "defs.h"

extern unsigned int num_files;

int _dl_resolve_sym (const char *name, int disallow)
{
	int j, sym_loc;

	/*
	 * check each file for the symbol.  if we find it, we have to
	 * add to that value the start of the file.
	 */
	for (j = 0, sym_loc = 0; (j < num_files) && !sym_loc; j++)
		if ((j != disallow) && (j != 1))
			sym_loc = (unsigned int) _elf_lookup_sym (j, name);
	//printf ("found in %d\n", j - 1);
	if (!sym_loc)
		error ("linker: undefined symbol %s in %s\n", name, files[disallow].name);
	return sym_loc + files[j - 1].addr;
}

/*
 * resolve external references for the given file number.
 */
void _dl_patchup (int filenum)
{
	char *sym_name;
	unsigned int *got, i, j, sym_loc;
	elf32_sym_t *dynsym;
	elf32_rel_t *rel;
	file_info *file;

	file = &files[filenum];
	if (file->nopatch)
		return;
	dynsym = (elf32_sym_t *) (file->addr + file->dynsym_off);

	/*
	 * set special got entries.
	 *
	 * the third entry is the address to jump to in the dynamic linker
	 * to patch a plt entry when we do lazy binding.	the second entry
	 * is reserved for us and gets pushed on the stack before jumping
	 * to that entry point.  since we're not yet cool enough to do lazy
	 * binding, we set things up to generate a page fault if a plt
	 * entry tries to call the dynamic resolver.
	 */
	got = (unsigned int *) (file->addr + file->got_off); /* giggle, giggle */
	*(got + 1) = filenum;
	*(got + 2) = 0; /* b00m */

	printf ("patching up %s....\n", files[filenum].name);

	/*
	 * patch function references via plt entries in the got.  this could
	 * be done lazily.
	 */
	if (files[filenum].jmprel_off)
		{
			printf ("    functions:\n");

			/* find list of functions that need resolution */
			rel = (elf32_rel_t *) (file->addr + file->jmprel_off);
printf ("--- offset is %x %x\n", file->jmprel_off, file->addr);

			/*
			 * iterate through them looking through each file to resolve it.
			 * n.b. at this point the symtab_off and strtab_off fields for
			 * each file contain absolute addresses with respect to our
			 * current environment, not offsets from the start of the file.
			 */
			for (i = 0; i < (files[filenum].jmprel_size / sizeof (elf32_rel_t)); i++)
				{
					/* find the name of the symbol in the .symstr */
					sym_name = (char *) (file->addr + file->dynstr_off +
						dynsym[ELF32_R_SYM (rel[i].r_info)].st_name);

					sym_loc = _dl_resolve_sym (sym_name, filenum);

					/* patch me up */
					*((unsigned int *) rel[i].r_offset) = sym_loc;
					printf ("        %6x -> %6x, %s\n", filenum ? rel[i].r_offset +
						files[filenum].addr : rel[i].r_offset, sym_loc, sym_name);
				}
		}

	if (files[filenum].rel_off)
		{
			printf ("    data:\n");

			/* find list of data references in the got that need resolution */
			rel = (elf32_rel_t *) (file->addr + file->rel_off);

			/* patch them up.  previous comments apply. */
			for (i = 0; i < (files[filenum].rel_size / sizeof (elf32_rel_t)); i++)
				{
					sym_name = (char *) (file->addr + file->dynstr_off +
						dynsym[ELF32_R_SYM (rel[i].r_info)].st_name);
					if (!*sym_name)
						continue;

					sym_loc = _dl_resolve_sym (sym_name, filenum ? -1 : 0);
					//sym_loc = _dl_resolve_sym (sym_name, filenum);

					switch (ELF32_R_TYPE (rel[i].r_info))
						{
							case R_386_32:
								break;
							case R_386_PC32:
								break;
							case R_386_COPY:
								break;
							default:
								error ("linker: unhandled relocation type %d\n",
									ELF32_R_TYPE (rel[i].r_info));
								break;
						}

						printf ("        %6x -> %6x, %s\n", filenum ?
							rel[i].r_offset + files[filenum].addr : rel[i].r_offset,
							sym_loc, sym_name);
				}
		}
}

