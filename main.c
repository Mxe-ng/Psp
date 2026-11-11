#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <pspgu.h>
#include <pspgum.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

PSP_MODULE_INFO("NarutoShippuden_PSP", 0, 1, 1);
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

// Estructura para proyectiles (Kunais y Rasengan)
typedef struct {
    int active;
    float x, y;
    int dir; // 1: derecha, -1: izquierda
    int type; // 0: Kunai, 1: Rasengan
} Projectile;

#define MAX_PROJ 10
Projectile projectiles[MAX_PROJ];

void spawnProjectile(float x, float y, int dir, int type) {
    for (int i = 0; i < MAX_PROJ; i++) {
        if (!projectiles[i].active) {
            projectiles[i].active = 1;
            projectiles[i].x = x;
            projectiles[i].y = y;
            projectiles[i].dir = dir;
            projectiles[i].type = type;
            break;
        }
    }
}

// Dibujar a Naruto detallado (Pelo rubio, banda ninja, chaqueta naranja)
void drawNaruto(int x, int y, int facing_right, int is_attacking, int is_jutsu) {
    // Pelo rubio erizado
    drawRect(x + 3, y - 12, 26, 14, 0xFF00EEFF); 
    // Banda ninja con placa metálica
    drawRect(x + 4, y + 1, 22, 6, 0xFFFF6600);
    drawRect(x + 11, y + 2, 8, 4, 0xFFCCCCCC);
    // Cara
    drawRect(x + 5, y + 7, 20, 12, 0xFFD4EEFF); 

    // Torso (Chaqueta naranja con detalles)
    drawRect(x + 2, y + 19, 26, 26, 0xFF0066FF); 
    drawRect(x + 7, y + 20, 16, 20, 0xFFFF5500); 

    // Piernas (Pantalón shinobi)
    drawRect(x + 5, y + 45, 8, 16, 0xFF222222);
    drawRect(x + 17, y + 45, 8, 16, 0xFF222222);

    // Brazos y efectos de ataque / Jutsu (Rasengan)
    if (is_jutsu) {
        int rx = facing_right ? x + 30 : x - 15;
        drawRect(rx, y + 22, 14, 14, 0xFFFFDD00);
        drawRect(rx + 2, y + 24, 10, 10, 0xFFFFFFFF);
    } else if (is_attacking) {
        if (facing_right) {
            drawRect(x + 26, y + 22, 18, 8, 0xFFD4EEFF);
        } else {
            drawRect(x - 14, y + 22, 18, 8, 0xFFD4EEFF);
        }
    } else {
        if (facing_right) {
            drawRect(x + 24, y + 22, 6, 14, 0xFFFF5500);
        } else {
            drawRect(x - 2, y + 22, 6, 14, 0xFFFF5500);
        }
    }
}

