#include <pspkernel.h>
#include <pspdebug.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <stdlib.h>

PSP_MODULE_INFO("Juego2DPSP", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

/* Callback para salir del juego correctamente desde el menú */
int exit_callback(int arg1, int arg2, void *common) {
    sceKernelExitGame();
    return 0;
}

int CallbackThread(SceSize args, void *argp) {
    int cbid = sceKernelCreateCallback("Exit Callback", exit_callback, NULL);
    sceKernelRegisterExitCallback(cbid);
    sceKernelSleepThreadCB();
    return 0;
}

void setupCallbacks(void) {
    int thid = sceKernelCreateThread("update_thread", CallbackThread, 0x11, 0xFA0, 0, 0);
    if (thid >= 0) {
        sceKernelStartThread(thid, 0, 0);
    }
}

int main(void) {
    pspDebugScreenInit();
    setupCallbacks();

    // Configurar lectura de controles digitales (cruceta y botones)
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_DIGITAL);
    SceCtrlData pad;

    // Variables del jugador y obstáculo
    int player_x = 240; // Centro de la pantalla de la PSP (480x272)
    int player_y = 230;
    
    int obstacle_x = rand() % 440;
    int obstacle_y = 0;
    int obstacle_speed = 3;

    int score = 0;
    int game_over = 0;

    while (1) {
        sceCtrlReadBufferPositive(&pad, 1);

        if (!game_over) {
            // Movimiento del jugador con la cruceta izquierda / derecha
            if (pad.Buttons & PSP_CTRL_LEFT)  player_x -= 6;
            if (pad.Buttons & PSP_CTRL_RIGHT) player_x += 6;

            // Limitar al jugador dentro de los bordes de la pantalla
            if (player_x < 10)  player_x = 10;
            if (player_x > 450) player_x = 450;

            // Mover el obstáculo hacia abajo
            obstacle_y += obstacle_speed;

            // Si el obstáculo llega al fondo, reaparece arriba y suma puntos
            if (obstacle_y > 250) {
                obstacle_y = 0;
                obstacle_x = rand() % 440;
                score += 10;
                // Aumentar dificultad gradualmente
                if (score % 50 == 0) obstacle_speed++;
            }

            // Detección de colisión simple (Caja de impacto)
            if (obstacle_y >= 210 && obstacle_y <= 235) {
                if (player_x >= obstacle_x - 15 && player_x <= obstacle_x + 25) {
                    game_over = 1; // ¡Colisión!
                }
            }
        } else {
            // Reiniciar juego con el botón START
            if (pad.Buttons & PSP_CTRL_START) {
                player_x = 240;
                obstacle_y = 0;
                score = 0;
                obstacle_speed = 3;
                game_over = 0;
            }
        }

        // --- RENDERIZADO EN PANTALLA ---
        pspDebugScreenClear();

        // Interfaz superior
        pspDebugScreenSetXY(2, 1);
        pspDebugScreenPrintf("=== MINI JUEGO PSP ===");
        pspDebugScreenSetXY(35, 1);
        pspDebugScreenPrintf("PUNTOS: %d", score);

        if (!game_over) {
            // Dibujar obstáculo (enemigo)
            pspDebugScreenSetXY(obstacle_x / 8, obstacle_y / 8);
            pspDebugScreenPrintf("[ X ]");

            // Dibujar jugador
            pspDebugScreenSetXY(player_x / 8, player_y / 8);
            pspDebugScreenPrintf("  A  ");
        } else {
            // Pantalla de Game Over
            pspDebugScreenSetXY(18, 12);
            pspDebugScreenPrintf("¡GAME OVER!");
            pspDebugScreenSetXY(14, 14);
            pspDebugScreenPrintf("Pulsa START para reiniciar");
        }

        // Sincronizar a 60 FPS estables usando el refresco vertical de la PSP
        sceDisplayWaitVblankStart();
    }

    return 0;
}
