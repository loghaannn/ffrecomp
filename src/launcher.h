/*
 * launcher.h -- the settings screen shown before the game, and the settings
 * file behind it ($XDG_CONFIG_HOME/FatalFrame/settings.ini).
 */
#ifndef FFR_LAUNCHER_H
#define FFR_LAUNCHER_H

typedef struct FfrSettings {
    int width, height;   /* window size */
    int fullscreen;
    int scale;           /* render scale 1..4 */
    int vsync;
    int skip_intro;
    int show_launcher;   /* show the settings screen at startup */
} FfrSettings;

void ffr_settings_load(FfrSettings *s);
void ffr_settings_save(const FfrSettings *s);

/* Runs the settings screen in its own window. Returns 1 to start the game,
 * 0 to quit. Saves the settings when the game is started. */
int ffr_launcher_run(FfrSettings *s);

#endif