/* First-run launcher window for the Linux AppImage.
 *
 * AppRun starts this only while the game still has to be built. It lets the
 * user pick their ROM, checks it, makes sure the build tools are installed,
 * runs "AppRun --build-only" with a progress display and exits 0 once the
 * game is ready, so AppRun can start it. Any other exit means "do not play".
 *
 * Once the game is built the window shows a Play screen, also on every later
 * start, with the Window/Full screen switch and the mouse-aim checkbox; AppRun
 * reads the saved choices when it starts the game. Optional background music
 * (an MP3 shipped next to the launcher) loops until the build completes or
 * Play is pressed.
 *
 * The window contains no game data; the ROM is only read to verify it.
 */
#include <gtk/gtk.h>
#define SDL_MAIN_HANDLED
#include <SDL.h>
#define MINIMP3_IMPLEMENTATION
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wunused-function"
#include "third_party/minimp3/minimp3_ex.h"
#pragma GCC diagnostic pop
#include <dirent.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#define ROM_SIZE 33554432
#define WINDOW_WIDTH 960
#define WINDOW_HEIGHT 620
#define LOG_LINE_LIMIT 4000
#define GENERATE_SUBSTEPS 23
#define MUSIC_VOLUME 0.6

typedef struct {
  const char *id;
  const char *label;
  double weight; /* Typical seconds; only used to pace the progress bar. */
} Step;

static const Step kSteps[] = {
  {"unpack-offline", "Unpacking the bundled dependencies", 40},
  {"download-decomp", "Downloading the Jet Force Gemini decompilation (under 1 MB)", 15},
  {"download-ido", "Downloading the IDO compiler (4.8 MB)", 15},
  {"clone-jfg-decomp", "Downloading the Jet Force Gemini decompilation", 120},
  {"pin-jfg-decomp", "Checking out the decompilation", 1},
  {"clone-n64recomp", "Downloading N64Recomp", 5},
  {"pin-n64recomp", "Checking out N64Recomp", 1},
  {"clone-n64modernruntime", "Downloading N64ModernRuntime", 130},
  {"pin-n64modernruntime", "Checking out N64ModernRuntime", 1},
  {"clone-rt64", "Downloading the RT64 renderer", 115},
  {"pin-rt64", "Checking out RT64", 1},
  {"submodules-jfg-decomp", "Downloading decompilation tools", 375},
  {"submodules-n64recomp", "Downloading recompiler components", 604},
  {"submodules-n64modernruntime", "Downloading runtime components", 1111},
  {"submodules-rt64", "Downloading renderer components", 2215},
  {"generate", "Translating the game code from your ROM", 902},
  {"configure", "Preparing the build", 5},
  {"compile", "Compiling the game", 600},
};
#define STEP_COUNT ((int)(sizeof kSteps / sizeof kSteps[0]))

typedef struct {
  /* Options. */
  char *apprun;
  char *rom_store;
  char *rom_sha1;
  char *background_path;
  char *icon_path;
  char *build_log;
  char *settings_path;
  char *music_path;
  gboolean offline;
  gboolean ready_at_start; /* The game is already built: open on the Play screen. */

  /* Saved choices (launcher.ini), read by AppRun when it starts the game. */
  gboolean fullscreen;
  gboolean mouse_aim;
  gboolean music_muted;

  /* Widgets. */
  GtkWidget *window;
  GtkWidget *stack;
  GdkPixbuf *background;
  GtkWidget *welcome_status;
  GtkWidget *use_imported;
  GtkWidget *tools_text;
  GtkWidget *build_step;
  GtkWidget *build_detail;
  GtkWidget *build_progress;
  GtkWidget *build_elapsed;
  GtkWidget *build_log_view;
  GtkWidget *error_message;
  GtkWidget *error_output;
  GtkWidget *ready_status;
  GtkWidget *music_bar;
  GtkWidget *mute_button;
  GtkWidget *fullscreen_switches[2];
  GtkWidget *mouse_checks[2];
  int settings_boxes;

  /* Music. */
  mp3dec_ex_t music;
  SDL_AudioDeviceID music_device;
  gboolean music_playing;

  /* Build state. */
  char *selected_rom; /* NULL: build with the already imported ROM. */
  GPid child;
  gboolean running;
  gboolean cancelling;
  gboolean quit_after_cancel;
  int open_streams;
  gboolean exited;
  int exit_status;
  guint timer;
  gint64 started;
  int step;
  double step_fraction;
  int substeps;
  char *workspace;
  char *failure;
  char *failure_log;
  gboolean built;
  GString *tail; /* Last lines of output, for the error screen. */
  int result;
} App;

static App app;

/* ---------- small helpers ---------- */

static GtkWidget *label_new(const char *text, const char *css_class) {
  GtkWidget *label = gtk_label_new(text);
  gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
  gtk_label_set_line_wrap_mode(GTK_LABEL(label), PANGO_WRAP_WORD_CHAR);
  gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_CENTER);
  gtk_label_set_max_width_chars(GTK_LABEL(label), 70);
  if (css_class) gtk_style_context_add_class(gtk_widget_get_style_context(label), css_class);
  return label;
}

static GtkWidget *button_new(const char *text, const char *css_class, GCallback callback) {
  GtkWidget *button = gtk_button_new_with_label(text);
  gtk_style_context_add_class(gtk_widget_get_style_context(button), css_class);
  if (callback) g_signal_connect(button, "clicked", callback, NULL);
  return button;
}

static void set_status(GtkWidget *label, const char *text, const char *css_class) {
  GtkStyleContext *style = gtk_widget_get_style_context(label);
  gtk_style_context_remove_class(style, "jfg-ok");
  gtk_style_context_remove_class(style, "jfg-error");
  if (css_class) gtk_style_context_add_class(style, css_class);
  gtk_label_set_text(GTK_LABEL(label), text ? text : "");
  gtk_widget_set_visible(label, text && *text);
}

static GtkWidget *log_view_new(GtkWidget **view_out, int height) {
  GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_widget_set_size_request(scroll, -1, height);
  GtkWidget *view = gtk_text_view_new();
  gtk_text_view_set_editable(GTK_TEXT_VIEW(view), FALSE);
  gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(view), FALSE);
  gtk_text_view_set_monospace(GTK_TEXT_VIEW(view), TRUE);
  gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(view), GTK_WRAP_CHAR);
  gtk_text_view_set_left_margin(GTK_TEXT_VIEW(view), 8);
  gtk_text_view_set_top_margin(GTK_TEXT_VIEW(view), 6);
  gtk_container_add(GTK_CONTAINER(scroll), view);
  gtk_style_context_add_class(gtk_widget_get_style_context(scroll), "jfg-log");
  *view_out = view;
  return scroll;
}

static void text_view_set(GtkWidget *view, const char *text) {
  gtk_text_buffer_set_text(gtk_text_view_get_buffer(GTK_TEXT_VIEW(view)), text ? text : "", -1);
}

static void text_view_append_line(GtkWidget *view, const char *line) {
  GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(view));
  GtkTextIter end;
  gtk_text_buffer_get_end_iter(buffer, &end);
  gtk_text_buffer_insert(buffer, &end, line, -1);
  gtk_text_buffer_get_end_iter(buffer, &end);
  gtk_text_buffer_insert(buffer, &end, "\n", 1);
  int lines = gtk_text_buffer_get_line_count(buffer);
  if (lines > LOG_LINE_LIMIT) {
    GtkTextIter start, cut;
    gtk_text_buffer_get_start_iter(buffer, &start);
    gtk_text_buffer_get_iter_at_line(buffer, &cut, lines - LOG_LINE_LIMIT);
    gtk_text_buffer_delete(buffer, &start, &cut);
  }
  gtk_text_buffer_get_end_iter(buffer, &end);
  GtkTextMark *mark = gtk_text_buffer_create_mark(buffer, NULL, &end, FALSE);
  gtk_text_view_scroll_mark_onscreen(GTK_TEXT_VIEW(view), mark);
  gtk_text_buffer_delete_mark(buffer, mark);
}

