#include <stdlib.h>
#include <stdio.h>
#include "circbuf.h"
#include "ads131.h"
#include "main.h"



/* ������������� ������, ���� ��� ���!  */
int cb_init(CircularBuffer * cb, int size)
{
    cb->size = size + 1;	/* include empty elem */
    cb->start = 0;
    cb->end = 0;

    if (cb->elems == NULL) {
	cb->elems = (ElemType *) mem_Malloc(cb->size * sizeof(ElemType));

	if (cb->elems == NULL)	/* �� ���� �������� ������ */
	    return -1;
    }
   return 0;
}


/* ������� �����, ���� �� ����������! */
void cb_free(CircularBuffer * cb)
{
    if (cb->elems != NULL) {
	mem_Free(cb->elems);	/* OK if null */
	cb->elems = 0;
    }
}

/**
 * ����� ������?
 */
int cb_is_full(CircularBuffer * cb)
{
    return (cb->end + 1) % cb->size == cb->start;
}

/**
 * ����� ������?
 */
int cb_is_empty(CircularBuffer * cb)
{
    return cb->end == cb->start;
}

/**
 * �������� �����
 */
void cb_clear(CircularBuffer * cb)
{
    cb->start = 0;
    cb->end = 0;    
}


/* Write an element, overwriting oldest element if buffer is full. App can
   choose to avoid the overwrite by checking cbIsFull(). */
void cb_write(CircularBuffer * cb, ElemType * elem)
{
// ������ �� ������������?  
    mem_copy(&cb->elems[cb->end], elem, sizeof(ElemType));
      
    cb->end = (cb->end + 1) % cb->size;
    if (cb->end == cb->start) {
	cb->start = (cb->start + 1) % cb->size;	/* full, overwrite */
    }
}

/* Read oldest element. App must ensure !cbIsEmpty() first. */
void cb_read(CircularBuffer * cb, ElemType * elem)
{
     mem_copy(elem, &cb->elems[cb->start], sizeof(ElemType));

     cb->start = (cb->start + 1) % cb->size;
}
