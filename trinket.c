#include <SDL.h>
#include <stdio.h>
#include "jgl.c"

#define WIDTH 3072
#define HEIGHT 1920
#define PI 3.14159265358979323846
#define BGCOLOR 0x00202020

#define return_defer(value) do {result = (value); goto defer;} while (0)

/* Interface */

static uint32_t pixels[WIDTH*HEIGHT], *dst=&pixels[0];
static float zbuffer[WIDTH*HEIGHT] = {0};

static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;
static SDL_Texture *texture = NULL;
static SDL_Rect window_rect = {0, 0, WIDTH, HEIGHT};

/* Abstraction */

typedef struct {
    float x, y;
} Vector2;

typedef struct {
    float x, y, z;
} Vector3;

typedef struct {
    uint32_t color;
    Vector3 *a, *b;
} Edge;

typedef struct {
    int vert_len, edge_len;
    Vector3 position, *vertices;
    Edge *edges;
} Mesh;

typedef struct {
    int len;
    Vector3 position, scale, rotation;
    Mesh meshes[128];
} Scene;

typedef struct {
    Vector3 *items;
    size_t capacity;
    size_t count;
} Embeddings;

typedef struct {
    float range;
    Vector3 origin, rotation, torigin, trotation;
} Camera;

typedef struct {
    uint8_t down;
    float x, y;
} Mouse;


static Vector3 vertices[0x10000], *_vertices = &vertices[0];
static Edge edges[0x8000], *_edges = &edges[0];
static Scene scene;
static Camera cam;
static Mouse mouse;

/* Helpers */

static Vector3 *set3d(Vector3 *v, float x, float y, float z)
{
    v->x = x;
    v->y = y;
    v->z = z;
    return v;
}

static Vector2 vector2(float x, float y)
{
    Vector2 v2;
    v2.x = x;
    v2.y = y;
    return v2;
}

static Vector3 vector3(float x, float y, float z)
{
    Vector3 v3;
    set3d(&v3, x, y, z);
    return v3;
}

static Camera initialize_camera(float pitch, float yaw, float roll, float range)
{
    Camera c;
    set3d(&c.rotation, pitch, yaw, roll);
    set3d(&c.trotation, pitch, yaw, roll);
    set3d(&c.origin, 0, 0, 0);
    set3d(&c.torigin, 0, 0, 0);
    c.range = range;
    return c;
}

Vector3 remap(Vector3 v, float lx, float hx, float ly, float hy, float lz, float hz)
{
    v.z = (v.z - lz)/(hz - lz)*2 - 1;
    v.x = (v.x - lx)/(hx - lx)*2 - 1;
    v.y = (v.y - ly)/(hy - ly)*2 - 1;
    return v;
}

static Vector2 cam_project(Camera *c, Vector3 v3)
{
    float r = 500 / (v3.z + c->range);
    return vector2(WIDTH/2 + r*v3.x, HEIGHT/2 + r*v3.y);
}

/* Geometry */

static float lerp(float a, float b, float speed, float range)
{
    if (a < b-range || a > b+range) a += (b-a) / speed;
    else a = b;
    return a;
}

static Vector2 rot2d(Vector2 a, Vector2 b, float deg)
{
    float rad = deg * (PI/180);
    float angle = atan2f(b.y-a.y, b.x-a.x) + rad;
    float mag = sqrtf((a.x-b.x)*(a.x-b.x) + (a.y-b.y)*(a.y-b.y));
    return vector2(a.x + mag*cosf(angle), a.y + mag*sinf(angle));
}

static int equ3d(Vector3 v1, Vector3 v2)
{
    return v1.x == v2.x && v1.y == v2.y && v1.z == v2.z;
}

static Vector3 *addv3d(Vector3 *v, float x, float y, float z)
{
    return set3d(v, v->x+x, v->y+y, v->z+z);
}

static Vector3 add3d(Vector3 *a, Vector3 *b)
{
    return vector3(a->x + b->x, a->y + b->y, a->z + b->z);
}

static Vector3 mult3d(Vector3 *a, Vector3 *b)
{
    return vector3(a->x * b->x, a->y * b->y, a->z * b->z);
}

static Vector3 *translate3d(Vector3 *v, Vector3 *t)
{
    *v = add3d(v, t);
    return v;
}

static Vector3 *scale3d(Vector3 *v, Vector3 *t)
{
    *v = mult3d(v, t);
    return v;
}

