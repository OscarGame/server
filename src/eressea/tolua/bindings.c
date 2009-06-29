/* vi: set ts=2:
+-------------------+
|                   |  Enno Rehling <enno@eressea.de>
| Eressea PBEM host |  Christian Schlittchen <corwin@amber.kn-bremen.de>
| (c) 1998 - 2008   |  Katja Zedel <katze@felidae.kn-bremen.de>
|                   |  Henning Peters <faroul@beyond.kn-bremen.de>
+-------------------+

This program may not be used, modified or distributed
without prior permission by the authors of Eressea.
*/

#include <config.h>
#include "bindings.h"
#include "bind_unit.h"
#include "bind_faction.h"
#include "bind_region.h"
#include "helpers.h"

#include <kernel/eressea.h>

#include <kernel/alliance.h>
#include <kernel/skill.h>
#include <kernel/equipment.h>
#include <kernel/calendar.h>
#include <kernel/unit.h>
#include <kernel/terrain.h>
#include <kernel/message.h>
#include <kernel/region.h>
#include <kernel/reports.h>
#include <kernel/building.h>
#include <kernel/plane.h>
#include <kernel/race.h>
#include <kernel/item.h>
#include <kernel/order.h>
#include <kernel/ship.h>
#include <kernel/teleport.h>
#include <kernel/faction.h>
#include <kernel/save.h>

#include <gamecode/creport.h>
#include <gamecode/summary.h>
#include <gamecode/laws.h>
#include <gamecode/monster.h>

#include <spells/spells.h>
#include <modules/autoseed.h>
#include <modules/score.h>
#include <attributes/key.h>

#include <util/attrib.h>
#include <util/base36.h>
#include <util/language.h>
#include <util/lists.h>
#include <util/log.h>
#include <util/rand.h>
#include <util/rng.h>
#include <util/storage.h>

#include <tolua.h>
#include <lua.h>

#include <time.h>
#include <assert.h>

int tolua_orderlist_next(lua_State *L)
{
  order** order_ptr = (order **)lua_touserdata(L, lua_upvalueindex(1));
  order* ord = *order_ptr;
  if (ord != NULL) {
    char cmd[8192];
    write_order(ord, cmd, sizeof(cmd));
    tolua_pushstring(L, cmd);
    *order_ptr = ord->next;
    return 1;
  }
  else return 0;  /* no more values to return */
}

int tolua_spelllist_next(lua_State *L)
{
  spell_list** spell_ptr = (spell_list **)lua_touserdata(L, lua_upvalueindex(1));
  spell_list* slist = *spell_ptr;
  if (slist != NULL) {
    tolua_pushusertype(L, slist->data, "spell");
    *spell_ptr = slist->next;
    return 1;
  }
  else return 0;  /* no more values to return */
}

int tolua_itemlist_next(lua_State *L)
{
  item** item_ptr = (item **)lua_touserdata(L, lua_upvalueindex(1));
  item* itm = *item_ptr;
  if (itm != NULL) {
    tolua_pushstring(L, itm->type->rtype->_name[0]);
    *item_ptr = itm->next;
    return 1;
  }
  else return 0;  /* no more values to return */
}

static int 
tolua_autoseed(lua_State * L)
{
  const char * filename = tolua_tostring(L, 1, 0);
  int new_island = tolua_toboolean(L, 2, 0);
  newfaction * players = read_newfactions(filename);

  if (players!=NULL) {
    while (players) {
      int n = listlen(players);
      int k = (n+ISLANDSIZE-1)/ISLANDSIZE;
      k = n / k;
      n = autoseed(&players, k, new_island?0:TURNS_PER_ISLAND);
      if (n==0) {
        break;
      }
    }
  }
  return 0;
}


static int
tolua_getkey(lua_State* L)
{
  const char * name = tolua_tostring(L, 1, 0);

  int flag = atoi36(name);
  attrib * a = find_key(global.attribs, flag);
  lua_pushboolean(L, a!=NULL);

  return 1;
}

