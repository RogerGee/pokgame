/* pokgame/utils - tilemaker.c

    tilemaker builds tilesheets iteratively from source images; given a source
   file, the program updates a destination database in the current directory
   with tiles from source; a dimension is given for the tiles as well as the
   source number of rows and columns; only tiles which do not exist in the database
   are placed into the database and they must not be repeats of tiles already ripped
   from the source image

    usage:
      tilemaker init <image-dimension>                          // create new database in current directory
      tilemaker compile <compiled-output-raw-image> [rect|seq]  // build tileset image from database
      tilemaker add <source-raw-image> <cols> <rows>            // update database with tiles from source
      tilemaker rm <position-index> ...                         // remove tiles at specified positions
      tilemaker pass <position-index> ...                       // marks tiles as passable (put at end of list)
      tilemaker mv <source-index> <dest-index>                  // reorders tiles such that source is at dest position
*/
#include "lib.h"
#include <dstructs/dynarray.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <dirent.h>

const char* programName;
static const char* const CONFIG_FILE = ".tilemaker.config";
static const char* const TILEDIR = ".tiles";
static struct global_config
{
    int modified;
    size_t dimension;

} config;

static void read_config();
static void write_config();
static struct dynamic_array* get_tile_files(int* highest);
static int image_compar(uint8_t* imgA,uint8_t* imgB,size_t bytec);
static int int_fn_compar(const char** left,const char** right);
static inline int from_tile_name(const char* name);

static void init(size_t dimension);
static void compile(const char* outputFile,int option);
static void add(const char* sourceFile,size_t columns,size_t rows);
static void rm(const char* argv[],int number);
static void pass(const char* argv[],int number);
static void mv(const char* source,const char* destination);

int main(int argc,const char* argv[])
{
    programName = argv[0];
    if (argc < 2) {
        fprintf(stderr,"%s: no arguments\n",argv[0]);
        return 1;
    }

    if (strcmp(argv[1],"init") == 0) {
        size_t dimension;
        if (argc < 3) {
            fprintf(stderr,"%s: init <dimension>\n",argv[0]);
            return 1;
        }
        dimension = atoi(argv[2]);
        if (dimension > 128 || dimension <= 0) {
            fprintf(stderr,"%s: dimension of %zu is invalid\n",argv[0],dimension);
            return 1;
        }
        init(dimension);
    }
    else if (strcmp(argv[1],"compile") == 0) {
        int option = COMPILED_RECTANGLE;
        if (argc < 3) {
            fprintf(stderr,"%s: compile <output-image> [rect|seq]\n",argv[0]);
            return 1;
        }
        if (argc >= 4) {
            if (strcmp(argv[3],"rect") == 0)
                option = COMPILED_RECTANGLE;
            else if (strcmp(argv[3],"seq") == 0)
                option = COMPILED_SEQUENTIAL;
            else {
                fprintf(stderr,"%s: compile: bad option '%s'\n",argv[0],argv[3]);
                return 1;
            }
        }
        read_config();
        compile(argv[2],option);
    }
    else if (strcmp(argv[1],"add") == 0) {
        size_t c, r;
        if (argc < 5) {
            fprintf(stderr,"%s: add <source-image> <columns> <rows>\n",argv[0]);
            return 1;
        }
        c = atoi(argv[3]);
        r = atoi(argv[4]);
        read_config();
        add(argv[2],c,r);
    }
    else if (strcmp(argv[1],"rm") == 0) {
        if (argc < 3) {
            fprintf(stderr,"%s: rm position-index ...\n",argv[0]);
            return 1;
        }
        read_config();
        rm(argv+2,argc-2);
    }
    else if (strcmp(argv[1],"pass") == 0) {
        if (argc < 3) {
            fprintf(stderr,"%s: pass position-index ...\n",argv[0]);
            return 1;
        }
        read_config();
        pass(argv+2,argc-2);
    }
    else if (strcmp(argv[1],"mv") == 0) {
        if (argc < 4) {
            fprintf(stderr,"%s: mv src-index dst-index\n",argv[0]);
            return 1;
        }
        read_config();
        mv(argv[2],argv[3]);
    }
    else {
        fprintf(stderr,"%s: the command '%s' was not recognized\n",argv[0],argv[1]);
        return 1;
    }
    write_config();
    return 0;
}

