#include <SDL.h>
#include "jgl.c"

#define WIDTH 3072
#define HEIGHT 1920
#define PI 3.14159265358979323846
#define BGCOLOR 0x00202020

#define return_defer(value) do {result = (value); goto defer;} while (0)

/* Interface */

static uint32_t pixels[WIDTH*HEIGHT] = {0};
static float zbuffer[WIDTH*HEIGHT] = {0};

static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;
static SDL_Texture *texture = NULL;
static SDL_Rect window_rect = {0, 0, WIDTH, HEIGHT};
static uint32_t *dst;

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

/* Primitives */

static Mesh *addline(Mesh *m, Vector3 a, Vector3 b, uint32_t color)
{
    addedge(m, addvertex(m, a.x, a.y, a.z), addvertex(m, b.x, b.y, b.z), color);
    return m;
}

/* Mesh transforms */

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

/********************************/

int main(int argc, char* argv[]) {
    int result = 0;

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

    
    for (;;) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                return_defer(0);
            }
        }

        SDL_SetRenderDrawColor(renderer, 255, 0, 255, 255);
        SDL_RenderClear(renderer);

        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        
        Canvas can = jgl_canvas(pixels, WIDTH, HEIGHT, WIDTH);
	    jgl_fill(can, BGCOLOR);
	    jgl_fill_circle(can, WIDTH/2, HEIGHT/2, 100, 0xFF00FF00);
        dst = pixels;
        SDL_UpdateTexture(texture, &window_rect, dst, WIDTH*sizeof(uint32_t));

        //SDL_SetRenderDrawColor(renderer, 255, 0, 255, 255);
        //SDL_RenderClear(renderer);

        SDL_RenderCopy(renderer, texture, NULL, &window_rect);
//        SDL_Rect rect = {WIDTH/4, HEIGHT/4, WIDTH/2, HEIGHT/2};
//        SDL_RenderFillRect(renderer, &rect);

        SDL_RenderPresent(renderer);
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
