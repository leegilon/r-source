/*
 *  R : A Computer Language for Statistical Data Analysis
 *  Copyright (C) 1995, 1996, 1997  Robert Gentleman and Ross Ihaka
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *
 *  I/O Support Code
 *
 *  This is a general IO support package to provide R with a uniform
 *  interface to reading data from the console, files and internal
 *  text strings.
 *
 *  This is probably overkill, but it works much better than the
 *  previous anarchy.
 */

#include "IOStuff.h"


/* Move the iob->write_buf pointer to the next */
/* BufferListItem in the chain. If there no next */
/* buffer item, then one is added. */

static int NextWriteBufferListItem(IoBuffer *iob)
{
    if (iob->write_buf->next) {
	iob->write_buf = iob->write_buf->next;
    }
    else {
	BufferListItem *new;
	if (!(new = (BufferListItem*)malloc(sizeof(BufferListItem))))
	    return 0;
	new->next = NULL;
	iob->write_buf->next = new;
	iob->write_buf = iob->write_buf->next;
    }
    iob->write_ptr = iob->write_buf->buf;
    iob->write_offset = 0;
    return 1;
}

/* Move the iob->read_buf pointer to the next */
/* BufferListItem in the chain. */

static int NextReadBufferListItem(IoBuffer *iob)
{
    iob->read_buf = iob->read_buf->next;
    iob->read_ptr = iob->read_buf->buf;
    iob->read_offset = 0;
    return 1;
}


/* Reset the read/write pointers of an IoBuffer */

int R_IoBufferWriteReset(IoBuffer *iob)
{
    if (iob == NULL || iob->start_buf == NULL)
	return 0;
    iob->write_buf = iob->start_buf;
    iob->write_ptr = iob->write_buf->buf;
    iob->write_offset = 0;
    iob->read_buf = iob->start_buf;
    iob->read_ptr = iob->read_buf->buf;
    iob->read_offset = 0;
    return 1;
}

/* Reset the read pointer of an IoBuffer */

int R_IoBufferReadReset(IoBuffer *iob)
{
    if (iob == NULL || iob->start_buf == NULL)
	return 0;
    iob->read_buf = iob->start_buf;
    iob->read_ptr = iob->read_buf->buf;
    iob->read_offset = 0;
    return 1;
}

/* Allocate an initial BufferListItem for IoBuffer */
/* Initialize the counts and pointers. */

int R_IoBufferInit(IoBuffer *iob)
{
    if (iob == NULL) return 0;
    iob->start_buf = (BufferListItem*)malloc(sizeof(BufferListItem));
    if (iob->start_buf == NULL) return 0;
    iob->start_buf->next = NULL;
    return R_IoBufferWriteReset(iob);
}

/* Free any BufferListItem's associated with an IoBuffer */
/* This resets pointers to NULL, which could be detected */
/* in other calls. */

int R_IoBufferFree(IoBuffer *iob)
{
    BufferListItem *thisItem, *nextItem;
    if (iob == NULL || iob->start_buf == NULL)
	return 0;
    thisItem = iob->start_buf;
    while (thisItem) {
	nextItem = thisItem->next;
	free(thisItem);
	thisItem = nextItem;
    }
    return 1;
}

/* Add a character to an IoBuffer */

int R_IoBufferPutc(int c, IoBuffer *iob)
{
    if (iob->write_offset == IOBSIZE)
	NextWriteBufferListItem(iob);
    *(iob->write_ptr)++ = c;
    iob->write_offset++;
    return 0;/*not used*/
}

/* Add a (null terminated) string to an IoBuffer */

int R_IoBufferPuts(char *s, IoBuffer *iob)
{
    char *p;
    int n = 0;
    for (p = s; *p; p++) {
	R_IoBufferPutc(*p, iob);
	n++;
    }
    return n;
}

/* Read a character from an IoBuffer */

int R_IoBufferGetc(IoBuffer *iob)
{
    if (iob->read_buf == iob->write_buf &&
       iob->read_offset >= iob->write_offset)
	return EOF;
    if (iob->read_offset == IOBSIZE) NextReadBufferListItem(iob);
    iob->read_offset++;
    return *(iob->read_ptr)++;
}

/* Push a character back onto an IoBuffer */
/* (Only one character of pushback is guaranteed) */

int R_IoBufferUngetc(int c, IoBuffer *iob)
{
    if (iob->read_offset == 0) return EOF;
    --(iob->read_offset);
    --(iob->read_ptr);
    *(iob->read_ptr) = c;
    return c;
}

	/* Initialization code for text buffers */

static void transferChars(char *p, char *q)
{
    while (*q) *p++ = *q++;
    *p++ = '\n';
    *p++ = '\0';
}

int R_TextBufferInit(TextBuffer *txtb, SEXP text)
{
    int i, k, l, n;
    if (isString(text)) {
	n = length(text);
	l = 0;
	for (i = 0; i < n; i++) {
	    if (STRING(text)[i] != R_NilValue) {
		k = strlen(CHAR(STRING(text)[i]));
		if (k > l)
		    l = k;
	    }
	}
	txtb->vmax = vmaxget();
	txtb->buf = R_alloc(l+2, sizeof(char));	/* '\n' and '\0' */
	txtb->bufp = txtb->buf;
	txtb->text = text;
	txtb->ntext = n;
	txtb->offset = 0;
	transferChars(txtb->buf,
		      CHAR(STRING(txtb->text)[txtb->offset]));
	txtb->offset++;
	return 1;
    }
    else {
	txtb->vmax = vmaxget();
	txtb->buf = NULL;
	txtb->bufp = NULL;
	txtb->text = R_NilValue;
	txtb->ntext = 0;
	txtb->offset = 1;
	return 0;
    }
}

/* Finalization code for text buffers */

int R_TextBufferFree(TextBuffer *txtb)
{
    vmaxset(txtb->vmax);
    return 0;/* not used */
}

/* Getc for text buffers */

int R_TextBufferGetc(TextBuffer *txtb)
{
    if (txtb->buf == NULL)
	return EOF;
    if (*(txtb->bufp) == '\0') {
	if (txtb->offset == txtb->ntext) {
	    txtb->buf = NULL;
	    return EOF;
	}
	else {
	    transferChars(txtb->buf,
			  CHAR(STRING(txtb->text)[txtb->offset]));
	    txtb->bufp = txtb->buf;
	    txtb->offset++;
	}
    }
    return *txtb->bufp++;
}

int R_TextBufferUngetc(int c, TextBuffer *txtb)
{
    return (*--txtb->bufp = c);
}