void read_config()
{
    size_t i;
    FILE* fin;
    char buf[512];
    fin = fopen(CONFIG_FILE,"r");
    if (fin == NULL) {
        fprintf(stderr,"%s: cannot read config file: %s; is this a tilemaker directory?\n",programName,strerror(errno));
        exit(EXIT_FAILURE);
    }
    /* read 'dimension' field */
    if (fread(buf,sizeof(size_t),1,fin) != 1) {
        fprintf(stderr,"%s: fail fread()\n",programName);
        exit(EXIT_FAILURE);
    }
    config.dimension = 0;
    for (i = 0;i < sizeof(size_t);++i)
        config.dimension |= buf[i] << (8*i);
    config.modified = 0;
    fclose(fin);
}
void write_config()
{
    size_t i, iter;
    FILE* fout;
    char buf[512];
    if (config.modified) {
        fout = fopen(CONFIG_FILE,"w");
        if (fout == NULL) {
            fprintf(stderr,"%s: could not write to config file: %s\n",programName,strerror(errno));
            exit(EXIT_FAILURE);
        }
        iter = 0;
        /* write 'dimension' field */
        for (i = 0;i < sizeof(size_t);++i)
            buf[iter++] = (config.dimension >> (8*i)) & 0xff;
        fwrite(buf,iter,1,fout);
        fclose(fout);
    }
}
struct dynamic_array* get_tile_files(int* highest)
{
    DIR* tiledir;
    struct dynamic_array* arry;
    arry = dynamic_array_new();
    /* open tile dir; cd to it for convinience */
    if (chdir(TILEDIR) != 0) {
        fprintf(stderr,"%s: fail chdir()\n",programName);
        exit(EXIT_FAILURE);
    }
    tiledir = opendir(".");
    if (tiledir == NULL) {
        fprintf(stderr,"%s: could not open database: %s; is this a tilemaker directory?\n",programName,strerror(errno));
        exit(EXIT_FAILURE);
    }
    *highest = 0;
    /* grab all the file names in the tiles directory */
    while (1) {
        int a;
        char* fn;
        struct dirent* entry;
        entry = readdir(tiledir);
        if (entry == NULL)
            break;
        if (entry->d_type == DT_REG) {
            a = from_tile_name(entry->d_name);
            if (a > *highest)
                *highest = a;
            fn = malloc(sizeof(entry->d_name));
            strcpy(fn,entry->d_name);
            dynamic_array_pushback(arry,fn);
        }
    }
    if (chdir("..") != 0) {
        fprintf(stderr,"%s: fail chdir()\n",programName);
        exit(EXIT_FAILURE);
    }
    /* sort file names to maintain stability */
    qsort(arry->da_data,arry->da_top,sizeof(char*),(int (*)(const void*,const void*))int_fn_compar);
    *highest += 1;
    closedir(tiledir);
    return arry;
}
int image_compar(uint8_t* imgA,uint8_t* imgB,size_t bytec)
{
    size_t iter;
    for (iter = 0;iter < bytec;++iter)
        if (imgA[iter] != imgB[iter])
            return 0;
    return 1;
}
int int_fn_compar(const char** left,const char** right)
{
    char c[2];
    const char* l, *r;
    l = *left; r = *right;
    c[0] = *l; c[1] = *r;
    if ( isalpha(c[0]) ) {
        if ( isdigit(c[1]) ) {
            if (c[0] == 'p') /* passable tiles */
                return 1;
        }
        ++l;
    }
    if ( isalpha(c[1]) ) {
        if ( isdigit(c[0]) ) {
            if (c[1] == 'p') /* passable tiles */
                return -1;
        }
        ++r;
    }
    return atoi(l) - atoi(r);
}
inline int from_tile_name(const char* name)
{
    return atoi(isalpha(name[0]) ? name+1 : name);
}

void init(size_t dimension)
{
    if (mkdir(TILEDIR,0777) == -1) {
        if (errno == EEXIST)
            fprintf(stderr,"%s: files exist; is this already a tilemaker directory?\n",programName);
        else
            fprintf(stderr,"%s: could not create tiles directory: %s\n",programName,strerror(errno));
        exit(EXIT_FAILURE);
    }
    config.dimension = dimension;
    config.modified = 1;
}