static void show_page(const char *name) {
  gtk_stack_set_visible_child_name(GTK_STACK(app.stack), name);
}

static void music_stop(void);

static void finish(int result, const char *reason) {
  if (reason) g_printerr("jfg-launcher: %s\n", reason);
  music_stop();
  app.result = result;
  gtk_main_quit();
}

/* ---------- saved choices ---------- */

static void settings_load(void) {
  char *text = NULL;
  if (!app.settings_path || !g_file_get_contents(app.settings_path, &text, NULL, NULL)) return;
  char **lines = g_strsplit(text, "\n", -1);
  for (char **line = lines; *line; line++) {
    if (strcmp(*line, "fullscreen=1") == 0) app.fullscreen = TRUE;
    if (strcmp(*line, "mouse_aim=1") == 0) app.mouse_aim = TRUE;
    if (strcmp(*line, "music_muted=1") == 0) app.music_muted = TRUE;
  }
  g_strfreev(lines);
  g_free(text);
}

static void settings_save(void) {
  if (!app.settings_path) return;
  char *folder = g_path_get_dirname(app.settings_path);
  g_mkdir_with_parents(folder, 0755);
  g_free(folder);
  char *text = g_strdup_printf("version=1\nfullscreen=%d\nmouse_aim=%d\nmusic_muted=%d\n", app.fullscreen ? 1 : 0,
                               app.mouse_aim ? 1 : 0, app.music_muted ? 1 : 0);
  GError *error = NULL;
  if (!g_file_set_contents(app.settings_path, text, -1, &error)) {
    g_printerr("jfg-launcher: could not save %s: %s\n", app.settings_path, error->message);
    g_error_free(error);
  }
  g_free(text);
}

/* ---------- music ---------- */

/* Runs on SDL's audio thread; the decoder is only touched here once playing. */
static void music_fill(void *user, Uint8 *stream, int length) {
  (void)user;
  mp3d_sample_t *out = (mp3d_sample_t *)stream;
  size_t wanted = (size_t)length / sizeof(mp3d_sample_t), have = 0;
  int restarts = 0;
  while (have < wanted && restarts < 2) {
    size_t got = mp3dec_ex_read(&app.music, out + have, wanted - have);
    if (got == 0) {
      restarts++; /* End of the track: loop from the start. */
      if (mp3dec_ex_seek(&app.music, 0) != 0) break;
      continue;
    }
    restarts = 0;
    have += got;
  }
  if (have < wanted) memset(out + have, 0, (wanted - have) * sizeof(mp3d_sample_t));
  for (size_t i = 0; i < have; i++) out[i] = (mp3d_sample_t)(out[i] * MUSIC_VOLUME);
}

static char *music_title(void) {
  char *name = g_path_get_basename(app.music_path);
  char *dot = strrchr(name, '.');
  if (dot && g_ascii_strcasecmp(dot, ".mp3") == 0) *dot = '\0';
  return name;
}

static gboolean music_start(void) {
  if (!app.music_path || !g_file_test(app.music_path, G_FILE_TEST_IS_REGULAR)) return FALSE;
  /* Otherwise SDL turns SIGTERM/SIGINT into SDL events nobody reads, and the
     launcher would ignore requests to quit (logout, kill, Ctrl+C). */
  SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1");
  if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
    g_printerr("jfg-launcher: no audio for music: %s\n", SDL_GetError());
    return FALSE;
  }
  if (mp3dec_ex_open(&app.music, app.music_path, MP3D_SEEK_TO_SAMPLE) != 0 || app.music.info.hz <= 0 ||
      app.music.info.channels <= 0) {
    g_printerr("jfg-launcher: could not read the music file %s\n", app.music_path);
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    return FALSE;
  }
  SDL_AudioSpec wanted, obtained;
  SDL_zero(wanted);
  wanted.freq = app.music.info.hz;
  wanted.format = AUDIO_S16SYS;
  wanted.channels = (Uint8)app.music.info.channels;
  wanted.samples = 4096;
  wanted.callback = music_fill;
  app.music_device = SDL_OpenAudioDevice(NULL, 0, &wanted, &obtained, 0);
  if (app.music_device == 0) {
    g_printerr("jfg-launcher: could not open audio for music: %s\n", SDL_GetError());
    mp3dec_ex_close(&app.music);
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    return FALSE;
  }
  app.music_playing = TRUE;
  SDL_PauseAudioDevice(app.music_device, app.music_muted ? 1 : 0);
  return TRUE;
}

static void music_stop(void) {
  if (app.music_playing) {
    SDL_CloseAudioDevice(app.music_device);
    mp3dec_ex_close(&app.music);
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    app.music_playing = FALSE;
  }
  if (app.music_bar) gtk_widget_hide(app.music_bar);
}

static void update_mute_button(void) {
  if (!app.mute_button) return;
  gtk_button_set_image(GTK_BUTTON(app.mute_button),
                       gtk_image_new_from_icon_name(app.music_muted ? "audio-volume-muted-symbolic"
                                                                    : "audio-volume-high-symbolic",
                                                    GTK_ICON_SIZE_BUTTON));
  gtk_widget_set_tooltip_text(app.mute_button, app.music_muted ? "Unmute the music" : "Mute the music");
}

static void on_mute(GtkButton *button, gpointer data) {
  (void)button;
  (void)data;
  app.music_muted = !app.music_muted;
  if (app.music_playing) SDL_PauseAudioDevice(app.music_device, app.music_muted ? 1 : 0);
  update_mute_button();
  settings_save();
}

/* ---------- Window / Full screen and mouse aim ---------- */

static void on_fullscreen_changed(GObject *object, GParamSpec *spec, gpointer data);
static void on_mouse_aim_toggled(GtkToggleButton *button, gpointer data);

/* Several pages show the same controls; keep every copy in step. */
static void sync_settings_controls(void) {
  for (int i = 0; i < app.settings_boxes; i++) {
    g_signal_handlers_block_by_func(app.fullscreen_switches[i], on_fullscreen_changed, NULL);
    gtk_switch_set_active(GTK_SWITCH(app.fullscreen_switches[i]), app.fullscreen);
    g_signal_handlers_unblock_by_func(app.fullscreen_switches[i], on_fullscreen_changed, NULL);
    g_signal_handlers_block_by_func(app.mouse_checks[i], on_mouse_aim_toggled, NULL);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(app.mouse_checks[i]), app.mouse_aim);
    g_signal_handlers_unblock_by_func(app.mouse_checks[i], on_mouse_aim_toggled, NULL);
  }
}

static void on_fullscreen_changed(GObject *object, GParamSpec *spec, gpointer data) {
  (void)spec;
  (void)data;
  app.fullscreen = gtk_switch_get_active(GTK_SWITCH(object));
  sync_settings_controls();
  settings_save();
}

static void on_mouse_aim_toggled(GtkToggleButton *button, gpointer data) {
  (void)data;
  app.mouse_aim = gtk_toggle_button_get_active(button);
  sync_settings_controls();
  settings_save();
}

