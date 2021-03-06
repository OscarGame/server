/* 
+-------------------+
|                   |  Enno Rehling <enno@eressea.de>
| Eressea PBEM host |  Christian Schlittchen <corwin@amber.kn-bremen.de>
| (c) 1998 - 2008   |  Katja Zedel <katze@felidae.kn-bremen.de>
|                   |  Henning Peters <faroul@beyond.kn-bremen.de>
+-------------------+

This program may not be used, modified or distributed
without prior permission by the authors of Eressea.
*/

#ifdef _MSC_VER
#include <platform.h>
#endif

#include "bind_faction.h"
#include "bind_unit.h"
#include "bindings.h"
#include "helpers.h"

#include <kernel/alliance.h>
#include <kernel/faction.h>
#include <kernel/unit.h>
#include <kernel/item.h>
#include <kernel/faction.h>
#include <kernel/messages.h>
#include <kernel/plane.h>
#include <kernel/race.h>
#include <kernel/region.h>
#include <kernel/spellbook.h>
#include <attributes/key.h>

#include <util/base36.h>
#include <util/language.h>
#include <util/log.h>
#include <util/macros.h>
#include <util/password.h>

#include <tolua.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

typedef struct helpmode {
    const char *name;
    int status;
} helpmode;

static helpmode helpmodes[] = {
    { "all", HELP_ALL },
    { "money", HELP_MONEY },
    { "fight", HELP_FIGHT },
    { "observe", HELP_OBSERVE },
    { "give", HELP_GIVE },
    { "guard", HELP_GUARD },
    { "stealth", HELP_FSTEALTH },
    { "travel", HELP_TRAVEL },
    { NULL, 0 }
};

int tolua_factionlist_next(lua_State * L)
{
    faction **faction_ptr = (faction **)lua_touserdata(L, lua_upvalueindex(1));
    faction *f = *faction_ptr;
    if (f != NULL) {
        tolua_pushusertype(L, (void *)f, TOLUA_CAST "faction");
        *faction_ptr = f->next;
        return 1;
    }
    else
        return 0;                   /* no more values to return */
}

static int tolua_faction_get_units(lua_State * L)
{
    faction *self = (faction *)tolua_tousertype(L, 1, 0);
    unit **unit_ptr = (unit **)lua_newuserdata(L, sizeof(unit *));

    luaL_getmetatable(L, TOLUA_CAST "unit");
    lua_setmetatable(L, -2);

    *unit_ptr = self->units;

    lua_pushcclosure(L, tolua_unitlist_nextf, 1);
    return 1;
}

int tolua_faction_add_item(lua_State * L)
{
    faction *self = (faction *)tolua_tousertype(L, 1, 0);
    const char *iname = tolua_tostring(L, 2, 0);
    int number = (int)tolua_tonumber(L, 3, 0);
    int result = -1;

    if (iname != NULL) {
        const resource_type *rtype = rt_find(iname);
        if (rtype && rtype->itype) {
            item *i = i_change(&self->items, rtype->itype, number);
            result = i ? i->number : 0;
        }                           /* if (itype!=NULL) */
    }
    lua_pushinteger(L, result);
    return 1;
}

static int tolua_faction_get_maxheroes(lua_State * L)
{
    faction *self = (faction *)tolua_tousertype(L, 1, 0);
    lua_pushinteger(L, maxheroes(self));
    return 1;
}

static int tolua_faction_get_heroes(lua_State * L)
{
    faction *self = (faction *)tolua_tousertype(L, 1, 0);
    lua_pushinteger(L, countheroes(self));
    return 1;
}

static int tolua_faction_get_score(lua_State * L)
{
    faction *self = (faction *)tolua_tousertype(L, 1, 0);
    lua_pushnumber(L, (lua_Number)self->score);
    return 1;
}

static int tolua_faction_get_id(lua_State * L)
{
    faction *self = (faction *)tolua_tousertype(L, 1, 0);
    lua_pushinteger(L, self->no);
    return 1;
}

static int tolua_faction_set_id(lua_State * L)
{
    faction *self = (faction *)tolua_tousertype(L, 1, 0);
    int id = (int)tolua_tonumber(L, 2, 0);
    if (findfaction(id) == NULL) {
        renumber_faction(self, id);
        lua_pushboolean(L, 1);
    }
    else {
        lua_pushboolean(L, 0);
    }
    return 1;
}