static uint8_t* compile_rectangle(struct dynamic_array* arry,size_t* sz)
{
    /* note: the rectangular image is for viewing (validation) */
    uint8_t* tile;
    uint8_t* result;
    size_t i, iter, rows;
    size_t tilebytes, widthbytes, dimbytes;
    /* compute row count for compiled image; use 16 as the constant column count */
    rows = arry->da_top / 16;
    if (rows % 16 != 0)
        ++rows;
    /* compile the image; fill in extra space with black pixels */
    dimbytes = config.dimension * 3;
    tilebytes = dimbytes * config.dimension;
    tile = malloc(tilebytes);
    if (tile == NULL) {
        fprintf(stderr,"%s: could not allocate enough memory for operation\n",programName);
        exit(EXIT_FAILURE);
    }
    widthbytes = 3 * 16 * config.dimension;
    *sz = widthbytes * rows * config.dimension;
    result = malloc(*sz);
    if (result == NULL) {
        fprintf(stderr,"%s: could not allocate enough memory for operation\n",programName);
        exit(EXIT_FAILURE);
    }
    if (chdir(TILEDIR) != 0) {
        fprintf(stderr,"%s: fail chdir()\n",programName);
        exit(EXIT_FAILURE);
    }
    for (i = 0,iter = 0;i < rows;++i) {
        size_t j;
        for (j = 0;j < 16;++j) {
            size_t k, l, m;
            size_t offset;
            int doTile = 0;
            if (iter < arry->da_top) {
                /* read tile image into memory */
                load_image_rgb(arry->da_data[iter++],tile,config.dimension,config.dimension,0);
                doTile = 1;
            }
            /* write tile data into image at position {j,i} */
            offset = i * 16 * tilebytes + j * config.dimension * 3;
            m = 0;
            for (k = 0;k < config.dimension;++k) {
                size_t cpy = offset;
                for (l = 0;l < dimbytes;++l)
                    result[cpy++] = doTile ? tile[m++] : 0xff;
                offset += widthbytes;
            }
        }
    }
    if (chdir("..") != 0) {
        fprintf(stderr,"%s: fail chdir()\n",programName);
        exit(EXIT_FAILURE);
    }
    free(tile);
    return result;
}
static uint8_t* compile_sequential(struct dynamic_array* arry,size_t* sz)
{
    /* note: this image is for pokgame; we include 8 bytes up front for the image dimensions;
       pokgame uses this to determine the number of tiles */
    size_t i, iter, tilebytes;
    uint8_t* tile;
    uint8_t* result;
    uint32_t height;
    tilebytes = 3 * config.dimension * config.dimension;
    tile = malloc(tilebytes);
    if (tile == NULL) {
        fprintf(stderr,"%s: could not allocate enough memory for operation\n",programName);
        exit(EXIT_FAILURE);
    }
    *sz = arry->da_top * tilebytes + 8;
    result = malloc(*sz);
    if (result == NULL) {
        fprintf(stderr,"%s: could not allocate enough memory for operation\n",programName);
        exit(EXIT_FAILURE);
    }
    /* write width and height to result image */
    iter = 0;
    height = config.dimension * arry->da_top;
    for (i = 0;i < 4;++i)
        result[iter++] = ((uint32_t)config.dimension >> (8*i)) & 0xff;
    for (i = 0;i < 4;++i)
        result[iter++] = (height >> (8*i)) & 0xff;
    /* place tiles inside the result image */
    if (chdir(TILEDIR) != 0) {
        fprintf(stderr,"%s: fail chdir()\n",programName);
        exit(EXIT_FAILURE);
    }
    for (i = 0;i < arry->da_top;++i) {
        size_t j;
        /* read tile image into memory */
        load_image_rgb(dynamic_array_getat(arry,i),tile,config.dimension,config.dimension,0);
        for (j = 0;j < tilebytes;++j)
            result[iter++] = tile[j];
    }
    if (chdir("..") != 0) {
        fprintf(stderr,"%s: fail chdir()\n",programName);
        exit(EXIT_FAILURE);
    }
    free(tile);
    return result;
}
void compile(const char* outputFile,int option)
{
    int a;
    FILE* fout;
    size_t size;
    size_t iter;
    uint8_t* result;
    struct dynamic_array* arry;
    arry = get_tile_files(&a);
    /* get result image */
    if (option == COMPILED_RECTANGLE)
        result = compile_rectangle(arry,&size);
    else
        result = compile_sequential(arry,&size);
    /* write image to file */
    fout = fopen(outputFile,"w");
    if (fout == NULL) {
        fprintf(stderr,"%s: could not open output file: %s\n",programName,strerror(errno));
        exit(EXIT_FAILURE);
    }
    iter = 0;
    while (size > 0) {
        size_t r = fwrite(result+iter,1,size,fout);
        if (r == 0)
            break;
        size -= r;
        iter += r;
    }
    /* cleanup */
    fclose(fout);
    free(result);
    dynamic_array_free_ex(arry,free);
}

