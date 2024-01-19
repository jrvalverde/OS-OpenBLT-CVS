/* $Id: //depot/blt/kernel/thread.c#4 $
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

#include <string.h>
#include <blt/os.h>
#include "aspace.h"
#include "kernel.h"
#include "thread.h"

int thr_detach (unsigned int eip)
{
	int i, clone;
	void *ptr, *phys, *src, *dst;
	aspace_t *a;
	task_t *t;
	area_t *text, *orig_heap;

	a = aspace_create ();
	text = rsrc_find_area (current->text_area);
	ptr = kgetpages2 (text->length, 3, (uint32 *) &phys);
	for (i = 0; i < text->length * 0x1000; i++)
		*((char *) ptr + i) = *((char *) 0x1000 + i);

	t = new_thread (a, eip, 0);
	//t->rsrc.owner = current;
	t->text_area = area_create (a, text->length * 0x1000, 0x1000, &phys,
		0x1010);
	//a->heap_id = area_create (a, 0x2000, 0x1000 + text->length * 0x1000,
	//	&ptr, 0);
	orig_heap = rsrc_find_area (current->addr->heap_id);
	a->heap_id = area_create (a, orig_heap->length * 0x1000, 0x1000 +
		text->length * 0x1000, &ptr, 0);
	clone = area_clone (current->addr, a->heap_id, 0, &dst, 0);
	for (i = 0; i < orig_heap->length * 0x1000; i++)
		*((char *) dst + i) = *((char *) (orig_heap->virt_addr * 0x1000) + i);
	area_destroy (a, clone);

	strlcpy (t->rsrc.name, current->rsrc.name, sizeof (t->rsrc.name));
	strlcat (t->rsrc.name, "+", sizeof (t->rsrc.name));

	rsrc_set_owner (&a->rsrc, t);
	rsrc_set_owner (&t->rsrc, t);
	return t->rsrc.id;
}

int thr_join (int thr_id, int options)
{
	task_t *task = rsrc_find_task(thr_id);
	
	if(task) {
		wait_on((resource_t *)task);
		return ERR_NONE;
	} else {
		return ERR_RESOURCE;
	}
}