static Vector3 *rot3d(Vector3 *p, Vector3 *o, Vector3 *t)
{
    if (t->x){
        Vector2 r = rot2d(vector2(o->y, o->z), vector2(p->y, p->z), t->x);
        p->y = r.x;
        p->z = r.y;
    }
    if (t->y){
        Vector2 r = rot2d(vector2(o->x, o->z), vector2(p->x, p->z), t->y);
        p->x = r.x;
        p->z = r.y;
    }
    if (t->z){
        Vector2 r = rot2d(vector2(o->x, o->y), vector2(p->x, p->y), t->z);
        p->x = r.x;
        p->y = r.y;
    }
    return p;
}

/* Primitives */

static Vector3 *addvertex(Mesh *m, float x, float y, float z)
{
    int i;
    Vector3 v = vector3(x,y,z);
    translate3d(&v, &scene.position);
    scale3d(&v, &scene.scale);
    rot3d(&v, &scene.position, &scene.rotation);
    for (i=0; i < m->vert_len; ++i)
        if (equ3d(m->vertices[i], v))
            return &m->vertices[i];
    m->vert_len++;
    return set3d(_vertices++, v.x, v.y, v.z);
}

static Edge *addedge(Mesh *m, Vector3 *a, Vector3 *b, uint32_t color)
{
    _edges->a = a;
    _edges->b = b;
    _edges->color = color;
    m->edge_len++;
    return _edges++;
}

static Mesh *addline(Mesh *m, Vector3 a, Vector3 b, uint32_t color)
{
    addedge(m, addvertex(m, a.x, a.y, a.z), addvertex(m, b.x, b.y, b.z), color);
    return m;
}

/* Drawing */

static void clear(uint32_t *dst)
{
    for (int i; i<WIDTH*HEIGHT; ++i)
        dst[i] = BGCOLOR;
}

static void drawline(uint32_t *dst, Vector2 p1, Vector2 p2, uint32_t color)
{
    int x1 = (int)p1.x, y1 = (int)p1.y, x2 = (int)p2.x, y2 = (int)p2.y;
    int dx = abs(x2-x1), sx = x1 < x2 ? 1 : -1;
    int dy = -abs(y2-y1), sy = y1 < y2 ? 1 : -1;
    int err = dx + dy, e2;
    for (;;) {
        if (x1 > 0 && y1 > 0 && x1 < WIDTH && y1 < HEIGHT) {
            dst[y1*WIDTH+x1] = color;
        }
        if (x1 == x2 && y1 == y2) break;
        e2 = 2*err;
        if (e2 >= dy) {
            err += dy;
            x1 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y1 += sy;
        }
    }
}

static void draw(uint32_t *dst) 
{
    clear(dst);	
    int i, j;
	for(i = 0; i < scene.len; i++) {
		Mesh *m = &scene.meshes[i];
		for(j = 0; j < m->edge_len; j++) {
			Edge *edge = &m->edges[j];
			Vector3 a = add3d(edge->a, &m->position);
			Vector3 b = add3d(edge->b, &m->position);
			rot3d(&a, &cam.origin, &cam.rotation);
			rot3d(&b, &cam.origin, &cam.rotation);
			drawline(pixels, cam_project(&cam, add3d(&cam.origin, &a)), cam_project(&cam, add3d(&cam.origin, &b)), edge->color);
		}
	}
    dst = pixels;
	SDL_UpdateTexture(texture, NULL, dst, WIDTH * sizeof(uint32_t));
	SDL_RenderClear(renderer);
	SDL_RenderCopy(renderer, texture, NULL, NULL);
	SDL_RenderPresent(renderer);
}

/* Mesh transforms */

Mesh *translate(Mesh *m, float x, float y, float z)
{
    int i;
    Vector3 t = vector3(x, y, z);
    for (i=0; i<m->vert_len; i++)
        translate3d(&m->vertices[i], &t);
    return m;
}

Mesh *scale(Mesh *m, float x, float y, float z)
{
    int i;
    Vector3 t = vector3(x, y, z);
    for (i=0; i<m->vert_len; i++)
        scale3d(&m->vertices[i], &t);
    return m;
}

Mesh *rotate(Mesh *m, float pitch, float yaw, float roll)
{
    int i;
    Vector3 t = vector3(pitch, yaw, roll);
    for (i=0; i<m->vert_len; i++)
        rot3d(&m->vertices[i], &m->position, &t);
    return m;
}

Mesh *extrude(Mesh *m, float x, float y, float z, uint32_t color)
{
    int i, vl = m->vert_len, el = m->edge_len;
    for (i=0; i<vl; i++)
        addedge(m, &m->vertices[i], addvertex(m, m->vertices[i].x+x, m->vertices[i].y+y, m->vertices[i].z+z), color);
    for (i=0; i<el; i++)
        addedge(m, &m->vertices[vl + m->edges[i].a - &m->vertices[0]], &m->vertices[vl + m->edges[i].b - &m->vertices[0]], color);
    return m;
}

