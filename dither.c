#include <windows.h>
#include <stdlib.h>
#include <stdio.h>

// 内部类型定义
#pragma pack(1)
typedef struct { 
    WORD   bfType;
    DWORD  bfSize;
    WORD   bfReserved1;
    WORD   bfReserved2;
    DWORD  bfOffBits;
    DWORD  biSize;
    DWORD  biWidth;
    DWORD  biHeight;
    WORD   biPlanes;
    WORD   biBitCount;
    DWORD  biCompression;
    DWORD  biSizeImage;
    DWORD  biXPelsPerMeter;
    DWORD  biYPelsPerMeter;
    DWORD  biClrUsed;
    DWORD  biClrImportant;
} BMPFILEHEADER;
#pragma pack()

/* BMP 对象的类型定义 */
typedef struct
{
    int   width;   /* 宽度 */
    int   height;  /* 高度 */
    int   stride;  /* 行字节数 */
    void *pdata;   /* 指向数据 */
} BMP;

/* 内部函数实现 */
static int ALIGN(int x, int y) {
    // y must be a power of 2.
    return (x + y - 1) & ~(y - 1);
}

/* 函数实现 */
static int bmp_load(BMP *pb, char *file)
{
    BMPFILEHEADER header = {0};
    FILE         *fp     = NULL;
    BYTE         *pdata  = NULL;
    int           i;

    fp = fopen(file, "rb");
    if (!fp) return -1;

    fread(&header, sizeof(header), 1, fp);
    pb->width  = header.biWidth;
    pb->height = header.biHeight;
    pb->stride = ALIGN(header.biWidth * 3, 4);
    pb->pdata  = malloc(pb->stride * pb->height);
    if (pb->pdata) {
        pdata  = (BYTE*)pb->pdata + pb->stride * pb->height;
        for (i=0; i<pb->height; i++) {
            pdata -= pb->stride;
            fread(pdata, pb->stride, 1, fp);
        }
    }

    fclose(fp);
    return pb->pdata ? 0 : -1;
}

#if 0
static int bmp_create(BMP *pb, int w, int h)
{
    pb->width  = w;
    pb->height = h;
    pb->stride = ALIGN(w * 3, 4);
    pb->pdata  = malloc(pb->stride * h);
    return pb->pdata ? 0 : -1;
}
#endif

static int bmp_save(BMP *pb, char *file)
{
    BMPFILEHEADER header = {0};
    FILE         *fp     = NULL;
    BYTE         *pdata;
    int           i;

    header.bfType     = ('B' << 0) | ('M' << 8);
    header.bfSize     = sizeof(header) + pb->stride * pb->height;
    header.bfOffBits  = sizeof(header);
    header.biSize     = 40;
    header.biWidth    = pb->width;
    header.biHeight   = pb->height;
    header.biPlanes   = 1;
    header.biBitCount = 24;
    header.biSizeImage= pb->stride * pb->height;

    fp = fopen(file, "wb");
    if (fp) {
        fwrite(&header, sizeof(header), 1, fp);
        pdata = (BYTE*)pb->pdata + pb->stride * pb->height;
        for (i=0; i<pb->height; i++) {
            pdata -= pb->stride;
            fwrite(pdata, pb->stride, 1, fp);
        }
        fclose(fp);
    }

    return fp ? 0 : -1;
}

static void bmp_free(BMP *pb)
{
    if (pb->pdata) {
        free(pb->pdata);
        pb->pdata = NULL;
    }
    pb->width  = 0;
    pb->height = 0;
    pb->stride = 0;
}

static void bmp_setpixel(BMP *pb, int x, int y, int r, int g, int b)
{
    BYTE *pbyte = pb->pdata;
    if (x >= pb->width || y >= pb->height) return;
    r = r < 0 ? 0 : r < 255 ? r : 255;
    g = g < 0 ? 0 : g < 255 ? g : 255;
    g = b < 0 ? 0 : b < 255 ? b : 255;
    pbyte[x * 3 + 0 + y * pb->stride] = r;
    pbyte[x * 3 + 1 + y * pb->stride] = g;
    pbyte[x * 3 + 2 + y * pb->stride] = b;
}

