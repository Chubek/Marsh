#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct GCNode GCNode;
typedef struct GCHeap GCHeap;

struct GCNode {
  void *data;
  GCNode *next;
  bool alive;
};

struct GCHeap {
  GCNode *root;
};

static GCNode *create_gc_node(void *data) {
  GCNode *node = (GCNode *)malloc(sizeof(GCNode));

  if (node != NULL) {
    node->data = data;
    node->next = NULL;
    node->alive = 0;
  }

  return node;
}

GCHeap *gc_heap_create(void) {
  GCHeap *heap = (GCHeap *)malloc(sizeof(GCHeap));

  if (heap != NULL) {
    heap->root = NULL;
  }

  return heap;
}

void *gc_heap_alloc(GCHeap *heap, size_t nmemb, size_t size) {
  void *data = calloc(nmemb, size);

  if (data != NULL) {
    GCNode *node = create_gc_node(data);
    if (node != NULL) {
      node->next = heap->root;
      heap->root = node;
    } else {
      free(data);
    }
  }

  return data;
}

void *gc_heap_realloc(GCHeap *heap, void *data, size_t create_size) {
  GCNode *node = NULL;
  for (node = heap->root; node != NULL; node = node->next)
    if (node->data == data)
      break;

  if (node == NULL)
    return data;

  node->data = realloc(node->data, create_size);
  return node->data;
}

void gc_heap_free(GCHeap *heap, void *data) {
  GCNode *node = NULL;
  for (node = heap->root; node != NULL; node = node->next) {
    if (node->data == data)
      break;
  }

  if (node == NULL)
    return;

  free(node->data);
  free(node);
}

void *gc_heap_memdup(GCHeap *heap, void *data, size_t size) {
  void *dup = gc_heap_alloc(heap, size);
  return memmove(dup, data, size);
}

static void mark(GCNode *node) {
  if (node == NULL || node->alive)
    return;

  if (node->data == NULL)
    node->alive = false;
}

static void sweep(GCHeap *heap) {
  GCNode *current = heap->root;
  GCNode *prev = NULL;

  while (current != NULL) {
    if (!current->alive) {
      GCNode *temp = current;
      current = current->next;
      if (prev != NULL) {
        prev->next = current;
      } else {
        heap->root = current;
      }
      free(temp->data);
      free(temp);
    } else {
      current->alive = false;
      prev = current;
      current = current->next;
    }
  }
}

void gc_heap_collect(GCHeap *heap) {
  mark(heap->root);
  sweep(heap);
}

void gc_heap_dump(GCHeap *heap) {
  GCNode *current = heap->root;
  while (current != NULL) {
    GCNode *temp = current;
    current = current->next;
    free(temp->data);
    free(temp);
  }
  free(heap);
}

void gc_heap_visualize(GCHeap *heap, FILE *stream) {
  fprintf(stream, "GCHeap Visualization:\n");
  fprintf(stream, "Root: %p\n", (void *)heap->root);

  GCNode *current = heap->root;
  int node_count = 0;
  while (current != NULL) {
    fprintf(stream, "Node %d: Address=%p, Alive=%s, Data(first byte)=%02x\n",
            node_count, (void *)current, current->alive ? "Yes" : "No",
            *((unsigned char *)current->data));

    current = current->next;
    node_count++;
  }
}