static GtkWidget *settings_box_new(void) {
  int index = app.settings_boxes++;
  GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
  gtk_style_context_add_class(gtk_widget_get_style_context(box), "jfg-settings");

  GtkWidget *display = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
  gtk_widget_set_halign(display, GTK_ALIGN_CENTER);
  GtkWidget *window_label = label_new("Window", "jfg-text");
  app.fullscreen_switches[index] = gtk_switch_new();
  gtk_widget_set_valign(app.fullscreen_switches[index], GTK_ALIGN_CENTER);
  gtk_widget_set_tooltip_text(app.fullscreen_switches[index],
                              "How the game starts. F11 still switches while playing.");
  g_signal_connect(app.fullscreen_switches[index], "notify::active", G_CALLBACK(on_fullscreen_changed), NULL);
  gtk_box_pack_start(GTK_BOX(display), window_label, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(display), app.fullscreen_switches[index], FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(display), label_new("Full screen", "jfg-text"), FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(box), display, FALSE, FALSE, 0);

  app.mouse_checks[index] = gtk_check_button_new_with_label("Mouse aim (play with keyboard & mouse)");
  gtk_widget_set_halign(app.mouse_checks[index], GTK_ALIGN_CENTER);
  gtk_style_context_add_class(gtk_widget_get_style_context(app.mouse_checks[index]), "jfg-check");
  g_signal_connect(app.mouse_checks[index], "toggled", G_CALLBACK(on_mouse_aim_toggled), NULL);
  gtk_box_pack_start(GTK_BOX(box), app.mouse_checks[index], FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(box),
                     label_new("Player 1 uses keyboard and mouse: mouse to look and aim, WASD to move, left click to "
                               "fire. Untick to play with a gamepad. Experimental, single-player.",
                               "jfg-small"),
                     FALSE, FALSE, 0);
  return box;
}

/* ---------- ROM check ---------- */

/* Returns NULL when the file is the supported ROM, else a message to show. */
static char *check_rom(const char *path) {
  struct stat info;
  if (stat(path, &info) != 0 || !S_ISREG(info.st_mode))
    return g_strdup("That file can't be opened.");
  if (info.st_size != ROM_SIZE)
    return g_strdup("That isn't the supported ROM. It must be the 32 MiB Jet Force Gemini (USA) .z64 file; "
                    "byte-swapped .v64/.n64 dumps, other regions and hacks aren't supported.");
  FILE *file = fopen(path, "rb");
  if (!file) return g_strdup_printf("That file can't be read: %s", g_strerror(errno));
  unsigned char header[4];
  gboolean read_header = fread(header, 1, sizeof header, file) == sizeof header;
  if (read_header && !(header[0] == 0x80 && header[1] == 0x37)) {
    fclose(file);
    if ((header[0] == 0x37 && header[1] == 0x80) || (header[0] == 0x40 && header[1] == 0x12))
      return g_strdup("That ROM is byte-swapped (a .v64/.n64 dump). Use the big-endian .z64 version of the "
                      "Jet Force Gemini (USA) ROM.");
    return g_strdup("That doesn't look like an N64 ROM.");
  }
  rewind(file);
  GChecksum *checksum = g_checksum_new(G_CHECKSUM_SHA1);
  guchar buffer[65536];
  size_t count;
  while ((count = fread(buffer, 1, sizeof buffer, file)) > 0) g_checksum_update(checksum, buffer, (gssize)count);
  gboolean failed = ferror(file) != 0;
  fclose(file);
  gboolean matches = !failed && g_ascii_strcasecmp(g_checksum_get_string(checksum), app.rom_sha1) == 0;
  g_checksum_free(checksum);
  if (failed) return g_strdup("That file couldn't be read completely.");
  if (!matches)
    return g_strdup("That ROM doesn't match the supported Jet Force Gemini (USA) version. Other regions, "
                    "revisions and hacks aren't supported.");
  return NULL;
}

/* ---------- build process ---------- */

static void collect_descendants(GPid root, GArray *out) {
  /* Children may start their own process groups (timeout does), so walk /proc. */
  DIR *proc = opendir("/proc");
  if (!proc) return;
  GArray *pids = g_array_new(FALSE, FALSE, sizeof(pid_t));
  GArray *parents = g_array_new(FALSE, FALSE, sizeof(pid_t));
  struct dirent *entry;
  while ((entry = readdir(proc))) {
    char *end;
    long pid = strtol(entry->d_name, &end, 10);
    if (*end || pid <= 0) continue;
    char path[64], buf[512];
    snprintf(path, sizeof path, "/proc/%ld/stat", pid);
    FILE *stat_file = fopen(path, "r");
    if (!stat_file) continue;
    size_t n = fread(buf, 1, sizeof buf - 1, stat_file);
    fclose(stat_file);
    buf[n] = 0;
    char *close_paren = strrchr(buf, ')');
    long ppid;
    char state;
    if (!close_paren || sscanf(close_paren + 1, " %c %ld", &state, &ppid) != 2) continue;
    pid_t p = (pid_t)pid, pp = (pid_t)ppid;
    g_array_append_val(pids, p);
    g_array_append_val(parents, pp);
  }
  closedir(proc);
  g_array_append_val(out, root);
  for (guint i = 0; i < out->len; i++) {
    pid_t parent = g_array_index(out, pid_t, i);
    for (guint j = 0; j < pids->len; j++)
      if (g_array_index(parents, pid_t, j) == parent) g_array_append_val(out, g_array_index(pids, pid_t, j));
  }
  g_array_free(pids, TRUE);
  g_array_free(parents, TRUE);
}

static void signal_build(int signal_number) {
  if (!app.running) return;
  GArray *tree = g_array_new(FALSE, FALSE, sizeof(pid_t));
  collect_descendants(app.child, tree);
  for (guint i = 0; i < tree->len; i++) kill(g_array_index(tree, pid_t, i), signal_number);
  g_array_free(tree, TRUE);
}

static gboolean force_kill(gpointer data) {
  (void)data;
  signal_build(SIGKILL);
  return G_SOURCE_REMOVE;
}

static void cancel_build(gboolean quit) {
  if (!app.running) {
    if (quit) finish(1, "closed");
    return;
  }
  app.cancelling = TRUE;
  app.quit_after_cancel = app.quit_after_cancel || quit;
  gtk_label_set_text(GTK_LABEL(app.build_step), "Stopping the build…");
  signal_build(SIGTERM);
  g_timeout_add_seconds(5, force_kill, NULL);
}

/* The offline edition skips the downloads, so they must not pace the bar. */
static double step_weight(int i) {
  const char *id = kSteps[i].id;
  if (app.offline && (g_str_has_prefix(id, "clone-") || g_str_has_prefix(id, "pin-") ||
                      g_str_has_prefix(id, "submodules-")))
    return 1;
  if (!app.offline && (strcmp(id, "unpack-offline") == 0 || strcmp(id, "download-decomp") == 0 ||
                       strcmp(id, "download-ido") == 0))
    return 1;
  if (app.offline && strcmp(id, "generate") == 0) return 300; /* No downloads inside it either. */
  return kSteps[i].weight;
}

static double overall_fraction(void) {
  double total = 0, done = 0;
  for (int i = 0; i < STEP_COUNT; i++) {
    total += step_weight(i);
    if (i < app.step) done += step_weight(i);
  }
  if (app.step >= 0 && app.step < STEP_COUNT) done += step_weight(app.step) * app.step_fraction;
  return total > 0 ? CLAMP(done / total, 0.0, 1.0) : 0.0;
}

