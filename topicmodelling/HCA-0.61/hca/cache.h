/*
 * Cache handling
 * Copyright (C) 2009-2014 Wray Buntine
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
#ifndef __CACHE_H
#define __CACHE_H

#include <unistd.h>
#include "lgamma.h" 
#include "util.h" 
#include "stable.h"
#include "pctl.h"
#include "stats.h"
#include "srng.h"

void cache_update(char *par);
void cache_init(int maxM, int maxW) ;
void cache_free();

/*
 *  Cache
 */
typedef struct D_cache_s {
  struct gcache_s lgb, lgba, qda;
  stable_t *SX;
  struct gcache_s lgbw, lgbaw, qdaw;
  stable_t *SY;
  struct gcache_s lgalphac, lgalphatot;  
  struct gcache_s lgbetac, lgbetatot;
  stable_t *SD;  
  /*  used if one/some have a=0 */
  stable_t *S0;
} D_cache_t;

extern D_cache_t ddC;

#endif
