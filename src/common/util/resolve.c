/* vi: set ts=2:
 *
 *	
 *	Eressea PB(E)M host Copyright (C) 1998-2003
 *      Christian Schlittchen (corwin@amber.kn-bremen.de)
 *      Katja Zedel (katze@felidae.kn-bremen.de)
 *      Henning Peters (faroul@beyond.kn-bremen.de)
 *      Enno Rehling (enno@eressea.de)
 *      Ingo Wilken (Ingo.Wilken@informatik.uni-oldenburg.de)
 *
 *  based on:
 *
 * Atlantis v1.0  13 September 1993 Copyright 1993 by Russell Wallace
 * Atlantis v1.7                    Copyright 1996 by Alex Schr�der
 *
 * This program may not be used, modified or distributed without
 * prior permission by the authors of Eressea.
 * This program may not be sold or used commercially without prior written
 * permission from the authors.
 */

#include <config.h>
#include <assert.h>
#include <stdlib.h>
#include "resolve.h"
#include "variant.h"

typedef struct unresolved {
	void * ptrptr;
		/* address to pass to the resolve-function */
	variant data;
		/* information on how to resolve the missing object */
	resolve_fun resolve;
		/* function to resolve the unknown object */
} unresolved;

#define BLOCKSIZE 1024
static unresolved * ur_list;
static unresolved * ur_begin;
static unresolved * ur_current;

void
ur_add(variant data, void * ptrptr, resolve_fun fun)
{
  if (ur_list==NULL) {
    ur_list = malloc(BLOCKSIZE*sizeof(unresolved));
    ur_begin = ur_current = ur_list;
  }
  else if (ur_current-ur_begin==BLOCKSIZE-1) {
    ur_begin = malloc(BLOCKSIZE*sizeof(unresolved));
    ur_current->data.v = ur_begin;
    ur_current = ur_begin;
  }
  ur_current->data = data;
  ur_current->resolve = fun;
  ur_current->ptrptr = ptrptr;

  ++ur_current;
  ur_current->resolve = NULL;
  ur_current->data.v = NULL;
}

void
resolve(void)
{
  unresolved * ur = ur_list;
  while (ur) {
    if (ur->resolve==NULL) {
      ur = ur->data.v;
      free(ur_list);
      ur_list = ur;
      continue;
    }
    ur->resolve(ur->data, ur->ptrptr);
    ++ur;
  }
  ur_list = ur_begin = ur_current;
}
