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

#include "api.h"

/* Fortran-like (e.g. as in BLAS) type prefixes for F77 interface */
#if defined(FFTW_SINGLE)
#  define x77(name) CONCAT(sfftw_, name)
#  define X77(NAME) CONCAT(SFFTW_, NAME)
#elif defined(FFTW_LDOUBLE)
/* FIXME: what is best?  BLAS uses D..._X, apparently.  Ugh. */
#  define x77(name) CONCAT(lfftw_, name)
#  define X77(NAME) CONCAT(LFFTW_, NAME)
#else
#  define x77(name) CONCAT(dfftw_, name)
#  define X77(NAME) CONCAT(DFFTW_, NAME)
#endif

#ifdef F77_FUNC_

#define F77x(a, A) F77_FUNC_(a, A)
#define F77(a, A) F77x(x77(a), X77(A))

/* C analogues to Fortran integer type: */
typedef int fint;

static int *reverse_n(fint rnk, const fint *n)
{
     int *nrev;
     fint i;
     A(FINITE_RNK(rnk));
     nrev = MALLOC(sizeof(int) * rnk, PROBLEMS);
     for (i = 0; i < rnk; ++i)
          nrev[rnk - i - 1] = n[i];
     return nrev;
}

static X(iodim) *make_dims(fint rnk, const fint *n,
			   const fint *is, const fint *os)
{
     X(iodim) *dims;
     fint i;
     A(FINITE_RNK(rnk));
     dims = MALLOC(sizeof(X(iodim)) * rnk, PROBLEMS);
     for (i = 0; i < rnk; ++i) {
          dims[i].n = n[i];
          dims[i].is = is[i];
          dims[i].os = os[i];
     }
     return dims;
}


void F77(execute, EXECUTE)(X(plan) *p)
{
     X(execute)(*p);
}

void F77(destroy_plan, DESTROY_PLAN)(X(plan) *p)
{
     X(destroy_plan)(*p);
}

void F77(cleanup, CLEANUP)(void)
{
     X(cleanup)();
}

void F77(forget_wisdom, FORGET_WISDOM)(void)
{
     X(forget_wisdom)();
}

typedef struct {
     void (*f77_absorber)(char *, void *);
     void *data;
} absorber_data;

static void absorber(char c, void *d)
{
     absorber_data *ad = (absorber_data *) d;
     ad->f77_absorber(&c, ad->data);
}

void F77(export_wisdom, EXPORT_WISDOM)(void (*f77_absorber)(char *, void *),
				       void *data)
{
     absorber_data ad;
     ad.f77_absorber = f77_absorber;
     ad.data = data;
     X(export_wisdom)(absorber, (void *) &ad);
}

typedef struct {
     void (*f77_emitter)(int *, void *);
     void *data;
} emitter_data;

static int emitter(void *d)
{
     emitter_data *ed = (emitter_data *) d;
     int c;
     ed->f77_emitter(&c, ed->data);
     return (c < 0 ? EOF : c);
}

void F77(import_wisdom, IMPORT_WISDOM)(int *isuccess,
				       void (*f77_emitter)(int *, void *),
				       void *data)
{
     emitter_data ed;
     ed.f77_emitter = f77_emitter;
     ed.data = data;
     *isuccess = X(import_wisdom)(emitter, (void *) &ed);
}

void F77(import_system_wisdom, IMPORT_SYSTEM_WISDOM)(void)
{
     X(import_system_wisdom)();
}

void F77(print_plan, PRINT_PLAN)(X(plan) *p)
{
     X(print_plan)(*p, stdout);
}

void F77(plan_dft, PLAN_DFT)(X(plan) *p, fint *rank, const fint *n,
			     C *in, C *out, fint *sign, fint *flags)
{
     int *nrev = reverse_n(*rank, n);
     *p = X(plan_dft)(*rank, nrev, in, out, *sign, *flags);
     X(ifree0)(nrev);
}

void F77(plan_dft_1d, PLAN_DFT_1D)(X(plan) *p, fint *n, C *in, C *out,
				   fint *sign, fint *flags)
{
     *p = X(plan_dft_1d)(*n, in, out, *sign, *flags);
}

void F77(plan_dft_2d, PLAN_DFT_2D)(X(plan) *p, fint *nx, fint *ny,
				   C *in, C *out, fint *sign, fint *flags)
{
     *p = X(plan_dft_2d)(*ny, *nx, in, out, *sign, *flags);
}

void F77(plan_dft_3d, PLAN_DFT_3D)(X(plan) *p, fint *nx, fint *ny, fint *nz,
				   C *in, C *out,
				   fint *sign, fint *flags)
{
     *p = X(plan_dft_3d)(*nz, *ny, *nx, in, out, *sign, *flags);
}

void F77(plan_many_dft, PLAN_MANY_DFT)(X(plan) *p, fint *rank, const fint *n,
				       fint *howmany,
				       C *in, const fint *inembed,
				       fint *istride, fint *idist,
				       C *out, const fint *onembed,
				       fint *ostride, fint *odist,
				       fint *sign, fint *flags)
{
     int *nrev = reverse_n(*rank, n);
     int *inembedrev = reverse_n(*rank, inembed);
     int *onembedrev = reverse_n(*rank, onembed);
     *p = X(plan_many_dft)(*rank, nrev, *howmany,
			   in, inembedrev, *istride, *idist,
			   out, onembedrev, *ostride, *odist,
			   *sign, *flags);
     X(ifree0)(onembedrev);
     X(ifree0)(inembedrev);
     X(ifree0)(nrev);
}

void F77(plan_guru_dft, PLAN_GURU_DFT)(X(plan) *p, fint *rank, const fint *n,
				       const fint *is, const fint *os,
				       fint *howmany_rank, const fint *h_n,
				       const fint *h_is, const fint *h_os,
				       R *ri, R *ii, R *ro, R *io, fint *flags)
{
     X(iodim) *dims = make_dims(*rank, n, is, os);
     X(iodim) *howmany_dims = make_dims(*howmany_rank, h_n, h_is, h_os);
     *p = X(plan_guru_dft)(*rank, dims, *howmany_rank, howmany_dims,
			   ri, ii, ro, io, *flags);
     X(ifree0)(howmany_dims);
     X(ifree0)(dims);
}

void F77(execute_dft, EXECUTE_DFT)(X(plan) *p, R *ri, R *ii, R *ro, R *io)
{
     X(execute_dft)(*p, ri, ii, ro, io);
}

#endif				/* F77_FUNC_ */