static int
tolua_setkey(lua_State* L)
{
  const char * name = tolua_tostring(L, 1, 0);
  int value = tolua_toboolean(L, 2, 0);

  int flag = atoi36(name);
  attrib * a = find_key(global.attribs, flag);
  if (a==NULL && value) {
    add_key(&global.attribs, flag);
  } else if (a!=NULL && !value) {
    a_remove(&global.attribs, a);
  }

  return 0;
}

static int
tolua_rng_int(lua_State* L)
{
  lua_pushnumber(L, (lua_Number)rng_int());
  return 1;
}

static int
tolua_read_orders(lua_State* L)
{
  const char * filename = tolua_tostring(L, 1, 0);
  int result = readorders(filename);
  lua_pushnumber(L, (lua_Number)result);
  return 1;
}

static int
tolua_message_unit(lua_State* L)
{
  unit * sender = (unit *)tolua_tousertype(L, 1, 0);
  unit * target = (unit *)tolua_tousertype(L, 2, 0);
  const char * str = tolua_tostring(L, 3, 0);
  if (!target) tolua_error(L, "target is nil", NULL);
  if (!sender) tolua_error(L, "sender is nil", NULL);
  deliverMail(target->faction, sender->region, sender, str, target);
  return 0;
}

static int
tolua_message_faction(lua_State * L)
{
  unit * sender = (unit *)tolua_tousertype(L, 1, 0);
  faction * target = (faction *)tolua_tousertype(L, 2, 0);
  const char * str = tolua_tostring(L, 3, 0);
  if (!target) tolua_error(L, "target is nil", NULL);
  if (!sender) tolua_error(L, "sender is nil", NULL);

  deliverMail(target, sender->region, sender, str, NULL);
  return 0;
}

static int
tolua_message_region(lua_State * L)
{
  unit * sender = (unit *)tolua_tousertype(L, 1, 0);
  const char * str = tolua_tostring(L, 2, 0);

  if (!sender) tolua_error(L, "sender is nil", NULL);
  ADDMSG(&sender->region->msgs, msg_message("mail_result", "unit message", sender, str));

  return 0;
}

static void
free_script(attrib * a)
{
  lua_State * L = (lua_State *)global.vm_state;
  if (a->data.i>0) {
    luaL_unref(L, LUA_REGISTRYINDEX, a->data.i);
  }
}

attrib_type at_script = {
  "script",
  NULL, free_script, NULL,
  NULL, NULL, ATF_UNIQUE
};

static int
call_script(lua_State * L, struct unit * u)
{
  const attrib * a = a_findc(u->attribs, &at_script);
  if (a==NULL) a = a_findc(u->race->attribs, &at_script);
  if (a!=NULL && a->data.i>0) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, a->data.i);
    if (lua_pcall(L, 1, 0, 0)!=0) {
      const char* error = lua_tostring(L, -1);
      log_error(("call_script (%s): %s", unitname(u), error));
      lua_pop(L, 1);
    }
  }
  return -1;
}

static void
setscript(lua_State * L, struct attrib ** ap)
{
  attrib * a = a_find(*ap, &at_script);
  if (a == NULL) {
    a = a_add(ap, a_new(&at_script));
  } else if (a->data.i>0) {
    luaL_unref(L, LUA_REGISTRYINDEX, a->data.i);
  }
  a->data.i = luaL_ref(L, LUA_REGISTRYINDEX);
}

static int
tolua_update_guards(lua_State * L)
{
  update_guards();
  return 0;
}

static int
tolua_get_turn(lua_State * L)
{
  tolua_pushnumber(L, (lua_Number)turn);
  return 1;
}

static int
tolua_atoi36(lua_State * L)
{
  const char * s = tolua_tostring(L, 1, 0);
  tolua_pushnumber(L, (lua_Number)atoi36(s));
  return 1;
}

static int
tolua_itoa36(lua_State * L)
{
  int i = (int)tolua_tonumber(L, 1, 0);
  tolua_pushstring(L, itoa36(i));
  return 1;
}

static int
tolua_dice_rand(lua_State * L)
{
  const char * s = tolua_tostring(L, 1, 0);
  tolua_pushnumber(L, dice_rand(s));
  return 1;
}