static void update_compile_fraction(void) {
  if (!app.workspace || app.step < 0 || strcmp(kSteps[app.step].id, "compile") != 0) return;
  char *path = g_build_filename(app.workspace, "compile.log", NULL);
  FILE *file = fopen(path, "rb");
  g_free(path);
  if (!file) return;
  char buf[8192];
  if (fseek(file, -(long)(sizeof buf - 1), SEEK_END) != 0) rewind(file);
  size_t n = fread(buf, 1, sizeof buf - 1, file);
  fclose(file);
  buf[n] = 0;
  /* Ninja prints "[done/total] ..." lines; use the last one. */
  for (char *p = buf + n; p > buf; p--) {
    long done, total;
    if (p[-1] == '\n' && p[0] == '[' && sscanf(p, "[%ld/%ld]", &done, &total) == 2 && total > 0) {
      app.step_fraction = CLAMP((double)done / (double)total, 0.0, 1.0);
      return;
    }
  }
}

static void refresh_progress(void) {
  update_compile_fraction();
  double fraction = overall_fraction();
  gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(app.build_progress), fraction);
  char *percent = g_strdup_printf("%d%%", (int)(fraction * 100.0));
  gtk_progress_bar_set_text(GTK_PROGRESS_BAR(app.build_progress), percent);
  g_free(percent);
  gint64 seconds = (g_get_monotonic_time() - app.started) / G_USEC_PER_SEC;
  char *elapsed = seconds >= 3600
      ? g_strdup_printf("Elapsed %d:%02d:%02d", (int)(seconds / 3600), (int)(seconds / 60 % 60), (int)(seconds % 60))
      : g_strdup_printf("Elapsed %d:%02d", (int)(seconds / 60), (int)(seconds % 60));
  gtk_label_set_text(GTK_LABEL(app.build_elapsed), elapsed);
  g_free(elapsed);
}

static gboolean on_timer(gpointer data) {
  (void)data;
  refresh_progress();
  return G_SOURCE_CONTINUE;
}

static void set_step(const char *id) {
  for (int i = 0; i < STEP_COUNT; i++) {
    if (strcmp(kSteps[i].id, id) == 0) {
      app.step = i;
      app.step_fraction = 0;
      gtk_label_set_text(GTK_LABEL(app.build_step), kSteps[i].label);
      gtk_label_set_text(GTK_LABEL(app.build_detail), "");
      return;
    }
  }
}

static void remember_tail(const char *line) {
  g_string_append(app.tail, line);
  g_string_append_c(app.tail, '\n');
  if (app.tail->len > 6000) g_string_erase(app.tail, 0, (gssize)(app.tail->len - 6000));
}

static void handle_line(const char *line) {
  text_view_append_line(app.build_log_view, line);
  remember_tail(line);
  const char *value;
  if ((value = g_str_has_prefix(line, "Local build workspace: ") ? line + 23 : NULL)) {
    g_free(app.workspace);
    app.workspace = g_strdup(value);
  } else if (g_str_has_prefix(line, "JFG-SUPPORT setup_step=")) {
    set_step(line + 23);
  } else if (g_str_has_prefix(line, "Built: ")) {
    app.built = TRUE;
  } else if (app.step >= 0 && strcmp(kSteps[app.step].id, "generate") == 0 && g_str_has_prefix(line, "  ") &&
             line[2] >= 'a' && line[2] <= 'z' && !strchr(line + 2, ' ')) {
    app.substeps++;
    app.step_fraction = MIN((double)app.substeps / GENERATE_SUBSTEPS, 0.98);
    gtk_label_set_text(GTK_LABEL(app.build_detail), line + 2);
  } else if (strstr(line, "failed") || strstr(line, "missing") || strstr(line, "error")) {
    const char *log_marker = strstr(line, "log: ");
    const char *full_marker = strstr(line, "Full output: ");
    if (log_marker || full_marker) {
      g_free(app.failure_log);
      app.failure_log = g_strdup(log_marker ? log_marker + 5 : full_marker + 13);
    }
    if (!full_marker || !app.failure) {
      g_free(app.failure);
      app.failure = g_strdup(line);
    }
  }
}

static void show_build_error(void);

static void build_finished(void) {
  if (app.timer) g_source_remove(app.timer);
  app.timer = 0;
  app.running = FALSE;
  g_spawn_close_pid(app.child);
  if (app.cancelling) {
    if (app.quit_after_cancel) {
      finish(1, "build stopped and window closed by the user");
    } else {
      app.cancelling = FALSE;
      set_status(app.welcome_status, "The build was stopped. You can start it again at any time.", NULL);
      show_page("welcome");
    }
    return;
  }
  if (g_spawn_check_exit_status(app.exit_status, NULL) && app.built) {
    app.step = STEP_COUNT;
    refresh_progress();
    music_stop(); /* The music accompanies the build only. */
    set_status(app.ready_status, "✓ Your game was built successfully.", "jfg-ok");
    show_page("ready");
    return;
  }
  g_printerr("jfg-launcher: build failed: %s\n", app.failure ? app.failure : "(no message)");
  show_build_error();
}

static void maybe_finished(void) {
  if (app.exited && app.open_streams == 0) build_finished();
}

static gboolean on_output(GIOChannel *channel, GIOCondition condition, gpointer data) {
  (void)data;
  if (condition & G_IO_IN) {
    char *line = NULL;
    gsize length = 0, terminator = 0;
    GIOStatus status = g_io_channel_read_line(channel, &line, &length, &terminator, NULL);
    if (status == G_IO_STATUS_NORMAL && line) {
      line[terminator] = 0;
      /* Progress bars draw with carriage returns; keep the last segment. */
      char *segment = strrchr(line, '\r');
      handle_line(segment ? segment + 1 : line);
      g_free(line);
      return G_SOURCE_CONTINUE;
    }
    g_free(line);
    if (status == G_IO_STATUS_AGAIN) return G_SOURCE_CONTINUE;
  }
  app.open_streams--;
  maybe_finished();
  return G_SOURCE_REMOVE;
}

static void on_child_exit(GPid pid, gint status, gpointer data) {
  (void)pid;
  (void)data;
  app.exited = TRUE;
  app.exit_status = status;
  maybe_finished();
}

static void watch_stream(int fd) {
  GIOChannel *channel = g_io_channel_unix_new(fd);
  g_io_channel_set_encoding(channel, NULL, NULL);
  g_io_channel_set_close_on_unref(channel, TRUE);
  g_io_add_watch(channel, G_IO_IN | G_IO_HUP | G_IO_ERR, on_output, NULL);
  g_io_channel_unref(channel);
  app.open_streams++;
}

static void start_build(void) {
  GPtrArray *argv = g_ptr_array_new();
  g_ptr_array_add(argv, app.apprun);
  g_ptr_array_add(argv, "--build-only");
  if (app.selected_rom) {
    g_ptr_array_add(argv, "--rom");
    g_ptr_array_add(argv, app.selected_rom);
  }
  g_ptr_array_add(argv, NULL);
  char **env = g_get_environ();
  env = g_environ_setenv(env, "JFG_LAUNCHER_CHILD", "1", TRUE);
  env = g_environ_setenv(env, "PYTHONUNBUFFERED", "1", TRUE);

  int out_fd = -1, err_fd = -1;
  GError *error = NULL;
  gboolean spawned = g_spawn_async_with_pipes(NULL, (char **)argv->pdata, env, G_SPAWN_DO_NOT_REAP_CHILD, NULL,
                                              NULL, &app.child, NULL, &out_fd, &err_fd, &error);
  g_ptr_array_free(argv, TRUE);
  g_strfreev(env);
  if (!spawned) {
    g_free(app.failure);
    app.failure = g_strdup_printf("The build couldn't be started: %s", error->message);
    g_error_free(error);
    show_build_error();
    return;
  }
  app.running = TRUE;
  app.cancelling = FALSE;
  app.exited = FALSE;
  app.open_streams = 0;
  app.built = FALSE;
  app.step = -1;
  app.step_fraction = 0;
  app.substeps = 0;
  g_clear_pointer(&app.failure, g_free);
  g_clear_pointer(&app.failure_log, g_free);
  g_string_truncate(app.tail, 0);
  text_view_set(app.build_log_view, "");
  gtk_label_set_text(GTK_LABEL(app.build_step), "Starting the build…");
  gtk_label_set_text(GTK_LABEL(app.build_detail), "");
  app.started = g_get_monotonic_time();
  watch_stream(out_fd);
  watch_stream(err_fd);
  g_child_watch_add(app.child, on_child_exit, NULL);
  app.timer = g_timeout_add_seconds(1, on_timer, NULL);
  refresh_progress();
  show_page("building");
}

