#include <stdlib.h>
#include <stdio.h>
//#include <conio.h>

typedef unsigned char  uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int   uint32_t;

static int ALIGN(int x, int y) {
    // y must be a power of 2.
    return (x + y - 1) & ~(y - 1);
}

static void create_pal_gray(uint8_t *pal, int nbits)
{
    int n = (1 << nbits) - 1;
    int d = 255 / n;
    int c;
    for (c=0; c<=255; c+=d) {
        *pal++ = c;
        *pal++ = c;
        *pal++ = c;
//      printf("%3d %3d %3d\n", c, c, c);
    }
}

static void create_pal_color(uint8_t *pal, int nbits)
{
    int n = (1 << nbits) - 1;
    int d = 255 / n;
    int r, g, b;
    for (r=0; r<=255; r+=d) {
        for (g=0; g<=255; g+=d) {
            for (b=0; b<=255; b+=d) {
                *pal++ = r;
                *pal++ = g;
                *pal++ = b;
//              printf("%3d %3d %3d\n", r, g, b);
            }
        }
    }
}

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    int     n;
} COLORITEM;

static int color_item_cmp(const void *a, const void *b)
{
    return ((COLORITEM*)b)->n - ((COLORITEM*)a)->n;
}

static COLORITEM* cal_color_freq_list(char *file)
{
    COLORITEM*list   = NULL;
    FILE     *fp     = NULL;
    uint8_t  *image  = NULL;
    uint16_t *colors = NULL;
    uint8_t  *pbyte  = NULL;
    uint32_t  width  = 0;
    uint32_t  height = 0;
    uint32_t  stride = 0;
    int       total  = 0;
    long      len    = 0;
    int       i, j;

    fp = fopen(file, "rb");
    if (!fp) return NULL;

    // read width & height
    fseek(fp, 18, SEEK_SET);
    fread(&width , sizeof(uint32_t), 1, fp);
    fread(&height, sizeof(uint32_t), 1, fp);
    stride = ALIGN(width*3, 4);
    len    = stride * height;

    // allocate memory
    image  = malloc(len);
    colors = calloc(1, sizeof(uint16_t) * (1<<15));
    if (!image || !colors) {
        printf("failed to allocate memory !\n");
        goto exit;
    }

    // read image data
    fseek(fp, 54, SEEK_SET );
    fread(image, len, 1, fp);

    // counter color freq
    pbyte = image;
    for (i=0; i<height; i++) {
        for (j=0; j<width; j++) {
            int r = *pbyte++ >> 3;
            int g = *pbyte++ >> 3;
            int b = *pbyte++ >> 3;
            int c = (r << 0) | (g << 5) | (b << 10);
            if (!colors[c]) {
                total++;
            }
            if (colors[c] < 0xffff) {
                colors[c]++;
            }
        }
        pbyte -= width * 3;
        pbyte += stride;
    }

    list = malloc(total * sizeof(COLORITEM));
    for (i=0,j=0; i<(1<<15); i++) {
        if (colors[i]) {
            list[j].r = (i >> 0) & 0x1f;
            list[j].g = (i >> 5) & 0x1f;
            list[j].b = (i >>10) & 0x1f;
            list[j].r = (list[j].r << 3) | (list[j].r & 0x7);
            list[j].g = (list[j].g << 3) | (list[j].g & 0x7);
            list[j].b = (list[j].b << 3) | (list[j].b & 0x7);
            list[j].n = colors[i];
            j++;
        }
    }
    qsort(list, total, sizeof(COLORITEM), color_item_cmp);

#if 0
    printf("total color: %d\n", total);
    for (i=0; i<total; i++) {
        printf("%02x %02x %02x %d\n", list[i].r, list[i].g, list[i].b, list[i].n);
    }
    printf("\n");
#endif

exit:
    if (image ) free(image );
    if (colors) free(colors);
    if (fp) fclose(fp);
    return list;
}

int main(void)
{
#if 0
    BYTE pal[256*3];
    create_pal_gray (pal, 1); printf("\n");
    create_pal_gray (pal, 2); printf("\n");
    create_pal_gray (pal, 4); printf("\n");
    create_pal_color(pal, 1); printf("\n");
    create_pal_color(pal, 2); printf("\n");
#endif

    COLORITEM *list = cal_color_freq_list("test.bmp");
    free(list);

    return 0;
}