static int
tolua_addequipment(lua_State * L)
{
  const char * eqname = tolua_tostring(L, 1, 0);
  const char * iname = tolua_tostring(L, 2, 0);
  const char * value = tolua_tostring(L, 3, 0);
  int result = -1;
  if (iname!=NULL) {
    const struct item_type * itype = it_find(iname);
    if (itype!=NULL) { 
      equipment_setitem(create_equipment(eqname), itype, value);
      result = 0;
    }
  }
  lua_pushnumber(L, (lua_Number)result);
  return 1;
}

static int
tolua_get_season(lua_State * L)
{
  int turnno = (int)tolua_tonumber(L, 1, 0);
  gamedate gd;
  get_gamedate(turnno, &gd);
  tolua_pushstring(L, seasonnames[gd.season]);
  return 1;
}

static int
tolua_learn_skill(lua_State * L)
{
  unit * u = (unit *)tolua_tousertype(L, 1, 0);
  const char * skname = tolua_tostring(L, 2, 0);
  float chances = (float)tolua_tonumber(L, 3, 0);
  skill_t sk = sk_find(skname);
  if (sk!=NOSKILL) {
    learn_skill(u, sk, chances);
  }
  return 0;
}

static int
tolua_update_scores(lua_State * L)
{
  score();
  return 0;
}

static int
tolua_update_owners(lua_State * L)
{
  region * r;
  for (r=regions;r;r=r->next) {
    update_owners(r);
  }
  return 0;
}

static int
tolua_update_subscriptions(lua_State * L)
{
  update_subscriptions();
  return 0;
}

static int 
tolua_remove_empty_units(lua_State * L)
{
  remove_empty_units();
  return 0;
}

static int
tolua_get_nmrs(lua_State * L)
{
  int result = -1;
  int n = (int)tolua_tonumber(L, 1, 0);
  if (n<=NMRTimeout()) {
    if (nmrs==NULL) {
      update_nmrs();
      result = nmrs[n];
    }
  }
  tolua_pushnumber(L, (lua_Number)result);
  return 1;
}

static int
tolua_equipunit(lua_State * L)
{
  unit * u = (unit *)tolua_tousertype(L, 1, 0);
  const char * eqname = tolua_tostring(L, 2, 0);

  equip_unit(u, get_equipment(eqname));

  return 0;
}

static int
tolua_equipment_setitem(lua_State * L)
{
  int result = -1;
  const char * eqname = tolua_tostring(L, 1, 0);
  const char * iname = tolua_tostring(L, 2, 0);
  const char * value = tolua_tostring(L, 3, 0);
  if (iname!=NULL) {
    const struct item_type * itype = it_find(iname);
    if (itype!=NULL) {
      equipment_setitem(create_equipment(eqname), itype, value);
      result = 0;
    }
  }
  tolua_pushnumber(L, (lua_Number)result);
  return 1;
}

static int
tolua_levitate_ship(lua_State * L)
{
  ship * sh = (ship *)tolua_tousertype(L, 1, 0);
  unit * mage = (unit *)tolua_tousertype(L, 2, 0);
  double power = (double)tolua_tonumber(L, 3, 0);
  int duration = (int)tolua_tonumber(L, 4, 0);
  int cno = levitate_ship(sh, mage, power, duration);
  tolua_pushnumber(L, (lua_Number)cno);
  return 1;
}

static int
tolua_set_unitscript(lua_State * L)
{
  struct unit * u = (struct unit *)tolua_tousertype(L, 1, 0);
  if (u) {
    lua_pushvalue(L, 2);
    setscript(L, &u->attribs);
    lua_pop(L, 1);
  }
  return 0;
}

static int
tolua_set_racescript(lua_State * L)
{
  const char * rcname = tolua_tostring(L, 1, 0);
  race * rc = rc_find(rcname);

  if (rc!=NULL) {
    lua_pushvalue(L, 2);
    setscript(L, &rc->attribs);
    lua_pop(L, 1);
  }
  return 0;
}