/* ---------- tools check ---------- */

/* Returns TRUE when everything is installed; otherwise fills the tools page. */
static gboolean tools_ready(void) {
  char *argv[] = {app.apprun, "--check-tools", NULL};
  char **env = g_environ_setenv(g_get_environ(), "JFG_LAUNCHER_CHILD", "1", TRUE);
  char *output = NULL;
  int status = 0;
  GError *error = NULL;
  gboolean ran = g_spawn_sync(NULL, argv, env, G_SPAWN_STDERR_TO_DEV_NULL, NULL, NULL, &output, NULL, &status, &error);
  g_strfreev(env);
  if (ran && g_spawn_check_exit_status(status, NULL)) {
    g_free(output);
    return TRUE;
  }
  text_view_set(app.tools_text, ran ? output : error->message);
  if (error) g_error_free(error);
  g_free(output);
  show_page("tools");
  return FALSE;
}

static void continue_to_build(void) {
  if (tools_ready()) start_build();
}

/* ---------- callbacks ---------- */

static void on_select_rom(GtkButton *button, gpointer data) {
  (void)button;
  (void)data;
  GtkFileChooserNative *chooser = gtk_file_chooser_native_new(
      "Select your Jet Force Gemini (USA) ROM", GTK_WINDOW(app.window), GTK_FILE_CHOOSER_ACTION_OPEN, "_Select",
      "_Cancel");
  GtkFileFilter *rom_filter = gtk_file_filter_new();
  gtk_file_filter_set_name(rom_filter, "N64 ROM (*.z64)");
  gtk_file_filter_add_pattern(rom_filter, "*.z64");
  gtk_file_filter_add_pattern(rom_filter, "*.Z64");
  gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(chooser), rom_filter);
  GtkFileFilter *all_filter = gtk_file_filter_new();
  gtk_file_filter_set_name(all_filter, "All files");
  gtk_file_filter_add_pattern(all_filter, "*");
  gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(chooser), all_filter);
  gint response = gtk_native_dialog_run(GTK_NATIVE_DIALOG(chooser));
  char *path = response == GTK_RESPONSE_ACCEPT ? gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(chooser)) : NULL;
  g_object_unref(chooser);
  if (!path) return;
  char *problem = check_rom(path);
  if (problem) {
    set_status(app.welcome_status, problem, "jfg-error");
    g_free(problem);
    g_free(path);
    return;
  }
  g_free(app.selected_rom);
  app.selected_rom = path;
  set_status(app.welcome_status, "✓ Supported Jet Force Gemini (USA) ROM", "jfg-ok");
  continue_to_build();
}

static void on_use_imported(GtkButton *button, gpointer data) {
  (void)button;
  (void)data;
  g_clear_pointer(&app.selected_rom, g_free);
  continue_to_build();
}

static void on_check_again(GtkButton *button, gpointer data) {
  (void)button;
  (void)data;
  continue_to_build();
}

static void on_copy_tools(GtkButton *button, gpointer data) {
  (void)data;
  GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(app.tools_text));
  GtkTextIter start, end;
  gtk_text_buffer_get_bounds(buffer, &start, &end);
  char *text = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
  gtk_clipboard_set_text(gtk_clipboard_get(GDK_SELECTION_CLIPBOARD), text, -1);
  g_free(text);
  gtk_button_set_label(button, "Copied");
}

static void on_play(GtkButton *button, gpointer data) {
  (void)button;
  (void)data;
  settings_save();
  finish(0, NULL); /* AppRun starts the game with the saved choices. */
}

static void on_close(GtkButton *button, gpointer data) {
  (void)button;
  (void)data;
  finish(1, "closed from the error screen");
}

static void on_back(GtkButton *button, gpointer data) {
  (void)button;
  (void)data;
  show_page("welcome");
}

static void on_cancel_build(GtkButton *button, gpointer data) {
  (void)button;
  (void)data;
  cancel_build(FALSE);
}

static void on_try_again(GtkButton *button, gpointer data) {
  (void)button;
  (void)data;
  continue_to_build();
}

static void on_open_log(GtkButton *button, gpointer data) {
  (void)button;
  (void)data;
  const char *path = app.failure_log ? app.failure_log : app.build_log;
  char *uri = g_filename_to_uri(path, NULL, NULL);
  if (uri) gtk_show_uri_on_window(GTK_WINDOW(app.window), uri, GDK_CURRENT_TIME, NULL);
  g_free(uri);
}

static gboolean on_delete(GtkWidget *widget, GdkEvent *event, gpointer data) {
  (void)widget;
  (void)event;
  (void)data;
  if (!app.running) {
    finish(1, "window closed");
    return TRUE;
  }
  GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(app.window), GTK_DIALOG_MODAL, GTK_MESSAGE_QUESTION,
                                             GTK_BUTTONS_NONE, "Stop building the game?");
  gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog),
                                           "The build is still running. If you stop now, it starts again from the "
                                           "beginning next time.");
  gtk_dialog_add_buttons(GTK_DIALOG(dialog), "Keep building", GTK_RESPONSE_CANCEL, "Stop and close",
                         GTK_RESPONSE_ACCEPT, NULL);
  gint response = gtk_dialog_run(GTK_DIALOG(dialog));
  gtk_widget_destroy(dialog);
  if (response == GTK_RESPONSE_ACCEPT) cancel_build(TRUE);
  return TRUE;
}

static void show_build_error(void) {
  const char *message = app.failure ? app.failure : "The build stopped unexpectedly.";
  char *text = g_strdup_printf("%s\n\nIf it keeps failing, open the log and include it when you ask for help.",
                               message);
  gtk_label_set_text(GTK_LABEL(app.error_message), text);
  g_free(text);
  text_view_set(app.error_output, app.tail->len ? app.tail->str : "(no output)");
  show_page("error");
}

/* ---------- drawing ---------- */

static gboolean on_draw_background(GtkWidget *widget, cairo_t *cr, gpointer data) {
  (void)data;
  double width = gtk_widget_get_allocated_width(widget);
  double height = gtk_widget_get_allocated_height(widget);
  cairo_set_source_rgb(cr, 0.03, 0.02, 0.06);
  cairo_paint(cr);
  if (app.background) {
    double image_width = gdk_pixbuf_get_width(app.background);
    double image_height = gdk_pixbuf_get_height(app.background);
    double scale = MAX(width / image_width, height / image_height);
    cairo_save(cr);
    cairo_translate(cr, (width - image_width * scale) / 2, (height - image_height * scale) / 2);
    cairo_scale(cr, scale, scale);
    gdk_cairo_set_source_pixbuf(cr, app.background, 0, 0);
    cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_GOOD);
    cairo_paint(cr);
    cairo_restore(cr);
  }
  /* Darken the artwork so text stays readable, more towards the edges. */
  cairo_set_source_rgba(cr, 0.02, 0.01, 0.06, 0.58);
  cairo_paint(cr);
  cairo_pattern_t *vignette = cairo_pattern_create_radial(width / 2, height / 2, MIN(width, height) * 0.25,
                                                          width / 2, height / 2, MAX(width, height) * 0.75);
  cairo_pattern_add_color_stop_rgba(vignette, 0, 0, 0, 0, 0);
  cairo_pattern_add_color_stop_rgba(vignette, 1, 0, 0, 0, 0.55);
  cairo_set_source(cr, vignette);
  cairo_paint(cr);
  cairo_pattern_destroy(vignette);
  return FALSE;
}

