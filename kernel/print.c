/*
 * Copyright (c) 2002 Matteo Frigo
 * Copyright (c) 2002 Steven G. Johnson
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

/* $Id: print.c,v 1.7 2002-06-13 15:04:24 athena Exp $ */

#include "ifftw.h"
#include <stdarg.h>
#include <stdio.h>

#define BSZ 32

static void myputs(printer *p, const char *s)
{
     char c;
     while ((c = *s++))
          p->putchr(p, c);
}

static void print(printer *p, const char *format, ...)
{
     char buf[BSZ];
     const char *s = format;
     char c;
     va_list ap;
     uint i;

     for (i = 0; i < p->indent; ++i)
          p->putchr(p, ' ');

     va_start (ap, format);
     while ((c = *s++)) {
          switch (c) {
	      case '%':
		   switch ((c = *s++)) {
		       case 'c': {
			    int x = va_arg(ap, int);
			    p->putchr(p, x);
			    break;
		       }
		       case 's': {
			    char *x = va_arg(ap, char *);
			    myputs(p, x);
			    break;
		       }
		       case 'd': {
			    int x = va_arg(ap, int);
			    sprintf(buf, "%d", x);
			    goto putbuf;
		       }
		       case 'v': {
			    /* print optional vector length */
			    uint x = va_arg(ap, uint);
			    if (x > 1) {
				 sprintf(buf, "-x%u", x);
				 goto putbuf;
			    }
			    break;
		       }
		       case 'o': {
			    /* integer option.  Usage: %oNAME= */
			    int x = va_arg(ap, int);
			    if (x)
				 p->putchr(p, '/');
			    while ((c = *s++) != '=')
				 if (x)
				      p->putchr(p, c);
			    if (x) {
				 sprintf(buf, "=%d", x);
				 goto putbuf;
			    }
			    break;
		       }
		       case 'u': {
			    uint x = va_arg(ap, uint);
			    sprintf(buf, "%u", x);
			    goto putbuf;
		       }
		       case '(': {
			    /* newline, augment indent level */
			    p->putchr(p, '\n');
			    p->indent += p->indent_incr;
			    break;
		       }
		       case ')': {
			    /* decrement indent level */
			    p->indent -= p->indent_incr;
			    break;
		       }
		       case 'p': {
			    /* print plan */
			    plan *x = va_arg(ap, plan *);
			    if (x) 
				 x->adt->print(x, p);
			    else 
				 goto putnull;
			    break;
		       }
		       case 'P': {
			    /* print problem */
			    problem *x = va_arg(ap, problem *);
			    if (x)
				 x->adt->print(x, p);
			    else
				 goto putnull;
			    break;
		       }
		       case 't': {
			    /* print tensor */
			    tensor *x = va_arg(ap, tensor *);
			    if (x)
				 X(tensor_print)(*x, p);
			    else
				 goto putnull;
			    break;
		       }
		       default:
			    A(0 /* unknown format */);
			    break;

		   putbuf:
			    myputs(p, buf);
			    break;
		   putnull:
			    myputs(p, "(null)");
			    break;
		   }
		   break;
	      default:
		   p->putchr(p, c);
		   break;
          }
     }
     va_end(ap);
}

printer *X(mkprinter)(size_t size, void (*putchr)(printer *p, char c))
{
     printer *s = (printer *)fftw_malloc(size, OTHER);
     s->print = print;
     s->putchr = putchr;
     s->indent = 0;
     s->indent_incr = 2;
     return s;
}

void X(printer_destroy)(printer *p)
{
     X(free)(p);
}
