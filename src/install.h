#ifndef FFR_INSTALL_H
#define FFR_INSTALL_H
#include <stddef.h>

/* Resolve the game directory into out (see install.c): the argument (a game
 * folder, disc image or archive -- installed on first use), FFR_GAME_DIR,
 * <save_dir>/game, or <exe_dir>game, else a file dialog. 0 on success. */
int ffr_find_game(const char *arg, const char *save_dir, const char *exe_dir,
                  char *out, size_t n);

/* 0 when dir/default.xbe is the one this build was recompiled from. */
int ffr_check_xbe(const char *dir);

#endif