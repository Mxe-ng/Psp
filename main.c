/*
 * PSPTeca - Biblioteca de series / animes / videos para PSP (homebrew)
 *
 * Estructura en el Memory Stick:
 *   ms0:/VIDEOS/<Serie o Anime>/<episodio>.pmp|mp4|m4v|avi
 *
 * Controles:
 *   Arriba/Abajo : mover      X : entrar / reproducir
 *   O : volver                START : salir
 *
 * Marca con ">" el ultimo episodio visto de cada serie.
 * La reproduccion se delega a un reproductor instalado (PMPlayer por defecto).
 */
#include <pspkernel.h>
#include <pspdebug.h>
#include <pspctrl.h>
#include <pspdisplay.h>
#include <psploadexec.h>
#include <pspiofilemgt.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>

PSP_MODULE_INFO("PSPTeca", PSP_MODULE_USER, 1, 0);
PSP_MAIN_THREAD_ATTR(PSP_THREAD_ATTR_USER);

#define ROOT_DIR     "ms0:/VIDEOS"
#define PLAYER_EBOOT "ms0:/PSP/GAME/PMPLAYER/EBOOT.PBP"
#define MAX_ENTRIES  512
#define ROWS         26

typedef struct {
    char name[128];
} Entry;

static Entry entries[MAX_ENTRIES];
static int   count = 0;
static char  currentSeries[128] = "";   /* vacio = pantalla de series */
static char  lastWatched[128]   = "";

/* ---------- salida segura con HOME ---------- */
static int exit_callback(int a, int b, void *c) { sceKernelExitGame(); return 0; }
static int callback_thread(SceSize args, void *argp) {
    int cb = sceKernelCreateCallback("exit", exit_callback, NULL);
    sceKernelRegisterExitCallback(cb);
    sceKernelSleepThreadCB();
    return 0;
}
static void setup_callbacks(void) {
    int t = sceKernelCreateThread("cb", callback_thread, 0x11, 0xFA0, 0, NULL);
    if (t >= 0) sceKernelStartThread(t, 0, NULL);
}

/* ---------- utilidades ---------- */
static int is_video(const char *n) {
    const char *e = strrchr(n, '.');
    if (!e) return 0;
    return !strcasecmp(e, ".pmp") || !strcasecmp(e, ".mp4") ||
           !strcasecmp(e, ".m4v") || !strcasecmp(e, ".avi");
}

static int cmp(const void *a, const void *b) {
    return strcasecmp(((const Entry *)a)->name, ((const Entry *)b)->name);
}

static void load_dir(const char *path, int wantDirs) {
    SceUID d = sceIoDopen(path);
    SceIoDirent ent;
    count = 0;
    if (d < 0) return;
    memset(&ent, 0, sizeof(ent));
    while (sceIoDread(d, &ent) > 0 && count < MAX_ENTRIES) {
        int isDir = FIO_S_ISDIR(ent.d_stat.st_mode);
        if (ent.d_name[0] != '.' &&
            ((wantDirs && isDir) || (!wantDirs && !isDir && is_video(ent.d_name)))) {
            strncpy(entries[count].name, ent.d_name, 127);
            entries[count].name[127] = 0;
            count++;
        }
        memset(&ent, 0, sizeof(ent));
    }
    sceIoDclose(d);
    qsort(entries, count, sizeof(Entry), cmp);
}

/* ---------- progreso: archivo .ultimo dentro de cada serie ---------- */
static void progress_path(const char *series, char *out) {
    sprintf(out, "%s/%s/.ultimo", ROOT_DIR, series);
}

static void read_last(const char *series) {
    char p[300];
    SceUID f;
    lastWatched[0] = 0;
    progress_path(series, p);
    f = sceIoOpen(p, PSP_O_RDONLY, 0777);
    if (f >= 0) {
        int n = sceIoRead(f, lastWatched, 127);
        if (n < 0) n = 0;
        lastWatched[n] = 0;
        sceIoClose(f);
    }
}