static const char *kCss =
    "window { background-color: #07040f; }\n"
    ".jfg-card { background-color: rgba(10, 6, 28, 0.74); border-radius: 18px;"
    "  border: 1px solid rgba(130, 160, 255, 0.35); box-shadow: 0 0 28px rgba(90, 110, 255, 0.35);"
    "  padding: 30px 40px; }\n"
    ".jfg-title { color: #ffffff; font-size: 28px; font-weight: 800;"
    "  text-shadow: 0 0 14px rgba(255, 70, 200, 0.85), 0 0 4px rgba(80, 140, 255, 0.9); }\n"
    ".jfg-heading { color: #ffffff; font-size: 20px; font-weight: 700;"
    "  text-shadow: 0 0 10px rgba(255, 70, 200, 0.7); }\n"
    ".jfg-text { color: rgba(236, 236, 255, 0.92); font-size: 14px; }\n"
    ".jfg-small { color: rgba(210, 210, 245, 0.68); font-size: 11px; }\n"
    ".jfg-ok { color: #74f2a6; font-weight: 700; }\n"
    ".jfg-error { color: #ff8396; font-weight: 700; }\n"
    "button.jfg-primary { background-image: linear-gradient(90deg, #ff2fa8, #3d6bff); color: #ffffff;"
    "  border: none; border-radius: 10px; padding: 10px 28px; font-weight: 700; font-size: 14px;"
    "  box-shadow: 0 0 16px rgba(255, 60, 190, 0.55); text-shadow: none; }\n"
    "button.jfg-primary:hover { background-image: linear-gradient(90deg, #ff5cbf, #6188ff); }\n"
    "button.jfg-primary label { color: #ffffff; }\n"
    "button.jfg-secondary { background-image: none; background-color: rgba(255, 255, 255, 0.08);"
    "  color: #e9e9ff; border: 1px solid rgba(255, 255, 255, 0.28); border-radius: 10px;"
    "  padding: 10px 22px; text-shadow: none; box-shadow: none; }\n"
    "button.jfg-secondary:hover { background-color: rgba(255, 255, 255, 0.16); }\n"
    "button.jfg-secondary label { color: #e9e9ff; }\n"
    "progressbar { color: #ffffff; font-weight: 700; }\n"
    "progressbar trough { min-height: 14px; border-radius: 7px; border: none;"
    "  background-color: rgba(255, 255, 255, 0.12); }\n"
    "progressbar progress { min-height: 14px; border-radius: 7px; border: none;"
    "  background-image: linear-gradient(90deg, #ff2fa8, #3d6bff); }\n"
    ".jfg-log { border-radius: 8px; border: 1px solid rgba(255, 255, 255, 0.15); }\n"
    ".jfg-log textview, .jfg-log textview text { background-color: rgba(0, 0, 0, 0.6); color: #d7d7f2;"
    "  font-size: 10px; }\n"
    "expander title, expander title label { color: rgba(225, 225, 255, 0.85); }\n"
    "expander arrow { color: rgba(225, 225, 255, 0.85); }\n"
    ".jfg-settings { margin-top: 2px; margin-bottom: 2px; }\n"
    "checkbutton.jfg-check label { color: rgba(236, 236, 255, 0.92); font-size: 14px; }\n"
    "switch { background-image: none; background-color: rgba(255, 255, 255, 0.16);"
    "  border: 1px solid rgba(255, 255, 255, 0.3); }\n"
    "switch:checked { background-image: linear-gradient(90deg, #ff2fa8, #3d6bff); border: none; }\n"
    "switch slider { background-image: none; background-color: #ffffff; border: none; }\n"
    "button.jfg-play { font-size: 20px; padding: 12px 64px; }\n"
    ".jfg-music { background-color: rgba(10, 6, 28, 0.62); border-radius: 18px;"
    "  border: 1px solid rgba(130, 160, 255, 0.3); padding: 4px 6px 4px 14px; }\n"
    ".jfg-music-title { color: #ffffff; font-size: 12px; font-weight: 700; letter-spacing: 1px;"
    "  text-shadow: 0 0 8px rgba(255, 70, 200, 0.8); }\n"
    "button.jfg-mute { background-image: none; background-color: transparent; border: none;"
    "  box-shadow: none; padding: 2px 6px; min-height: 0; min-width: 0; color: #ffffff; }\n"
    "button.jfg-mute:hover { background-color: rgba(255, 255, 255, 0.14); border-radius: 12px; }\n";

/* ---------- pages ---------- */

static GtkWidget *card_new(void) {
  GtkWidget *card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
  gtk_style_context_add_class(gtk_widget_get_style_context(card), "jfg-card");
  gtk_widget_set_halign(card, GTK_ALIGN_CENTER);
  gtk_widget_set_valign(card, GTK_ALIGN_CENTER);
  gtk_widget_set_size_request(card, 640, -1);
  return card;
}

static GtkWidget *button_row_new(void) {
  GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
  gtk_widget_set_halign(row, GTK_ALIGN_CENTER);
  gtk_widget_set_margin_top(row, 6);
  return row;
}

static GtkWidget *welcome_page(void) {
  GtkWidget *card = card_new();
  gtk_box_pack_start(GTK_BOX(card), label_new("Mabecs Jet Force Gemini Recomp", "jfg-title"), FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(card),
                     label_new("Select your Jet Force Gemini (USA) ROM to begin. The game is then built on this "
                               "computer from your own ROM. That only happens once; then press Play.",
                               "jfg-text"),
                     FALSE, FALSE, 0);
  app.welcome_status = label_new("", NULL);
  gtk_widget_set_no_show_all(app.welcome_status, TRUE);
  gtk_box_pack_start(GTK_BOX(card), app.welcome_status, FALSE, FALSE, 0);
  GtkWidget *row = button_row_new();
  gtk_box_pack_start(GTK_BOX(row), button_new("Select ROM…", "jfg-primary", G_CALLBACK(on_select_rom)), FALSE,
                     FALSE, 0);
  app.use_imported = button_new("Use imported ROM", "jfg-secondary", G_CALLBACK(on_use_imported));
  gtk_widget_set_no_show_all(app.use_imported, TRUE);
  gtk_box_pack_start(GTK_BOX(row), app.use_imported, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(card), row, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(card),
                     label_new("No game data is included. Your ROM is checked and stays on this computer.",
                               "jfg-small"),
                     FALSE, FALSE, 0);
  return card;
}

static GtkWidget *tools_page(void) {
  GtkWidget *card = card_new();
  gtk_box_pack_start(GTK_BOX(card), label_new("A few build tools are missing", "jfg-heading"), FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(card),
                     label_new("The game is built from your ROM with standard developer tools. Install the "
                               "packages below in a terminal, then choose Check again.",
                               "jfg-text"),
                     FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(card), log_view_new(&app.tools_text, 230), FALSE, FALSE, 0);
  GtkWidget *row = button_row_new();
  gtk_box_pack_start(GTK_BOX(row), button_new("Check again", "jfg-primary", G_CALLBACK(on_check_again)), FALSE,
                     FALSE, 0);
  gtk_box_pack_start(GTK_BOX(row), button_new("Copy", "jfg-secondary", G_CALLBACK(on_copy_tools)), FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(row), button_new("Back", "jfg-secondary", G_CALLBACK(on_back)), FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(card), row, FALSE, FALSE, 0);
  return card;
}

