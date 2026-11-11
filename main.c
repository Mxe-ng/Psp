#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <pspgu.h>
#include <pspgum.h>
#include <stdio.h>
#include <malloc.h>

PSP_MODULE_INFO("NarutoBattle_PSP", 0, 1, 1);
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

int main(void) {
    SetupCallbacks();

    // Inicialización segura del entorno gráfico GU
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

    int p1_x = 60;
    int p2_x = 350;
    int p1_vida = 150;
    int velocidad = 3;

    SceCtrlData pad;
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

    while(1) {
        sceCtrlReadBufferPositive(&pad, 1);

        if (pad.Buttons & PSP_CTRL_RIGHT) {
            p1_x += velocidad;
            if (p1_x > 380) p1_x = 380;
        }
        if (pad.Buttons & PSP_CTRL_LEFT) {
            p1_x -= velocidad;
            if (p1_x < 10) p1_x = 10;
        }

        if (pad.Buttons & PSP_CTRL_CIRCLE) {
            if (p1_x + 40 >= p2_x) {
                if (p1_vida > 0) p1_vida -= 1;
            }
        }

        // Renderizado de pantalla estable
        sceGuStart(GU_DIRECT, list);
        sceGuClearColor(0xFF101018);
        sceGuClearDepth(0);
        sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);

        // Suelo
        drawRect(0, 210, 480, 62, 0xFF225588);

        // Barras de vida (HUD estilo Storm)
        drawRect(20, 15, 160, 16, 0xFF000000);
        drawRect(22, 17, p1_vida, 12, 0xFF00FF00);

        drawRect(300, 15, 160, 16, 0xFF000000);
        drawRect(302, 17, 140, 12, 0xFF0000FF);

        // Personajes (P1 azul, P2 rojo)
        drawRect(p1_x, 155, 32, 55, 0xFF0077FF); 
        drawRect(p2_x, 155, 32, 55, 0xFF2222CC); 

        sceGuFinish();
        sceGuSync(0, 0);

        sceDisplayWaitVblankStart();
        sceGuSwapBuffers();
    }

    sceGuTerm();
    return 0;
}
