#ifndef JGL_C_
#define JGL_C_

#include <stddef.h>
#include <stdint.h>
#include <float.h>
#include <stdbool.h>
#include <stdio.h>
#include <math.h>
#include <errno.h>
#include <limits.h>

typedef struct {
	uint32_t *pixels;
	size_t width;
	size_t height;
	size_t stride;
} Canvas;

#define PIXEL(c, x, y) (c).pixels[(y)*(c).stride + (x)]

Canvas jgl_canvas(uint32_t *pixels, size_t width, size_t height, size_t stride)
{
	Canvas c = {
		.pixels = pixels,
		.width = width,
		.height = height,
		.stride = stride,
	};
	return c;
}

typedef enum {
	COMP_RED = 0,
	COMP_GREEN,
	COMP_BLUE,
	COMP_ALPHA,
	COUNT_COMPS,
} Comp_Index;

void unpack_rgba32(uint32_t c, uint8_t comp[COUNT_COMPS])
{
	for (size_t i = 0; i < COUNT_COMPS; ++i) {
		comp[i] = c&0xff;
		c >>=8;
	}
}

uint32_t pack_rgba32( uint8_t comp[COUNT_COMPS])
{
	uint32_t result = 0;
	for (size_t i = 0; i < COUNT_COMPS; ++i) {
		result |= comp[i]<<(8*i);
	}
	return result;
}

uint8_t jgl_mix_comps(uint16_t c1, uint16_t c2, uint16_t a)
{
	return c1 + (c2 - c1)*a/255;
}

// macros are an alternate for the scheme above that uses a comp_index enum and rgba pack/unpack functions.

#define JGL_RED(color)   (((color)&0x000000FF)>>(8*0))
#define JGL_GREEN(color) (((color)&0x0000FF00)>>(8*1))
#define JGL_BLUE(color)  (((color)&0x00FF0000)>>(8*2))
#define JGL_ALPHA(color) (((color)&0xFF000000)>>(8*3))
#define JGL_RGBA(r, g, b, a) ((((r)&0xFF)<<(8*0)) | (((g)&0xFF)<<(8*1)) | (((b)&0xFF)<<(8*2)) | (((a)&0xFF)<<(8*3)))

uint32_t blend_colors(uint32_t *c1, uint32_t c2)
{
	uint32_t r1 = JGL_RED(*c1);
	uint32_t g1 = JGL_GREEN(*c1);
	uint32_t b1 = JGL_BLUE(*c1);
	uint32_t a1 = JGL_ALPHA(*c1);

	uint32_t r2 = JGL_RED(c2);
	uint32_t g2 = JGL_GREEN(c2);
	uint32_t b2 = JGL_BLUE(c2);
	uint32_t a2 = JGL_ALPHA(c2);

	r1 = (r1*(255-a2) + r2*a2)/255; if (r1 > 255) r1 = 255;
	g1 = (g1*(255-a2) + g2*a2)/255; if (g1 > 255) g1 = 255;
	b1 = (b1*(255-a2) + b2*a2)/255; if (b1 > 255) b1 = 255;

	*c1 = JGL_RGBA(r1, g1, b1, a1);
}

uint32_t jgl_mix_colors(uint32_t c1, uint32_t c2)
{
	// 0xAABBGGRR
	// 0xRR 0xGG 0xBB 0xAA
	uint8_t comp1[COUNT_COMPS];
	unpack_rgba32(c1, comp1);

	uint8_t comp2[COUNT_COMPS];
	unpack_rgba32(c2, comp2);

	for (size_t i = 0; i < COMP_ALPHA; ++i) {
		comp1[i] = jgl_mix_comps(comp1[i], comp2[i], comp2[COMP_ALPHA]);
	}

	return pack_rgba32(comp1);
}

uint32_t mix_colors3(uint32_t c1, uint32_t c2, uint32_t c3, float t1, float t2, float t3)
{
	uint32_t r1 = JGL_RED(c1);
	uint32_t g1 = JGL_GREEN(c1);
	uint32_t b1 = JGL_BLUE(c1);
	uint32_t a1 = JGL_ALPHA(c1);

	uint32_t r2 = JGL_RED(c2);
	uint32_t g2 = JGL_GREEN(c2);
	uint32_t b2 = JGL_BLUE(c2);
	uint32_t a2 = JGL_ALPHA(c2);

	uint32_t r3= JGL_RED(c3);
	uint32_t g3= JGL_GREEN(c3);
	uint32_t b3= JGL_BLUE(c3);
	uint32_t a3= JGL_ALPHA(c3);

	uint32_t r4 = (r1*t1) + (r2*t2) + (r3*t3);
	uint32_t g4 = (g1*t1) + (g2*t2) + (g3*t3);
	uint32_t b4 = (b1*t1) + (b2*t2) + (b3*t3);
	uint32_t a4 = (a1*t1) + (a2*t2) + (a3*t3);

	return JGL_RGBA(r4, g4, b4, a4);
}