static int
tolua_spawn_dragons(lua_State * L)
{
  spawn_dragons();
  return 0;
}

static int
tolua_spawn_undead(lua_State * L)
{
  spawn_undead();
  return 0;
}

static int
tolua_spawn_braineaters(lua_State * L)
{
  float chance = (float)tolua_tonumber(L, 1, 0);
  spawn_braineaters(chance);
  return 0;
}

static int
tolua_planmonsters(lua_State * L)
{
  faction * f = get_monsters();

  if (f!=NULL) {
    unit * u;
    plan_monsters();
    for (u=f->units;u;u=u->nextF) {
      call_script(L, u);
    }
  }

  return 0;
}

static int
tolua_init_reports(lua_State* L)
{
  int result = init_reports();
  tolua_pushnumber(L, (lua_Number)result);
  return 1;
}

static int
tolua_write_report(lua_State* L)
{
  faction * f = (faction * )tolua_tousertype(L, 1, 0);
  time_t ltime = time(0);
  int result = write_reports(f, ltime);

  tolua_pushnumber(L, (lua_Number)result);
  return 1;
}

static int
tolua_write_reports(lua_State* L)
{
  int result;

  init_reports();
  result = reports();
  tolua_pushnumber(L, (lua_Number)result);
  return 1;
}

static int
tolua_process_orders(lua_State* L)
{
  ++turn;
  processorders();
  return 0;
}
static int
tolua_write_passwords(lua_State* L)
{
  int result = writepasswd();
  lua_pushnumber(L, (lua_Number)result);
  return 0;
}

static struct summary * sum_begin = 0;

static int
tolua_init_summary(lua_State* L)
{
  sum_begin = make_summary();
  return 0;
}

static int
tolua_write_summary(lua_State* L)
{
  if (sum_begin) {
    struct summary * sum_end = make_summary();
    report_summary(sum_end, sum_begin, false);
    report_summary(sum_end, sum_begin, true);
    return 0;
  }
  return 0;
}

static int
tolua_free_game(lua_State* L)
{
  free_gamedata();
  return 0;
}

static int
tolua_write_map(lua_State* L)
{
  const char * filename = tolua_tostring(L, 1, 0);
  if (filename) {
    crwritemap(filename);
  }
  return 0;
}

static int
tolua_write_game(lua_State* L)
{
  const char * filename = tolua_tostring(L, 1, 0);
  const char * mode =  tolua_tostring(L, 2, 0);

  int result, m = IO_BINARY;
  if (mode && strcmp(mode, "text")==0) m = IO_TEXT;
  remove_empty_factions(true);
  result = writegame(filename, m);

  tolua_pushnumber(L, (lua_Number)result);
  return 1;
}

static int
tolua_read_game(lua_State* L)
{
  const char * filename = tolua_tostring(L, 1, 0);
  const char * mode =  tolua_tostring(L, 2, 0);

  int rv, m = IO_BINARY;
  if (mode && strcmp(mode, "text")==0) m = IO_TEXT;
  rv = readgame(filename, m, false);

  tolua_pushnumber(L, (lua_Number)rv);
  return 1;
}

static int
tolua_get_faction(lua_State* L)
{
  int no = tolua_toid(L, 1, 0);
  faction * f = findfaction(no);

  tolua_pushusertype(L, f, "faction");
  return 1;
}

static int
tolua_get_region(lua_State* L)
{
  int x = (int)tolua_tonumber(L, 1, 0);
  int y = (int)tolua_tonumber(L, 2, 0);
  struct plane * pl = findplane(x, y);
  region * r;
  assert(!pnormalize(&x, &y, pl));
  r = findregion(x, y);

  tolua_pushusertype(L, r, "region");
  return 1;
}

static int
tolua_get_region_byid(lua_State* L)
{
  int uid = (int)tolua_tonumber(L, 1, 0);
  region * r = findregionbyid(uid);

  tolua_pushusertype(L, r, "region");
  return 1;
}

static int
tolua_get_building(lua_State* L)
{
  int no = tolua_toid(L, 1, 0);
  building * b = findbuilding(no);

  tolua_pushusertype(L, b, "building");
  return 1;
}

