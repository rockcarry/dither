#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// 类型定义
typedef unsigned char  uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int   uint32_t;

/* 内部函数实现 */
static int ALIGN(int x, int y) {
    // y must be a power of 2.
    return (x + y - 1) & ~(y - 1);
}


//++ for bmp file ++//
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
//++ for bmp file ++//


//++ for create palette
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
//-- for create palette


//++ for octree
typedef struct tagNODE {
    uint32_t        leaf;
    uint32_t        pcnt;
    struct tagNODE *prev;
    struct tagNODE *next;
    struct tagNODE *child[8];
} NODE;

#define OCTREE_MAX_DEPTH           8
#define NODE_GET_RSUM(node)        ((uint32_t)((node)->child[1]))
#define NODE_GET_GSUM(node)        ((uint32_t)((node)->child[2]))
#define NODE_GET_BSUM(node)        ((uint32_t)((node)->child[3]))
#define NODE_SET_RSUM(node, rsum)  do { (node)->child[1] = (NODE*)(rsum); } while (0)
#define NODE_SET_GSUM(node, gsum)  do { (node)->child[2] = (NODE*)(gsum); } while (0)
#define NODE_SET_BSUM(node, bsum)  do { (node)->child[3] = (NODE*)(bsum); } while (0)

typedef struct {
    NODE  levels[OCTREE_MAX_DEPTH + 1];
    int   colors;
} OCTREE;

static int compare_node(const void *arg1, const void *arg2)
{
    NODE *node1 = *(NODE**)arg1;
    NODE *node2 = *(NODE**)arg2;
    return node1->pcnt - node2->pcnt;
}

static void octree_init(OCTREE *tree)
{
    memset(tree, 0, sizeof(OCTREE));
}

static void octree_free(OCTREE *tree)
{
    NODE *head;
    NODE *node;
    int   i;
    for (i=1; i<=OCTREE_MAX_DEPTH; i++) {
        head = tree->levels[i].next;
        while (head) {
            node = head;
            head = head->next;
            free(node);
        }
    }
}

static void octree_add_color(OCTREE *tree, int r, int g, int b)
{
    NODE *node = &tree->levels[0];
    int   idx, i;

    node->pcnt++; // increase pcnt for root node

    for (i=1; i<=OCTREE_MAX_DEPTH; i++) {
        idx = ((r >> (6 - i)) & (1 << 2))
            | ((g >> (7 - i)) & (1 << 1))
            | ((b >> (8 - i)) & (1 << 0));
        if (!node->child[idx]) {
            // allocate node
            node->child[idx] = calloc(1, sizeof(NODE));

            //++ link node
            node->child[idx]->next = tree->levels[i].next;
            node->child[idx]->prev =&tree->levels[i];
            if (tree->levels[i].next) {
                tree->levels[i].next->prev = node->child[idx];
            }
            tree->levels[i].next = node->child[idx];
            //-- link node

            // update pcnt which means total node number of this level
            tree->levels[i].pcnt += 1;

            if (i == OCTREE_MAX_DEPTH) {
                node->child[idx]->leaf = 1; // it is a leaf
                tree->colors++; // update total number of colors
            }
        }
        node = node->child[idx]; // child
        node->pcnt++; // increase pcnt for child
    }

    // update rsum & gsum & bsum for leaf node
    NODE_SET_RSUM(node, NODE_GET_RSUM(node) + r);
    NODE_SET_GSUM(node, NODE_GET_GSUM(node) + g);
    NODE_SET_BSUM(node, NODE_GET_BSUM(node) + b);
}