void jgl_fill(Canvas c, uint32_t color)
{
	for (size_t y = 0; y < c.height; ++y) {
		for (size_t x = 0; x < c.width; ++x) {
			PIXEL(c, x, y) = color;
		}
	}
}

void jgl_fill_rect(Canvas c, int x0, int y0, size_t w, size_t h, uint32_t color)
{
	for (int dy = 0; dy < (int) h; ++dy) {
		int y = y0 + dy;
		if (0 <= y && y < (int) c.height) {
			for(int dx = 0; dx < (int) w; ++dx) {
				int x = x0 + dx;
				if (0 <= x && x < (int) c.width) {
					blend_colors(&PIXEL(c, x, y), color);
				}
			}
		}
	}
}

void jgl_fill_circle(Canvas c, int cx, int cy, size_t r, uint32_t color)
{
	int x1 = cx - (int) r;
	int y1 = cy - (int) r;
	int x2 = cx + (int) r;
	int y2 = cy + (int) r;
	for (int y = y1; y <= y2; ++y) {
		if (0 <= y && y < (int) c.height) {
			for (int x = x1; x <= x2; ++x) {
				if (0 <= x && x < (int) c.width) {
					int dx = x - cx;
					int dy = y - cy;
					if (dx*dx + dy*dy <= (int) r * (int) r) {
						blend_colors(&PIXEL(c, x, y), color);
					}
				}
			}
		}
	}
}

void jgl_circlez(Canvas c, int cx, int cy, size_t r, float z)
{
    int x1 = cx - (int) r;
	int y1 = cy - (int) r;
	int x2 = cx + (int) r;
	int y2 = cy + (int) r;
	for (int y = y1; y <= y2; ++y) {
		if (0 <= y && y < (int) c.height) {
			for (int x = x1; x <= x2; ++x) {
				if (0 <= x && x < (int) c.width) {
					int dx = x - cx;
					int dy = y - cy;
					if (dx*dx + dy*dy <= (int) r * (int) r) {
					    PIXEL(c, x, y) = *(uint32_t*)&z;
					}
				}
			}
		}
	}
}

void swap_int(int *x1, int *x2)
{
	int temp = *x1;
	*x1 = *x2;
	*x2 = temp;
}

void swap_float(float *x1, float *x2)
{
	float temp = *x1;
	*x1 = *x2;
	*x2 = temp;
}

void sort_triangle_pts_by_y(int *x1, int *y1,
			    int *x2, int *y2,
			    int *x3, int *y3)
{
	if (*y1 > *y2) {
		swap_int(x1, x2);
		swap_int(y1, y2);
	}
	if (*y2 > *y3) {
		swap_int(x2, x3);
		swap_int(y2, y3);
	}
	if (*y1 > *y2) {
		swap_int(x1, x2);
		swap_int(y1, y2);
	}
}

void barycentric(float x1, float y1, float x2, float y2, float x3, float y3,
				 float xp, float yp, 
				 float *u1, float *u2, float *u3)
{
	*u1 = ((y2 - y3)*(xp - x3) + (x3 - x2)*(yp - y3)) / ((x1 - x3)*(y2 - y3) - (x2 - x3)*(y1 - y3));
	*u2 = ((y3 - y1)*(xp - x3) + (x1 - x3)*(yp - y3)) / ((x1 - x3)*(y2 - y3) - (x2 - x3)*(y1 - y3));
	*u3 = 1 - *u1 - *u2;
}

void jgl_draw_line(uint32_t *pixels, size_t px_width, size_t px_height, int x1, int y1, int x2, int y2, uint32_t color)
{
	int dx = x2 - x1;
	int dy = y2 - y1;

	if (dx != 0) {
		int c = y1 - dy*x1/dx;

		if (x1 > x2) swap_int(&x1, &x2);
		for (int x = x1; x <= x2; ++x) {
			if (0 <= x && x < (int) px_width) {
				int sy1 = dy*x/dx + c;
				int sy2 = dy*(x + 1)/dx + c;
				if (sy1 > sy2) swap_int(&sy1, &sy2);
				for (int y = sy1; y <= sy2; ++y) {
					if (0 <= y && y < (int) px_height) {
						pixels[y*px_width + x] = color; 
					}
				}
			}
		}
	}
	else {
		int x = x1;
		if (0 <= x && x < (int) px_width) {
			if (y1 > y2) swap_int(&y1, &y2);
			for (int y = y1; y <= y2; ++y) {
				if (0 <= y && y < (int) px_height) {
					pixels[y*px_width + x] = color;
				}
			}
		}
	}	
}