static int
tolua_get_ship(lua_State* L)
{
  int no = tolua_toid(L, 1, 0);
  ship * sh = findship(no);

  tolua_pushusertype(L, sh, "ship");
  return 1;
}

static int
tolua_get_alliance(lua_State* L)
{
  int no = tolua_toid(L, 1, 0);
  alliance * f = findalliance(no);

  tolua_pushusertype(L, f, "alliance");
  return 1;
}

static int
tolua_get_unit(lua_State* L)
{
  int no = tolua_toid(L, 1, 0);
  unit * u = findunit(no);

  tolua_pushusertype(L, u, "unit");
  return 1;
}

static int
tolua_alliance_create(lua_State* L)
{
  int id = (int)tolua_tonumber(L, 1, 0);
  const char * name = tolua_tostring(L, 2, 0);

  alliance * alli = makealliance(id, name);

  tolua_pushusertype(L, alli, "alliance");
  return 1;
}

static int
tolua_get_regions(lua_State* L)
{
  region ** region_ptr = (region**)lua_newuserdata(L, sizeof(region *));

  luaL_getmetatable(L, "region");
  lua_setmetatable(L, -2);

  *region_ptr = regions;

  lua_pushcclosure(L, tolua_regionlist_next, 1);
  return 1;
}

static int
tolua_get_factions(lua_State* L)
{
  faction ** faction_ptr = (faction**)lua_newuserdata(L, sizeof(faction *));

  luaL_getmetatable(L, "faction");
  lua_setmetatable(L, -2);

  *faction_ptr = factions;

  lua_pushcclosure(L, tolua_factionlist_next, 1);
  return 1;
}

static int
tolua_get_alliance_factions(lua_State* L)
{
  alliance * self = (alliance *)tolua_tousertype(L, 1, 0);
  faction_list ** faction_ptr = (faction_list**)lua_newuserdata(L, sizeof(faction_list *));

  luaL_getmetatable(L, "faction_list");
  lua_setmetatable(L, -2);

  *faction_ptr = self->members;

  lua_pushcclosure(L, tolua_factionlist_iter, 1);
  return 1;
}

static int tolua_get_alliance_id(lua_State* L)
{
  alliance* self = (alliance*) tolua_tousertype(L, 1, 0);
  tolua_pushnumber(L, (lua_Number)self->id);
  return 1;
}

static int tolua_get_alliance_name(lua_State* L)
{
  alliance* self = (alliance*) tolua_tousertype(L, 1, 0);
  tolua_pushstring(L, self->name);
  return 1;
}

static int tolua_set_alliance_name(lua_State* L)
{
  alliance* self = (alliance*)tolua_tousertype(L, 1, 0);
  alliance_setname(self, tolua_tostring(L, 2, 0));
  return 0;
}

#include <libxml/tree.h>
#include <util/functions.h>
#include <util/xml.h>
#include <kernel/spell.h>

