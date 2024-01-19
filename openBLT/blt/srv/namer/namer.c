/* $Id: //depot/blt/srv/namer/namer.c#4 $
**
** Copyright 1998 Brian J. Swetland
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
#include <blt/syscall.h>
#include <blt/namer.h>
#include <blt/error.h>
#include <string.h>
#include <stdlib.h>

#if 0
char blah[128];
#define DEBUG(x...) {snprintf(blah,128,x); os_console(blah);}
#else
#define DEBUG(x...) (void)0
#endif

typedef struct nlist
{
	struct nlist *prev;
	struct nlist *next;
} nlist;

typedef struct nnode
{
	nlist link;
	struct nnode *parent;
	char *name;
	uint32 type;
	token_t token;
	uint32 size;
	void *data;
} nnode;

static nnode *root;

static nnode **lookup_token;
static uint32 lookup_size = 0;
static token_t lookup_max = 0;

/* -------- internal interface -------------------------------- */

nnode *resolve(token_t token)
{
	if(token >= lookup_size) return NULL;
	return lookup_token[token];
}

token_t forge(nnode *node)
{
	int i;
	
	if(lookup_max > lookup_size){
again:
		i = lookup_size;
		lookup_token[i] = node;
		lookup_size++;
		return i;
	}
	
	for(i=1;i<lookup_size;i++){
		if(lookup_token[i] == NULL){
			lookup_token[i] = node;
			return i;
		}
	}
	
	lookup_max *= 2;
	lookup_token = realloc(lookup_token, lookup_max * sizeof(nnode *));
	goto again;
}

void destroy(token_t token)
{
	if(token < lookup_size) lookup_token[token] = NULL;
}

nnode *find_entry(nnode *root, const char *name)
{
	nlist *l,*r;
	
	if(root->type != TYPE_DIRECTORY) return NULL;
	
	if(name[0] == '.'){
		if(name[1] == 0) return root;
		if(name[1] == '.') {
			if(name[2] == 0) return root->parent;
		}
	}
	
	r = (nlist *) root->data;
	for(l = r->next; l != r; l = l->next) {
		if(!strcmp(name,((nnode *)l)->name)) return ((nnode*) l);
	}
	return NULL;
}

nnode *add_entry(nnode *root, const char *name, uint32 type)
{
	nnode *n;
	nlist *l,*r;
	int len;
	
	if(root->type != TYPE_DIRECTORY) return NULL;
	
	if(find_entry(root,name)) return NULL;
	
	r = (nlist *) root->data;
	
	n = (nnode *) malloc(sizeof(nnode));
	len = strlen(name) + 1;
	n->name = (char *) malloc(len);
	memcpy(n->name,name,len);
	n->parent = root;
	n->token = forge(n);
	n->type = type;
	n->size = 0;
	if(type == TYPE_DIRECTORY){
		l = (nlist *) malloc(sizeof(nlist));
		l->next = l;
		l->prev = l;
		n->data = l;
	} else {
		n->data = NULL;
	}
	
	n->link.next = r->next;
	n->link.prev = r->next->prev;
	r->next->prev = &n->link;
	r->next = &n->link;
	root->size++;
	return n;
}

void
remove_entry(nnode *parent, nnode *n)
{
	parent->size--;
	n->link.next->prev = n->link.prev;
	n->link.prev->next = n->link.next;
}

/* -------- external interface -------------------------------- */

token_t 
_namer_walk(token_t token, const char *name)
{
	nnode *n = resolve(token);
	DEBUG("namer: walk(%d,\"%s\")",token,name);
	if(!n) return TOKEN_NONE;
	n = find_entry(n,name);
	return n ? n->token : TOKEN_NONE;
}

int
_namer_stat(token_t token, const char *name, token_info_t *info)
{
	nnode *n = resolve(token);
	if(!n) return -1;
	n = find_entry(n,name);
	if(!n) return -1;
	info->token = n->token;
	info->type = n->type;
	info->size = n->size;
	return 0;
}

token_t
_namer_create(token_t token, const char *name, uint32 type)
{
	nnode *n = resolve(token);
	DEBUG("namer: create(%d,\"%s\",%x)",token,name,type);
	if(!n) return TOKEN_NONE;
	n = add_entry(n, name, type);
	if(n) {
		return n->token;
	} else {
		return TOKEN_NONE;
	}
}

int 
_namer_delete(token_t token)
{
	nnode *n = resolve(token);
	DEBUG("namer: delete(%d)",token);
	if(!n || (n==root)) return -1;
	if((n->type == TYPE_DIRECTORY) && (n->size)) return -1;
	remove_entry(n->parent,n);
	destroy(n->token);
	if(n->size) free(n->data);
	free(n);
	return 0;
}