bool jgl_normalize_triangle(size_t width, size_t height, int x1, int y1, int x2, int y2, int x3, int y3, int *lx, int *hx, int *ly, int *hy)
{
    *lx = x1;
    *hx = x1;
    if (*lx > x2) *lx = x2;
    if (*lx > x3) *lx = x3;
    if (*hx < x2) *hx = x2;
    if (*hx < x3) *hx = x3;
    if (*lx < 0) *lx = 0;
    if ((size_t) *lx >= width) return false;;
    if (*hx < 0) return false;;
    if ((size_t) *hx >= width) *hx = width-1;

    *ly = y1;
    *hy = y1;
    if (*ly > y2) *ly = y2;
    if (*ly > y3) *ly = y3;
    if (*hy < y2) *hy = y2;
    if (*hy < y3) *hy = y3;
    if (*ly < 0) *ly = 0;
    if ((size_t) *ly >= height) return false;;
    if (*hy < 0) return false;;
    if ((size_t) *hy >= height) *hy = height-1;

    return true;
}
// TODO: rewrite triangle generating functions using normalize_triangle

void jgl_triangle3c(Canvas c,
		       int x1, int y1,
		       int x2, int y2,
		       int x3, int y3,
		       uint32_t c1, uint32_t c2, uint32_t c3)
{	
	if (y1 > y2) {
		swap_int(&x1, &x2);
		swap_int(&y1, &y2);
		swap_int(&c1, &c2);
	} 
	if (y2 > y3) {
		swap_int(&x2, &x3);
		swap_int(&y2, &y3);
		swap_int(&c2, &c3);
	} 
	if (y1 > y2) {
		swap_int(&x1, &x2);
		swap_int(&y1, &y2);
		swap_int(&c1, &c2);
	} 

	int dx12 = x2 - x1;
	int dy12 = y2 - y1;
	int dx13 = x3 - x1;
	int dy13 = y3 - y1;
			
	for (int y = y1; y < y2; ++y) {
		if (0 <= y && (size_t) y < c.height) {
			int s1 = dy12 != 0 ? (y - y1)*dx12/dy12 + x1 : x1;
			int s2 = dy13 != 0 ? (y - y1)*dx13/dy13 + x1 : x1;
			if (s1 > s2) swap_int(&s1, &s2);
			for (int x = s1; x <= s2; ++x) {
				if (0 <= x && (size_t) x < c.width) {
					float u1, u2, u3;
					barycentric(x1, y1, x2, y2, x3, y3, x, y, &u1, &u2, &u3);
					uint32_t color = mix_colors3(c1, c2, c3, u1, u2, u3);
					blend_colors(&PIXEL(c, x, y), color);
				}
			}
		}
	}

	int dx32 = x2 - x3;
    int dy32 = y2 - y3;
    int dx31 = x1 - x3;
    int dy31 = y1 - y3;

	for (int y = y2; y <= y3; ++y) {
		if (0 <= y && (size_t) y < c.height) {
			int s1 = dy32 != 0 ? (y - y3)*dx32/dy32 + x3 : x3;
			int s2 = dy31 != 0 ? (y - y3)*dx31/dy31 + x3 : x3;
			if (s1 > s2) swap_int(&s1, &s2);
			for (int x = s1; x <= s2; ++x) {
				if (0 <= x && (size_t) x < c.width) {
					float u1, u2, u3;
					barycentric(x1, y1, x2, y2, x3, y3, x, y, &u1, &u2, &u3);
					uint32_t color = mix_colors3(c1, c2, c3, u1, u2, u3);
					blend_colors(&PIXEL(c, x, y), color);
				}
			}
		}
	}
}

