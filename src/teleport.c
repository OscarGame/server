/*
Copyright (c) 1998-2015, Enno Rehling <enno@eressea.de>
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
#include "teleport.h"

/* kernel includes */
#include <kernel/equipment.h>
#include <kernel/unit.h>
#include <kernel/region.h>
#include <kernel/race.h>
#include <kernel/terrain.h>
#include <kernel/faction.h>
#include <kernel/plane.h>

/* util includes */
#include <util/log.h>
#include <util/rng.h>

#include "skill.h"
#include "monsters.h"

/* libc includes */
#include <assert.h>

#define TE_CENTER 1000
#define TP_RADIUS 2
#define TP_DISTANCE 4

int real2tp(int rk)
{
    /* in C:
     * -4 / 5 = 0;
     * +4 / 5 = 0;
     * !!!!!!!!!!;
     */
    return TE_CENTER + (rk + (TP_DISTANCE * 5000)) / TP_DISTANCE - 5000;
}

static region *tpregion(const region * r)
{
    region *rt =
        findregion(real2tp(r->x), real2tp(r->y));
    if (!is_astral(rt))
        return NULL;
    return rt;
}

region_list *astralregions(const region * r, bool(*valid) (const region *))
{
    region_list *rlist = NULL;
    int x, y;

    assert(is_astral(r));
    if (!is_astral(r)) {
        log_error("astralregions was called with a non-astral region.\n");
        return NULL;
    }
    r = r_astral_to_standard(r);
    if (r == NULL)
        return NULL;

    for (x = -TP_RADIUS; x <= +TP_RADIUS; ++x) {
        for (y = -TP_RADIUS; y <= +TP_RADIUS; ++y) {
            region *rn;
            int dist = koor_distance(0, 0, x, y);
            int nx = r->x + x, ny = r->y + y;

            if (dist > TP_RADIUS)
                continue;
            pnormalize(&nx, &ny, rplane(r));
            rn = findregion(nx, ny);
            if (rn != NULL && (valid == NULL || valid(rn)))
                add_regionlist(&rlist, rn);
        }
    }
    return rlist;
}

region *r_standard_to_astral(const region * r)
{
    assert(!is_astral(r));
    return tpregion(r);
}

region *r_astral_to_standard(const region * r)
{
    int x, y;
    region *r2;

    assert(is_astral(r));
    x = (r->x - TE_CENTER) * TP_DISTANCE;
    y = (r->y - TE_CENTER) * TP_DISTANCE;
    pnormalize(&x, &y, NULL);
    r2 = findregion(x, y);
    if (r2 == NULL || rplane(r2))
        return NULL;

    return r2;
}

region_list *all_in_range(const region * r, int n,
    bool(*valid) (const region *))
{
    int x, y;
    region_list *rlist = NULL;
    plane *pl = rplane(r);

    if (r == NULL)
        return NULL;

    for (x = r->x - n; x <= r->x + n; x++) {
        for (y = r->y - n; y <= r->y + n; y++) {
            if (koor_distance(r->x, r->y, x, y) <= n) {
                region *r2;
                int nx = x, ny = y;
                pnormalize(&nx, &ny, pl);
                r2 = findregion(nx, ny);
                if (r2 != NULL && (valid == NULL || valid(r2)))
                    add_regionlist(&rlist, r2);
            }
        }
    }

    return rlist;
}

#define MAX_BRAIN_SIZE 100

void spawn_braineaters(float chance)
{
    const race * rc_brain = get_race(RC_HIRNTOETER);
    region *r;
    faction *f = get_monsters();
    int next = rng_int() % (int)(chance * 100);

    if (f == NULL || rc_brain == NULL) {
        return;
    }

    for (r = regions; r; r = r->next) {
        unit *u, *ub = NULL;
        if (!is_astral(r) || fval(r->terrain, FORBIDDEN_REGION))
            continue;

        for (u = r->units; u; u = u->next) {
            if (u->_race == rc_brain) {
                if (!ub) {
                    ub = u;
                }
                else {
                    int n = u->number + ub->number;
                    if (n <= MAX_BRAIN_SIZE) {
                        scale_number(ub, n);
                        u->number = 0;
                    }
                    else {
                        ub = u;
                    }
                }
            }
        }

        /* Neues Monster ? */
        if (next-- == 0) {
            u = create_unit(r, f, 1 + rng_int() % 10 + rng_int() % 10,
                rc_brain, 0, NULL, NULL);
            equip_unit(u, "seed_braineater");

            next = rng_int() % (int)(chance * 100);
        }
    }
}

bool is_astral(const region * r)
{
    plane *pl = get_astralplane();
    return (pl && rplane(r) == pl);
}

plane *get_astralplane(void)
{
    plane *astralspace = 0;
    static int config;
    static bool rule_astralplane;
    
    if (config_changed(&config)) {
        rule_astralplane = config_get_int("modules.astralspace", 1) != 0;
    }
    if (!rule_astralplane) {
        return NULL;
    }
    astralspace = getplanebyname("Astralraum");
    if (!astralspace) {
        astralspace = create_new_plane(1, "Astralraum",
            TE_CENTER - 500, TE_CENTER + 500,
            TE_CENTER - 500, TE_CENTER + 500, 0);
    }
    return astralspace;
}

void create_teleport_plane(void)
{
    region *r;
    plane *hplane = get_homeplane();
    plane *aplane = get_astralplane();

    const terrain_type *fog = get_terrain("fog");

    for (r = regions; r; r = r->next) {
        plane *pl = rplane(r);
        if (pl == hplane) {
            region *ra = tpregion(r);

            if (ra == NULL) {
                int x = real2tp(r->x);
                int y = real2tp(r->y);
                pnormalize(&x, &y, aplane);

                ra = new_region(x, y, aplane, 0);
                terraform_region(ra, fog);
            }
        }
    }
}

bool inhabitable(const region * r)
{
    return fval(r->terrain, LAND_REGION);
}
