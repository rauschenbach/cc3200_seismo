#ifndef _CIRCBUF_H
#define _CIRCBUF_H

#include "my_defs.h"
#include "ads131.h"


/* Opaque buffer element type.  This would be defined by the application. */
typedef ADS131_DATA_h ElemType;

/* Circular buffer object */
#pragma pack(4)
typedef struct {
    ElemType   *elems;  /* vector of elements                   */
    u32         size;   /* maximum number of elements           */
    u32         start;  /* index of oldest element              */
    u32         end;    /* index at which to write new element  */
} CircularBuffer;


int   cb_init(CircularBuffer*, int);
void  cb_free(CircularBuffer*);
void  cb_clear(CircularBuffer *);
int   cb_is_full(CircularBuffer*);
int   cb_is_empty(CircularBuffer*);
void  cb_write(CircularBuffer *, ElemType *);
void  cb_read(CircularBuffer *, ElemType *);


#endif /*  circbuf.h  */