static void bmp_getpixel(BMP *pb, int x, int y, int *r, int *g, int *b)
{
    BYTE *pbyte = pb->pdata;
    *r = pbyte[x * 3 + 0 + y * pb->stride];
    *g = pbyte[x * 3 + 1 + y * pb->stride];
    *b = pbyte[x * 3 + 2 + y * pb->stride];
}

static int find_closest_palette_color(BYTE *palette, int palsize, int r, int g, int b)
{
    int mindist = 0x7fffffff;
    int closest = 0;
    int i;

    for (i=0; i<palsize; i++) {
        int curdist = (r - palette[i*3+0]) * (r - palette[i*3+0])
                    + (g - palette[i*3+1]) * (g - palette[i*3+1])
                    + (b - palette[i*3+2]) * (b - palette[i*3+2]);
        if (mindist > curdist) {
            mindist = curdist;
            closest = i;
        }
    }

    return closest;
}

int main(int argc, char *argv[])
{
    char  bmpfile[MAX_PATH] = "test.bmp";
    char  palfile[MAX_PATH] = "palette.dat";
    BYTE  palette[256*3]    = { 0, 0, 0, 255, 255, 255 };
    int   palsize =  2;
    BMP   bmp     = {0};
    int   ret     =  0;
    FILE *fp      = NULL;
    int   i, x, y;

    // handle commmand line
    if (argc >= 3) {
        strcpy(palfile, argv[2]);
    }
    if (argc >= 2) {
        strcpy(bmpfile, argv[1]);
    }

    // load bmp file
    ret = bmp_load(&bmp, bmpfile);
    if (ret < 0) {
        printf("failed to load bmp file: %s\n", bmpfile);
        goto end;
    }

    // load palette
    fp = fopen(palfile, "rb");
    if (fp) {
        for (i=0; ; i++) {
            int r, g, b;
            ret = fscanf(fp, "%d %d %d", &r, &g, &b);
            if (ret == -1) {
                palsize = i;
                break;
            } else {
                palette[i * 3 + 0] = r;
                palette[i * 3 + 1] = g;
                palette[i * 3 + 2] = b;
            }
        }
        fclose(fp);
    }

    // do dither
    for (y=0; y<bmp.height; y++) {
        for (x=0; x<bmp.width; x++) {
            int oldr, oldg, oldb;
            int newr, newg, newb;
            int errr, errg, errb;

            // for pixel (x, y)
            bmp_getpixel(&bmp, x, y, &oldr, &oldg, &oldb);
            i    = find_closest_palette_color(palette, palsize, oldr, oldg, oldb);
            newr = palette[i * 3 + 0];
            newg = palette[i * 3 + 1];
            newb = palette[i * 3 + 2];
            bmp_setpixel(&bmp, x, y, newr, newg, newb);

            // calculate the error
            errr = oldr - newr;
            errg = oldg - newg;
            errb = oldb - newb;

            // for pixel (x+1, y)
            bmp_getpixel(&bmp, x+1, y, &newr, &newg, &newb);
            newr += errr * 7 / 16;
            newg += errg * 7 / 16;
            newb += errb * 7 / 16;
            bmp_setpixel(&bmp, x+1, y, newr, newg, newb);

            // for pixel (x-1, y+1)
            bmp_getpixel(&bmp, x-1, y+1, &newr, &newg, &newb);
            newr += errr * 3 / 16;
            newg += errg * 3 / 16;
            newb += errb * 3 / 16;
            bmp_setpixel(&bmp, x-1, y+1, newr, newg, newb);

            // for pixel (x, y+1)
            bmp_getpixel(&bmp, x, y+1, &newr, &newg, &newb);
            newr += errr * 5 / 16;
            newg += errg * 5 / 16;
            newb += errb * 5 / 16;
            bmp_setpixel(&bmp, x, y+1, newr, newg, newb);

            // for pixel (x+1, y+1)
            bmp_getpixel(&bmp, x+1, y+1, &newr, &newg, &newb);
            newr += errr * 1 / 16;
            newg += errg * 1 / 16;
            newb += errb * 1 / 16;
            bmp_setpixel(&bmp, x+1, y+1, newr, newg, newb);
        }
    }

    // save dither bmp
    ret = bmp_save(&bmp, "dither.bmp");
    if (ret < 0) {
        printf("failed to save dither bmp !\n");
    } else {
        printf("save dither bmp ok !\n");
    }

end:
    bmp_free(&bmp);
    return 0;
}





