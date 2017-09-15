#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// 类型定义
typedef unsigned char  uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int   uint32_t;

// 内部类型定义
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
    uint8_t      *pdata  = NULL;
    int           i;

    fp = fopen(file, "rb");
    if (!fp) return -1;

    fread(&header, sizeof(header), 1, fp);
    pb->width  = header.biWidth;
    pb->height = header.biHeight;
    pb->stride = ALIGN(header.biWidth * 3, 4);
    pb->pdata  = malloc(pb->stride * pb->height);
    if (pb->pdata) {
        pdata  = (uint8_t*)pb->pdata + pb->stride * pb->height;
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
    uint8_t      *pdata;
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
        pdata = (uint8_t*)pb->pdata + pb->stride * pb->height;
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
    uint8_t *pbyte = pb->pdata;
    if (x >= pb->width || y >= pb->height) return;
    r = r < 0 ? 0 : r < 255 ? r : 255;
    g = g < 0 ? 0 : g < 255 ? g : 255;
    b = b < 0 ? 0 : b < 255 ? b : 255;
    pbyte[x * 3 + 0 + y * pb->stride] = r;
    pbyte[x * 3 + 1 + y * pb->stride] = g;
    pbyte[x * 3 + 2 + y * pb->stride] = b;
}

static void bmp_getpixel(BMP *pb, int x, int y, int *r, int *g, int *b)
{
    uint8_t *pbyte = pb->pdata;
    if (x >= pb->width || y >= pb->height) {
        *r = *g = *b = 0;
        return;
    }
    *r = pbyte[x * 3 + 0 + y * pb->stride];
    *g = pbyte[x * 3 + 1 + y * pb->stride];
    *b = pbyte[x * 3 + 2 + y * pb->stride];
}

#if 0
static int find_closest_palette_color(uint8_t *palette, int palsize, int r, int g, int b)
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
#endif

//++ octree
typedef struct tagNODE {
    int r;
    int g;
    int b;
    int c;
    struct tagNODE *child[8];
} NODE;

static NODE* octree_create(uint8_t *pal, int size)
{
    NODE *root = calloc(1, sizeof(NODE));
    NODE *node = NULL;
    int   r, g, b, i, j;
    int   idx;

    for (i=0; i<size; i++) {
        r = pal[i*3+0];
        g = pal[i*3+1];
        b = pal[i*3+2];

        node = root; // from root
        for (j=1; j<=8; j++) {
            idx = ((r >> (6 - j)) & (1 << 2))
                | ((g >> (7 - j)) & (1 << 1))
                | ((b >> (8 - j)) & (1 << 0));
            if (node->child[idx] == NULL) {
                node->child[idx]    = calloc(1, sizeof(NODE));
                node->child[idx]->c = -1;  // i != -1 means it is a leaf
            }
            node = node->child[idx];
        }

        node->r = r;
        node->g = g;
        node->b = b;
        node->c = i;
    }

    return root;
}

static void octree_destroy(NODE *root)
{
    int  i;
    for (i=0; i<8; i++) {
        if (root->child[i]) {
            octree_destroy(root->child[i]);
        }
    }
    free(root);
}

static void octree_traverse(NODE *node, int data[5])
{
    int i;

    if (node->c != -1) {
        int curdist = (data[0] - node->r) * (data[0] - node->r)
                    + (data[1] - node->g) * (data[1] - node->g)
                    + (data[2] - node->b) * (data[2] - node->b);
        if (data[3] > curdist) {
            data[3] = curdist;
            data[4] = node->c;
        }
        return;
    }

    for (i=0; i<8; i++) {
        if (node->child[i]) {
            octree_traverse(node->child[i], data);
        }
    }
}

static int octree_find_color(NODE *root, int r, int g, int b)
{
    NODE *node    = root;
    int   idx     = 0;
    int   data[5] = { r, g, b, 0x7fffffff, -1 };
    int   i;

    for (i=1; i<=8; i++) {
        idx = ((r >> (6 - i)) & (1 << 2))
            | ((g >> (7 - i)) & (1 << 1))
            | ((b >> (8 - i)) & (1 << 0));
        if (!node->child[idx]) break;
        node = node->child[idx];
    }

    octree_traverse(node, data);
    return data[4];
}
//-- octree

int main(int argc, char *argv[])
{
    char    bmpfile[MAX_PATH] = "test.bmp";
    char    palfile[MAX_PATH] = "palette.pal";
    char    outfile[MAX_PATH] = "dither-";
    uint8_t palette[256*3]    = { 0, 0, 0, 255, 255, 255 };
    int     palsize =  2;
    BMP     bmp     = {0};
    int     ret     =  0;
    FILE   *fp      = NULL;
    int     i, x, y;
    NODE   *octree  = NULL;

    // handle commmand line
    if (argc >= 3) {
        strcpy(palfile, argv[2]);
    }
    if (argc >= 2) {
        strcpy(bmpfile, argv[1]);
    }
    strcat(outfile, bmpfile);

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

    // create octree
    octree = octree_create(palette, palsize);

    // do dither
    for (y=0; y<bmp.height; y++) {
        for (x=0; x<bmp.width; x++) {
            int oldr, oldg, oldb;
            int newr, newg, newb;
            int errr, errg, errb;

            // for pixel (x, y)
            bmp_getpixel(&bmp, x, y, &oldr, &oldg, &oldb);
//          i    = find_closest_palette_color(palette, palsize, oldr, oldg, oldb);
            i    = octree_find_color(octree, oldr, oldg, oldb);
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

    // destroy octree
    octree_destroy(octree);

    // save dither bmp
    ret = bmp_save(&bmp, outfile);
    if (ret < 0) {
        printf("failed to save dither bmp !\n");
    } else {
        printf("save dither bmp ok !\n");
    }

end:
    bmp_free(&bmp);
    return 0;
}





