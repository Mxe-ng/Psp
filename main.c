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

// Estructura para proyectiles (Kunais, Rasengan, Chidori)
typedef struct {
    int active;
    float x, y;
    int dir; 
    int type; // 0: Kunai, 1: Rasengan, 2: Chidori
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

// Dibujar a Naruto HD
void drawNaruto(int x, int y, int facing_right, int action_state) {
    if (action_state == 3) {
        drawRect(x - (facing_right * 12), y + 10, 28, 40, 0x5500AFFF);
    }
    // Pelo rubio
    drawRect(x + 2, y - 14, 28, 16, 0xFF00D4FF);
    drawRect(x + 5, y - 18, 22, 6, 0xFF00AAEE);
    // Banda ninja
    drawRect(x + 3, y + 1, 24, 6, 0xFF112244);
    drawRect(x + 10, y + 2, 10, 4, 0xFFE0E0E0);
    // Rostro y Torso
    drawRect(x + 5, y + 7, 20, 12, 0xFFFFE0D0);
    drawRect(x + 2, y + 19, 26, 26, 0xFF0055FF);
    drawRect(x + 7, y + 19, 16, 26, 0xFFFF4400);
    // Pantalones
    drawRect(x + 5, y + 45, 8, 16, 0xFF2C3E50);
    drawRect(x + 17, y + 45, 8, 16, 0xFF2C3E50);

    if (action_state == 1) {
        int ax = facing_right ? x + 26 : x - 14;
        drawRect(ax, y + 22, 16, 8, 0xFFFFE0D0);
    } else if (action_state == 2) {
        int rx = facing_right ? x + 30 : x - 16;
        drawRect(rx, y + 20, 16, 16, 0xFFFFDD00);
        drawRect(rx + 3, y + 23, 10, 10, 0xFFFFFFFF);
    } else {
        if (facing_right) drawRect(x + 24, y + 22, 6, 14, 0xFFFF4400);
        else drawRect(x - 2, y + 22, 6, 14, 0xFFFF4400);
    }
}

// Dibujar a Sasuke HD
void drawSasuke(int x, int y, int facing_right, int action_state) {
    // Pelo negro
    drawRect(x + 2, y - 12, 28, 14, 0xFF111111);
    drawRect(x + 1, y - 6, 6, 10, 0xFF111111);
    drawRect(x + 23, y - 6, 6, 10, 0xFF111111);
    // Rostro y Ropa
    drawRect(x + 5, y + 2, 20, 14, 0xFFFFE0D0);
    drawRect(x + 3, y + 16, 24, 25, 0xFF222222);
    drawRect(x + 10, y + 18, 10, 10, 0xFFFFFFFF);
    // Pantalones
    drawRect(x + 6, y + 41, 7, 16, 0xFF111111);
    drawRect(x + 17, y + 41, 7, 16, 0xFF111111);

    if (action_state == 2) {
        int cx = facing_right ? x + 30 : x - 16;
        drawRect(cx, y + 20, 16, 16, 0xFF00AAFF);
        drawRect(cx + 3, y + 23, 10, 10, 0xFFFFFFFF);
    } else {
        if (facing_right) drawRect(x + 24, y + 18, 6, 14, 0xFF222222);
        else drawRect(x - 2, y + 18, 6, 14, 0xFF222222);
    }
}

int main(void) {
    SetupCallbacks();

    sceGuInit();
    sceGuStart(GU_DIRECT, list);
    sceGuDrawBuffer(GU_PSM_8888, (void*)0, 512);
    sceGuDispBuffer(480, 272, (void*)0x88000, 512);
    sceGuDepthBuffer((void*)0x110000, 512);
    sceGuOffset(2048 - (480 / 2), 2048 - (272 / 2));
    sceGuViewport(2048, 2048, 480, 272);
    sceGuDepthRange(0xcf7f, 0x0f7f);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuScissor(0, 0, 480, 272);
    sceGuFinish();
    sceGuSync(0, 0);
    sceGuDisplay(1);

    int game_state = 1;
    int menu_opcion = 0;

    int p1_x = 80;
    int p1_y = 150;
    int p1_vida = 150;
    int p1_chakra = 50;
    int p1_facing = 1;
    int p1_action = 0;

    int p2_x = 360;
    int p2_y = 150;
    int p2_vida = 150;
    int p2_facing = -1;
    int p2_action = 0;
    int ai_timer = 0;

    SceCtrlData pad;
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

    while(1) {
        sceCtrlReadBufferPositive(&pad, 1);

        if (game_state == 1) {
            if (pad.Buttons & PSP_CTRL_UP || pad.Buttons & PSP_CTRL_DOWN) {
                menu_opcion = 1 - menu_opcion;
                sceKernelDelayThread(180000);
            }
            if (pad.Buttons & PSP_CTRL_CROSS) {
                game_state = 2;
                p1_vida = 150;
                p2_vida = 150;
                p1_x = 80;
                p2_x = 360;
                p1_chakra = 60;
                for(int i=0; i<MAX_PROJ; i++) projectiles[i].active = 0;
                sceKernelDelayThread(250000);
            }
        }
        else if (game_state == 2) {
            p1_action = 0;

            if (pad.Buttons & PSP_CTRL_RIGHT) {
                p1_x += 4;
                p1_facing = 1;
                if (p1_x > 430) p1_x = 430;
            }
            if (pad.Buttons & PSP_CTRL_LEFT) {
                p1_x -= 4;
                p1_facing = -1;
                if (p1_x < 10) p1_x = 10;
            }

            if (pad.Buttons & PSP_CTRL_CROSS) {
                p1_y = 90;
            } else {
                p1_y = 150;
            }

            if (pad.Buttons & PSP_CTRL_LTRIGGER) {
                if (p1_chakra < 150) p1_chakra += 3;
            }

            if (pad.Buttons & PSP_CTRL_RTRIGGER) {
                p1_action = 3;
                p1_x += (p1_facing * 22);
                if (p1_x > 430) p1_x = 430;
                if (p1_x < 10) p1_x = 10;
                sceKernelDelayThread(120000);
            }

            if (pad.Buttons & PSP_CTRL_SQUARE) {
                spawnProjectile(p1_x + (p1_facing > 0 ? 30 : -10), p1_y + 24, p1_facing, 0, 0);
                sceKernelDelayThread(160000);
            }

            if (pad.Buttons & PSP_CTRL_TRIANGLE) {
                if (p1_chakra >= 35) {
                    p1_chakra -= 35;
                    p1_action = 2;
                    spawnProjectile(p1_x + (p1_facing > 0 ? 35 : -15), p1_y + 20, p1_facing, 1, 0);
                    sceKernelDelayThread(220000);
                }
            }

            if (pad.Buttons & PSP_CTRL_CIRCLE) {
                p1_action = 1;
                if (abs(p1_x - p2_x) < 45) {
                    p2_vida -= 3;
                }
            }

            // IA Sasuke
            ai_timer++;
            p2_action = 0;
            if (p2_x > p1_x + 50) {
                p2_x -= 3;
                p2_facing = -1;
            } else if (p2_x < p1_x - 50) {
                p2_x += 3;
                p2_facing = 1;
            } else {
                if (ai_timer % 70 == 0) {
                    p2_action = 2;
                    spawnProjectile(p2_x + (p2_facing > 0 ? 35 : -15), p2_y + 20, p2_facing, 2, 1);
                } else if (ai_timer % 40 == 0) {
                    spawnProjectile(p2_x + (p2_facing > 0 ? 30 : -10), p2_y + 24, p2_facing, 0, 1);
                } else if (abs(p2_x - p1_x) < 40) {
                    p2_action = 1;
                    p1_vida -= 2;
                }
            }

            // Actualizar Proyectiles
            for (int i = 0; i < MAX_PROJ; i++) {
                if (projectiles[i].active) {
                    projectiles[i].x += (projectiles[i].dir * 7);

                    if (projectiles[i].owner == 1) {
                        if (abs((int)projectiles[i].x - p1_x) < 25) {
                            p1_vida -= (projectiles[i].type == 2 ? 30 : 12);
                            projectiles[i].active = 0;
                        }
                    } else {
                        if (abs((int)projectiles[i].x - p2_x) < 25) {
                            p2_vida -= (projectiles[i].type == 1 ? 35 : 12);
                            projectiles[i].active = 0;
                        }
                    }

                    if (projectiles[i].x < 0 || projectiles[i].x > 480) {
                        projectiles[i].active = 0;
                    }
                }
            }

            if (p2_vida <= 0 || p1_vida <= 0) {
                game_state = 1;
                sceKernelDelayThread(800000);
            }
        }

        sceGuStart(GU_DIRECT, list);
        sceGuClearColor(0xFF0A0612);
        sceGuClearDepth(0);
        sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);

        if (game_state == 1) {
            drawRect(40, 25, 400, 45, 0xFF181028);
            drawRect(45, 28, 390, 39, 0xFF2A1B4E);
            drawRect(60, 100, 360, 50, menu_opcion == 0 ? 0xFFFF5500 : 0xFF1E1E2F);
            drawRect(60, 170, 360, 50, menu_opcion == 1 ? 0xFFFF5500 : 0xFF1E1E2F);
        }
        else if (game_state == 2) {
            drawRect(0, 0, 480, 205, 0xFF120B24);
            drawRect(80, 25, 3, 3, 0xFFFFFFFF);
            drawRect(200, 45, 2, 2, 0xFFDDFFFF);
            drawRect(360, 30, 3, 3, 0xFFFFFFFF);
            
            drawRect(0, 205, 480, 67, 0xFF1A252F);
            drawRect(0, 205, 480, 8, 0xFF27AE60);
            drawRect(0, 213, 480, 4, 0xFF1E8449);

            drawRect(20, 15, 160, 16, 0xFF000000);
            drawRect(22, 17, p1_vida > 0 ? p1_vida : 0, 12, 0xFF2ECC71);
            drawRect(20, 34, 160, 12, 0xFF000000);
            drawRect(22, 36, p1_chakra > 150 ? 150 : p1_chakra, 8, 0xFFE67E22);

            drawRect(300, 15, 160, 16, 0xFF000000);
            drawRect(302, 17, p2_vida > 0 ? p2_vida : 0, 12, 0xFFE74C3C);

            drawNaruto(p1_x, p1_y, p1_facing, p1_action);
            drawSasuke(p2_x, p2_y, p2_facing, p2_action);

            for (int i = 0; i < MAX_PROJ; i++) {
                if (projectiles[i].active) {
                    if (projectiles[i].type == 0) {
                        drawRect((int)projectiles[i].x, (int)projectiles[i].y, 14, 4, 0xFFCCCCCC);
                    } else if (projectiles[i].type == 1) {
                        drawRect((int)projectiles[i].x, (int)projectiles[i].y, 16, 16, 0xFFFFCC00);
                    } else if (projectiles[i].type == 2) {
                        drawRect((int)projectiles[i].x, (int)projectiles[i].y, 16, 16, 0xFF00AAFF);
                    }
                }
            }
        }

        sceGuFinish();
        sceGuSync(0, 0);

        sceDisplayWaitVblankStart();
        sceGuSwapBuffers();
    }

    sceGuTerm();
    return 0;
}