static int tolua_faction_get_magic(lua_State * L)
{
    faction *self = (faction *)tolua_tousertype(L, 1, 0);
    lua_pushstring(L, magic_school[self->magiegebiet]);
    return 1;
}

static int tolua_faction_set_magic(lua_State * L)
{
    faction *self = (faction *)tolua_tousertype(L, 1, 0);
    const char *type = tolua_tostring(L, 2, 0);
    int mtype;

    for (mtype = 0; mtype != MAXMAGIETYP; ++mtype) {
        if (strcmp(magic_school[mtype], type) == 0) {
            self->magiegebiet = (magic_t)mtype;
            break;
        }
    }
    return 0;
}

static int tolua_faction_get_age(lua_State * L)
{
    faction *self = (faction *)tolua_tousertype(L, 1, 0);
    lua_pushinteger(L, self->age);
    return 1;
}

static int tolua_faction_set_age(lua_State * L)
{
    faction *self = (faction *)tolua_tousertype(L, 1, 0);
    int age = (int)tolua_tonumber(L, 2, 0);
    self->age = age;
    return 0;
}

static int tolua_faction_get_flags(lua_State * L)
{
    faction *self = (faction *)tolua_tousertype(L, 1, 0);
    lua_pushinteger(L, self->flags);
    return 1;
}

static int tolua_faction_set_flags(lua_State * L)
{
    faction *self = (faction *)tolua_tousertype(L, 1, 0);
    int flags = (int)tolua_tonumber(L, 2, self->flags);
    self->flags = flags;
    return 1;
}

static int tolua_faction_get_options(lua_State * L)
{
    faction *self = (faction *)tolua_tousertype(L, 1, 0);
    lua_pushinteger(L, self->options);
    return 1;
}

static int tolua_faction_set_options(lua_State * L)
{
    faction *self = (faction *)tolua_tousertype(L, 1, 0);
    int options = (int)tolua_tonumber(L, 2, self->options);
    self->options = options;
    return 1;
}

static int tolua_faction_get_lastturn(lua_State * L)
{
    faction *self = (faction *)tolua_tousertype(L, 1, 0);
    lua_pushinteger(L, self->lastorders);
    return 1;
}

static int tolua_faction_set_lastturn(lua_State * L)
{
    faction *self = (faction *)tolua_tousertype(L, 1, 0);
    if (self) {
        self->lastorders = (int)tolua_tonumber(L, 2, self->lastorders);
    }
    return 0;
}

static int tolua_faction_renumber(lua_State * L)
{
    faction *self = (faction *)tolua_tousertype(L, 1, 0);
    int no = (int)tolua_tonumber(L, 2, 0);

    renumber_faction(self, no);
    return 0;
}

static int tolua_faction_addnotice(lua_State * L)
{
    faction *self = (faction *)tolua_tousertype(L, 1, 0);
    const char *str = tolua_tostring(L, 2, 0);

    addmessage(NULL, self, str, MSG_MESSAGE, ML_IMPORTANT);
    return 0;
}

static int tolua_faction_getkey(lua_State * L)
{
    faction *self = (faction *)tolua_tousertype(L, 1, 0);
    const char *name = tolua_tostring(L, 2, 0);
    int flag = atoi36(name);

    lua_pushinteger(L, key_get(self->attribs, flag));
    return 1;
}

static int tolua_faction_setkey(lua_State * L)
{
    faction *self = (faction *)tolua_tousertype(L, 1, 0);
    const char *name = tolua_tostring(L, 2, 0);
    int value = (int)tolua_tonumber(L, 3, 0);
    int flag = atoi36(name);

    if (value) {
        key_set(&self->attribs, flag, value);
    }
    else {
        key_unset(&self->attribs, flag);
    }
    return 0;
}

static int tolua_faction_get_messages(lua_State * L)
{
    faction *self = (faction *)tolua_tousertype(L, 1, 0);
    int i = 1;
    mlist *ml;
    if (!self->msgs) {
        return 0;
    }
    lua_newtable(L);
    for (ml = self->msgs->begin; ml; ml = ml->next, ++i) {
        lua_pushnumber(L, i);
        lua_pushstring(L, ml->msg->type->name);
        lua_rawset(L, -3);
    }
    return 1;
}

static int tolua_faction_count_msg_type(lua_State *L) {
    faction *self = (faction *)tolua_tousertype(L, 1, 0);
    const char *str = tolua_tostring(L, 2, 0);
    int n = 0;
    if (self->msgs) {
        mlist * ml = self->msgs->begin;
        while (ml) {
            if (strcmp(str, ml->msg->type->name) == 0) {
                ++n;
            }
            ml = ml->next;
        }
    }
    lua_pushinteger(L, n);
    return 1;
}