// Dibujar a Sasuke detallado (Pelo negro, ropa tradicional oscura)
void drawSasuke(int x, int y, int facing_right) {
    // Pelo negro erizado
    drawRect(x + 2, y - 10, 26, 12, 0xFF111111);
    // Cara
    drawRect(x + 5, y + 2, 20, 14, 0xFFD4EEFF);
    // Banda o frente
    drawRect(x + 6, y + 3, 18, 4, 0xFF222222);

    // Torso (Ropa tradicional oscura de Sasuke)
    drawRect(x + 3, y + 16, 24, 25, 0xFF332211);
    drawRect(x + 10, y + 16, 10, 25, 0xFF112244);

    // Pantalones
    drawRect(x + 6, y + 41, 7, 16, 0xFF111111);
    drawRect(x + 17, y + 41, 7, 16, 0xFF111111);

    // Brazos
    if (facing_right) {
        drawRect(x + 24, y + 18, 6, 14, 0xFF332211);
    } else {
        drawRect(x - 2, y + 18, 6, 14, 0xFF332211);
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

    int game_state = 1; // 1: Menú Principal, 2: Combate
    int menu_opcion = 0; // 0: Modo Historia, 1: Batalla Libre

    int p1_x = 80;
    int p1_y = 150;
    int p1_vida = 150;
    int p1_chakra = 50;
    int p1_facing = 1;

    int p2_x = 360;
    int p2_y = 150;
    int p2_vida = 150;
    int p2_facing = -1;
    int p2_timer = 0;

    SceCtrlData pad;
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

    while(1) {
        sceCtrlReadBufferPositive(&pad, 1);

        if (game_state == 1) {
            // Menú Principal
            if (pad.Buttons & PSP_CTRL_UP || pad.Buttons & PSP_CTRL_DOWN) {
                menu_opcion = 1 - menu_opcion;
                sceKernelDelayThread(150000);
            }
            if (pad.Buttons & PSP_CTRL_CROSS) {
                game_state = 2;
                p1_vida = 150;
                p2_vida = 150;
                p1_x = 80;
                p2_x = 360;
                for(int i=0; i<MAX_PROJ; i++) projectiles[i].active = 0;
                sceKernelDelayThread(200000);
            }
        }
        else if (game_state == 2) {
            // Movimiento Jugador 1
            if (pad.Buttons & PSP_CTRL_RIGHT) {
                p1_x += 3;
                p1_facing = 1;
                if (p1_x > 420) p1_x = 420;
            }
            if (pad.Buttons & PSP_CTRL_LEFT) {
                p1_x -= 3;
                p1_facing = -1;
                if (p1_x < 10) p1_x = 10;
            }

            // Salto
            if (pad.Buttons & PSP_CTRL_CROSS) {
                p1_y = 95;
            } else {
                p1_y = 150;
            }

            // Recargar Chakra (Botón L)
            if (pad.Buttons & PSP_CTRL_LTRIGGER) {
                if (p1_chakra < 150) p1_chakra += 2;
            }

            // Dash Direccional (Botón R - Va hacia donde mira el personaje)
            if (pad.Buttons & PSP_CTRL_RTRIGGER) {
                p1_x += (p1_facing * 16);
                if (p1_x > 420) p1_x = 420;
                if (p1_x < 10) p1_x = 10;
                sceKernelDelayThread(100000);
            }

            // Lanzar Kunai (Cuadrado)
            if (pad.Buttons & PSP_CTRL_SQUARE) {
                spawnProjectile(p1_x + (p1_facing > 0 ? 30 : -10), p1_y + 25, p1_facing, 0);
                sceKernelDelayThread(150000);
            }

            // Jutsu Rasengan (Triángulo - Gasta Chakra)
            if (pad.Buttons & PSP_CTRL_TRIANGLE) {
                if (p1_chakra >= 30) {
                    p1_chakra -= 30;
                    spawnProjectile(p1_x + (p1_facing > 0 ? 35 : -15), p1_y + 20, p1_facing, 1);
                    sceKernelDelayThread(200000);
                }
            }

            // Ataque Físico (Círculo)
            if (pad.Buttons & PSP_CTRL_CIRCLE) {
                if (abs(p1_x - p2_x) < 45) {
                    p2_vida -= 2;
                }
            }

            // Inteligencia Artificial de Sasuke (Enemigo se mueve y ataca)
            p2_timer++;
            if (p2_x > p1_x + 35) {
                p2_x -= 2;
                p2_facing = -1;
            } else if (p2_x < p1_x - 35) {
                p2_x += 2;
                p2_facing = 1;
            } else {
                if (p2_timer % 45 == 0) {
                    p1_vida -= 5;
                }
            }

            // Actualizar Proyectiles
            for (int i = 0; i < MAX_PROJ; i++) {
                if (projectiles[i].active) {
                    projectiles[i].x += (projectiles[i].dir * 6);
                    if (projectiles[i].type == 0 && abs((int)projectiles[i].x - p2_x) < 25) {
                        p2_vida -= 15;
                        projectiles[i].active = 0;
                    }
                    if (projectiles[i].type == 1 && abs((int)projectiles[i].x - p2_x) < 35) {
                        p2_vida -= 40;
                        projectiles[i].active = 0;
                    }
                    if (projectiles[i].x < 0 || projectiles[i].x > 480) {
                        projectiles[i].active = 0;
                    }
                }
            }

            // Fin de la pelea
            if (p2_vida <= 0 || p1_vida <= 0) {
                game_state = 1;
                sceKernelDelayThread(800000);
            }
        }

        // --- RENDERIZADO GRÁFICO ---
        sceGuStart(GU_DIRECT, list);
        sceGuClearColor(0xFF0F0B1E); // Cielo nocturno
        sceGuClearDepth(0);
        sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);

        if (game_state == 1) {
            // Menú Principal
            drawRect(40, 25, 400, 35, 0xFF111122);
            drawRect(60, 90, 360, 50, menu_opcion == 0 ? 0xFFFF6600 : 0xFF222233);
            drawRect(60, 160, 360, 50, menu_opcion == 1 ? 0xFFFF6600 : 0xFF222233);
        }
        else if (game_state == 2) {
            // Fondo y Estrellas
            drawRect(0, 0, 480, 205, 0xFF140F26);
            drawRect(100, 30, 3, 3, 0xFFFFFFFF);
            drawRect(280, 50, 2, 2, 0xFFFFFFFF);
            drawRect(380, 25, 3, 3, 0xFFFFFFFF);

            // Suelo detallado con césped y piedra
            drawRect(0, 205, 480, 67, 0xFF2C3E50); 
            drawRect(0, 205, 480, 6, 0xFF27AE60);  
            drawRect(0, 211, 480, 3, 0xFF1E8449);  

            // HUD / Barras de Vida y Chakra
            drawRect(20, 15, 160, 16, 0xFF000000);
            drawRect(22, 17, p1_vida > 0 ? p1_vida : 0, 12, 0xFF2ECC71);

            drawRect(20, 34, 160, 12, 0xFF000000);
            drawRect(22, 36, p1_chakra > 150 ? 150 : p1_chakra, 8, 0xFFE67E22);

            drawRect(300, 15, 160, 16, 0xFF000000);
            drawRect(302, 17, p2_vida > 0 ? p2_vida : 0, 12, 0xFFE74C3C);

            // Dibujar Personajes
            drawNaruto(p1_x, p1_y, p1_facing, pad.Buttons & PSP_CTRL_CIRCLE, pad.Buttons & PSP_CTRL_TRIANGLE);
            drawSasuke(p2_x, p2_y, p2_facing);

            // Dibujar Proyectiles (Kunais y Rasengan)
            for (int i = 0; i < MAX_PROJ; i++) {
                if (projectiles[i].active) {
                    if (projectiles[i].type == 0) {
                        drawRect((int)projectiles[i].x, (int)projectiles[i].y, 12, 4, 0xFFCCCCCC);
                    } else {
                        drawRect((int)projectiles[i].x, (int)projectiles[i].y, 16, 16, 0xFFFFCC00);
                        drawRect((int)projectiles[i].x + 3, (int)projectiles[i].y + 3, 10, 10, 0xFFFFFFFF);
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