static int
tolua_write_spells(lua_State* L)
{
  spell_f fun = (spell_f)get_function("lua_castspell");
  const char * filename = "magic.xml";
  xmlDocPtr doc = xmlNewDoc(BAD_CAST "1.0");
  xmlNodePtr root = xmlNewNode(NULL, BAD_CAST "spells");
  spell_list * splist;

  for (splist=spells; splist; splist=splist->next) {
    spell * sp = splist->data;
    if (sp->sp_function!=fun) {
      xmlNodePtr node = xmlNewNode(NULL, BAD_CAST "spell");
      xmlNewProp(node, BAD_CAST "name", BAD_CAST sp->sname);
      xmlNewProp(node, BAD_CAST "type", BAD_CAST magic_school[sp->magietyp]);
      xmlNewProp(node, BAD_CAST "rank", xml_i(sp->rank));
      xmlNewProp(node, BAD_CAST "level", xml_i(sp->level));
      xmlNewProp(node, BAD_CAST "index", xml_i(sp->id));
      if (sp->syntax) xmlNewProp(node, BAD_CAST "syntax", BAD_CAST sp->syntax);
      if (sp->parameter) xmlNewProp(node, BAD_CAST "parameters", BAD_CAST sp->parameter);
      if (sp->components) {
        spell_component * comp = sp->components;
        for (;comp->type!=0;++comp) {
          static const char * costs[] = { "fixed", "level", "linear" };
          xmlNodePtr cnode = xmlNewNode(NULL, BAD_CAST "resource");
          xmlNewProp(cnode, BAD_CAST "name", BAD_CAST comp->type->_name[0]);
          xmlNewProp(cnode, BAD_CAST "amount", xml_i(comp->amount));
          xmlNewProp(cnode, BAD_CAST "cost", BAD_CAST costs[comp->cost]);
          xmlAddChild(node, cnode); 
        }
      }

      if (sp->sptyp & TESTCANSEE) {
        xmlNewProp(node, BAD_CAST "los", BAD_CAST "true");
      }
      if (sp->sptyp & ONSHIPCAST) {
        xmlNewProp(node, BAD_CAST "ship", BAD_CAST "true");
      }
      if (sp->sptyp & OCEANCASTABLE) {
        xmlNewProp(node, BAD_CAST "ocean", BAD_CAST "true");
      }
      if (sp->sptyp & FARCASTING) {
        xmlNewProp(node, BAD_CAST "far", BAD_CAST "true");
      }
      if (sp->sptyp & SPELLLEVEL) {
        xmlNewProp(node, BAD_CAST "variable", BAD_CAST "true");
      }
      xmlAddChild(root, node); 
    }
  }
  xmlDocSetRootElement(doc, root);
  xmlKeepBlanksDefault(0);
  xmlSaveFormatFileEnc(filename, doc, "utf-8", 1);
  xmlFreeDoc(doc);
  return 0;
}

static int
tolua_get_spell_text(lua_State *L)
{
  const struct locale * loc = default_locale;
  spell * self = (spell *)tolua_tousertype(L, 1, 0);
  lua_pushstring(L, spell_info(self, loc));
  return 1;
}

static int
tolua_get_spell_school(lua_State *L)
{
  spell * self = (spell *)tolua_tousertype(L, 1, 0);
  lua_pushstring(L, magic_school[self->magietyp]);
  return 1;
}

static int
tolua_get_spell_level(lua_State *L)
{
  spell * self = (spell *)tolua_tousertype(L, 1, 0);
  lua_pushnumber(L, self->level);
  return 1;
}

static int
tolua_get_spell_name(lua_State *L)
{
  const struct locale * lang = default_locale;
  spell * self = (spell *)tolua_tousertype(L, 1, 0);
  lua_pushstring(L, spell_name(self, lang));
  return 1;
}