static GtkWidget *building_page(void) {
  GtkWidget *card = card_new();
  gtk_box_pack_start(GTK_BOX(card), label_new("Building your game", "jfg-heading"), FALSE, FALSE, 0);
  app.build_step = label_new("Starting the build…", "jfg-text");
  gtk_box_pack_start(GTK_BOX(card), app.build_step, FALSE, FALSE, 0);
  app.build_progress = gtk_progress_bar_new();
  gtk_progress_bar_set_show_text(GTK_PROGRESS_BAR(app.build_progress), TRUE);
  gtk_box_pack_start(GTK_BOX(card), app.build_progress, FALSE, FALSE, 0);
  GtkWidget *info = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
  app.build_detail = label_new("", "jfg-small");
  gtk_widget_set_halign(app.build_detail, GTK_ALIGN_START);
  app.build_elapsed = label_new("", "jfg-small");
  gtk_box_pack_start(GTK_BOX(info), app.build_detail, TRUE, TRUE, 0);
  gtk_box_pack_end(GTK_BOX(info), app.build_elapsed, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(card), info, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(card),
                     label_new(app.offline
                                   ? "Everything needed is included except two small downloads (about 6 MB), so "
                                     "this usually takes 15-30 minutes. You can keep using your computer "
                                     "meanwhile; a Play button appears when it's done."
                                   : "The first build downloads about 4 GB and usually takes 15-30 minutes. You can "
                                     "keep using your computer meanwhile; a Play button appears when it's done.",
                               "jfg-small"),
                     FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(card), settings_box_new(), FALSE, FALSE, 0);
  GtkWidget *expander = gtk_expander_new("Show details");
  gtk_container_add(GTK_CONTAINER(expander), log_view_new(&app.build_log_view, 150));
  gtk_box_pack_start(GTK_BOX(card), expander, FALSE, FALSE, 0);
  GtkWidget *row = button_row_new();
  gtk_box_pack_start(GTK_BOX(row), button_new("Cancel", "jfg-secondary", G_CALLBACK(on_cancel_build)), FALSE,
                     FALSE, 0);
  gtk_box_pack_start(GTK_BOX(card), row, FALSE, FALSE, 0);
  return card;
}

static GtkWidget *error_page(void) {
  GtkWidget *card = card_new();
  gtk_box_pack_start(GTK_BOX(card), label_new("The build didn't finish", "jfg-heading"), FALSE, FALSE, 0);
  app.error_message = label_new("", "jfg-text");
  gtk_label_set_selectable(GTK_LABEL(app.error_message), TRUE);
  gtk_box_pack_start(GTK_BOX(card), app.error_message, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(card), log_view_new(&app.error_output, 170), FALSE, FALSE, 0);
  GtkWidget *row = button_row_new();
  gtk_box_pack_start(GTK_BOX(row), button_new("Try again", "jfg-primary", G_CALLBACK(on_try_again)), FALSE, FALSE,
                     0);
  gtk_box_pack_start(GTK_BOX(row), button_new("Open log", "jfg-secondary", G_CALLBACK(on_open_log)), FALSE, FALSE,
                     0);
  gtk_box_pack_start(GTK_BOX(row), button_new("Close", "jfg-secondary", G_CALLBACK(on_close)), FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(card), row, FALSE, FALSE, 0);
  return card;
}

static GtkWidget *ready_page(void) {
  GtkWidget *card = card_new();
  gtk_box_pack_start(GTK_BOX(card), label_new("Mabecs Jet Force Gemini Recomp", "jfg-title"), FALSE, FALSE, 0);
  app.ready_status = label_new("", NULL);
  gtk_widget_set_no_show_all(app.ready_status, TRUE);
  gtk_box_pack_start(GTK_BOX(card), app.ready_status, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(card), settings_box_new(), FALSE, FALSE, 0);
  GtkWidget *row = button_row_new();
  GtkWidget *play = button_new("Play", "jfg-primary", G_CALLBACK(on_play));
  gtk_style_context_add_class(gtk_widget_get_style_context(play), "jfg-play");
  gtk_box_pack_start(GTK_BOX(row), play, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(card), row, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(card),
                     label_new("Your choices are remembered. F11 switches between window and full screen while "
                               "playing.",
                               "jfg-small"),
                     FALSE, FALSE, 0);
  return card;
}

static GtkWidget *music_bar_new(void) {
  GtkWidget *bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
  gtk_style_context_add_class(gtk_widget_get_style_context(bar), "jfg-music");
  gtk_widget_set_halign(bar, GTK_ALIGN_START);
  gtk_widget_set_valign(bar, GTK_ALIGN_START);
  gtk_widget_set_margin_start(bar, 16);
  gtk_widget_set_margin_top(bar, 14);
  char *title = music_title();
  char *text = g_strdup_printf("♪  NOW PLAYING: %s", title);
  GtkWidget *label = gtk_label_new(text);
  gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
  gtk_label_set_max_width_chars(GTK_LABEL(label), 100);
  gtk_style_context_add_class(gtk_widget_get_style_context(label), "jfg-music-title");
  g_free(text);
  g_free(title);
  app.mute_button = gtk_button_new();
  gtk_style_context_add_class(gtk_widget_get_style_context(app.mute_button), "jfg-mute");
  g_signal_connect(app.mute_button, "clicked", G_CALLBACK(on_mute), NULL);
  update_mute_button();
  gtk_box_pack_start(GTK_BOX(bar), label, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(bar), app.mute_button, FALSE, FALSE, 0);
  gtk_widget_show_all(bar);
  gtk_widget_set_no_show_all(bar, TRUE); /* Hidden again by music_stop() without show_all undoing it. */
  return bar;
}