void add(const char* sourceFile,size_t columns,size_t rows)
{
    int a, c, d;
    size_t i, x, y;
    uint8_t* src;
    size_t tilebytes, widthbytes, dimbytes;
    struct dynamic_array* arry;
    /* load source image */
    dimbytes = 3 * config.dimension;
    widthbytes = 3 * config.dimension * columns;
    tilebytes = config.dimension * dimbytes;
    src = malloc(columns*rows*tilebytes);
    if (src == NULL) {
        fprintf(stderr,"%s: could not allocate enough memory for operation\n",programName);
        exit(EXIT_FAILURE);
    }
    load_image_rgb(sourceFile,src,columns*config.dimension,rows*config.dimension,1);
    /* load tiles; first get all tile file names; then load them into memory, replacing
       the file names with the tile structure */
    arry = get_tile_files(&a);
    if (chdir(TILEDIR) != 0) {
        fprintf(stderr,"%s: fail chdir()\n",programName);
        exit(EXIT_FAILURE);
    }
    for (i = 0;i < arry->da_top;++i) {
        uint8_t* t;
        t = malloc(tilebytes);
        if (t == NULL) {
            fprintf(stderr,"%s: could not allocate enough memory for operation\n",programName);
            exit(EXIT_FAILURE);
        }
        load_image_rgb(arry->da_data[i],t,config.dimension,config.dimension,0);
        free(arry->da_data[i]);
        arry->da_data[i] = t;
    }
    /* reduce the source image into individual tile images */
    c = 0; d = 0;
    for (y = 0;y < rows;++y) {
        size_t offset = tilebytes*columns*y;
        for (x = 0;x < columns;++x) {
            uint8_t* tile;
            size_t m, n, off = offset;
            off += config.dimension * x * 3;
            /* allocate tile structure */
            tile = malloc(tilebytes);
            if (tile == NULL) {
                fprintf(stderr,"%s: could not allocate enough memory for operation\n",programName);
                exit(EXIT_FAILURE);
            }
            /* copy tile data */
            i = 0;
            for (m = 0;m < config.dimension;++m) {
                size_t cpy = off;
                for (n = 0;n < dimbytes;++n)
                    tile[i++] = src[cpy++];
                off += widthbytes;
            }
            /* check tile against all the other tiles */
            m = 1;
            for (i = 0;i < arry->da_top;++i) {
                if ( image_compar(tile,arry->da_data[i],tilebytes) ) {
                    m = 0;
                    break;
                }
            }
            /* save and add the tile or else free it */
            if (m) {
                char name[16];
                snprintf(name,sizeof(name),"%d",a++);
                save_image_rgb(name,tile,config.dimension,config.dimension,1);
                /* add the tile to the list */
                dynamic_array_pushback(arry,tile);
                ++c;
            }
            else {
                free(tile);
                ++d;
            }
        }
    }
    if (chdir("..") != 0) {
        fprintf(stderr,"%s: fail chdir()\n",programName);
        exit(EXIT_FAILURE);
    }
    printf("added %d tiles\n",c);
    printf("ignored %d tiles\n",d);
    dynamic_array_free_ex(arry,free);
    free(src);
}

void rm(const char* argv[],int number)
{
    int i, dum;
    struct dynamic_array* arry;
    arry = get_tile_files(&dum);
    if (chdir(TILEDIR) != 0) {
        fprintf(stderr,"%s: fail chdir()\n",programName);
        exit(EXIT_FAILURE);
    }
    for (i = 0;i < number;++i) {
        size_t pos = atoi(argv[i]) - 1;
        if (pos < arry->da_top) {
            if (unlink((const char*)arry->da_data[pos]) == -1)
                fprintf(stderr,"%s: could not remove '%s': %s\n",programName,(char*)arry->da_data[pos],strerror(errno));
            else
                printf("removed tile file '%s' at position %zu\n",(char*)arry->da_data[pos],pos+1);
        }
        else
            fprintf(stderr,"%s: incorrect position '%zu'\n",programName,pos+1);
    }
    if (chdir("..") != 0) {
        fprintf(stderr,"%s: fail chdir()\n",programName);
        exit(EXIT_FAILURE);
    }
    dynamic_array_free_ex(arry,free);
}

