#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <pspgu.h>
#include <pspgum.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

PSP_MODULE_INFO("NarutoShippuden_HD", 0, 1, 1);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);

static unsigned int __attribute__((aligned(16))) list[262144];

int exit_callback(int arg1, int arg2, void *common) {
    sceKernelExitGame();
    return 0;
}

int CallbackThread(SceSize args, void *argp) {
    int cbid = sceKernelCreateCallback("Exit Callback", (void*)exit_callback, NULL);
    sceKernelRegisterExitCallback(cbid);
    sceKernelSleepThreadCB();
    return 0;
}

int SetupCallbacks(void) {
    int thid = sceKernelCreateThread("update_thread", CallbackThread, 0x11, 0xFA0, 0, 0);
    if (thid >= 0)
        sceKernelStartThread(thid, 0, NULL);
    return thid;
}

typedef struct {
    unsigned int color;
    short x, y, z;
} Vertex;

void drawRect(int x, int y, int w, int h, unsigned int color) {
    Vertex* vertices = (Vertex*) sceGuGetMemory(2 * sizeof(Vertex));
    vertices[0].color = color;
    vertices[0].x = x;
    vertices[0].y = y;
    vertices[0].z = 0;

    vertices[1].color = color;
    vertices[1].x = x + w;
    vertices[1].y = y + h;
    vertices[1].z = 0;

    sceGuDrawArray(GU_SPRITES, GU_COLOR_8888 | GU_VERTEX_16BIT | GU_TRANSFORM_2D, 2, 0, vertices);
}

// Estructura para proyectiles (Kunais, Shurikens y Jutsus)
typedef struct {
    int active;
    float x, y;
    int dir; 
    int type; // 0: Kunai, 1: Rasengan (Naruto), 2: Chidori (Sasuke)
    int owner; // 0: Jugador, 1: Enemigo
} Projectile;

#define MAX_PROJ 16
Projectile projectiles[MAX_PROJ];

void spawnProjectile(float x, float y, int dir, int type, int owner) {
    for (int i = 0; i < MAX_PROJ; i++) {
        if (!projectiles[i].active) {
            projectiles[i].active = 1;
            projectiles[i].x = x;
            projectiles[i].y = y;
            projectiles[i].dir = dir;
            projectiles[i].type = type;
            projectiles[i].owner = owner;
            break;
        }
    }
}

// --- DISEÑO HD DE NARUTO ---
void drawNaruto(int x, int y, int facing_right, int action_state) {
    // Estela de Dash / Movimiento rápido
    if (action_state == 3) {
        drawRect(x - (facing_right * 12), y + 10, 28, 40, 0x5500AFFF);
    }

    // Pelo rubio erizado detallado (con múltiples capas de sombreado)
    drawRect(x + 2, y - 14, 28, 16, 0xFF00D4FF);
    drawRect(x + 5, y - 18, 22, 6, 0xFF00AAEE);

    // Banda ninja azul oscura con placa metálica brillante
    drawRect(x + 3, y + 1, 24, 6, 0xFF112244);
    drawRect(x + 10, y + 2, 10, 4, 0xFFE0E0E0);

    // Rostro
    drawRect(x + 5, y + 7, 20, 12, 0xFFFFE0D0);
    drawRect(facing_right ? x + 18 : x + 7, y + 10, 3, 3, 0xFF111111); // Ojos

    // Torso (Chaqueta ninja detallada con cuello alto y cremallera)
    drawRect(x + 2, y + 19, 26, 26, 0xFF0055FF);
    drawRect(x + 7, y + 19, 16, 26, 0xFFFF4400); // Chaqueta naranja central
    drawRect(x + 12, y + 19, 4, 26, 0xFF111111); // Cremallera

    // Pantalones shinobi oscuros
    drawRect(x + 5, y + 45, 8, 16, 0xFF2C3E50);
    drawRect(x + 17, y + 45, 8, 16, 0xFF2C3E50);

    // Brazos y Acciones (Golpe, Jutsu Rasengan o Reposo)
    if (action_state == 1) { // Atacando (Golpe físico)
        int ax = facing_right ? x + 26 : x - 14;
        drawRect(ax, y + 22, 16, 8, 0xFFFFE0D0);
    } 
    else if (action_state == 2) { // Lanzando Jutsu (Rasengan