static void build_ui(GtkWidget *toplevel) {
  GtkCssProvider *css = gtk_css_provider_new();
  gtk_css_provider_load_from_data(css, kCss, -1, NULL);
  gtk_style_context_add_provider_for_screen(gdk_screen_get_default(), GTK_STYLE_PROVIDER(css),
                                            GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  g_object_unref(css);
  g_object_set(gtk_settings_get_default(), "gtk-application-prefer-dark-theme", TRUE, NULL);

  if (app.background_path) app.background = gdk_pixbuf_new_from_file(app.background_path, NULL);

  GtkWidget *overlay = gtk_overlay_new();
  GtkWidget *backdrop = gtk_drawing_area_new();
  gtk_widget_set_size_request(backdrop, WINDOW_WIDTH, WINDOW_HEIGHT);
  g_signal_connect(backdrop, "draw", G_CALLBACK(on_draw_background), NULL);
  gtk_container_add(GTK_CONTAINER(overlay), backdrop);

  app.stack = gtk_stack_new();
  gtk_stack_set_transition_type(GTK_STACK(app.stack), GTK_STACK_TRANSITION_TYPE_CROSSFADE);
  gtk_stack_add_named(GTK_STACK(app.stack), welcome_page(), "welcome");
  gtk_stack_add_named(GTK_STACK(app.stack), tools_page(), "tools");
  gtk_stack_add_named(GTK_STACK(app.stack), building_page(), "building");
  gtk_stack_add_named(GTK_STACK(app.stack), error_page(), "error");
  gtk_stack_add_named(GTK_STACK(app.stack), ready_page(), "ready");
  gtk_overlay_add_overlay(GTK_OVERLAY(overlay), app.stack);
  if (app.music_path) {
    app.music_bar = music_bar_new();
    gtk_overlay_add_overlay(GTK_OVERLAY(overlay), app.music_bar);
  }
  sync_settings_controls();
  gtk_container_add(GTK_CONTAINER(toplevel), overlay);

  if (app.rom_store && g_file_test(app.rom_store, G_FILE_TEST_IS_REGULAR)) {
    char *problem = check_rom(app.rom_store);
    if (!problem) gtk_widget_show(app.use_imported);
    g_free(problem);
  }
}

/* ---------- screenshots (for checking the design without a visible window) ---------- */

static gboolean render_screenshots(char **requests) {
  for (char **request = requests; request && *request; request++) {
    char **parts = g_strsplit(*request, "=", 2);
    if (!parts[0] || !parts[1]) {
      g_printerr("--screenshot expects PAGE=FILE.png\n");
      g_strfreev(parts);
      return FALSE;
    }
    const char *page = parts[0];
    if (strcmp(page, "rom-error") == 0) {
      set_status(app.welcome_status,
                 "That ROM doesn't match the supported Jet Force Gemini (USA) version. Other regions, revisions "
                 "and hacks aren't supported.",
                 "jfg-error");
      page = "welcome";
    } else if (strcmp(page, "welcome") == 0) {
      set_status(app.welcome_status, NULL, NULL);
    } else if (strcmp(page, "tools") == 0) {
      text_view_set(app.tools_text,
                    "The first-run build needs tools that are not installed:\n"
                    "  missing tools: mips-linux-gnu-as mips-linux-gnu-objcopy\n\n"
                    "On Arch / CachyOS / Manjaro:\n"
                    "  paru -S mips-linux-gnu-binutils");
    } else if (strcmp(page, "building") == 0) {
      set_step("generate");
      app.substeps = 9;
      app.step_fraction = 9.0 / GENERATE_SUBSTEPS;
      gtk_label_set_text(GTK_LABEL(app.build_detail), "configure-recompiler");
      app.started = g_get_monotonic_time() - (gint64)2537 * G_USEC_PER_SEC;
      refresh_progress();
      text_view_set(app.build_log_view, "generate\nJFG-SUPPORT setup_step=generate\n  extract-rom\n  matching-elf\n");
    } else if (strcmp(page, "ready") == 0) {
      set_status(app.ready_status, "✓ Your game was built successfully.", "jfg-ok");
    } else if (strcmp(page, "error") == 0) {
      g_free(app.failure);
      app.failure = g_strdup("compile failed. Local diagnostic log: ~/.cache/jfg/appimage/…/compile.log");
      g_string_assign(app.tail, "compile\nJFG-SUPPORT setup_step=compile\nJFG-SUPPORT setup_exit=0x00000001\n"
                                "compile failed. Local diagnostic log: ~/.cache/jfg/appimage/…/compile.log\n");
      show_build_error();
    }
    gtk_stack_set_transition_type(GTK_STACK(app.stack), GTK_STACK_TRANSITION_TYPE_NONE);
    show_page(page);
    gtk_widget_queue_draw(app.window);
    for (int i = 0; i < 20; i++)
      while (gtk_events_pending()) gtk_main_iteration();
    GdkPixbuf *shot = gtk_offscreen_window_get_pixbuf(GTK_OFFSCREEN_WINDOW(app.window));
    GError *error = NULL;
    gboolean saved = shot && gdk_pixbuf_save(shot, parts[1], "png", &error, NULL);
    if (!saved) g_printerr("Could not save %s: %s\n", parts[1], error ? error->message : "no image");
    if (shot) g_object_unref(shot);
    g_clear_error(&error);
    g_strfreev(parts);
    if (!saved) return FALSE;
  }
  return TRUE;
}

int main(int argc, char **argv) {
  gboolean probe = FALSE;
  gboolean start_now = FALSE;
  char **screenshots = NULL;
  GOptionEntry entries[] = {
    {"apprun", 0, 0, G_OPTION_ARG_FILENAME, &app.apprun, "AppRun to build with", "PATH"},
    {"rom-store", 0, 0, G_OPTION_ARG_FILENAME, &app.rom_store, "Where the imported ROM is kept", "PATH"},
    {"rom-sha1", 0, 0, G_OPTION_ARG_STRING, &app.rom_sha1, "SHA-1 of the supported ROM", "HEX"},
    {"background", 0, 0, G_OPTION_ARG_FILENAME, &app.background_path, "Background image", "PATH"},
    {"icon", 0, 0, G_OPTION_ARG_FILENAME, &app.icon_path, "Window icon", "PATH"},
    {"build-log", 0, 0, G_OPTION_ARG_FILENAME, &app.build_log, "Build log to open on failure", "PATH"},
    {"probe", 0, 0, G_OPTION_ARG_NONE, &probe, "Exit 0 if a window can be shown", NULL},
    {"offline", 0, 0, G_OPTION_ARG_NONE, &app.offline, "The AppImage bundles its dependencies", NULL},
    {"ready", 0, 0, G_OPTION_ARG_NONE, &app.ready_at_start, "The game is built: open on the Play screen", NULL},
    {"settings", 0, 0, G_OPTION_ARG_FILENAME, &app.settings_path, "Where the player's choices are saved", "PATH"},
    {"music", 0, 0, G_OPTION_ARG_FILENAME, &app.music_path, "MP3 to loop in the background", "PATH"},
    {"start-build", 0, G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_NONE, &start_now,
     "Testing aid: build with the imported ROM straight away", NULL},
    {"screenshot", 0, 0, G_OPTION_ARG_STRING_ARRAY, &screenshots, "Render PAGE to FILE.png and exit",
     "PAGE=FILE"},
    {NULL, 0, 0, 0, NULL, NULL, NULL},
  };
  GError *error = NULL;
  if (!gtk_init_with_args(&argc, &argv, "- Jet Force Gemini Recomp first-run launcher", entries, NULL, &error)) {
    if (!probe) g_printerr("%s\n", error ? error->message : "Cannot open a window.");
    return 2;
  }
  if (probe) return 0;
  if (!app.apprun || !app.rom_sha1) {
    g_printerr("--apprun and --rom-sha1 are required.\n");
    return 2;
  }
  app.tail = g_string_new(NULL);
  app.step = -1;
  app.result = 1;
  settings_load();
  if (app.music_path && !g_file_test(app.music_path, G_FILE_TEST_IS_REGULAR)) g_clear_pointer(&app.music_path, g_free);

  if (screenshots) {
    app.window = gtk_offscreen_window_new();
    build_ui(app.window);
    gtk_widget_show_all(app.window);
    return render_screenshots(screenshots) ? 0 : 1;
  }

  app.window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title(GTK_WINDOW(app.window), "Mabecs Jet Force Gemini Recomp");
  gtk_window_set_default_size(GTK_WINDOW(app.window), WINDOW_WIDTH, WINDOW_HEIGHT);
  gtk_window_set_position(GTK_WINDOW(app.window), GTK_WIN_POS_CENTER);
  if (app.icon_path) gtk_window_set_icon_from_file(GTK_WINDOW(app.window), app.icon_path, NULL);
  g_signal_connect(app.window, "delete-event", G_CALLBACK(on_delete), NULL);
  build_ui(app.window);
  gtk_widget_show_all(app.window);
  if (app.ready_at_start) show_page("ready");
  if (!music_start() && app.music_bar) gtk_widget_hide(app.music_bar);
  if (start_now) continue_to_build();
  gtk_main();
  return app.result;
}
