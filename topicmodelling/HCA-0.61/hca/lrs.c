/*
 * Various likelihood-based calculations for testing
 * Copyright (C) 2011-2014 Wray Buntine
 * All rights reserved.
 *
 * This Source Code Form is subject to the terms of the Mozilla 
 * Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at
 *      http://mozilla.org/MPL/2.0/.
 *
 * Author: Wray Buntine (wray.buntine@monash.edu)
 *
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <time.h>

#include "yap.h"
#include "util.h"
#include "stable.h"
#include "lgamma.h"
#include "hca.h"
#include "data.h"
#include "stats.h"
#ifdef H_THREADS
#include <pthread.h>
#endif

uint32_t **classbytopic(char *resstem);

/*
 *   main work for lp_test_ML() done here
 *
 *   NB.  topic assignments z[] left as they are, so if
 *        called again have been warmed up
 */
static void lp_test_ML_one(double *lik, int *totw,
			   int procs, int thisp,
			   /*
			    *   fix==GibbsHold for hold-out testing
			    *   fix==GibbsNone for old max. likelihood testing
			    */
			   enum GibbsType fix)
{
  int i, r;
  float *fact = fvec(ddN.T*4);
  int StartTestDoc=ddN.D-ddN.TEST+thisp, EndTestDoc=ddN.D;
  D_MiSi_t dD;

  *lik=0.0;
  *totw=0;
  /*
   *  must account for other totals over docs
   *  which would be modified by adding the test doc
   *    Nwt[], Nt[] - we don't change these, usd in wordfact()
   *    TDt[t]  = \sum_d Tdt[d][t]
   *    TDT = \sum_t TDt[t]
   *  HENCE we use fix_Td()
   *
   *   assume topic assignments are set up in z[], 
   *   but stats not added elsewhere
   */
  if ( ddP.bdk!=NULL ) misi_init(&ddM,&dD);
  
  /*
   *   now run sampler on all test docs
   */
  for(i=StartTestDoc; i<EndTestDoc; i+=procs) {
    double hmean = -1e30;
    int  thisw =  add_doc(i, fix);
    if ( ddP.hold_all==0 && 
	 (thisw<=1 || (fix==GibbsHold && thisw>=ddD.NdT[i]-1) ) ) {
#ifdef TRACE_WT
      yap_message("remove_doc(d=%d,N=%d,T=%d) before continue\n",
		  i, (int)ddS.Nwt[TR_W][TR_T],(int)ddS.Twt[TR_W][TR_T]);
#endif
      remove_doc(i, fix);
#ifdef TRACE_WT
      yap_message("after remove_doc(d=%d,N=%d,T=%d)\n",
		  i, (int)ddS.Nwt[TR_W][TR_T],(int)ddS.Twt[TR_W][TR_T]);
#endif
    continue;
    }
    if ( ddP.bdk!=NULL ) misi_build(&dD,i,0);
    for (r=0; r<ddP.mltburn; r++) 
      gibbs_lda(fix, ddN.T, i, ddD.NdT[i], fact, &dD, 0, thisp);
    /*
     *   record harmonic mean of last (samples-burnin)
     */
    for (; r<ddP.mltiter; r++) 
      hmean = logadd(hmean,-gibbs_lda(fix, ddN.T, i, ddD.NdT[i], fact, &dD, 0, thisp));
    *lik += log(ddP.mltiter-ddP.mltburn) - hmean;
    *totw += thisw;
    if ( ddP.bdk!=NULL ) misi_unbuild(&dD,i,0);
#ifdef TRACE_WT
    yap_message("remove_doc(d=%d,N=%d,T=%d) end loop\n",
		i, (int)ddS.Nwt[TR_W][TR_T],(int)ddS.Twt[TR_W][TR_T]);
#endif
    remove_doc(i, fix);
#ifdef TRACE_WT
    yap_message("after remove_doc(d=%d,N=%d,T=%d) end loop\n",
		i, (int)ddS.Nwt[TR_W][TR_T],(int)ddS.Twt[TR_W][TR_T]);
#endif
  }
  free(fact);
  if ( ddP.bdk!=NULL ) misi_free(&dD);
}

/*
 *   does two types of likelihood calcs:
 *
 *     GibbsNone:  adds own counts to the populations to
 *                 affect the estimated model ... but only
                   temporarily during sampling
 *     GibbsHold:  splits data into 2 ... first part is like
 *                 GibbsNone, second part uses the topic proportions
 *                 estimated from first and then does a standard
 *                 word-prob. estimate using the topic proportions
 */
double lp_test_ML(int procs,
		  /*
		   *   fix==GibbsHold for hold-out testing
		   *   fix==GibbsNone for old max. likelihood testing
		   */
		  enum GibbsType fix);

#ifdef H_THREADS
typedef struct testml_s {
  double lik;
  int totw;
  int thisp;
  int procs;
  enum GibbsType fix;
} testml_t;
static void *lp_test_ML_p(void *pargs) {
  testml_t *par =(testml_t *) pargs;
  lp_test_ML_one(&par->lik, &par->totw, par->procs, par->thisp, par->fix);
  return NULL;
}
double lp_test_ML(int procs, enum GibbsType fix) {
  double lik = 0;
  int totw = 0;
  pthread_t thread[procs];
  testml_t parg[procs];
  int p;
  
  if ( procs==1 ) {
     for(p = 0 ; p < procs ; p++) {
       parg[p].fix = fix;
       parg[p].procs = procs;
       parg[p].thisp = p;
       if ( pthread_create(&thread[p],NULL,lp_test_ML_p,(void*) &parg[p]) != 0)
         yap_message("thread failed %d\n",p+1 );
     }
     //waiting for threads to finish
     for (p = 0; p < procs; p++){
       pthread_join(thread[p], NULL);
       lik += parg[p].lik;
       totw += parg[p].totw;
     }
  } else {
     lp_test_ML_one(&lik, &totw, 1, 0, fix);
  }
  if ( totw==0 )
    return 0;
  else 
    return lik/totw;
}
#else
double lp_test_ML(int procs, enum GibbsType fix) {
  double lik;
  int totw;
  lp_test_ML_one(&lik, &totw, 1, 0, fix);
  if ( totw==0 )
    return 0;
  else 
    return lik/totw;
}
#endif

