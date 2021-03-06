/*
 +-------------------+  Christian Schlittchen <corwin@amber.kn-bremen.de>
 |                   |  Enno Rehling <enno@eressea.de>
 | Eressea PBEM host |  Katja Zedel <katze@felidae.kn-bremen.de>
 | (c) 1998 - 2003   |  Henning Peters <faroul@beyond.kn-bremen.de>
 |                   |  Ingo Wilken <Ingo.Wilken@informatik.uni-oldenburg.de>
 +-------------------+  Stefan Reich <reich@halbling.de>

 This program may not be used, modified or distributed
 without prior permission by the authors of Eressea.
 */

#ifndef H_UTIL_NRMESSAGE
#define H_UTIL_NRMESSAGE

#include <stddef.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

    struct locale;
    struct message;
    struct message_type;
    struct nrmessage_type;

    void free_nrmesssages(void);

    void nrt_register(const struct message_type *mtype);
    const char *nrt_string(const struct message_type *mtype,
            const struct locale *lang);

    size_t nr_render(const struct message *msg, const struct locale *lang,
        char *buffer, size_t size, const void *userdata);

#ifdef __cplusplus
}
#endif
#endif