static void save_last(const char *series, const char *ep) {
    char p[300];
    SceUID f;
    progress_path(series, p);
    f = sceIoOpen(p, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
    if (f >= 0) {
        sceIoWrite(f, ep, strlen(ep));
        sceIoClose(f);
    }
}

/* ---------- reproducir: delega en un reproductor instalado ---------- */
static void play(const char *series, const char *ep) {
    char path[300];
    struct SceKernelLoadExecParam p;
    sprintf(path, "%s/%s/%s", ROOT_DIR, series, ep);
    save_last(series, ep);

    memset(&p, 0, sizeof(p));
    p.size = sizeof(p);
    p.args = strlen(path) + 1;
    p.argp = path;
    p.key  = NULL;
    sceKernelLoadExec(PLAYER_EBOOT, &p);
}

/* ---------- interfaz ---------- */
static void draw(int sel, int top) {
    int i;
    pspDebugScreenSetXY(0, 0);
    pspDebugScreenPrintf("=== PSPTeca ===  %s\n",
        currentSeries[0] ? currentSeries : "Series y Animes");
    pspDebugScreenPrintf("X: abrir  O: atras  START: salir\n");
    pspDebugScreenPrintf("--------------------------------------------\n");

    if (count == 0) {
        pspDebugScreenPrintf("\n (vacio) Coloca tus videos en %s/<Serie>/\n", ROOT_DIR);
    }
    for (i = top; i < top + ROWS && i < count; i++) {
        int isLast = currentSeries[0] && lastWatched[0] &&
                     !strcmp(entries[i].name, lastWatched);
        pspDebugScreenPrintf("%s%s %-55.55s\n",
            i == sel ? ">>" : "  ",
            isLast ? "*" : " ",
            entries[i].name);
    }
    for (i = (count - top < ROWS ? count - top : ROWS); i < ROWS; i++)
        pspDebugScreenPrintf("%70s\n", "");
}

int main(void) {
    SceCtrlData pad;
    unsigned int old = 0;
    int sel = 0, top = 0;

    setup_callbacks();
    pspDebugScreenInit();
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

    sceIoMkdir(ROOT_DIR, 0777);
    load_dir(ROOT_DIR, 1);

    while (1) {
        unsigned int pressed;
        sceCtrlReadBufferPositive(&pad, 1);
        pressed = pad.Buttons & ~old;
        old = pad.Buttons;

        if (pressed & PSP_CTRL_DOWN && sel < count - 1) sel++;
        if (pressed & PSP_CTRL_UP   && sel > 0)         sel--;
        if (sel < top) top = sel;
        if (sel >= top + ROWS) top = sel - ROWS + 1;

        if (pressed & PSP_CTRL_CROSS && count > 0) {
            if (!currentSeries[0]) {
                /* entrar a una serie */
                strcpy(currentSeries, entries[sel].name);
                read_last(currentSeries);
                {
                    char p[300];
                    sprintf(p, "%s/%s", ROOT_DIR, currentSeries);
                    load_dir(p, 0);
                }
                sel = 0; top = 0;
                /* posicionar en el ultimo visto */
                if (lastWatched[0]) {
                    int i;
                    for (i = 0; i < count; i++)
                        if (!strcmp(entries[i].name, lastWatched)) {
                            sel = i;
                            top = i >= ROWS ? i - ROWS + 1 : 0;
                            break;
                        }
                }
            } else {
                play(currentSeries, entries[sel].name);
            }
        }

        if (pressed & PSP_CTRL_CIRCLE && currentSeries[0]) {
            currentSeries[0] = 0;
            load_dir(ROOT_DIR, 1);
            sel = 0; top = 0;
        }

        if (pressed & PSP_CTRL_START) break;

        sceDisplayWaitVblankStart();
        draw(sel, top);
    }

    sceKernelExitGame();
    return 0;
}