/*
 *   Similar logic to above.
 *   We do sampling on the current document but keep the
 *   training parameters etc. fixed.  The current document is
 *   not left in statistics after its done with.
 *   Sampling is done to estimate topic probabilities, which
 *   are then cross correlated with class to predict class.
 */
double lp_test_Pred(char *resstem) {
  int i, r;
  double *tvec = dvec(ddN.T);
  double *cvec = dvec(ddN.C);
  float *fact = fvec(ddN.T*4);
  int StartTestDoc=ddN.D-ddN.TEST, EndTestDoc=ddN.D;
  double accr=0.0;
  double **TbyCprob = dmat(ddN.T,ddN.C);
  uint32_t **confusion = u32mat(ddN.C,ddN.C);
  double **pconfusion = dmat(ddN.C,ddN.C);
  D_MiSi_t dD;

  if ( ddP.bdk!=NULL ) misi_init(&ddM,&dD);
  {
    /*
     *   build class probability vectors for topics
     */
    uint32_t **TbyCcnt = classbytopic(NULL);
    int t,c;
    for (t=0; t<ddN.T; t++) {
      double ntot = 0;
      for (c=0; c<ddN.C; c++)
	ntot += TbyCcnt[t][c];
      for (c=0; c<ddN.C; c++)
	TbyCprob[t][c] = ((double)TbyCcnt[t][c])/ntot;
    }
    free(TbyCcnt[0]);
    free(TbyCcnt);
  }

  /*
   *  must account for other totals over docs
   *  which would be modified by adding the test doc
   *    Nwt[], Nt[] - we don't change these, used in wordfact()
   *    TDt[t]  = \sum_d Tdt[d][t]
   *    TDT = \sum_t TDt[t]
   *  HENCE we use fix_Td()
   *
   *   assume topic assignments are set up in z[], 
   *   but stats not added elsewhere
   */
  
  /*
   *   now run sampler on all test docs
   */
  for(i=StartTestDoc; i<EndTestDoc; i++) {
    int  thisw =  add_doc(i, GibbsNone);
    int  t, c, cmax, tc;
    if ( thisw==0 ) {
      remove_doc(i, GibbsNone);
      continue;
    }
    if ( ddP.bdk!=NULL ) misi_build(&dD,i,0);
    for (t=0; t<ddN.T; t++)  tvec[t] = 0;
    for (c=0; c<ddN.C; c++)  cvec[c] = 0;
    for (r=0; r<ddP.prdburn; r++) 
      gibbs_lda(GibbsNone, ddN.T, i, ddD.NdT[i], fact, &dD, 0, 0);
    /*
     *   record topics of last (samples-burnin)
     */
    for (; r<ddP.prditer; r++) {
      double ptot = 0;
      int Td_;
      gibbs_lda(GibbsNone, ddN.T, i, ddD.NdT[i], fact, &dD, 0, 0);
      /*
       *  do this to predict the topic proportions for this round
       */
      if ( ddP.PYalpha ) 
        Td_ = comp_Td(i); 
      else Td_ = 1;
      for (t=0; t<ddN.T; t++) 
	ptot += fact[t] = topicprob(i, t, Td_);
      /*
       *   and save stats
       */
      for (t=0; t<ddN.T; t++) 
	tvec[t] += fact[t]/ptot;
    }
    /*
     *   compute predictions
     */
    for (t=0; t<ddN.T; t++) {
      double ff = tvec[t]/(ddP.prditer-ddP.prdburn);
      for (c=0; c<ddN.C; c++) 
	cvec[c] += ff * TbyCprob[t][c];
    }
    if ( verbose>2 ) {      
      /*
       *   print details 
       */
       yap_message("Doc %d: ", i);
      for (t=0; t<ddN.T; t++) 
	yap_message(" %lf", tvec[t]/(ddP.prditer-ddP.prdburn));
      yap_message(" :: ");
      for (c=0; c<ddN.C; c++) 
	yap_message(" %lf", cvec[c]);
      yap_message("\n");
    }
    /*
     *   now tabulate errors
     */
    tc = ddD.c[i];
    cmax = 0;
    for (c=0; c<ddN.C; c++) {
      if ( cvec[c]>cvec[cmax] )
	cmax = c;
      pconfusion[tc][c] += cvec[c];
    }
    confusion[tc][cmax]++;
    if ( tc==cmax )
      accr++;
    if ( ddP.bdk!=NULL ) misi_unbuild(&dD,i,0);
    remove_doc(i, GibbsNone);
  }
  free(fact);
  free(tvec);
  if ( ddP.bdk!=NULL ) misi_free(&dD);
  {
    char *fname;
    fname = yap_makename(resstem,".cnfs");
    write_u32sparse(ddN.C,ddN.C,confusion,fname);
    free(fname);
    fname = yap_makename(resstem,".pcnfs");
    write_dmat(fname,ddN.C,ddN.C,pconfusion);
    free(fname);
  }
  free(TbyCprob[0]);  free(TbyCprob);  
  free(confusion[0]);  free(confusion);  
  free(pconfusion[0]);  free(pconfusion);  
  
  return accr/(double)(EndTestDoc-StartTestDoc);
}