static int tolua_faction_get_policy(lua_State * L)
{
    faction *self = (faction *)tolua_tousertype(L, 1, 0);
    faction *other = (faction *)tolua_tousertype(L, 2, 0);
    const char *policy = tolua_tostring(L, 3, 0);

    int result = 0, mode;
    for (mode = 0; helpmodes[mode].name != NULL; ++mode) {
        if (strcmp(policy, helpmodes[mode].name) == 0) {
            result = get_alliance(self, other) & mode;
            break;
        }
    }

    lua_pushinteger(L, result);
    return 1;
}

static int tolua_faction_set_policy(lua_State * L)
{
    faction *self = (faction *)tolua_tousertype(L, 1, 0);
    faction *other = (faction *)tolua_tousertype(L, 2, 0);
    const char *policy = tolua_tostring(L, 3, 0);
    int value = tolua_toboolean(L, 4, 0);

    int mode;
    for (mode = 0; helpmodes[mode].name != NULL; ++mode) {
        if (strcmp(policy, helpmodes[mode].name) == 0) {
            if (value) {
                set_alliance(self, other, get_alliance(self,
                    other) | helpmodes[mode].status);
            }
            else {
                set_alliance(self, other, get_alliance(self,
                    other) & ~helpmodes[mode].status);
            }
            break;
        }
    }

    return 0;
}

static int tolua_faction_normalize(lua_State * L)
{
    faction *f = (faction *)tolua_tousertype(L, 1, 0);
    region *r = (region *)tolua_tousertype(L, 2, 0);
    if (r) {
        plane *pl = rplane(r);
        int nx = r->x, ny = r->y;
        pnormalize(&nx, &ny, pl);
        adjust_coordinates(f, &nx, &ny, pl);
        lua_pushinteger(L, nx);
        lua_pushinteger(L, ny);
        return 2;
    }
    return 0;
}

static int tolua_faction_set_origin(lua_State * L)
{
    faction *f = (faction *)tolua_tousertype(L, 1, 0);
    region *r = (region *)tolua_tousertype(L, 2, 0);
    plane *pl = rplane(r);
    int id = pl ? pl->id : 0;

    faction_setorigin(f, id, r->x - plane_center_x(pl), r->y - plane_center_y(pl));
    return 0;
}

static int tolua_faction_get_origin(lua_State * L)
{
    faction *self = (faction *)tolua_tousertype(L, 1, 0);

    ursprung *origin = self->ursprung;
    int x, y;
    while (origin != NULL && origin->id != 0) {
        origin = origin->next;
    }
    if (origin) {
        x = origin->x;
        y = origin->y;
    }
    else {
        x = 0;
        y = 0;
    }

    lua_pushinteger(L, x);
    lua_pushinteger(L, y);
    return 2;
}

static int tolua_faction_destroy(lua_State * L)
{
    faction **fp, *f = (faction *)tolua_tousertype(L, 1, 0);
    /* TODO: this loop is slow af, but what can we do? */
    for (fp = &factions; *fp; fp = &(*fp)->next) {
        if (*fp == f) {
            destroyfaction(fp);
            break;
        }
    }
    return 0;
}

static int tolua_faction_get(lua_State * L)
{
    int no = tolua_toid(L, 1, 0);
    faction *f = findfaction(no);
    tolua_pushusertype(L, f, TOLUA_CAST "faction");
    return 1;
}

static int tolua_faction_create(lua_State * L)
{
    const char *racename = tolua_tostring(L, 1, 0);
    const char *email = tolua_tostring(L, 2, 0);
    const char *lang = tolua_tostring(L, 3, 0);
    struct locale *loc = lang ? get_locale(lang) : default_locale;
    faction *f = NULL;
    const struct race *frace = rc_find(racename ? racename : "human");
    if (frace != NULL) {
        f = addfaction(email, NULL, frace, loc, 0);
    }
    if (!f) {
        log_error("cannot create %s faction for %s, unknown race.", racename, email);
    }
    tolua_pushusertype(L, f, TOLUA_CAST "faction");
    return 1;
}

static int tolua_faction_get_password(lua_State * L)
{
    faction *self = (faction *)tolua_tousertype(L, 1, 0);
    tolua_pushstring(L, self->_password);
    return 1;
}