/* Creation */

Mesh *addmesh(Scene *s)
{
    if (s->len == 128) {
        return NULL;
    }
    s->meshes[s->len].vertices = _vertices;
    s->meshes[s->len].edges = _edges;
    return &s->meshes[s->len++];
}

Mesh *createplane(Scene *s, float w, float h, float xsegs, float ysegs, uint32_t color)
{
    int ix, iy;
    Mesh *m = addmesh(s);
    for (ix=0; ix<xsegs+1; ix++)
        addline(m, vector3(ix * (w/xsegs) - w/2, h/2, 0), vector3(ix * (w/xsegs) - w/2, -h/2, 0), color);
    for (iy=0; iy<ysegs+1; iy++)
        addline(m, vector3(w/2, iy * (h/ysegs) - h/2, 0), vector3(-w/2, iy * (h/ysegs) - h/2, 0), color);
    return m;
}

Mesh *createbox(Scene *s, float w, float h, float z, uint32_t color)
{
    return extrude(createplane(s, w, h, 1, 1, color), 0, 0, z, color);
}

/* Options */

static void update(Camera *c, double speed)
{
    if(!equ3d(c->rotation, c->trotation) || !equ3d(c->origin, c->torigin)) {
        set3d(&c->rotation, lerp(c->rotation.x, c->trotation.x, speed, 1), lerp(c->rotation.y, c->trotation.y, speed, 1), lerp(c->rotation.z, c->trotation.z, speed, 1));
        set3d(&c->origin, lerp(c->origin.x, c->torigin.x, speed, 1), lerp(c->origin.y, c->torigin.y, speed, 1), lerp(c->origin.z, c->torigin.z, speed, 1));                    
     }
}

static void modrange(int mod)
{
    int res = cam.range + mod;
    if (res > 0 && res < 90 ) cam.range = res;
}

static void handle_key(SDL_Event *event)
{
    int shift = SDL_GetModState() & KMOD_LSHIFT || SDL_GetModState() & KMOD_RSHIFT;

    switch(event->key.keysym.sym) {
    case SDLK_LEFT:
    case SDLK_a:
        if (shift) addv3d(&cam.torigin, 0.5, 0, 0);
        else addv3d(&cam.trotation, 0, -10, 0);
        break;
    case SDLK_RIGHT:
    case SDLK_d:
        if (shift) addv3d(&cam.torigin, -0.5, 0, 0);
        else addv3d(&cam.trotation, 0, 10, 0);
        break;
    case SDLK_UP:
    case SDLK_w:
        if (shift) addv3d(&cam.torigin, 0, 0.5, 0);
        else addv3d(&cam.trotation, 10, 0, 0);
        break;
    case SDLK_DOWN:
    case SDLK_s:
        if (shift) addv3d(&cam.torigin, 0, -0.5, 0);
        else addv3d(&cam.trotation, -10, 0, 0);
        break;
    }
}

/********************************/

int main(int argc, char* argv[]) {
    int result = 0;

    scene.len = 0;
    set3d(&scene.position, 0, 0, 0);
    set3d(&scene.scale, 1, 1, 1);
    set3d(&scene.rotation, 0, 0, 0);
    cam.range = 50;
    set3d(&cam.rotation, 180, 0, 0);
    set3d(&cam.trotation, 180, 0, 0);

    if (SDL_Init(SDL_INIT_VIDEO) <0 ) return_defer(1);

    window = SDL_CreateWindow(
        "Transparent Overlay",
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        WIDTH, HEIGHT,
        SDL_WINDOW_BORDERLESS | SDL_WINDOW_SKIP_TASKBAR 
    );
    if (window == NULL) return_defer(1);

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (renderer == NULL) return_defer(1);
    
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, WIDTH, HEIGHT);
    if (texture == NULL) return_defer(1);

    SDL_SetWindowOpacity(window, 0.25f);
    
    rotate(createbox(&scene, 20, 20, 20, 0xff00ff00), 120, 45, 0);
    draw(dst);    
    for (;;) {
        update(&cam, 5);
        draw(dst);

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch(event.type) {
            case SDL_QUIT:
                return_defer(0);
                break;
            case SDL_KEYDOWN:
                handle_key(&event);
                break;
            case SDL_MOUSEWHEEL:
                modrange(event.wheel.y);
                break;  
            }
        }
        
    }

    
defer:
    switch (result) {
        case 0:
            printf("OK\n");
            break;
        default:
            fprintf(stderr, "SDL ERROR: %s\n", SDL_GetError());
    }
    if (texture) SDL_DestroyTexture(texture);    
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);
    SDL_Quit();
    return result;
}