void jgl_triangle3z(Canvas c, int x1, int y1, int x2, int y2, int x3, int y3, float z1, float z2, float z3)
{
	if (y1 > y2) {
		swap_int(&x1, &x2);
		swap_int(&y1, &y2);
		swap_float(&z1, &z2);
	} 
	if (y2 > y3) {
		swap_int(&x2, &x3);
		swap_int(&y2, &y3);
		swap_float(&z2, &z3);
	} 
	if (y1 > y2) {
		swap_int(&x1, &x2);
		swap_int(&y1, &y2);
		swap_float(&z1, &z2);
	} 

	int dx12 = x2 - x1;
	int dy12 = y2 - y1;
	int dx13 = x3 - x1;
	int dy13 = y3 - y1;
			
	for (int y = y1; y < y2; ++y) {
		if (0 <= y && (size_t) y < c.height) {
			int s1 = dy12 != 0 ? (y - y1)*dx12/dy12 + x1 : x1;
			int s2 = dy13 != 0 ? (y - y1)*dx13/dy13 + x1 : x1;
			if (s1 > s2) swap_int(&s1, &s2);
			for (int x = s1; x <= s2; ++x) {
				if (0 <= x && (size_t) x < c.width) {
					float u1, u2, u3;
					barycentric(x1, y1, x2, y2, x3, y3, x, y, &u1, &u2, &u3);
					float z = z1*u1 + z2*u2 +z3*u3;
					PIXEL(c, x, y) = *(uint32_t*)&z;
				}
			}
		}
	}

	int dx32 = x2 - x3;
    int dy32 = y2 - y3;
    int dx31 = x1 - x3;
    int dy31 = y1 - y3;

	for (int y = y2; y <= y3; ++y) {
		if (0 <= y && (size_t) y < c.height) {
			int s1 = dy32 != 0 ? (y - y3)*dx32/dy32 + x3 : x3;
			int s2 = dy31 != 0 ? (y - y3)*dx31/dy31 + x3 : x3;
			if (s1 > s2) swap_int(&s1, &s2);
			for (int x = s1; x <= s2; ++x) {
				if (0 <= x && (size_t) x < c.width) {
					float u1, u2, u3;
					barycentric(x1, y1, x2, y2, x3, y3, x, y, &u1, &u2, &u3);
					float z = z1*u1 + z2*u2 +z3*u3;
					PIXEL(c, x, y) = *(uint32_t*)&z;
				}
			}
		}
	}
}

void jgl_fill_triangle(Canvas c,
		       int x1, int y1,
		       int x2, int y2,
		       int x3, int y3,
		       uint32_t color)
{	
	sort_triangle_pts_by_y(&x1, &y1, &x2, &y2, &x3, &y3);
	
	int dx12 = x2 - x1;
	int dy12 = y2 - y1;
	int dx13 = x3 - x1;
	int dy13 = y3 - y1;
	int dx23 = x3 - x2;
	int dy23 = y3 - y2;
			
	for (int y = y1; y < y2; ++y) {
		if (0 <= y && (size_t) y < c.height) {
			int s1 = dy12 != 0 ? (y - y1)*dx12/dy12 + x1 : x1;
			int s2 = dy13 != 0 ? (y - y1)*dx13/dy13 + x1 : x1;
			if (s1 > s2) swap_int(&s1, &s2);
			for (int x = s1; x <= s2; ++x) {
				if (0 <= x && (size_t) x < c.width) {
					blend_colors(&PIXEL(c, x, y), color);
				}
			}
		}
	}
	for (int y = y2; y <= y3; ++y) {
		if (0 <= y && (size_t) y < c.height) {
			int s1 = dy23 != 0 ? (y - y3)*dx23/dy23 + x3 : x3;
			int s2 = dy13 != 0 ? (y - y3)*dx13/dy13 + x3 : x3;
			if (s1 > s2) swap_int(&s1, &s2);
			for (int x = s1; x <= s2; ++x) {
				if (0 <= x && (size_t) x < c.width) {
					blend_colors(&PIXEL(c, x, y), color);
				}
			}
		}
	}
}

/*
typedef int Errno;

#define return_defer(value) do { result = (value); goto defer; }while (0)

Errno jgl_save_to_ppm(uint32_t *pixels, size_t width, size_t height, const char *file_path)
{
	int result = 0;
	FILE *f = NULL;

	{
		f = fopen(file_path, "wb");
		if (f == NULL) return_defer(errno);

		fprintf(f, "P6\n%zu %zu 255\n", width, height);
		if (ferror(f)) return_defer(errno);

		for (size_t i = 0; i < width*height; ++i) {
			// 0xAABBGGRR
			uint32_t pixel = pixels[i];
			uint8_t bytes[3] = {
				(pixel>>(8*0))&0xFF,
				(pixel>>(8*1))&0xFF,
				(pixel>>(8*2))&0xFF
			};
			fwrite(bytes, sizeof(bytes), 1, f);
			if (ferror(f)) return_defer(errno);
		}
	}
	
defer:
	if (f)  fclose(f);
	return result;
}
*/

#endif // JGL_C_
