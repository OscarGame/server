/*
Copyright (c) 1998-2010, Enno Rehling <enno@eressea.de>
                         Katja Zedel <katze@felidae.kn-bremen.de
                         Christian Schlittchen <corwin@amber.kn-bremen.de>

Permission to use, copy, modify, and/or distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
**/

#include <platform.h>
#include <kernel/config.h>
#include "race.h"

#include "alchemy.h"
#include "build.h"
#include "building.h"
#include "equipment.h"
#include "faction.h"
#include "group.h"
#include "item.h"
#include "magic.h"
#include "names.h"
#include "pathfinder.h"
#include "region.h"
#include "ship.h"
#include "skill.h"
#include "terrain.h"
#include "unit.h"
#include "version.h"

/* util includes */
#include <util/attrib.h>
#include <util/bsdstring.h>
#include <util/functions.h>
#include <util/language.h>
#include <util/log.h>
#include <util/rng.h>
#include <util/storage.h>

/* attrib includes */
#include <attributes/raceprefix.h>

/* libc includes */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <ctype.h>

/** external variables **/
race * races;
int num_races = 0;

race_list * 
get_familiarraces(void)
{
  static int init = 0;
  static race_list * familiarraces;

  if (!init) {
    race * rc = races;
    for (;rc!=NULL;rc=rc->next) {
      if (rc->init_familiar!=NULL) {
        racelist_insert(&familiarraces, rc);
      }
    }
    init = false;
  }
  return familiarraces;
}

void 
racelist_clear(struct race_list **rl)
{
  while (*rl) {
    race_list * rl2 = (*rl)->next;
    free(*rl);
    *rl = rl2;
  }
}

void 
racelist_insert(struct race_list **rl, const struct race *r)
{
  race_list *rl2 = (race_list*)malloc(sizeof(race_list));

  rl2->data = r;
  rl2->next = *rl;

  *rl = rl2;
}

race *
rc_new(const char * zName)
{
  char zBuffer[80];
  race * rc = calloc(sizeof(race), 1);
  if (strchr(zName, ' ')!=NULL) {
    log_error(("race '%s' has an invalid name. remove spaces\n", zName));
    assert(strchr(zName, ' ')==NULL);
  }
  strcpy(zBuffer, zName);
  rc->_name[0] = strdup(zBuffer);
  sprintf(zBuffer, "%s_p", zName);
  rc->_name[1] = strdup(zBuffer);
  sprintf(zBuffer, "%s_d", zName);
  rc->_name[2] = strdup(zBuffer);
  sprintf(zBuffer, "%s_x", zName);
  rc->_name[3] = strdup(zBuffer);
  rc->precombatspell = NULL;

  rc->attack[0].type = AT_COMBATSPELL;
  rc->attack[1].type = AT_NONE;
  return rc;
}

race *
rc_add(race * rc)
{
  rc->index = num_races++;
  rc->next = races;
  return races = rc;
}

static const char * racealias[][2] = {
  { "uruk", "orc" }, /* there was a time when the orc race was called uruk (and there were other orcs). That was really confusing */
  { "skeletton lord", "skeleton lord" }, /* we once had a typo here. it is fixed */
  { NULL, NULL }
};

race *
rc_find(const char * name)
{
  const char * rname = name;
  race * rc = races;
  int i;

  for (i=0;racealias[i][0];++i) {
    if (strcmp(racealias[i][0], name)==0) {
      rname = racealias[i][1];
      break;
    }
  }
  while (rc && !strcmp(rname, rc->_name[0])==0) rc = rc->next;
  return rc;
}

/** dragon movement **/
boolean
allowed_dragon(const region * src, const region * target)
{
	if (fval(src->terrain, ARCTIC_REGION) && fval(target->terrain, SEA_REGION)) return false;
	return allowed_fly(src, target);
}

char ** race_prefixes = NULL;

extern void 
add_raceprefix(const char * prefix)
{
  static size_t size = 4;
  static unsigned int next = 0;
  if (race_prefixes==NULL) race_prefixes = malloc(size * sizeof(char*));
  if (next+1==size) {
    size *= 2;
    race_prefixes = realloc(race_prefixes, size * sizeof(char*));
  }
  race_prefixes[next++] = strdup(prefix);
  race_prefixes[next] = NULL;
}

/* Die Bezeichnungen d�rfen wegen der Art des Speicherns keine
 * Leerzeichen enthalten! */

/*                      "den Zwergen", "Halblingsparteien" */

void
set_show_item(faction *f, item_t i)
{
	attrib *a = a_add(&f->attribs, a_new(&at_showitem));
	a->data.v = (void*)olditemtype[i];
}

boolean
r_insectstalled(const region * r)
{
  return fval(r->terrain, ARCTIC_REGION);
}

const char *
rc_name(const race * rc, int n)
{
  return rc?mkname("race", rc->_name[n]):NULL;
}

const char *
raceprefix(const unit *u)
{
  const attrib * asource = u->faction->attribs;

  if (fval(u, UFL_GROUP)) {
    const attrib * agroup = agroup = a_findc(u->attribs, &at_group);
    if (agroup!=NULL) asource = ((const group *)(agroup->data.v))->attribs;
  }
  return get_prefix(asource);
}

const char *
racename(const struct locale *loc, const unit *u, const race * rc)
{
  const char * prefix = raceprefix(u);

  if (prefix!=NULL) {
    static char lbuf[80];
    char * bufp = lbuf;
    size_t size = sizeof(lbuf) - 1;
    int ch, bytes;

    bytes = (int)strlcpy(bufp, LOC(loc, mkname("prefix", prefix)), size);
    if (wrptr(&bufp, &size, bytes)!=0) WARN_STATIC_BUFFER();

    bytes = (int)strlcpy(bufp, LOC(loc, rc_name(rc, u->number != 1)), size);
    assert(~bufp[0] & 0x80|| !"unicode/not implemented");
    ch = tolower(*(unsigned char *)bufp);
    bufp[0] = (char)ch;
    if (wrptr(&bufp, &size, bytes)!=0) WARN_STATIC_BUFFER();
    *bufp = 0;

    return lbuf;
  }
  return LOC(loc, rc_name(rc, u->number != 1));
}

int
rc_specialdamage(const race * ar, const race * dr, const struct weapon_type * wtype)
{
  race_t art = old_race(ar);
  int m, modifier = 0;

  if (wtype!=NULL && wtype->modifiers!=NULL) for (m=0;wtype->modifiers[m].value;++m) {
    /* weapon damage for this weapon, possibly by race */
    if (wtype->modifiers[m].flags & WMF_DAMAGE) {
      race_list * rlist = wtype->modifiers[m].races;
      if (rlist!=NULL) {
        while (rlist) {
          if (rlist->data == ar) break;
          rlist = rlist->next;
        }
        if (rlist==NULL) continue;
      }
      modifier += wtype->modifiers[m].value;
    }
  }
  switch (art) {
    case RC_HALFLING:
      if (wtype!=NULL && dragonrace(dr)) {
        modifier += 5;
      }
      break;
    default:
      break;
  }
  return modifier;
}

void
write_race_reference(const race * rc, struct storage * store)
{
  store->w_tok(store, rc?rc->_name[0]:"none");
}

variant
read_race_reference(struct storage * store)
{
  variant result;
  char zName[20];
  store->r_tok_buf(store, zName, sizeof(zName));

  if (strcmp(zName, "none")==0) {
    result.v = NULL;
    return result;
  } else {
    result.v = rc_find(zName);
  }
  assert(result.v!=NULL);
  return result;
}