static int octree_reduce(OCTREE *tree, int maxcolor)
{
    NODE **list = NULL;
    NODE  *node = NULL;
    int    ret  = -1;
    int    rsum, gsum, bsum;
    int    num_node;
    int    i, j, k;

    for (i=OCTREE_MAX_DEPTH-1; i>=1; i--) {
        // allocate a list for qsort
        num_node = tree->levels[i].pcnt;
        if (num_node == 0) continue;
        list = malloc(num_node * sizeof(NODE*));
        if (!list) break;

        // copy node to list
        j    = 0;
        node = tree->levels[i].next;
        while (node) {
            list[j++] = node;
            node = node->next;
        }

        // qsort list
        qsort(list, num_node, sizeof(NODE*), compare_node);

        // traverse level link list and do reduce
        for (j=0; j<num_node; j++) {
           if (tree->colors <= maxcolor) {
                ret = 0;
                goto done;
            }

            rsum = gsum = bsum = 0; // init rsum, gsum & bsum
            for (k=0; k<8; k++) {
                NODE *child = list[j]->child[k];
                if (!child) continue;
                // reduce rsum, gsum & bsum
                rsum += NODE_GET_RSUM(child);
                gsum += NODE_GET_GSUM(child);
                bsum += NODE_GET_BSUM(child);

                // remove child from level link list
                if (child->prev) child->prev->next = child->next;
                if (child->next) child->next->prev = child->prev;

                free(child); // free memory
                list[j]->child[k] = NULL; // set to NULL
                tree->levels[i+1].pcnt--; // update child level node count
                tree->colors--;           // update number of total colors
            }

            // set reduced rsum, gsum & bsum
            NODE_SET_RSUM(list[j], rsum);
            NODE_SET_GSUM(list[j], gsum);
            NODE_SET_BSUM(list[j], bsum);
            list[j]->leaf = 1; // it is a leaf
            tree->colors++;    // update number of total colors
        }

        // free list
        free(list);
    }

done:
    return ret;
}

static void octree_getpal(OCTREE *tree, uint8_t *pal)
{
    NODE *node;
    int   i;
    for (i=OCTREE_MAX_DEPTH; i>=1; i--) {
        node = tree->levels[i].next;
        while (node) {
            if (node->leaf) {
                *pal++ = NODE_GET_RSUM(node) / node->pcnt;
                *pal++ = NODE_GET_GSUM(node) / node->pcnt;
                *pal++ = NODE_GET_BSUM(node) / node->pcnt;
            }
            node = node->next;
        }
    }
}
//-- for octree



static void build_best_match_pal(uint8_t *pal, int maxcolor, char *file)
{
    BMP      bmp  = {};
    OCTREE   tree = {};
    int      r, g, b;
    int      i, j;

    bmp_load(&bmp, file);
    octree_init(&tree);
    for (i=0; i<bmp.height; i++) {
        for (j=0; j<bmp.width; j++) {
            bmp_getpixel(&bmp, j, i, &r, &g, &b);
            octree_add_color(&tree, r, g, b);
        }
    }
    octree_reduce(&tree, maxcolor);
    octree_getpal(&tree, pal);
    octree_free(&tree);
    bmp_free(&bmp);

    for (i=0; i<maxcolor; i++) {
        printf("%3d %3d %3d\n", pal[i*3+0], pal[i*3+1], pal[i*3+2]);
    }
}

int main(int argc, char *argv[])
{
    uint8_t pal[256*3] = {0};
    int     size       =  0;
    int     i, n;

    if (argc < 3) {
        printf("+----------------------+\n");
        printf(" palette util tool v1.0 \n");
        printf(" written by rockcarry   \n");
        printf("+----------------------+\n");
        printf("\nusage:\n\n");
        printf("palette -g N\n");
        printf(" - create standard gray palette, N is the bits number for gray scale.\n\n");
        printf("palette -c N\n");
        printf(" - create standard color palette, N is the bits number for color component.\n\n");
        printf("palette -p filename N\n");
        printf(" - create best match color palette from bmpfile, N is the max color number.\n\n");
        return 0;
    }

    if (strcmp(argv[1], "-g") == 0) {
        n = atoi(argv[2]);
        n = n < 8 ? n : 8;
        size = 1 << n;
        create_pal_gray(pal, n);
    }

    if (strcmp(argv[1], "-c") == 0) {
        n = atoi(argv[2]);
        n = n < 2 ? n : 2;
        size = 1 << (n*3);
        create_pal_color(pal, n);
    }

    if (strcmp(argv[1], "-p") == 0) {
        if (argc < 4) n = 256;
        else n = atoi(argv[3]);
        n = n < 256 ? n : 256;
        size = n;
        build_best_match_pal(pal, n, argv[2]);
    }

    for (i=0; i<size; i++) {
        printf("%3d %3d %3d\n", pal[i*3+0], pal[i*3+1], pal[i*3+2]);
    }

    return 0;
}