int 
_namer_write(token_t token, uint32 type, uint32 size, void *data)
{
	nnode *n = resolve(token);
	DEBUG("namer: write(%d,%x,%d,...)",token,type,size);
	if(!n || (n->type == TYPE_DIRECTORY)) return -1;
	if(n->size) free(n->data);
	n->size = size;
	if(size){
		n->data = malloc(size);
		memcpy(n->data,data,size);
	} else {
		n->data = NULL;
	}
	return 0;
}

int 
_namer_read(token_t token, uint32 type, uint32 *size, void *data)
{
	nnode *n = resolve(token);
	DEBUG("namer: read(%d,%x,%d,...)",token,type,*size);
	if(!n) return -1;
	if(type == TYPE_NAME){
		int len = strlen(n->name) + 1;
		if(len > *size) return -1;
		memcpy(data,n->name,len);
		*size = n;
		return 0;
	}
	if(n->type == TYPE_DIRECTORY){
		if(type == TYPE_ENTRIES){
			int count = n->size;
			uint32 sz = 0;
			nlist *l = n->link.next;
			if(count*4 > *size) count = *size/4;
			while(count){
				((token_t *) data)[count] = ((nnode *)l)->token;
				l = l->next;
				count--;
				sz += 4;
			}
			*size = sz;
			return 0;
		} else {
			return -1;
		}
	}	
	if(type != n->type) return -1;
	if(n->size > *size) return -1;
	*size = n->size;
	if(*size) memcpy(data,n->data,*size);
	return 0;
}

static 
void namer_init(void)
{
	root = (nnode *) malloc(sizeof(nnode));
	root->parent = root;
	root->name = "";
	root->type = TYPE_DIRECTORY;
	root->token = 0;
	root->size = 0;
	((nlist*) root->data) = (nlist *) malloc(sizeof(nlist));
	((nlist*) root->data)->next = ((nlist*) root->data);
	((nlist*) root->data)->prev = ((nlist*) root->data);    
	
	lookup_max = 64;
	lookup_token = (nnode **) malloc(sizeof(nnode *) * lookup_max);
	lookup_token[0] = NULL;
	lookup_token[1] = root;
	lookup_size = 2;	
}

int main(void)
{
    msg_hdr_t mh;
    namer_request_t *msg = (namer_request_t *) malloc(MAXMSG);    
    int l;

	namer_init();
	
	_namer_create(TOKEN_ROOT, "servers", TYPE_DIRECTORY);
	
    for(;;){        
        mh.dst = NAMER_PORT;
        mh.data = msg;
        mh.size = MAXMSG;
        l = port_recv(&mh);
        
        if(l < 0) {
            os_console("namer: can't get port");
            os_terminate(-1);            
        }

		mh.size = HDRSIZE;
		
		/* ensure basic message is valid */
		if((l < HDRSIZE) || (l > (MAXMSGDATA))) goto error;
		
		if(msg->opcode < OP_DELETE){
			if(msg->size == 0) goto error;
			msg->data[msg->size-1] = 0; /* force asciiz */
		}
				
		switch(msg->opcode){
		case OP_WALK:
			msg->opcode = 0;
			msg->token = _namer_walk(msg->token, (char*) msg->data);
			break;
			
		case OP_STAT:
			msg->opcode = _namer_stat(msg->token, (char*) msg->data, 
									  (token_info_t*) &msg->token);
			break;
			
		case OP_CREATE:
			msg->opcode = 0;
			msg->token = _namer_create(msg->token, (char*) msg->data, msg->type);
			break;
			
		case OP_DELETE:
			msg->opcode = _namer_delete(msg->token);
			break;
			
		case OP_WRITE:
			msg->opcode = _namer_write(msg->token, msg->type, msg->size, msg->data);
			break;
						
		case OP_READ:
			msg->opcode = _namer_read(msg->token, msg->type, &msg->size, msg->data);
			if(msg->opcode == 0) mh.size = HDRSIZE + msg->size;
			break;
			
		default:
			msg->opcode = -1;
		}
		
error:
		if(msg->opcode) {
			msg->type = 0;
			msg->size = 0;
			msg->token = 0;
		}
		
        mh.dst = mh.src;
        mh.src = NAMER_PORT;
        l = port_send(&mh);

/*        if(l == ERR_RESOURCE) os_console("namer: ERR_RESOURCE");
        if(l == ERR_PERMISSION) os_console("namer: ERR_PERMISSION");
        if(l == ERR_MEMORY) os_console("namer: ERR_MEMORY");*/
    }
    return 0;
}
