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

#ifndef H_KRNL_SPY
#define H_KRNL_SPY
#ifdef __cplusplus
extern "C" {
#endif

    struct unit;
    struct region;
    struct strlist;
    struct order;
    struct faction;
    struct ship;

    int setstealth_cmd(struct unit *u, struct order *ord);
    int spy_cmd(struct unit *u, struct order *ord);
    int sabotage_cmd(struct unit *u, struct order *ord);
    void spy_message(int spy, const struct unit *u,
        const struct unit *target);
    void set_factionstealth(struct unit * u, struct faction * f);
    void sink_ship(struct ship * sh);

#ifdef __cplusplus
}
#endif
#endif