static int tolua_faction_set_password(lua_State * L)
{
    faction *self = (faction *)tolua_tousertype(L, 1, 0);
    const char * passw = tolua_tostring(L, 2, 0);
    faction_setpassword(self, password_encode(passw, PASSWORD_DEFAULT));
    return 0;
}

static int tolua_faction_get_email(lua_State * L)
{
    faction *self = (faction *)tolua_tousertype(L, 1, 0);
    tolua_pushstring(L, faction_getemail(self));
    return 1;
}

static int tolua_faction_set_email(lua_State * L)
{
    faction *self = (faction *)tolua_tousertype(L, 1, 0);
    faction_setemail(self, tolua_tostring(L, 2, 0));
    return 0;
}

static int tolua_faction_get_locale(lua_State * L)
{
    faction *self = (faction *)tolua_tousertype(L, 1, 0);
    tolua_pushstring(L, locale_name(self->locale));
    return 1;
}

static int tolua_faction_set_locale(lua_State * L)
{
    faction *self = (faction *)tolua_tousertype(L, 1, 0);
    const char *name = tolua_tostring(L, 2, 0);
    const struct locale *loc = get_locale(name);
    if (loc) {
        self->locale = loc;
    }
    else {
        tolua_pushstring(L, "invalid locale");
        return 1;
    }
    return 0;
}

static int tolua_faction_get_race(lua_State * L)
{
    faction *self = (faction *)tolua_tousertype(L, 1, 0);
    tolua_pushstring(L, self->race->_name);
    return 1;
}

static int tolua_faction_set_race(lua_State * L)
{
    faction *self = (faction *)tolua_tousertype(L, 1, 0);
    const char *name = tolua_tostring(L, 2, 0);
    const race *rc = rc_find(name);
    if (rc != NULL) {
        self->race = rc;
    }

    return 0;
}

static int tolua_faction_get_name(lua_State * L)
{
    faction *self = (faction *)tolua_tousertype(L, 1, 0);
    tolua_pushstring(L, faction_getname(self));
    return 1;
}

static int tolua_faction_set_name(lua_State * L)
{
    faction *self = (faction *)tolua_tousertype(L, 1, 0);
    faction_setname(self, tolua_tostring(L, 2, 0));
    return 0;
}

static int tolua_faction_get_uid(lua_State * L)
{
    faction *f = (faction *)tolua_tousertype(L, 1, 0);
    lua_pushinteger(L, f->subscription);
    return 1;
}

static int tolua_faction_set_uid(lua_State * L)
{
    faction *f = (faction *)tolua_tousertype(L, 1, 0);
    f->subscription = (int)tolua_tonumber(L, 2, 0);
    return 0;
}

static int tolua_faction_get_info(lua_State * L)
{
    faction *self = (faction *)tolua_tousertype(L, 1, 0);
    tolua_pushstring(L, faction_getbanner(self));
    return 1;
}

static int tolua_faction_set_info(lua_State * L)
{
    faction *self = (faction *)tolua_tousertype(L, 1, 0);
    faction_setbanner(self, tolua_tostring(L, 2, 0));
    return 0;
}

static int tolua_faction_get_alliance(lua_State * L)
{
    faction *self = (faction *)tolua_tousertype(L, 1, 0);
    tolua_pushusertype(L, f_get_alliance(self), TOLUA_CAST "alliance");
    return 1;
}

static int tolua_faction_set_alliance(lua_State * L)
{
    struct faction *self = (struct faction *)tolua_tousertype(L, 1, 0);
    struct alliance *alli = (struct alliance *) tolua_tousertype(L, 2, 0);

    setalliance(self, alli);

    return 0;
}

static int tolua_faction_get_items(lua_State * L)
{
    faction *self = (faction *)tolua_tousertype(L, 1, 0);
    item **item_ptr = (item **)lua_newuserdata(L, sizeof(item *));

    luaL_getmetatable(L, TOLUA_CAST "item");
    lua_setmetatable(L, -2);

    *item_ptr = self->items;

    lua_pushcclosure(L, tolua_itemlist_next, 1);

    return 1;
}

static int tolua_faction_tostring(lua_State * L)
{
    faction *self = (faction *)tolua_tousertype(L, 1, 0);
    lua_pushstring(L, factionname(self));
    return 1;
}

