/*
 * Hyperparameters, test parameters, and their control
 * Copyright (C) 2012-2014 Wray Buntine
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
#ifndef __PCTL_H
#define __PCTL_H

#include <stdint.h>
#include "gibbs.h"
#include "dirdim.h"

#define BETA 100
#define APAR 0.0
#define BPAR 10
#define A0PAR 0.0
#define B0PAR 10
#define AWPAR 0.5
#define BWPAR 100
#define AW0PAR 0.5
#define BW0PAR 10
#define ACYCLES 11      //  by default update a's
#define BCYCLES 3      //  by default update b's
#define DIRCYCLES 4     //  by default update alpha/beta
#define STARTCYCLES 1      //  when to start sampling hypers

/*
 *  hyperparameters and test parameters
 */
typedef struct D_pars_s {
  /*
   *  sometimes we fix \phi, \theta or \alpha during testing (or training)
   */
  float **theta;
  float **phi;
  /*
   *    hyperparameters
   */
  double *alphapr;        // vector normalises to 1 when PYalpha!=H_None
  double alphac;          // individual constant, set from alphatot
  double alphatot;        // total of above or alphac*T
  double *betapr;         // vector normalises to 1 when PYbeta!=H_None
  double betac;           // individual constant, set from betatot
  double betatot;         // total of above or betac*W
  char   PYalpha;         // non-zero if using a/b/a0/b0, 1 if PDD, 2 if DP
  double apar, bpar;
  double a0, b0;          // PDD/PDP params for root
  char   PYbeta;          // non-zero if using aw/bw/aw0/bw0, 1=PDD, 2=DP
  double awpar, bwpar;
  double aw0, bw0;        // PDD/PDP params for W root
  double ad;              // PDP params for doc
  double *bdk;            // version with seperate bd for each topic
  /*
   *  test and report controls
   */
  int prditer, prdburn;     //  burnin and iterations for prediction tests
  int lrsiter, lrsburn;     //  burnin and iterations for LRS testing
  int mltiter, mltburn;     //  burnin and iterations for ML testing
  char *cofile;             //  set if want to do PMI-based coherency test
  int spiter, spburn;       //  burnin and iterations for sparsity testing
  int tprobiter, tprobburn;  //  burnin and iters for test docXtopic probs
  int probiter, probburn;   //  burnin and iters for train docXtopic probs
  int phiiter, phiburn;     // burnin and iterations for topicXword probs
  int alphaiter, alphaburn;     // burnin and iterations for topic prior probs
  int progiter, progburn;   //  progress reports
  int queryiter;            //  iterations for query
  int memory;               //  higher value means conserve more memory
  int training;             //  suggested training set size
  char *teststem;           //  stem for the test data, only if different
  /*
   *     window control ... only work on this much data at once
   */
  int window;               //  size
  int window_incr;          //  change by this much each cycle
  int window_cycle;         //  cycle to begin moving
  int window_left;          //  bounds, is treated modulo
  int window_right;
  /*
   *  special control for sampling P.bdk[]
   */
  int kbatch;
  uint16_t **docstats;
  /*
   *  querying, multiple queries stored in single vector
   */
  uint32_t *qword;      /*  dictionary index for word */
  int16_t *query;      /*  map from dictionary index to first qword[] index */
  int16_t *qposn;      /*  index to qword{} where this query starts */
  uint32_t *qid;       /*  query number for word, base is 0 */

  int     n_excludetopic;
  int     *excludetopic;  /*  list of topics to exclude from prob */
  uint32_t *bits_et;      /*  boolean vector version */
  
  int n_query;         /*  count of queries */
  int n_words;         /*  count of words in all queries */
  /*
   *  incrementing topics ... a maximum topic count maintained
   */
  int Tinit;                //   starting number of topics allowed (0=max)
  int Tcycle;               //   cycles when allowed to change
  int Tinc;                 //   allowed increment
  int Tfree;                //   after this many cycles, drop constraints
  /*
   *  hold out method
   *     hold_every>0 then hold out every n-th in doc
   *     hold_dict>0 then hold out every n-th in dictionary
   *     hold_fraction>0 then hold out final part
   */
  double hold_fraction;
  int    hold_dict;
  int    hold_every;
  int    hold_all;
} D_pars_t;

#define PCTL_BURSTY()          (ddP.bdk!=NULL)

/*
 *  hyperparameters control
 */
enum ParType { ParNone=0, ParA, ParB, ParA0, ParB0, 
	       ParAW, ParBW, ParAW0, ParBW0, 
               ParAD, ParBDK, 
	       ParAlpha, ParBeta };
typedef struct D_pctl_s {
  char *name;
  double *ptr;
  char fix;
  int start;
  int offset;
  int cycles;
  void (*sampler)(double *x);
  void (*samplerk)(double *x, int k);
} D_pctl_t;

#define Q_excludetopic(k) (ddP.bits_et[(k)/32U] & (1U << (((unsigned)k)%32U)))

extern D_pars_t ddP;
extern D_pctl_t ddT[];
enum ParType findpar(char *name);
void pctl_init();
void pctl_read(char *resstem, char *buf);
void pctl_fix(int ITER, int loadphi);
void pctl_report();
void pctl_sample(int iter, int procs);
void pctl_update(int iter);
void pctl_print(FILE *fp);
void pctl_samplereport();
int pctl_Tmax(int Tmax, int iter);
void pctl_dims();
int pctl_hold(int i);
int pctl_training(int D);
void pctl_free();

void pctl_fixbeta(char *file, char *resstem);
void pctl_fixalpha(char *file, char *resstem);
double pctl_gammaprior(double x);

#endif
