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

/* $Id: vrank-geq1-rdft2.c,v 1.5 2002-09-21 21:47:36 athena Exp $ */


#include "threads.h"

typedef struct {
     solver super;
     int vecloop_dim;
     const int *buddies;
     uint nbuddies;
} S;

typedef struct {
     plan_rdft2 super;

     plan **cldrn;
     int its, ots;
     uint nthr;
     const S *solver;
} P;

typedef struct {
     int its, ots;
     R *r, *rio, *iio;
     plan **cldrn;
} PD;

static void *spawn_apply(spawn_data *d)
{
     PD *ego = (PD *) d->data;
     int its = ego->its, ots = ego->ots;
     uint thr_num = d->thr_num;
     plan_rdft2 *cld = (plan_rdft2 *) ego->cldrn[d->thr_num];

     cld->apply((plan *) cld,
		ego->r + thr_num * its,
		ego->rio + thr_num * ots, ego->iio + thr_num * ots);
     return 0;
}

static void apply_r2hc(plan *ego_, R *r, R *rio, R *iio)
{
     P *ego = (P *) ego_;
     PD d;

     d.its = ego->its;
     d.ots = ego->ots;
     d.cldrn = ego->cldrn;
     d.r = r; d.rio = rio; d.iio = iio;

     X(spawn_loop)(ego->nthr, ego->nthr, spawn_apply, (void*) &d);
}

static void apply_hc2r(plan *ego_, R *r, R *rio, R *iio)
{
     P *ego = (P *) ego_;
     PD d;

     d.its = ego->ots;
     d.ots = ego->its;
     d.cldrn = ego->cldrn;
     d.r = r; d.rio = rio; d.iio = iio;

     X(spawn_loop)(ego->nthr, ego->nthr, spawn_apply, (void*) &d);
}

static void awake(plan *ego_, int flg)
{
     P *ego = (P *) ego_;
     uint i;
     for (i = 0; i < ego->nthr; ++i)
	  AWAKE(ego->cldrn[i], flg);
}

static void destroy(plan *ego_)
{
     P *ego = (P *) ego_;
     uint i;
     for (i = 0; i < ego->nthr; ++i)
	  X(plan_destroy)(ego->cldrn[i]);
     X(free)(ego->cldrn);
     X(free)(ego);
}

static void print(plan *ego_, printer *p)
{
     P *ego = (P *) ego_;
     const S *s = ego->solver;
     uint i;
     p->print(p, "(rdft2-thr-vrank>=1-x%u/%d)", ego->nthr, s->vecloop_dim);
     for (i = 0; i < ego->nthr; ++i)
	  if (i == 0 || (ego->cldrn[i] != ego->cldrn[i-1] &&
			 (i <= 1 || ego->cldrn[i] != ego->cldrn[i-2])))
	       p->print(p, "%(%p%)", ego->cldrn[i]);
     p->putchr(p, ')');
}

static int pickdim(const S *ego, const tensor *vecsz, int oop, uint *dp)
{
     return X(pickdim)(ego->vecloop_dim, ego->buddies, ego->nbuddies,
		       vecsz, oop, dp);
}

static int applicable0(const solver *ego_, const problem *p_,
		       const planner *plnr, uint *dp)
{
     if (RDFT2P(p_) && plnr->nthr > 1) {
          const S *ego = (const S *) ego_;
          const problem_rdft2 *p = (const problem_rdft2 *) p_;
	  if (FINITE_RNK(p->vecsz.rnk)
	      && p->vecsz.rnk > 0
	      && pickdim(ego, &p->vecsz, 
			 p->r != p->rio && p->r != p->iio, dp)) {
	       if (p->r != p->rio && p->r != p->iio)
		    return 1;  /* can always operate out-of-place */

	       return(X(rdft2_inplace_strides)(p, *dp));
	  }
     }

     return 0;
}

static int applicable(const solver *ego_, const problem *p_,
		      const planner *plnr, uint *dp)
{
     const S *ego = (const S *)ego_;

     if (!applicable0(ego_, p_, plnr, dp)) return 0;

     /* fftw2 behavior */
     if (NO_VRANK_SPLITSP(plnr) && (ego->vecloop_dim != ego->buddies[0]))
	  return 0;

     return 1;
}

static plan *mkplan(const solver *ego_, const problem *p_, planner *plnr)
{
     const S *ego = (const S *) ego_;
     const problem_rdft2 *p;
     P *pln;
     problem *cldp;
     uint vdim;
     iodim *d;
     plan **cldrn = (plan **) 0;
     uint i, block_size, nthr;
     int its, ots;
     tensor vecsz = {0, 0};

     static const plan_adt padt = {
	  X(rdft2_solve), awake, print, destroy
     };

     if (!applicable(ego_, p_, plnr, &vdim))
          return (plan *) 0;
     p = (const problem_rdft2 *) p_;

     d = p->vecsz.dims + vdim;

     block_size = (d->n + plnr->nthr - 1) / plnr->nthr;
     nthr = (d->n + block_size - 1) / block_size;
     plnr->nthr = (plnr->nthr + nthr - 1) / nthr;
     its = d->is * block_size;
     ots = d->os * block_size;

     cldrn = fftw_malloc(sizeof(plan *) * nthr, PLANS);
     for (i = 0; i < nthr; ++i) cldrn[i] = (plan *) 0;
     
     vecsz = X(tensor_copy)(p->vecsz);
     for (i = 0; i < nthr; ++i) {
	  vecsz.dims[vdim].n =
	       (i == nthr - 1) ? (d->n - i*block_size) : block_size;
	  cldp = X(mkproblem_rdft2)(p->sz, vecsz,
				    p->r + i*its,
				    p->rio + i*ots, p->iio + i*ots, p->kind);
	  cldrn[i] = MKPLAN(plnr, cldp);
	  X(problem_destroy)(cldp);
	  if (!cldrn[i])
	       goto nada;
     }
     X(tensor_destroy)(vecsz);

     pln = MKPLAN_RDFT2(P, &padt,
			R2HC_KINDP(p->kind) ? apply_r2hc : apply_hc2r);

     pln->cldrn = cldrn;
     pln->its = its;
     pln->ots = ots;
     pln->nthr = nthr;

     pln->solver = ego;
     pln->super.super.ops = X(ops_zero);
     pln->super.super.pcost = 0;
     for (i = 0; i < nthr; ++i) {
	  pln->super.super.ops = X(ops_add)(pln->super.super.ops,
					    cldrn[i]->ops);
	  pln->super.super.pcost += cldrn[i]->pcost;
     }

     return &(pln->super.super);

 nada:
     if (cldrn) {
	  for (i = 0; i < nthr; ++i)
	       if (cldrn[i])
		    X(plan_destroy)(cldrn[i]);
	  X(free)(cldrn);
     }
     X(tensor_destroy)(vecsz);
     return (plan *) 0;
}

static solver *mksolver(int vecloop_dim, const int *buddies, uint nbuddies)
{
     static const solver_adt sadt = { mkplan };
     S *slv = MKSOLVER(S, &sadt);
     slv->vecloop_dim = vecloop_dim;
     slv->buddies = buddies;
     slv->nbuddies = nbuddies;
     return &(slv->super);
}

void X(rdft2_thr_vrank_geq1_register)(planner *p)
{
     uint i;

     /* FIXME: Should we try other vecloop_dim values? */
     static const int buddies[] = { 1, -1 };

     const uint nbuddies = sizeof(buddies) / sizeof(buddies[0]);

     for (i = 0; i < nbuddies; ++i)
          REGISTER_SOLVER(p, mksolver(buddies[i], buddies, nbuddies));
}
