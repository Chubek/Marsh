#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>


#include "command_redirect.h"
#include "garbage_collect.h"



#define GC_INIT() redir_heap = gc_heap_create()
#define GC_CALLOC(nmemb, size) gc_heap_alloc(redir_heap, nmemb, size)
#define GC_ALLOC(size) GC_CALLOC(1, size)
#define GC_REALLOC(mem, new_size) gc_heap_realloc(redir_heap, mem, new_size)
#define GC_FREE(mem) gc_heap_free(redir_heap, mem)
#define GC_COLLECT() gc_heap_collect(redir_heap)
#define GC_DUMP() gc_heap_dump(redir_heap)
#define GC_MEMDUP(mem, size) gc_heap_memdup(redir_heap, mem, size)
#define GC_VIZ(stream) gc_heap_visualize(redir_heap, stream)

typedef enum TargetKind TargetKind;

typedef enum {
	REDIR_OUT,
	REDIR_OUT_NOCLOBBER,
	REDIR_APPEND,
	REDIR_IN,
	REDIR_OUT_STDERR,
	REDIR_APPEND_STDERR,
	REDIR_OUT_HYBRID,
	REDIR_APPEND_HYBRID,
	REDIR_HERE_STR,
	REDIR_HERE_STR_ESCAPED,
	REDIR_HERE_DOC,
	REDIR_HERE_DOC_ESCAPED,
	REDIR_DUP_INFD,
	REDIR_DUP_OUTFD,
	REDIR_OPEN_RW,
} IORedirKind;

typedef struct IORedir {
	struct IORedir *next_p;
	IORedirKind redir_kind;
	int source_fd;
	
	enum TargetKind {
		TARGET_FILENAME,
		TARGET_FILEDESC,
	} target_kind;

	union {
		char *target_name;
		int target_desc;
	};
} IORedir;



IORedir *create_io_redir(IORedirKind redir_kind, int source_fd, TargetKind target_kind, void *target) {
	IORedir *redir = 
		(IORedir *)GC_ALLOC(sizeof(IORedir));
	redir->next_p = NULL;
	redir->redir_kind = redir_kind;
	redir->source_fd = source_fd;
	redir->target_kind = target_kind;
	
	if (target_kind == TARGET_FILENAME)
		redir->target_name = (char*)target;
	else
		redir->target_desc = *((int*)target);

	return redir;
}

void io_redir_add_next(IORedir *redir, IORedir *next) {
	if (redir == NULL || next == NULL)
		return;

	if (redir->next_p == NULL)
		redir->next_p = next;
	else {
		for (IORedir *temp = redir; temp->next_p != NULL; temp = temp->next_p);
		next->next_p = NULL;
		temp->next_p = next;
	}
}
