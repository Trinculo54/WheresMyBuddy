/*
 * wheresmybuddy.h -- what WheresMyBuddy offers other FH2 mods (MIT, like
 * FH2ML's SDK headers).
 *
 * WheresMyBuddy publishes this through FH2ML (FH2ML_v4.h) from its
 * Initialize. From your v4 mod, on your own thread:
 *
 *     if (S->WaitForMods(30000)) {
 *         const WheresMyBuddyFriendsV1* wmb = (const WheresMyBuddyFriendsV1*)
 *             S->GetInterface(WHERESMYBUDDY_FRIENDS, 1);
 *         if (wmb) { ... }
 *     }
 *
 * (or list "wheresmybuddy" under "dependencies" in your mod.json, and look
 * it up in your Initialize).
 *
 * The names change when WheresMyBuddy refreshes the friends list from Xbox
 * Live, which happens in the background a few seconds after startup, so
 * ask again rather than keeping a copy. Every function is thread-safe.
 * Members are only ever added at the end; check `size`.
 */
#ifndef WHERESMYBUDDY_H
#define WHERESMYBUDDY_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define WHERESMYBUDDY_FRIENDS "WheresMyBuddy.Friends"

typedef struct WheresMyBuddyFriendsV1 {
    uint32_t size; /* sizeof(WheresMyBuddyFriendsV1) as WheresMyBuddy knows it */

    /* How many names are on Drivatars right now (0 if the mod is off). */
    int (*drivatar_count)(void);
    /* Copies the index-th name on the Drivatars (UTF-8, NUL-terminated) to
       out. Returns its length, or -1 if index is out of range. Names longer
       than out_size - 1 are cut short. */
    int (*drivatar_name)(int index, char* out, int out_size);
    /* Copies the gamertag of whoever is signed in to Xbox Live. Returns its
       length, or 0 if nobody is (yet). */
    int (*signed_in_gamertag)(char* out, int out_size);
} WheresMyBuddyFriendsV1;

#ifdef __cplusplus
}
#endif

#endif /* WHERESMYBUDDY_H */
