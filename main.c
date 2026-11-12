// "NINJA TORMENTA" - juego de peleas ninja ORIGINAL para PSP (homebrew, PSPSDK)
// Personajes, musica y efectos originales. Optimizado: CPU a 333 MHz, fondo pre-renderizado,
// pools fijos sin malloc, sin alpha por pixel en las rutas calientes.
//
// Controles:  Cruz/Stick = mover (arriba o X = saltar)   CUADRADO = golpe (combo x3)
//             TRIANGULO = kunai (8 chakra)   CIRCULO = esquive / SUSTITUCION al ser golpeado (25 chakra)
//             L = cargar chakra   R = guardia   CUADRADO+TRIANGULO con chakra 100 = TECNICA FINAL
#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <psprtc.h>
#include <pspaudio.h>
#include <psppower.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>

PSP_MODULE_INFO("NinjaTormenta", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

#define SW 480
#define SH 272
#define STRIDE 512
#define PI_F 3.14159265f
#define RGB(r,g,b) (0xFF000000u | ((unsigned)(b) << 16) | ((unsigned)(g) << 8) | (unsigned)(r))

#include "audio.inc"   // sintetizador: SFX_HIT/SELECT/CONFIRM/SPARE/LOSE/TALK/BLIP + 3 musicas
#include "gfx.inc"     // canvas, rect, circle, fuente 5x7, text, textc...

/* ===================================================================== */
/*  CONSTANTES Y TIPOS                                                   */
/* ===================================================================== */
#define GY 214               // suelo
#define MAXPJ 12
#define MAXPT 96
#define ROUND_FRAMES (60 * 60)

enum { PJ_KUNAI = 1, PJ_ORB = 2 };
enum { ST_TITLE, ST_INTRO, ST_FIGHT, ST_RDEND, ST_MATCH };

typedef struct { float x, y, vx; int kind, on, owner, t; } Pj;
typedef struct { float x, y, vx, vy; int life, max, kind; u32 col; } Pt;

typedef struct {
    int l, r, jump, atk, kun, dash, guard, charge, ult;   // l,r,guard,charge = mantenido; resto = pulsacion
} In;

typedef struct {
    float x, y, vx, vy, ch;
    int hp, dir, atk, atkn, chain, hurt, dash, dcd, ground, guard, charging;
    int subw, lastd, ult, hitdone, kcd, wins, ko, ai_t, ai_a, glow;
    float logx; int logt;                    // tronco de sustitucion
    u32 col, col2; const char *name;
} F;

static Pj pj[MAXPJ];
static Pt pt[MAXPT];
static u32 bg[STRIDE * SH] __attribute__((aligned(64)));
static float stars_x[40], stars_y[40];
static float sintab[256];
static int shake = 0, flashv = 0, hitstop = 0;

static inline float fsin(float a) { return sintab[((int)(a * 40.7437f)) & 255]; }   // 256/(2*PI)

/* ===================================================================== */
/*  PARTICULAS                                                           */
/* ===================================================================== */
static void part(float x, float y, float vx, float vy, int life, int kind, u32 col) {
    for (int i = 0; i < MAXPT; i++) if (pt[i].life <= 0) {
        pt[i] = (Pt){x, y, vx, vy, life, life, kind, col};
        return;
    }
}
static void sparks(float x, float y, int n, u32 col) {
    for (int i = 0; i < n; i++) {
        float a = (rand() % 628) / 100.0f, s = 1.0f + (rand() % 300) / 100.0f;
        part(x, y, cosf(a) * s, sinf(a) * s - 0.5f, 10 + rand() % 10, 0, col);
    }
}
static void smoke(float x, float y, int n) {
    for (int i = 0; i < n; i++)
        part(x + (rand() % 20 - 10), y - (rand() % 40), (rand() % 100 - 50) / 100.0f, -0.4f - (rand() % 60) / 100.0f,
             22 + rand() % 14, 1, RGB(210, 210, 225));
}
static void update_parts(void) {
    for (int i = 0; i < MAXPT; i++) if (pt[i].life > 0) {
        pt[i].x += pt[i].vx; pt[i].y += pt[i].vy;
        if (pt[i].kind == 0) pt[i].vy += 0.12f;
        pt[i].life--;
    }
}
static void draw_parts(void) {
    for (int i = 0; i < MAXPT; i++) if (pt[i].life > 0) {
        if (pt[i].kind == 0) rect((int)pt[i].x, (int)pt[i].y, 3, 3, pt[i].col);
        else {
            int r = 4 + (pt[i].max - pt[i].life) / 3;
            circleA((int)pt[i].x, (int)pt[i].y, r, pt[i].col, 20 + 120 * pt[i].life / pt[i].max);
        }
    }
}

/* ===================================================================== */
/*  ESCENARIO (se dibuja UNA vez a bg[] y luego solo memcpy)             */
/* ===================================================================== */
static void render_bg(void) {
    for (int y = 0; y < SH; y++) {
        float t = y / (float)GY; if (t > 1) t = 1;
        u32 c = RGB(10 + (int)(40 * t), 8 + (int)(18 * t), 30 + (int)(70 * t));
        u32 *p = &canvas[y * STRIDE];
        for (int x = 0; x < SW; x++) p[x] = c;
    }
    for (int i = 0; i < 40; i++) {
        int sz = (i % 5 == 0) ? 2 : 1;
        rect((int)stars_x[i], (int)stars_y[i], sz, sz, RGB(210, 220, 255));
    }
    // luna
    circleA(380, 62, 46, RGB(200, 210, 255), 22);
    circleA(380, 62, 34, RGB(220, 225, 255), 40);
    circle(380, 62, 26, RGB(238, 240, 255));
    circleA(372, 56, 6, RGB(205, 210, 235), 120);
    circleA(390, 70, 4, RGB(205, 210, 235), 120);
    // montanas lejanas y cercanas
    for (int x = 0; x < SW; x++) {
        int h1 = 150 + (int)(28 * sinf(x * 0.012f) + 16 * sinf(x * 0.031f + 1.0f));
        rect(x, h1, 1, GY - h1, RGB(26, 26, 62));
        int h2 = 178 + (int)(18 * sinf(x * 0.02f + 2.0f) + 9 * sinf(x * 0.05f));
        rect(x, h2, 1, GY - h2, RGB(18, 18, 44));
    }
    // arboles
    for (int i = 0; i < 12; i++) {
        int x = 10 + i * 42 + (i * 17) % 13;
        int h = 34 + (i * 29) % 30;
        rect(x - 2, GY - h, 4, h, RGB(10, 10, 26));
        circle(x, GY - h - 6, 12 + (i % 3) * 3, RGB(12, 16, 32));
    }
    // suelo
    rect(0, GY, SW, SH - GY, RGB(46, 36, 50));
    rect(0, GY, SW, 3, RGB(96, 80, 100));
    for (int y = GY + 6; y < SH; y += 9) rect(0, y, SW, 1, RGB(36, 28, 42));
    for (int x = 0; x < SW; x += 38) for (int y = GY + 8; y < SH; y += 18) rect(x + (y % 2) * 14, y, 10, 1, RGB(70, 58, 76));
    // faroles
    for (int i = 0; i < 3; i++) {
        int x = 70 + i * 170;
        rect(x - 1, GY - 52, 3, 52, RGB(40, 28, 24));
        circleA(x, GY - 56, 22, RGB(255, 190, 90), 40);
        circle(x, GY - 56, 6, RGB(255, 220, 130));
    }
    memcpy(bg, canvas, sizeof(canvas));
}

/* ===================================================================== */
/*  DIBUJO DE NINJAS                                                     */
/* ===================================================================== */
static void draw_ninja(const F *f, int t) {
    int x = (int)f->x, y = (int)f->y, d = f->dir;
    ellipseA(x, GY + 3, 15, 4, RGB(0, 0, 0), 110);

    if (f->logt > 0) {   // tronco de sustitucion
        int lx = (int)f->logx;
        rect(lx - 6, GY - 34, 12, 34, RGB(110, 76, 44));
        rect(lx - 6, GY - 34, 12, 3, RGB(150, 106, 64));
        rect(lx - 2, GY - 24, 4, 2, RGB(70, 46, 26));
    }

    u32 skin = RGB(236, 196, 160), dark = RGB(24, 22, 36);
    if (f->ko) {
        rect(x - 20, y - 9, 36, 9, f->col);
        rect(x - 22 * d, y - 12, 12, 12, skin);
        rect(x - 22 * d, y - 12, 12, 4, f->col2);
        return;
    }
    if (f->glow) {
        int pulse = 24 + (int)(fsin(t * 0.25f) * 4.0f);
        circleA(x, y - 30, pulse, f->col2, 38);
        circleA(x, y - 30, pulse - 8, f->col2, 38);
    }
    int lean = 0;
    if (f->hurt > 0) lean = -d * 4;
    if (f->ult > 0) lean = d * 2;
    int walking = f->ground && fabsf(f->vx) > 0.6f && f->atk == 0 && f->hurt == 0;
    int lo = walking ? (int)(fsin(f->x * 0.22f) * 6.0f) : 0;
    int crouch = (f->charging || f->guard) ? 4 : 0;

    if (f->dash > 0) {   // estela de velocidad
        for (int i = 1; i <= 3; i++) rectA(x - d * i * 12 - 6, y - 44, 12, 40, f->col2, 70 - i * 18);
    }

    // bufanda
    int sc = (int)(fsin(t * 0.3f + f->x * 0.05f) * 3.0f);
    rect(x - d * 8 + lean, y - 46 - crouch + sc, -d * 14 + (d > 0 ? 0 : 0), 4, f->col2);
    rect(x - d * 8 + lean - d * 14, y - 44 - crouch + sc, 6, 4, f->col2);
    // piernas
    if (!f->ground) { rect(x - 6, y - 18, 5, 12, dark); rect(x + 1, y - 14, 5, 10, dark); }
    else {
        rect(x - 6 + lo, y - 18 + crouch, 5, 18 - crouch, dark);
        rect(x + 1 - lo, y - 18 + crouch, 5, 18 - crouch, dark);
    }
    // torso
    rect(x - 7 + lean, y - 42 + crouch, 14, 25 - crouch, f->col);
    rect(x - 7 + lean, y - 26, 14, 3, f->col2);
    // cabeza
    circle(x + lean + d, y - 51 + crouch, 8, skin);
    rect(x + lean - 8 + d, y - 55 + crouch, 17, 5, f->col2);
    rect(x + lean + d * 3 - 3, y - 55 + crouch, 6, 5, RGB(210, 215, 230));
    rect(x + lean + d * 4, y - 50 + crouch, 3, 2, dark);
    // brazos / ataques
    if (f->atk > 0) {
        int prog = 14 - f->atk; if (prog > 8) prog = 8;
        int ext = 8 + prog * 3;
        if (f->atkn == 1) {          // patada
            rect(x, y - 16, d * (ext + 6), 5, dark);
            rect(x + d * (ext + 6) - (d > 0 ? 3 : 0), y - 18, 4, 8, skin);
        } else {                      // puno
            rect(x, y - 36, d * ext, 5, f->col);
            rect(x + d * ext - (d > 0 ? 4 : 0), y - 38, 5, 8, skin);
        }
    } else if (f->ult > 0) {
        rect(x, y - 38, d * 24, 5, f->col);
        rect(x + d * 24 - (d > 0 ? 4 : 0), y - 40, 5, 8, skin);
        rect(x - d * 24, y - 38, d * 24 * -1 + 0, 5, f->col);
    } else if (f->guard) {
        rect(x + d * 3, y - 44, 5, 20, RGB(190, 200, 225));
        rect(x + d * 3 - 1, y - 44, 7, 3, RGB(240, 245, 255));
    } else if (f->charging) {
        rect(x - 10, y - 32, 20, 4, f->col);
        for (int i = 0; i < 2; i++) part(x + (rand() % 30 - 15), y - 6, 0, -1.4f, 14, 0, f->col2);
    } else {
        int sw = walking ? (int)(fsin(f->x * 0.22f + 2.0f) * 6.0f) : 0;
        rect(x - d * 9 + lean, y - 36 + sw, 5, 14, f->col);
        rect(x + d * 4 + lean, y - 36 - sw, 5, 14, f->col);
    }
}

/* ===================================================================== */
/*  LOGICA DE COMBATE                                                    */
/* ===================================================================== */
static void spawn_pj(int kind, float x, float y, float vx, int owner) {
    for (int i = 0; i < MAXPJ; i++) if (!pj[i].on) { pj[i] = (Pj){x, y, vx, kind, 1, owner, 0}; return; }
}

// devuelve 1 si el golpe conecto
static int damage(F *t, const F *src, int dmg, float kb, int stun) {
    if (t->ko || t->dash > 0) return 0;
    float sdir = (src->x < t->x) ? 1.0f : -1.0f;       // hacia donde sale despedido
    int blocked = t->guard && t->ground && t->hurt == 0 && (t->dir == -(int)sdir);
    if (blocked) {
        dmg = (dmg + 3) / 4; kb *= 0.25f; stun = 6;
        sparks(t->x + t->dir * 8, t->y - 34, 6, RGB(200, 220, 255));
        sfx_play(SFX_SELECT);
        t->vx = sdir * kb;
        t->hp -= dmg;
        return 1;
    }
    t->hp -= dmg; t->lastd = dmg; t->subw = 12; t->hurt = stun; t->atk = 0; t->chain = 0; t->ult = 0;
    t->vx = sdir * kb; t->charging = 0;
    if (kb > 5.0f) { t->vy = -5.0f; t->ground = 0; }
    sparks(t->x, t->y - 34, 10 + dmg, RGB(255, 235, 170));
    sfx_play(SFX_HIT);
    shake = dmg > 8 ? 7 : 3; hitstop = dmg > 8 ? 6 : 3;
    return 1;
}

static void try_melee(F *f, F *o) {
    float reach = (f->atkn == 2) ? 40.0f : 34.0f;
    float dx = (o->x - f->x) * f->dir;
    if (dx > -4 && dx < reach && fabsf(o->y - f->y) < 36.0f) {
        int dmg = (f->atkn == 2) ? 10 : (f->atkn == 1 ? 6 : 5);
        float kb = (f->atkn == 2) ? 6.5f : 2.0f;
        if (damage(o, f, dmg, kb, f->atkn == 2 ? 26 : 12)) {
            f->ch += 6; if (f->ch > 100) f->ch = 100;
        }
    }
}

static void step_fighter(F *f, F *o, const In *in) {
    f->glow = (f->ch >= 100.0f);
    if (f->logt > 0) f->logt--;
    if (f->kcd > 0) f->kcd--;
    if (f->dcd > 0) f->dcd--;

    // gravedad / movimiento libre
    f->vy += 0.55f; f->y += f->vy;
    if (f->y >= GY) { f->y = GY; f->vy = 0; f->ground = 1; } else f->ground = 0;
    f->x += f->vx;
    if (f->x < 22) { f->x = 22; f->vx = 0; }
    if (f->x > SW - 22) { f->x = SW - 22; f->vx = 0; }
    if (f->ground) { f->vx *= 0.82f; if (fabsf(f->vx) < 0.05f) f->vx = 0; }

    if (f->ko) return;
    if (f->ch < 100.0f) { f->ch += 0.10f; if (f->ch > 100.0f) f->ch = 100.0f; }
    f->guard = 0; f->charging = 0;

    // SUSTITUCION con tronco
    if (f->hurt > 0) {
        f->hurt--;
        if (f->subw > 0) {
            f->subw--;
            if (in->dash && f->ch >= 25.0f) {
                f->ch -= 25.0f; f->hp += f->lastd; if (f->hp > 100) f->hp = 100;
                f->logx = f->x; f->logt = 24;
                smoke(f->x, f->y, 8);
                f->x = o->x - o->dir * 46.0f;
                if (f->x < 24) f->x = o->x + 46.0f;
                if (f->x > SW - 24) f->x = o->x - 46.0f;
                f->hurt = 0; f->vx = 0; f->vy = 0; f->dir = (o->x > f->x) ? 1 : -1;
                smoke(f->x, f->y, 8);
                sfx_play(SFX_BLIP);
                return;
            }
        }
        return;
    }
    if (f->ult > 0) {
        f->ult--;
        if (f->ult == 24) { spawn_pj(PJ_ORB, f->x + f->dir * 22, f->y - 34, f->dir * 6.5f, f == o ? 0 : (f->col == RGB(40, 120, 220) ? 0 : 1)); flashv = 90; shake = 6; sfx_play(SFX_SPARE); }
        return;
    }
    if (f->atk > 0) {
        f->atk--;
        if (f->atk == 8) { f->hitdone = 0; }
        if (f->atk <= 8 && !f->hitdone && f->atk >= 5) { try_melee(f, o); f->hitdone = 1; }
        if (f->atk == 0) f->chain = 14;
        return;
    }
    if (f->chain > 0) f->chain--;
    if (f->dash > 0) {
        f->dash--; f->vx = f->dir * 7.5f; return;
    }

    // ultimate
    if (in->ult && f->ch >= 100.0f && f->ground) { f->ult = 46; f->ch = 0; f->vx = 0; return; }
    // dash / esquive
    if (in->dash && f->ground && f->dcd == 0 && f->ch >= 5.0f) {
        f->dash = 10; f->dcd = 28; f->ch -= 5.0f;
        f->dir = in->r ? 1 : (in->l ? -1 : f->dir);
        smoke(f->x, f->y, 3); sfx_play(SFX_BLIP); return;
    }
    // ataque + combo
    if (in->atk) {
        f->atkn = (f->chain > 0) ? (f->atkn + 1) % 3 : 0;
        f->atk = 14; f->hitdone = 0; f->chain = 0;
        f->vx = f->dir * (f->atkn == 2 ? 3.2f : 1.4f);
        f->dir = (o->x > f->x) ? 1 : -1;
        return;
    }
    // kunai
    if (in->kun && f->ch >= 8.0f && f->kcd == 0) {
        f->ch -= 8.0f; f->kcd = 20;
        spawn_pj(PJ_KUNAI, f->x + f->dir * 12, f->y - 34, f->dir * 9.0f, f->col == RGB(40, 120, 220) ? 0 : 1);
        sfx_play(SFX_BLIP);
        return;
    }
    // guardia / cargar
    if (in->guard && f->ground) { f->guard = 1; f->vx = 0; return; }
    if (in->charge && f->ground) {
        f->charging = 1; f->vx = 0;
        f->ch += 0.75f; if (f->ch > 100.0f) f->ch = 100.0f;
        return;
    }
    // movimiento
    float sp = f->ground ? 2.4f : 2.0f;
    if (in->l) { f->x -= sp; f->dir = -1; }
    if (in->r) { f->x += sp; f->dir = 1; }
    if (f->ground) f->dir = (o->x > f->x) ? 1 : -1;
    if (in->jump && f->ground) { f->vy = -10.0f; f->ground = 0; }
}

static void update_pj(F *fs) {
    for (int i = 0; i < MAXPJ; i++) if (pj[i].on) {
        Pj *p = &pj[i];
        p->x += p->vx; p->t++;
        if (p->x < -20 || p->x > SW + 20) { p->on = 0; continue; }
        if (p->kind == PJ_ORB && (p->t & 1)) part(p->x, p->y + (rand() % 10 - 5), -p->vx * 0.2f, 0, 12, 1, RGB(255, 200, 100));
        F *t = &fs[1 - p->owner];
        if (fabsf(p->x - t->x) < (p->kind == PJ_ORB ? 22.0f : 12.0f) && p->y > t->y - 62.0f && p->y < t->y + 4.0f) {
            F src = fs[p->owner]; src.x = p->x - (p->vx > 0 ? 1.0f : -1.0f);
            int ok = (p->kind == PJ_ORB) ? damage(t, &src, 28, 8.0f, 34) : damage(t, &src, 5, 2.5f, 12);
            if (ok) {
                if (p->kind == PJ_ORB) { sparks(p->x, p->y, 30, RGB(255, 190, 90)); shake = 9; }
                p->on = 0;
            }
        }
    }
}

/* ===================================================================== */
/*  IA                                                                   */
/* ===================================================================== */
static void ai_think(F *f, F *o, In *in) {
    memset(in, 0, sizeof *in);
    float dx = o->x - f->x, dist = fabsf(dx);
    int toward = dx > 0;
    if (f->hurt > 0 && f->subw > 0 && f->ch >= 25.0f && (rand() % 100) < 45) in->dash = 1;
    if (f->ch >= 100.0f && dist < 150 && f->ground && (rand() % 100) < 6) { in->ult = 1; return; }
    if (--f->ai_t <= 0) {
        f->ai_t = 8 + rand() % 18;
        int r = rand() % 100;
        if (dist > 150)      f->ai_a = r < 60 ? 0 : (r < 82 ? 3 : (r < 92 ? 5 : 6));
        else if (dist > 50)  f->ai_a = r < 52 ? 0 : (r < 72 ? 1 : (r < 82 ? 6 : (r < 92 ? 4 : 7)));
        else                 f->ai_a = r < 52 ? 1 : (r < 70 ? 4 : (r < 86 ? 2 : 7));
        if (o->atk > 0 && dist < 60 && (rand() % 100) < 40) f->ai_a = 4;   // se cubre si lo atacan
    }
    switch (f->ai_a) {
    case 0: if (toward) in->r = 1; else in->l = 1; break;
    case 1: if (dist < 44) in->atk = 1; else { if (toward) in->r = 1; else in->l = 1; } break;
    case 2: if (toward) in->l = 1; else in->r = 1; break;
    case 3: in->kun = 1; break;
    case 4: in->guard = 1; break;
    case 5: in->charge = 1; break;
    case 6: in->jump = 1; if (toward) in->r = 1; else in->l = 1; break;
    case 7: in->dash = 1; if (toward) in->r = 1; else in->l = 1; break;
    default: break;
    }
}

/* ===================================================================== */
/*  HUD                                                                  */
/* ===================================================================== */
static void bar(int x, int y, int w, int h, int val, int mx, u32 fill, u32 back, int rtl) {
    rect(x - 2, y - 2, w + 4, h + 4, RGB(255, 255, 255));
    rect(x - 1, y - 1, w + 2, h + 2, RGB(20, 14, 34));
    rect(x, y, w, h, back);
    int fw = w * (val < 0 ? 0 : val) / mx;
    if (rtl) rect(x + w - fw, y, fw, h, fill); else rect(x, y, fw, h, fill);
}

static void draw_hud(const F *fs, int timer, int state) {
    for (int i = 0; i < 2; i++) {
        int rtl = i == 1;
        int x = rtl ? SW - 12 - 190 : 12;
        bar(x, 16, 190, 12, fs[i].hp, 100, RGB(255, 210, 40), RGB(110, 0, 24), rtl);
        bar(x, 34, 190, 5, (int)fs[i].ch, 100, fs[i].ch >= 100.0f ? RGB(255, 140, 255) : RGB(80, 190, 255), RGB(24, 30, 70), rtl);
        if (rtl) text(SW - 12 - text_w(1, fs[i].name), 3, 1, RGB(255, 255, 255), fs[i].name);
        else text(12, 3, 1, RGB(255, 255, 255), fs[i].name);
        for (int w = 0; w < 2; w++) {
            int cx = rtl ? SW - 18 - w * 12 : 18 + w * 12;
            circle(cx, 50, 4, RGB(255, 255, 255));
            circle(cx, 50, 3, w < fs[i].wins ? RGB(255, 210, 40) : RGB(40, 30, 60));
        }
        if (fs[i].ch >= 100.0f && (state == ST_FIGHT) && ((int)(fs[i].x) & 0) == 0) {
            int bx = rtl ? SW - 12 - 190 : 12;
            text(bx, 42 + 8, 1, RGB(255, 160, 255), "LISTO: CUADRADO+TRIANGULO");
        }
    }
    char tb[8]; snprintf(tb, sizeof tb, "%02d", timer / 60);
    rect(SW / 2 - 22, 8, 44, 26, RGB(255, 255, 255));
    rect(SW / 2 - 20, 10, 40, 22, RGB(20, 14, 34));
    textc(SW / 2, 13, 2, RGB(255, 230, 120), tb);
    // aviso de sustitucion
    for (int i = 0; i < 2; i++)
        if (i == 0 && fs[i].hurt > 0 && fs[i].subw > 0 && fs[i].ch >= 25.0f)
            textc((int)fs[i].x, (int)fs[i].y - 84, 2, RGB(255, 255, 255), "O!");
}

/* ===================================================================== */
/*  MAIN                                                                 */
/* ===================================================================== */
static void fighter_init(F *f, float x, int dir, u32 c1, u32 c2, const char *name) {
    int wins = f->wins;
    memset(f, 0, sizeof *f);
    f->x = x; f->y = GY; f->dir = dir; f->hp = 100; f->ch = 30; f->ground = 1;
    f->col = c1; f->col2 = c2; f->name = name; f->wins = wins; f->ai_t = 30;
}

int main(void) {
    setup_callbacks();
    scePowerSetClockFrequency(333, 333, 166);          // maxima velocidad de CPU
    sceDisplaySetMode(0, SW, SH);
    vram[0] = (u32 *)(0x44000000);
    vram[1] = (u32 *)(0x44000000 + STRIDE * SH * 4);
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
    srand((unsigned)sceKernelLibcTime(NULL));
    for (int i = 0; i < 256; i++) sintab[i] = sinf(i * 2.0f * PI_F / 256.0f);
    for (int i = 0; i < 40; i++) { stars_x[i] = (float)(rand() % SW); stars_y[i] = (float)(rand() % 130); }
    render_bg();

    int at = sceKernelCreateThread("audio", audio_thread, 0x12, 0x10000, THREAD_ATTR_USER, NULL);
    if (at >= 0) sceKernelStartThread(at, 0, NULL);

    F fs[2]; memset(fs, 0, sizeof fs);
    const u32 BLUE = RGB(40, 120, 220), RED = RGB(200, 50, 60);
    int state = ST_TITLE, tick = 0, timer = 0, st_t = 0, round = 1, winner = -1;
    u32 prev = 0; int cur = 0;

    while (!g_quit) {
        SceCtrlData pad;
        sceCtrlPeekBufferPositive(&pad, 1);
        u32 pressed = pad.Buttons & ~prev; prev = pad.Buttons;
        tick++;

        /* -------- entrada del jugador -------- */
        In pin; memset(&pin, 0, sizeof pin);
        float ax = (pad.Lx - 128) / 128.0f, ay = (pad.Ly - 128) / 128.0f;
        pin.l = (pad.Buttons & PSP_CTRL_LEFT) || ax < -0.35f;
        pin.r = (pad.Buttons & PSP_CTRL_RIGHT) || ax > 0.35f;
        pin.jump = (pressed & (PSP_CTRL_UP | PSP_CTRL_CROSS)) || (ay < -0.6f && !(prev & 0x100000));
        pin.atk = (pressed & PSP_CTRL_SQUARE) != 0;
        pin.kun = (pressed & PSP_CTRL_TRIANGLE) != 0;
        pin.dash = (pressed & PSP_CTRL_CIRCLE) != 0;
        pin.guard = (pad.Buttons & PSP_CTRL_RTRIGGER) != 0;
        pin.charge = (pad.Buttons & PSP_CTRL_LTRIGGER) != 0;
        pin.ult = (pad.Buttons & PSP_CTRL_SQUARE) && (pad.Buttons & PSP_CTRL_TRIANGLE) &&
                  (pressed & (PSP_CTRL_SQUARE | PSP_CTRL_TRIANGLE));
        if (pin.ult) { pin.atk = 0; pin.kun = 0; }

        /* -------- logica de estados -------- */
        if (state == ST_TITLE) {
            if (pressed & (PSP_CTRL_CROSS | PSP_CTRL_START)) {
                sfx_play(SFX_CONFIRM);
                fs[0].wins = fs[1].wins = 0; round = 1;
                fighter_init(&fs[0], 140, 1, BLUE, RGB(120, 220, 255), "KAZE");
                fighter_init(&fs[1], 340, -1, RED, RGB(255, 150, 90), "RAIKO");
                memset(pj, 0, sizeof pj); memset(pt, 0, sizeof pt);
                state = ST_INTRO; st_t = 0;
            }
        } else if (state == ST_INTRO) {
            st_t++;
            if (st_t == 70) sfx_play(SFX_CONFIRM);
            if (st_t > 110) { state = ST_FIGHT; timer = ROUND_FRAMES; }
        } else if (state == ST_FIGHT) {
            if (hitstop > 0) hitstop--;
            else {
                In ain; ai_think(&fs[1], &fs[0], &ain);
                step_fighter(&fs[0], &fs[1], &pin);
                step_fighter(&fs[1], &fs[0], &ain);
                update_pj(fs);
                // empuje para que no se atraviesen
                float gap = fs[1].x - fs[0].x;
                if (fabsf(gap) < 16.0f && fabsf(fs[0].y - fs[1].y) < 30.0f && !fs[0].dash && !fs[1].dash) {
                    float push = (16.0f - fabsf(gap)) * 0.5f * (gap >= 0 ? 1.0f : -1.0f);
                    fs[0].x -= push; fs[1].x += push;
                }
                timer--;
            }
            int over = 0;
            for (int i = 0; i < 2; i++) if (fs[i].hp <= 0) { fs[i].hp = 0; fs[i].ko = 1; over = 1; }
            if (timer <= 0) over = 1;
            if (over) {
                if (fs[0].hp > fs[1].hp) winner = 0; else if (fs[1].hp > fs[0].hp) winner = 1; else winner = -1;
                if (winner >= 0) fs[winner].wins++;
                if (winner == 0) sfx_play(SFX_SPARE); else sfx_play(SFX_LOSE);
                state = ST_RDEND; st_t = 0; hitstop = 0;
            }
        } else if (state == ST_RDEND) {
            st_t++;
            step_fighter(&fs[0], &fs[1], &(In){0});
            step_fighter(&fs[1], &fs[0], &(In){0});
            update_pj(fs);
            if (st_t > 150) {
                if (fs[0].wins >= 2 || fs[1].wins >= 2) { state = ST_MATCH; st_t = 0; }
                else {
                    round++;
                    fighter_init(&fs[0], 140, 1, BLUE, RGB(120, 220, 255), "KAZE");
                    fighter_init(&fs[1], 340, -1, RED, RGB(255, 150, 90), "RAIKO");
                    memset(pj, 0, sizeof pj);
                    state = ST_INTRO; st_t = 0;
                }
            }
        } else if (state == ST_MATCH) {
            st_t++;
            if ((pressed & PSP_CTRL_START) && st_t > 30) { state = ST_TITLE; sfx_play(SFX_SELECT); }
        }

        if (shake > 0) shake--;
        if (flashv > 0) flashv -= 6;
        if (flashv < 0) flashv = 0;
        update_parts();
        g_song = (state == ST_FIGHT || state == ST_INTRO) ? 0 : 1;
        if (state == ST_RDEND && winner == 1) g_song = 2;

        /* -------- dibujo -------- */
        ox = oy = 0;
        memcpy(canvas, bg, sizeof(canvas));
        if (shake > 0) { ox = (rand() % 5 - 2) * (shake > 4 ? 2 : 1); oy = rand() % 3 - 1; }

        if (state != ST_TITLE) {
            // el de atras es el que esta mas arriba (salto) -> orden simple por y
            int first = (fs[0].y <= fs[1].y) ? 1 : 0;
            draw_ninja(&fs[first], tick);
            draw_ninja(&fs[1 - first], tick);
            for (int i = 0; i < MAXPJ; i++) if (pj[i].on) {
                if (pj[i].kind == PJ_KUNAI) {
                    int d = pj[i].vx > 0 ? 1 : -1;
                    rect((int)pj[i].x - 6, (int)pj[i].y - 1, 12, 3, RGB(210, 215, 230));
                    rect((int)pj[i].x + d * 5, (int)pj[i].y - 2, 3, 5, RGB(250, 250, 255));
                    rect((int)pj[i].x - d * 8, (int)pj[i].y - 2, 3, 5, RGB(130, 60, 40));
                } else {
                    int r = 14 + (int)(fsin(pj[i].t * 0.5f) * 3.0f);
                    circleA((int)pj[i].x, (int)pj[i].y, r + 10, RGB(255, 170, 60), 40);
                    circle((int)pj[i].x, (int)pj[i].y, r, RGB(255, 210, 120));
                    circle((int)pj[i].x, (int)pj[i].y, r - 7, RGB(255, 250, 220));
                }
            }
            draw_parts();
            ox = oy = 0;
            if (flashv > 0) rectA(0, 0, SW, SH, RGB(255, 255, 255), flashv > 255 ? 255 : flashv);
            draw_hud(fs, timer, state);
        }

        if (state == ST_TITLE) {
            rectA(0, 0, SW, SH, RGB(0, 0, 14), 120);
            textc(240, 40, 4, RGB(255, 230, 120), "NINJA TORMENTA");
            textc(240, 84, 2, RGB(200, 190, 255), "COMBATE EN LA ARENA");
            // los dos luchadores del titulo
            F a = {0}, b = {0};
            a.x = 150; a.y = GY; a.dir = 1; a.ground = 1; a.col = BLUE; a.col2 = RGB(120, 220, 255);
            b.x = 330; b.y = GY; b.dir = -1; b.ground = 1; b.col = RED; b.col2 = RGB(255, 150, 90);
            draw_ninja(&a, tick); draw_ninja(&b, tick);
            if ((tick / 30) % 2 == 0) textc(240, 120, 2, RGB(255, 255, 255), "PULSA X PARA EMPEZAR");
            textc(240, 232, 1, RGB(210, 200, 255), "CUADRADO GOLPE  TRIANGULO KUNAI  CIRCULO ESQUIVE/SUSTITUCION");
            textc(240, 246, 1, RGB(210, 200, 255), "L CARGAR  R GUARDIA  CUADRADO+TRIANGULO CON CHAKRA 100: TECNICA FINAL");
        }
        if (state == ST_INTRO) {
            char rb[16]; snprintf(rb, sizeof rb, "RONDA %d", round);
            if (st_t < 70) textc(240, 100, 4, RGB(255, 255, 255), rb);
            else textc(240, 100, 4, RGB(255, 90, 90), "LUCHA!");
        }
        if (state == ST_RDEND) {
            rectA(0, 90, SW, 70, RGB(0, 0, 10), 150);
            if (st_t > 20) {
                textc(240, 104, 4, winner == 0 ? RGB(255, 230, 90) : RGB(255, 80, 90),
                      winner < 0 ? "EMPATE" : (winner == 0 ? "VICTORIA" : "DERROTA"));
            } else textc(240, 104, 4, RGB(255, 255, 255), "K.O.");
        }
        if (state == ST_MATCH) {
            rectA(0, 0, SW, SH, RGB(0, 0, 14), 170);
            textc(240, 60, 4, fs[0].wins >= 2 ? RGB(255, 230, 90) : RGB(255, 80, 90),
                  fs[0].wins >= 2 ? "GANASTE EL TORNEO" : "PERDISTE");
            char sb[32]; snprintf(sb, sizeof sb, "KAZE %d - %d RAIKO", fs[0].wins, fs[1].wins);
            textc(240, 120, 2, RGB(255, 255, 255), sb);
            textc(240, 200, 1, RGB(210, 200, 255), "PULSA START PARA VOLVER AL TITULO");
        }

        /* -------- presentar -------- */
        int back = cur ^ 1; cur = back;
        memcpy(vram[back], canvas, sizeof(canvas));
        sceDisplayWaitVblankStart();
        sceDisplaySetFrameBuf(vram[back], STRIDE, PSP_DISPLAY_PIXEL_FORMAT_8888, 1);
    }
    g_quit = 1;
    return 0;
}