void pass(const char* argv[],int number)
{
    int i, next;
    struct dynamic_array* arry;
    arry = get_tile_files(&next);
    if (chdir(TILEDIR) != 0) {
        fprintf(stderr,"%s: fail chdir()\n",programName);
        exit(EXIT_FAILURE);
    }
    for (i = 0;i < number;++i) {
        size_t pos = atoi(argv[i]) - 1;
        if (pos < arry->da_top) {
            /* rename file name to have special 'p' prefix for passable */
            char name[16];
            const char* oldName;
            oldName = (const char*)arry->da_data[pos];
            if ( isalpha(oldName[0]) ) {
                fprintf(stderr,"%s: file '%s' at position %zu is already special\n",programName,oldName,pos+1);
                continue;
            }
            snprintf(name,sizeof(name),"p%s",oldName);
            if (rename(oldName,name) == -1)
                fprintf(stderr,"%s: could not rename '%s': %s\n",programName,oldName,strerror(errno));
            else
                printf("marked tile file '%s' at position %zu as passable\n",oldName,pos+1);
        }
        else
            fprintf(stderr,"%s: incorrect position '%zu'\n",programName,pos+1);
    }
    if (chdir("..") != 0) {
        fprintf(stderr,"%s: fail chdir()\n",programName);
        exit(EXIT_FAILURE);
    }
    dynamic_array_free_ex(arry,free);
}

static void mv_recursive(const char* src,const char* dst)
{ /* recursively insert 'src' to 'dst' */
    static struct stat st;
    if (stat(dst,&st) == 0) {
        /* destination exists; move the destination to the next available space
           by renaming it recursively */
        int a;
        char name[16];
        a = from_tile_name(dst) + 1;
        if (isalpha(dst[0]))
            snprintf(name,sizeof(name),"%c%d",dst[0],a);
        else
            snprintf(name,sizeof(name),"%d",a);
        mv_recursive(dst,name);
        /* fall through to rename 'src' to 'dst' */
    }
    else if (errno != ENOENT) {
        fprintf(stderr,"%s: could not stat file '%s'\n",programName,dst);
        exit(EXIT_FAILURE);
    }
    /* else: file doesn't exist; control will fall through to move the file */
    if (rename(src,dst) == -1)
        fprintf(stderr,"%s: couldnot rename '%s' to '%s': %s\n",programName,src,dst,strerror(errno));
    printf("%s -> %s\n",src,dst);
}
void mv(const char* source,const char* destination)
{
    int top;
    int src, dst;
    const char* srcName;
    const char* dstName;
    struct dynamic_array* arry;
    arry = get_tile_files(&top);
    src = atoi(source)-1;
    dst = atoi(destination)-1;
    if (chdir(TILEDIR) != 0) {
        fprintf(stderr,"%s: fail chdir()\n",programName);
        exit(EXIT_FAILURE);
    }
    if (src<0 || dst<0 || src>=arry->da_top || dst>=arry->da_top) {
        fprintf(stderr,"%s: bad src./dest. positions\n",programName);
        exit(EXIT_FAILURE);
    }
    if (src == dst) {
        fprintf(stderr,"%s: src. and dest. are the same\n",programName);
        exit(EXIT_FAILURE);
    }
    srcName = (const char*)arry->da_data[src];
    dstName = (const char*)arry->da_data[dst];
    if (isalpha(srcName[0]) && !isalpha(dstName[0])) {
        fprintf(stderr,"%s: cannot move passable tile to impassable section\n",programName);
        exit(EXIT_FAILURE);
    }
    else if (!isalpha(srcName[0]) && isalpha(dstName[0])) {
        fprintf(stderr,"%s: cannot move impassable tile to passable section\n",programName);
        exit(EXIT_FAILURE);
    }
    /* rename source name to 'trans' so that it can be recycled */
    if (rename(srcName,"trans") == -1) {
        fprintf(stderr,"%s: fail rename()\n",programName);
        exit(EXIT_FAILURE);
    }
    /* recursively rename 'trans' to dest. */
    mv_recursive("trans",dstName);
    if (chdir("..") != 0) {
        fprintf(stderr,"%s: fail chdir()\n",programName);
        exit(EXIT_FAILURE);
    }
    puts("tile moved");
    dynamic_array_free_ex(arry,free);
}