static int tolua_get_spells(lua_State* L)
{
  spell_list * slist = spells;
  if (slist) {
    spell_list ** spell_ptr = (spell_list **)lua_newuserdata(L, sizeof(spell_list *));
    luaL_getmetatable(L, "spell_list");
    lua_setmetatable(L, -2);

    *spell_ptr = slist;
    lua_pushcclosure(L, tolua_spelllist_next, 1);
    return 1;
  }

  lua_pushnil(L);
  return 1;
}
int
tolua_eressea_open(lua_State* L)
{
  tolua_open(L);

  /* register user types */
  tolua_usertype(L, "spell");
  tolua_usertype(L, "spell_list");
  tolua_usertype(L, "order");
  tolua_usertype(L, "item");
  tolua_usertype(L, "alliance");
  tolua_usertype(L, "event");

  tolua_module(L, NULL, 0);
  tolua_beginmodule(L, NULL);
  {
    tolua_cclass(L, "alliance", "alliance", "", NULL);
    tolua_beginmodule(L, "alliance");
    {
      tolua_variable(L, "name", tolua_get_alliance_name, tolua_set_alliance_name);
      tolua_variable(L, "id", tolua_get_alliance_id, NULL);
      tolua_variable(L, "factions", &tolua_get_alliance_factions, NULL);
      tolua_function(L, "create", tolua_alliance_create);
    }
    tolua_endmodule(L);

    tolua_cclass(L, "spell", "spell", "", NULL);
    tolua_beginmodule(L, "spell");
    {
      tolua_function(L, "__tostring", tolua_get_spell_name);
      tolua_variable(L, "name", tolua_get_spell_name, 0);
      tolua_variable(L, "school", tolua_get_spell_school, 0);
      tolua_variable(L, "level", tolua_get_spell_level, 0);
      tolua_variable(L, "text", tolua_get_spell_text, 0);
    }
    tolua_endmodule(L);

    tolua_function(L, "get_region_by_id", tolua_get_region_byid);

    tolua_function(L, "get_faction", tolua_get_faction);
    tolua_function(L, "get_unit", tolua_get_unit);
    tolua_function(L, "get_alliance", tolua_get_alliance);
    tolua_function(L, "get_ship", tolua_get_ship),
    tolua_function(L, "get_building", tolua_get_building),
    tolua_function(L, "get_region", tolua_get_region),

    // deprecated_function(L, "add_faction");
    // deprecated_function(L, "faction_origin");
    tolua_function(L, "factions", tolua_get_factions);
    tolua_function(L, "regions", tolua_get_regions);

    tolua_function(L, "read_game", tolua_read_game);
    tolua_function(L, "write_game", tolua_write_game);
    tolua_function(L, "free_game", tolua_free_game);
    tolua_function(L, "write_map", &tolua_write_map);

    tolua_function(L, "read_orders", tolua_read_orders);
    tolua_function(L, "process_orders", tolua_process_orders);

    tolua_function(L, "init_reports", tolua_init_reports);
    tolua_function(L, "write_reports", tolua_write_reports);
    tolua_function(L, "write_report", tolua_write_report);

    tolua_function(L, "init_summary", tolua_init_summary);
    tolua_function(L, "write_summary", tolua_write_summary);
    tolua_function(L, "write_passwords", tolua_write_passwords),

    tolua_function(L, "message_unit", tolua_message_unit);
    tolua_function(L, "message_faction", tolua_message_faction);
    tolua_function(L, "message_region", tolua_message_region);

    /* scripted monsters */
    tolua_function(L, "plan_monsters", tolua_planmonsters);
    tolua_function(L, "spawn_braineaters", tolua_spawn_braineaters);
    tolua_function(L, "spawn_undead", tolua_spawn_undead);
    tolua_function(L, "spawn_dragons", tolua_spawn_dragons);

    tolua_function(L, "set_race_brain", tolua_set_racescript);
    tolua_function(L, "set_unit_brain", tolua_set_unitscript);

    /* spells and stuff */
    tolua_function(L, "levitate_ship", tolua_levitate_ship);

    tolua_function(L, "update_guards", tolua_update_guards);

    tolua_function(L, "get_turn", tolua_get_turn);
    tolua_function(L, "get_season", tolua_get_season);

    tolua_function(L, "equipment_setitem", tolua_equipment_setitem);
    tolua_function(L, "equip_unit", tolua_equipunit);
    tolua_function(L, "add_equipment", tolua_addequipment);

    tolua_function(L, "atoi36", tolua_atoi36);
    tolua_function(L, "itoa36", tolua_itoa36);
    tolua_function(L, "dice_roll", tolua_dice_rand);

    tolua_function(L, "get_nmrs", tolua_get_nmrs);
    tolua_function(L, "remove_empty_units", tolua_remove_empty_units);

    tolua_function(L, "update_subscriptions", tolua_update_subscriptions);
    tolua_function(L, "update_scores", tolua_update_scores);
    tolua_function(L, "update_owners", tolua_update_owners);

    tolua_function(L, "learn_skill", tolua_learn_skill);

    tolua_function(L, "autoseed", tolua_autoseed);

    tolua_function(L, "get_key", tolua_getkey);
    tolua_function(L, "set_key", tolua_setkey);

    tolua_function(L, "rng_int", tolua_rng_int);

    tolua_function(L, "spells", tolua_get_spells);
    tolua_function(L, "write_spells", tolua_write_spells);
  }
  tolua_endmodule(L);
  return 1;
}