void tolua_faction_open(lua_State * L)
{
    /* register user types */
    tolua_usertype(L, TOLUA_CAST "faction");
    tolua_usertype(L, TOLUA_CAST "faction_list");

    tolua_module(L, NULL, 0);
    tolua_beginmodule(L, NULL);
    {
        tolua_function(L, TOLUA_CAST "get_faction", tolua_faction_get);
        tolua_beginmodule(L, TOLUA_CAST "eressea");
        tolua_function(L, TOLUA_CAST "faction", tolua_faction_get);
        tolua_endmodule(L);
        tolua_cclass(L, TOLUA_CAST "faction", TOLUA_CAST "faction", TOLUA_CAST "",
            NULL);
        tolua_beginmodule(L, TOLUA_CAST "faction");
        {
            tolua_function(L, TOLUA_CAST "__tostring", tolua_faction_tostring);

            tolua_variable(L, TOLUA_CAST "id", tolua_faction_get_id,
                tolua_faction_set_id);
            tolua_variable(L, TOLUA_CAST "uid", tolua_faction_get_uid,
                tolua_faction_set_uid);
            tolua_variable(L, TOLUA_CAST "name", tolua_faction_get_name,
                tolua_faction_set_name);
            tolua_variable(L, TOLUA_CAST "info", tolua_faction_get_info,
                tolua_faction_set_info);
            tolua_variable(L, TOLUA_CAST "units", tolua_faction_get_units, NULL);
            tolua_variable(L, TOLUA_CAST "heroes", tolua_faction_get_heroes, NULL);
            tolua_variable(L, TOLUA_CAST "maxheroes", tolua_faction_get_maxheroes,
                NULL);
            tolua_variable(L, TOLUA_CAST "password", tolua_faction_get_password,
                tolua_faction_set_password);
            tolua_variable(L, TOLUA_CAST "email", tolua_faction_get_email,
                tolua_faction_set_email);
            tolua_variable(L, TOLUA_CAST "locale", tolua_faction_get_locale,
                tolua_faction_set_locale);
            tolua_variable(L, TOLUA_CAST "race", tolua_faction_get_race,
                tolua_faction_set_race);
            tolua_variable(L, TOLUA_CAST "alliance", tolua_faction_get_alliance,
                tolua_faction_set_alliance);
            tolua_variable(L, TOLUA_CAST "score", tolua_faction_get_score, NULL);
            tolua_variable(L, TOLUA_CAST "magic", tolua_faction_get_magic,
                tolua_faction_set_magic);
            tolua_variable(L, TOLUA_CAST "age", tolua_faction_get_age,
                tolua_faction_set_age);
            tolua_variable(L, TOLUA_CAST "options", tolua_faction_get_options,
                tolua_faction_set_options);
            tolua_variable(L, TOLUA_CAST "flags", tolua_faction_get_flags, tolua_faction_set_flags);
            tolua_variable(L, TOLUA_CAST "lastturn", tolua_faction_get_lastturn,
                tolua_faction_set_lastturn);

            tolua_function(L, TOLUA_CAST "set_policy", tolua_faction_set_policy);
            tolua_function(L, TOLUA_CAST "get_policy", tolua_faction_get_policy);
            tolua_function(L, TOLUA_CAST "get_origin", tolua_faction_get_origin);
            tolua_function(L, TOLUA_CAST "set_origin", tolua_faction_set_origin);
            tolua_function(L, TOLUA_CAST "normalize", tolua_faction_normalize);

            tolua_function(L, TOLUA_CAST "add_item", tolua_faction_add_item);
            tolua_variable(L, TOLUA_CAST "items", tolua_faction_get_items, NULL);

            tolua_function(L, TOLUA_CAST "renumber", tolua_faction_renumber);
            tolua_function(L, TOLUA_CAST "create", tolua_faction_create);
            tolua_function(L, TOLUA_CAST "get", tolua_faction_get);
            tolua_function(L, TOLUA_CAST "destroy", tolua_faction_destroy);
            tolua_function(L, TOLUA_CAST "add_notice", tolua_faction_addnotice);

            /* tech debt hack, siehe https://paper.dropbox.com/doc/Weihnachten-2015-5tOx5r1xsgGDBpb0gILrv#:h=Probleme-mit-Tests-(Nachtrag-0 */
            tolua_function(L, TOLUA_CAST "count_msg_type", tolua_faction_count_msg_type);
            tolua_variable(L, TOLUA_CAST "messages", tolua_faction_get_messages, NULL);

            tolua_function(L, TOLUA_CAST "get_key", tolua_faction_getkey);
            tolua_function(L, TOLUA_CAST "set_key", tolua_faction_setkey);
        }
        tolua_endmodule(L);
    }
    tolua_endmodule(L);
}
