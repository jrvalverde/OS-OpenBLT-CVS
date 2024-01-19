/* $Id: //depot/blt/lib/libblt/namer.c#4 $
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

#include <blt/namer.h>
#include <blt/error.h>
#include <blt/syscall.h>

#include <string.h>
#include <stdlib.h>

static int __namer_port = -1;
static int __namer_sem = -1;
static namer_request_t *__namer_req = NULL;

#define CHECK(errmsg) if((__namer_port < 0) && connect()) return (errmsg)
#define LOCK() sem_acquire(__namer_sem)
#define UNLOCK() sem_release(__namer_sem)
#define nreq __namer_req

static 
int connect(void)
{
	if((__namer_port = port_create(NAMER_PORT,"local_namer_port")) < 0) return -1;
	if((__namer_sem = sem_create(1,"local_namer_sem")) < 0) return -1;
	__namer_req = (namer_request_t *) malloc(MAXMSG);
	return 0;
}

int 
do_request(int opcode, token_t token, uint32 type, const void *data, int size)
{
	msg_hdr_t mh;
	
	if(size > MAXMSGDATA) return -1;
	
	nreq->opcode = opcode;
	nreq->token = token;
	nreq->size = size;
	nreq->type = type;
	if(size && data) memcpy(nreq->data,data,size);

	mh.dst = NAMER_PORT;
	mh.src = __namer_port;
	mh.size = HDRSIZE + size;
	mh.data = __namer_req;
	
	if(port_send(&mh) != mh.size) {
		return -1;
	}
	
	mh.dst = __namer_port;
	mh.size = MAXMSG;
	return port_recv(&mh);	
}

token_t
namer_walk(token_t token, const char *name)
{
	CHECK(TOKEN_NONE);	
	
	LOCK();
	if(do_request(OP_WALK, token, 0, name, strlen(name)+1) != HDRSIZE) {
		token = TOKEN_NONE;
	} else {
		token = nreq->token;
	}
	UNLOCK();
	
	return token;
}

int
namer_stat(token_t token, const char *name, token_info_t *info)
{
	int res;
	CHECK(-1);
		
	LOCK();
	if(do_request(OP_STAT, token, 0, name, strlen(name)+1) != HDRSIZE) {
		res = -1;
	} else {
		if(!(res = nreq->opcode)){
			memcpy(info,&nreq->token,INFOSIZE);
		}
	}
	UNLOCK();
	
	return res;
}

token_t 
namer_create(token_t token, const char *name, uint32 type)
{
	CHECK(TOKEN_NONE);
		
	LOCK();
	if(do_request(OP_CREATE, token, type, name, strlen(name)+1) != HDRSIZE) {
		token = TOKEN_NONE;
	} else {
		token = nreq->opcode ? TOKEN_NONE : nreq->token;
	}
	UNLOCK();
	
	return token;
}

int 
namer_delete(token_t token)
{
	int res;
	CHECK(-1);
		
	LOCK();
	if(do_request(OP_DELETE, token, 0, NULL, 0) != HDRSIZE) {
		res = -1;
	} else {
		res = nreq->opcode;
	}
	UNLOCK();
	
	return res;
}

int
namer_write(token_t token, uint32 type, uint32 size, void *data)
{
	int res;
	CHECK(-1);
		
	LOCK();
	if((res = do_request(OP_WRITE, token, type, data, size)) != HDRSIZE) {
		res = -1;
	} else {
		res = nreq->opcode;
	}
	UNLOCK();
	
	return res;
}

int
namer_read(token_t token, uint32 type, uint32 *size, void *data)
{
	int res;
	CHECK(-1);
		
	LOCK();
	if((res = do_request(OP_READ, token, type, NULL, *size)) < HDRSIZE) {
		res = -1;
	} else {
		if(nreq->size > *size){
			/* bad response - overrun */
			res = -1;
		} else {
			if(!(res = nreq->opcode)){
				memcpy(data, nreq->data, nreq->size);
				*size = nreq->size;
			}
		}
	}
	UNLOCK();
	
	return res;
}

token_t namer_resolve(const char *path, int create)
{
	token_t root;
	token_t next = TOKEN_ROOT;
	char *slash, *tmp;
	
	if(path[0] != '/') return TOKEN_NONE;
	path++;
	
	if(!(tmp = (char *) malloc(strlen(path)))) return TOKEN_NONE;
	memcpy(tmp,path,strlen(path)+1);
	path = tmp;
	
	while((root = next) && (*path)){
		if((slash = strchr(path,'/'))) *slash = 0;
		
		next = namer_walk(root,path);
		if(!next && create){
			next = namer_create(root,path,TYPE_DIRECTORY);
		}			
		
		if(slash) path = slash + 1;
	}
	free(tmp);
	return root;	
}

/* ----------- old interface wrappers ----------------- */

int namer_newhandle(void)
{
	return 1;
}

int namer_delhandle(int nh)
{
	return 0;
}

int namer_register(int nh, int port, char *name)
{
	token_t token = namer_walk(TOKEN_ROOT,"servers");
//	DEBUG("register %d %d \"%s\" (%d)\n",nh,port,name,token);
	if(token) {
		token = namer_create(token,name,TYPE_INT32);
		if(token) {
			return namer_write(token,TYPE_INT32,4,&port);
		}
	} 
	return -1;
}

int namer_find(int nh, char *name)
{	
	token_t token = namer_walk(TOKEN_ROOT,"servers");
//	DEBUG("find servers %d",token);
	if(token) {
		token = namer_walk(token,name);
//		DEBUG("find servers/%s %d",name,token);
		if(token){
			uint32 size = 4;
			int32 value;
			if(namer_read(token,TYPE_INT32,&size,&value)) return -1;
			if(size == 4) {
				return value;
			}
		}
	}
	return -1;
}
