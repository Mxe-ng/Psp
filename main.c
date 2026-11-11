#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <pspgu.h>
#include <pspgum.h>
#include <stdio.h>
#include <malloc.h>

PSP_MODULE_INFO("NarutoStorm_PSP", 0, 1, 1);
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

// Dibujar a Naruto con cuerpo detallado (Pelo rubio, banda, chaqueta naranja)
void drawNaruto(int x, int y, int facing_right, int is_attacking) {
    // Pelo rubio erizado
    drawRect(x + 4, y - 10, 22, 12, 0xFF00DDFF); 
    // Cara
    drawRect(x + 6, y + 2, 18, 14, 0xFFCCEEFF); 
    // Banda ninja en la frente
    drawRect(x + 5, y + 4, 20, 5, 0xFF333333); 
    drawRect(x + 12, y + 5, 6, 3, 0xFFFFFFFF); 

    // Torso (Chaqueta naranja)
    drawRect(x + 2, y + 16, 26, 24, 0xFF0066FF); 
    drawRect(x + 2, y + 20, 5, 16, 0xFFFF0000); 
    drawRect(x + 23, y + 20, 5, 16, 0xFFFF0000);

    // Piernas
    drawRect(x + 6, y + 40, 7, 18, 0xFF222222);
    drawRect(x + 17, y + 40, 7, 18, 0xFF222222);

    // Brazos / Ataque
    if (is_attacking) {
        if (facing_right) {
            drawRect(x + 24, y + 20, 16, 8, 0xFFCCEEFF); 
        } else {
            drawRect(x - 10, y + 20, 16, 8, 0xFFCCEEFF);
        }
    } else {
        drawRect(x - 2, y + 20, 6, 14, 0xFFCCEEFF);
        drawRect(x + 26, y + 20, 6, 14, 0xFFCCEEFF);
    }
}

// Dibujar a Sasuke con cuerpo detallado (Pelo negro, ropa oscura)
void drawSasuke(int x, int y) {
    // Pelo negro erizado
    drawRect(x + 4, y - 8, 22, 10, 0xFF111111);
    // Cara
    drawRect(x + 6, y + 2, 18, 14, 0xFFCCEEFF);
    // Torso (Ropa oscura de Sasuke)
    drawRect(x + 2, y + 16, 26, 24, 0xFF332222); 
    // Piernas
    drawRect(x + 6, y + 40, 7, 18, 0xFF111111);
    drawRect(x + 17, y + 40, 7, 18, 0xFF111111);
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

    int game_state = 1; // 1: Menú de Selección / Historia, 2: Batalla en curso
    int menu_seleccion = 0; // 0: Misión Modo Historia, 1: Batalla Libre

    int p1_x = 80;
    int p1_y = 150;
    int p1_vida = 140;
    int p1_chakra = 50;

    int p2_x = 360;
    int p2_y = 150;
    int p2_vida = 140;

    SceCtrlData pad;
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

    while(1) {
        sceCtrlReadBufferPositive(&pad, 1);

        if (game_state == 1) {
            // Pantalla de Menú / Selección de Modo Historia
            if (pad.Buttons & PSP_CTRL_UP || pad.Buttons & PSP_CTRL_DOWN) {
                menu_seleccion = 1 - menu_seleccion;
                sceKernelDelayThread(150000);
            }
            if (pad.Buttons & PSP_CTRL_CROSS) {
                game_state = 2; // Iniciar Combate
                p1_vida = 140;
                p2_vida = 140;
                p1_x = 80;
                p2_x = 360;
                sceKernelDelayThread(200000);
            }
        }
        else if (game_state == 2) {
            // Lógica de Combate
            if (pad.Buttons & PSP_CTRL_RIGHT) {
                p1_x += 3;
                if (p1_x > 400) p1_x = 400;
            }
            if (pad.Buttons & PSP_CTRL_LEFT) {
                p1_x -= 3;
                if (p1_x < 10) p1_x = 10;
            }

            if (pad.Buttons & PSP_CTRL_CROSS) {
                p1_y = 100; // Salto
            } else {
                p1_y = 150;
            }

            if (pad.Buttons & PSP_CTRL_LTRIGGER) {
                if (p1_chakra < 140) p1_chakra += 2; // Recargar Chakra
            }

            if (pad.Buttons & PSP_CTRL_RTRIGGER) {
                p1_x += 12; // Dash rápido
                if (p1_x > 400) p1_x = 400;
            }

            int atacando = 0;
            if (pad.Buttons & PSP_CTRL_CIRCLE) {
                atacando = 1;
                if (p1_x + 40 >= p2_x) {
                    if (p2_vida > 0) p2_vida -= 2;
                }
            }

            if (p2_vida <= 0) {
                game_state = 1; // Volver al menú al ganar la misión
                sceKernelDelayThread(500000);
            }
        }

        // Renderizado Gráfico
        sceGuStart(GU_DIRECT, list);
        sceGuClearColor(0xFF101018);
        sceGuClearDepth(0);
        sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);

        if (game_state == 1) {
            // Dibujar interfaz de Selección / Modo Historia
            drawRect(50, 40, 380, 50, menu_seleccion == 0 ? 0xFF0055AA : 0xFF222222); // Misión Historia
            drawRect(50, 110, 380, 50, menu_seleccion == 1 ? 0xFF0055AA : 0xFF222222); // Batalla Libre VS
        }
        else if (game_state == 2) {
            // Escenario de combate
            drawRect(0, 205, 480, 67, 0xFF1A334D); // Suelo

            // HUD de Estado (Vidas y Chakra)
            drawRect(20, 15, 140, 14, 0xFF000000);
            drawRect(22, 17, p1_vida, 10, 0xFF00FF00); 
            drawRect(20, 32, 140, 10, 0xFF000000);
            drawRect(22, 34, p1_chakra, 6, 0xFFFF8800); // Chakra

            drawRect(320, 15, 140, 14, 0xFF000000);
            drawRect(322, 17, p2_vida, 10, 0xFF0000FF); // Rival

            // Dibujar a Naruto y Sasuke con cuerpos detallados
            drawNaruto(p1_x, p1_y, 1, pad.Buttons & PSP_CTRL_CIRCLE);
            drawSasuke(p2_x, p2_y);
        }

        sceGuFinish();
        sceGuSync(0, 0);

        sceDisplayWaitVblankStart();
        sceGuSwapBuffers();
    }

    sceGuTerm();
    return 0;
}
