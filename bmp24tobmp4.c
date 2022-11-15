#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

/* BMP 对象的类型定义 */
typedef struct {
    int       width;   /* 宽度 */
    int       height;  /* 高度 */
    int       cdepth;  /* 颜色位数 */
    int       stride;  /* 行字节数 */
    uint8_t  *pdata;   /* 指向数据 */
} BMP;

#pragma pack(1)
typedef struct {
    uint16_t  bfType;
    uint32_t  bfSize;
    uint16_t  bfReserved1;
    uint16_t  bfReserved2;
    uint32_t  bfOffBits;
    uint32_t  biSize;
    uint32_t  biWidth;
    uint32_t  biHeight;
    uint16_t  biPlanes;
    uint16_t  biBitCount;
    uint32_t  biCompression;
    uint32_t  biSizeImage;
    uint32_t  biXPelsPerMeter;
    uint32_t  biYPelsPerMeter;
    uint32_t  biClrUsed;
    uint32_t  biClrImportant;
} BMPFILEHEADER;
#pragma pack()

static uint32_t DEF_PAL_DATA[256] = {
    0x000000, 0x000000, 0x000000, 0x000000,
    0x010101, 0x000000, 0x000000, 0x000000,
    0x000000, 0x000000, 0x000000, 0x6E6E6E,
    0x959595, 0xB4B4B4, 0xCFCFCF, 0xFFFFFF,
};

static void bmp_create(BMP *pb, int w, int h, int cdepth)
{
    pb->width = w;
    pb->height= h;
    pb->cdepth= cdepth;
    pb->stride=((w * cdepth + 7) / 8 + 3) & ~3;
    pb->pdata = malloc(pb->height * pb->stride);
}

static void bmp_destroy(BMP *pb)
{
    if (pb && pb->pdata) {
        free(pb->pdata);
        pb->pdata = NULL;
    }
}

static int bmp_load(BMP *pb, char *file)
{
    BMPFILEHEADER header = {0};
    FILE         *fp     = NULL;
    uint8_t      *pdata  = NULL;
    int           ret, i;

    fp = fopen(file, "rb");
    if (!fp) return -1;

    (void)ret;
    ret = fread(&header, 1, sizeof(header), fp);
    pb->width  = header.biWidth;
    pb->height = header.biHeight;
    pb->cdepth = header.biBitCount;
    pb->stride = ((pb->width * pb->cdepth + 7) / 8 + 3) & ~3;
    pb->pdata  = malloc(pb->stride * pb->height);
    if (pb->pdata) {
        fseek(fp, header.bfOffBits, SEEK_SET);
        pdata  = (uint8_t*)pb->pdata + pb->stride * pb->height;
        for (i=0; i<pb->height; i++) {
            pdata -= pb->stride;
            ret = fread(pdata, pb->stride, 1, fp);
        }
    }

    fclose(fp);
    return pb->pdata ? 0 : -1;
}

static int bmp_save(BMP *pb, char *file)
{
    BMPFILEHEADER header = {0};
    FILE         *fp     = NULL;
    uint8_t      *pdata;
    int           palbytes, i;

    palbytes  = (pb->cdepth <= 8) ? (1 << pb->cdepth) : 0;
    palbytes *= 4;

    header.bfType     = ('B' << 0) | ('M' << 8);
    header.bfSize     = sizeof(header) + palbytes + pb->stride * pb->height;
    header.bfOffBits  = sizeof(header) + palbytes;
    header.biSize     = 40;
    header.biWidth    = pb->width;
    header.biHeight   = pb->height;
    header.biPlanes   = 1;
    header.biBitCount = pb->cdepth;
    header.biSizeImage= pb->stride * pb->height;

    fp = fopen(file, "wb");
    if (fp) {
        fwrite(&header, sizeof(header), 1, fp);
        fwrite(DEF_PAL_DATA,  palbytes, 1, fp);
        pdata = (uint8_t*)pb->pdata + pb->stride * pb->height;
        for (i=0; i<pb->height; i++) {
            pdata -= pb->stride;
            fwrite(pdata, pb->stride, 1, fp);
        }
        fclose(fp);
    }

    return fp ? 0 : -1;
}

static void bmp_getpixel_24(BMP *pb, int x, int y, int *r, int *g, int *b)
{
    uint8_t *pbyte = (uint8_t*)pb->pdata;
    if (x >= pb->width || y >= pb->height) {
        *r = *g = *b = 0;
        return;
    }
    *b = pbyte[x * (pb->cdepth / 8) + 0 + y * pb->stride];
    *g = pbyte[x * (pb->cdepth / 8) + 1 + y * pb->stride];
    *r = pbyte[x * (pb->cdepth / 8) + 2 + y * pb->stride];
}

static void bmp_setpixel_4(BMP *pb, int x, int y, int c)
{
    if (x >= pb->width || y >= pb->height) return;
    int offset_byte = y * pb->stride + x * pb->cdepth / 8;
    int offset_bit  = 4 - x * pb->cdepth % 8;
    pb->pdata[offset_byte] &= ~(0xF << offset_bit);
    pb->pdata[offset_byte] |=  (c   << offset_bit);
}

static int color_dist(int r1, int g1, int b1, int r2, int g2, int b2)
{
    return (r1 - r2) * (r1 - r2) + (g1 - g2) * (g1 - g2) + (b1 - b2) * (b1 - b2);
}

static int find_color(uint32_t *pal, int n, int r, int g, int b)
{
    int minidx = 0, mindist = 0x7FFFFFFF, curdist, i;
    for (i=0; i<n; i++) {
        curdist = color_dist(r, g, b, (pal[i] >> 16) & 0xFF, (pal[i] >> 8) & 0xFF, (pal[i] >> 0) & 0xFF);
        if (curdist < mindist) mindist = curdist, minidx = i;
    }
    return minidx;
}

static void bmp24tobmp4(BMP *bmp4, BMP *bmp24)
{
    int i, j;
    for (i=0; i<bmp24->height; i++) {
        for (j=0; j<bmp24->width; j++) {
            int r, g, b;
            bmp_getpixel_24(bmp24, j, i, &r, &g, &b);
            bmp_setpixel_4 (bmp4 , j, i, find_color(DEF_PAL_DATA, 16, r, g, b));
        }
    }
}

int main(int argc, char *argv[])
{
    BMP bmp24 = {}, bmp4 = {};
    char *file = "test.bmp";
    if (argc > 1) file = argv[1];
    bmp_load(&bmp24, file);
    bmp_create(&bmp4, bmp24.width, bmp24.height, 4);
    bmp24tobmp4(&bmp4, &bmp24);
    bmp_save(&bmp4, file);
    bmp_destroy(&bmp4 );
    bmp_destroy(&bmp24);
    return 0;
}
