/* vlerq.c - this is the single-source-file version of Vlerq for Tcl */

/* %includeDir<../src>% */
/* %includeDir<../src_tcl>% */

/* %include<bits.c>% */
/*
 * bits.c - Implementation of bitwise operations.
 */

#include <stdlib.h>
#include <string.h>

/*
 * intern.h - Internal definitions.
 */

#include <stddef.h>
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif
#include <unistd.h>

typedef ptrdiff_t Int_t; /* large enough to hold a pointer */

typedef unsigned char Byte_t;

#ifdef OBJECT_TYPE
typedef OBJECT_TYPE Object_p;
#else
/*
 * defs.h - Binding-specific definitions needed by the general code.
 */

typedef struct Tcl_Obj *Object_p;
#endif

#if !defined(VALUES_MUST_BE_ALIGNED) && (defined(__hppa) || defined(__sparc__) || defined(__sgi__))
#define VALUES_MUST_BE_ALIGNED 1
#endif

#if defined(WORDS_BIGENDIAN)
#if !defined(_BIG_ENDIAN) || _BIG_ENDIAN == ""
#define _BIG_ENDIAN 1
#endif
#endif

#if DEBUG+0
#define Assert(x) do if (!(x)) FailedAssert(#x, __FILE__, __LINE__); while (0)
#define DbgIf(x)  if (GetShared()->debug && (x))
#else
#define Assert(x)
#define DbgIf(x)  if (0)
#endif

#define DBG_trace   (1<<0)
#define DBG_deps    (1<<1)

#define PUSH_KEEP_REFS                            \
  { Shared_p _shared = GetShared();               \
    struct Buffer _keep, *_prev = _shared->keeps; \
    InitBuffer(_shared->keeps = &_keep);
    
#define POP_KEEP_REFS         \
    ReleaseSequences(&_keep); \
    _shared->keeps = _prev;   \
  }

/* keep in sync with CharAsItemType() in column.c */
typedef enum ItemTypes {
    IT_unknown,
    IT_int,
    IT_wide,
    IT_float,
    IT_double,
    IT_string,
    IT_bytes,
    IT_object,
    IT_column,
    IT_view,
    IT_error
} ItemTypes;

typedef enum ErrorCodes {
    EC_cioor,
    EC_rioor,
    EC_cizwv,
    EC_nmcw,
    EC_rambe,
    EC_nalor,
    EC_wnoa
} ErrorCodes;

struct Shared {
    struct SharedInfo *info;   /* ext_tcl.c */
    int                refs;   /* ext_tcl.c */
    int                debug;  /* debug flags */
    struct Buffer     *keeps;  /* column.c */
    struct Sequence   *empty;  /* view.c */
};

typedef struct Shared   *Shared_p;
typedef struct SeqType  *SeqType_p;
typedef struct Sequence *Seq_p;
typedef struct Column   *Column_p;
typedef union Item      *Item_p;
typedef Seq_p            View_p;

typedef ItemTypes  (*Getter) (int row, Item_p item);
typedef void       (*Cleaner) (Seq_p seq);

struct SeqType {
  const char *name;      /* for introspection only */
  Getter      getfun;    /* item accessor, copied to Sequence during setup */
  short       cleanups;  /* octal for data[0..3].p: 0=none, 1=decref, 2=free */
  Cleaner     cleaner;   /* called before performing the above cleanups */
};

struct Sequence {
  int        count;   /* number of entries */
  int        refs;    /* reference count */
  SeqType_p  type;    /* type dispatch table */
  Getter     getter;  /* item accessor function */
  union { int i; void *p; Seq_p q; Object_p o; } data[4]; /* getter-specific */
};

#define IncRefCount(seq) AdjustSeqRefs(seq, 1)
#define DecRefCount(seq) AdjustSeqRefs(seq, -1)
#define KeepRef(seq) IncRefCount(LoseRef(seq))

typedef struct Column {
    Seq_p        seq;    /* the sequence which will handle accesses */
    int          pos;    /* position of this column in the view */
} Column;              
                       
typedef union Item {
    char         b [8];  /* used for raw access and byte-swapping */
    int          q [2];  /* used for hash calculation of wides and doubles */
    int          i;      /* IT_int */
    int64_t      w;      /* IT_wide */
    float        f;      /* IT_float */
    double       d;      /* IT_double */
    const void  *p;      /* used to convert pointers */
    const char  *s;      /* IT_string */
    struct { const Byte_t *ptr; int len; } u; /* IT_bytes */
    Object_p     o;      /* IT_object */
    Column       c;      /* IT_column */
    View_p       v;      /* IT_view */
    ErrorCodes   e;      /* IT_error */
} Item;

/* column.c */

void      *(AdjustSeqRefs) (void *refs, int count);
void       (AppendToStrVec) (const void *s, int bytes, Seq_p seq);
int        (CastObjToItem) (char type, Item_p item);
ItemTypes  (CharAsItemType) (char type);
Column     (CoerceColumn) (ItemTypes type, Object_p obj);
void       (FailedAssert) (const char *msg, const char *file, int line);
Column     (ForceStringColumn) (Column column);
Item       (GetColItem) (int row, Column column, ItemTypes type);
ItemTypes  (GetItem) (int row, Item_p item);
Item       (GetSeqItem) (int row, Seq_p seq, ItemTypes type);
Item       (GetViewItem) (View_p view, int row, int col, ItemTypes type);
Seq_p      (LoseRef) (Seq_p seq);
Seq_p      (NewBitVec) (int count);
Seq_p      (NewIntVec) (int count, int **dataptr);
Seq_p      (NewPtrVec) (int count, Int_t **dataptr);
Seq_p      (NewSequence) (int count, SeqType_p type, int auxbytes);
Seq_p      (NewSequenceNoRef) (int count, SeqType_p type, int auxbytes);
Seq_p      (NewSeqVec) (ItemTypes type, const Seq_p *p, int n);
Seq_p      (NewStrVec) (int istext);
Seq_p      (FinishStrVec) (Seq_p seq);
void       (ReleaseSequences) (struct Buffer *keep);
Seq_p      (ResizeSeq) (Seq_p seq, int pos, int diff, int elemsize);
Column     (SeqAsCol) (Seq_p seq);

/* buffer.c */

typedef struct Buffer *Buffer_p;
typedef struct Overflow *Overflow_p;

struct Buffer {
    union { char *c; int *i; const void **p; } fill;
    char       *limit;
    Overflow_p  head;
    Int_t       saved;
    Int_t       used;
    char       *ofill;
    char       *result;
    char        buf [128];
    char        slack [8];
};

#define ADD_ONEC_TO_BUF(b,x) (*(b).fill.c++ = (x))

#define ADD_CHAR_TO_BUF(b,x) \
          { char _c = (x); \
            if ((b).fill.c < (b).limit) *(b).fill.c++ = _c; \
              else AddToBuffer(&(b), &_c, sizeof _c); }

#define ADD_INT_TO_BUF(b,x) \
          { int _i = (x); \
            if ((b).fill.c < (b).limit) *(b).fill.i++ = _i; \
              else AddToBuffer(&(b), &_i, sizeof _i); }

#define ADD_PTR_TO_BUF(b,x) \
          { const void *_p = (x); \
            if ((b).fill.c < (b).limit) *(b).fill.p++ = _p; \
              else AddToBuffer(&(b), &_p, sizeof _p); }

#define BufferFill(b) ((b)->saved + ((b)->fill.c - (b)->buf))

void   (InitBuffer) (Buffer_p bp);
void   (ReleaseBuffer) (Buffer_p bp, int keep);
void   (AddToBuffer) (Buffer_p bp, const void *data, Int_t len);
void  *(BufferAsPtr) (Buffer_p bp, int fast);
Seq_p  (BufferAsIntVec) (Buffer_p bp);
int    (NextBuffer) (Buffer_p bp, char **firstp, int *countp);

/* view.c */

typedef enum MetaCols {
  MC_name, MC_type, MC_subv, MC_limit
} MetaCols;

#define V_Cols(view) ((Column_p) ((Seq_p) (view) + 1))
#define V_Types(view) (*(char**) ((view)->data))
#define V_Meta(view) (*(View_p*) ((view)->data+1))

#define ViewAsCol(v) SeqAsCol(v)
#define ViewWidth(view) ((view)->count)
#define ViewCol(view,col) (V_Cols(view)[col])
#define ViewColType(view,col) ((ItemTypes) V_Types(view)[col])
#define ViewCompat(view1,view2) MetaIsCompatible(V_Meta(view1), V_Meta(view2))

View_p  (DescAsMeta) (const char** desc, const char* end);
View_p  (EmptyMetaView) (void);
char    (GetColType) (View_p meta, int col);
View_p  (IndirectView) (View_p meta, Seq_p seq);
View_p  (NewView) (View_p meta);
View_p  (MakeIntColMeta) (const char *name);
int     (MetaIsCompatible) (View_p meta1, View_p meta2);
void    (ReleaseUnusedViews) (void);
void    (SetViewCols) (View_p view, int first, int count, Column src);
void    (SetViewSeqs) (View_p view, int first, int count, Seq_p src);

/* hash.c */

int    (HashMapAdd) (Seq_p hmap, int key, int value);
int    (HashMapLookup) (Seq_p hmap, int key, int defval);
Seq_p  (HashMapNew) (void);
int    (HashMapRemove) (Seq_p hmap, int key);

/* file.c */

typedef Seq_p MappedFile_p;

MappedFile_p  (InitMappedFile) (const char *data, Int_t length, Cleaner fun);
View_p        (MappedToView) (MappedFile_p map, View_p base);
View_p        (OpenDataFile) (const char *filename);

/* emit.c */

typedef void *(*SaveInitFun)(void*,Int_t);
typedef void *(*SaveDataFun)(void*,const void*,Int_t);

void   (MetaAsDesc) (View_p meta, Buffer_p buffer);
Int_t  (ViewSave) (View_p view, void *aux, SaveInitFun, SaveDataFun, int);

/* getters.c */

SeqType_p (PickIntGetter) (int bits);
SeqType_p (FixedGetter) (int bytes, int rows, int real, int flip);

/* bits.c */
      
char  *(Bits2elias) (const char *bytes, int count, int *outsize);
void   (ClearBit) (Seq_p bitmap, int row);
int    (CountBits) (Seq_p seq);
Seq_p  (MinBitCount) (Seq_p *pbitmap, int count);
int    (NextBits) (Seq_p bits, int *fromp, int *countp);
int    (NextElias) (const char *bytes, int count, int *inbits);
int    (SetBit) (Seq_p *pbitmap, int row);
void   (SetBitRange) (Seq_p bits, int from, int count);
int    (TestBit) (Seq_p bitmap, int row);
int    (TopBit) (int v);

/* mutable.c */

enum MutPrepares {
    MP_insdat, MP_adjdat, MP_usemap, MP_delmap, MP_adjmap,
    MP_insmap, MP_revmap, MP_limit
};

int     (IsMutable) (View_p view);
View_p  (MutableView) (View_p view);
Seq_p   (MutPrepare) (View_p view);
View_p  (ViewSet) (View_p view, int row, int col, Item_p item);

/* the following must be supplied in the language binding */

int            (ColumnByName) (View_p meta, Object_p obj);
struct Shared *(GetShared) (void);
void           (InvalidateView) (Object_p obj);
Object_p       (ItemAsObj) (ItemTypes type, Item_p item);
Object_p       (NeedMutable) (Object_p obj);
Column         (ObjAsColumn) (Object_p obj);
View_p         (ObjAsView) (Object_p obj);
int            (ObjToItem) (ItemTypes type, Item_p item);

int TestBit (Seq_p bitmap, int row) {
    if (bitmap != NULL && row < bitmap->count) {
        const Byte_t *bits = bitmap->data[0].p;
        return (bits[row>>3] >> (row&7)) & 1;
    }
    return 0;
}

int SetBit (Seq_p *pbitmap, int row) {
    Byte_t *bits;
    Seq_p bitmap = *pbitmap;
    
    if (bitmap == NULL)
        *pbitmap = bitmap = IncRefCount(NewBitVec(row+1));

    if (TestBit(bitmap, row))
        return 0;
    
    if (row >= bitmap->count) {
        int bytes = bitmap->data[1].i;
        if (row >= 8 * bytes) {
            Seq_p newbitmap = IncRefCount(NewBitVec(12 * bytes));
            memcpy(newbitmap->data[0].p, LoseRef(bitmap)->data[0].p, bytes);
            *pbitmap = bitmap = newbitmap;
        }
        bitmap->count = row + 1;
    }

    bits = bitmap->data[0].p;
    bits[row>>3] |= 1 << (row&7);
    
    return 8 * bitmap->data[1].i;
}

void ClearBit (Seq_p bitmap, int row) {
    if (TestBit(bitmap, row)) {
        Byte_t *bits = bitmap->data[0].p;
        bits[row>>3] &= ~ (1 << (row&7));
    }
}

void SetBitRange (Seq_p bits, int from, int count) {
    while (--count >= 0)
        SetBit(&bits, from++);
}

Seq_p MinBitCount (Seq_p *pbitmap, int count) {
    if (*pbitmap == NULL || (*pbitmap)->count < count) {
        SetBit(pbitmap, count);
        ClearBit(*pbitmap, count);
        --(*pbitmap)->count;
    }
    return *pbitmap;
}

int NextBits (Seq_p bits, int *fromp, int *countp) {
    int curr = *fromp + *countp;
    
    while (curr < bits->count && !TestBit(bits, curr))
        ++curr;
        
    *fromp = curr;
    
    while (curr < bits->count && TestBit(bits, curr))
        ++curr;
        
    *countp = curr - *fromp;
    return *countp;
}

int TopBit (int v) {
#define Vn(x) (v < (1<<x))
    return Vn(16) ? Vn( 8) ? Vn( 4) ? Vn( 2) ? Vn( 1) ? v-1 : 1
                                             : Vn( 3) ?  2 :  3
                                    : Vn( 6) ? Vn( 5) ?  4 :  5
                                             : Vn( 7) ?  6 :  7
                           : Vn(12) ? Vn(10) ? Vn( 9) ?  8 :  9
                                             : Vn(11) ? 10 : 11
                                    : Vn(14) ? Vn(13) ? 12 : 13
                                             : Vn(15) ? 14 : 15
                  : Vn(24) ? Vn(20) ? Vn(18) ? Vn(17) ? 16 : 17
                                             : Vn(19) ? 18 : 19
                                    : Vn(22) ? Vn(21) ? 20 : 21
                                             : Vn(23) ? 22 : 23
                           : Vn(28) ? Vn(26) ? Vn(25) ? 24 : 25
                                             : Vn(27) ? 26 : 27
                                    : Vn(30) ? Vn(29) ? 28 : 29
                                             : Vn(31) ? 30 : 31;
#undef Vn
}

static void EmitBits(char *bytes, int val, int *ppos) {
    char *curr = bytes + (*ppos >> 3);
    int bit = 8 - (*ppos & 7);
    int top = TopBit(val), n = top + 1;
    
    Assert(val > 0);
    
    while (top >= bit) {
        ++curr;
        top -= bit;
        bit = 8;
    }
    
    bit -= top;
    
    while (n >= bit) {
        *curr++ |= (char) (val >> (n-bit));
        val &= (1 << (n-bit)) - 1;
        n -= bit;
        bit = 8;
    }
    
    *curr |= (char) (val << (bit - n));
    *ppos = ((curr - bytes) << 3) + (8 - bit) + n;
}

char *Bits2elias (const char *bytes, int count, int *outbits) {
    int i, last, bits = 0;
    char *out;
    
    if (count <= 0) {
        *outbits = 0;
        return NULL;
    }
    
    out = calloc(1, (count+count/2)/8+1);
    last = *bytes & 1;
    *out = last << 7;
    *outbits = 1;
    for (i = 0; i < count; ++i) {
        if (((bytes[i>>3] >> (i&7)) & 1) == last)
            ++bits;
        else {
            EmitBits(out, bits, outbits);
            bits = 1;
            last ^= 1;
        }
    }

    if (bits)
        EmitBits(out, bits, outbits);
    
    return out;
}

int NextElias (const char *bytes, int count, int *inbits) {
    int val = 0;
    const char *in;
    
    if (*inbits == 0)
        *inbits = 1;
    
    in = bytes + (*inbits >> 3);
    if (in < bytes + count) {
        int i = 0, bit = 8 - (*inbits & 7);
        
        for (;;) {
            ++i;
            if (*in & (1 << (bit-1)))
                break;
            if (--bit <= 0) {
                if (++in >= bytes + count)
                    return 0;
                bit = 8;
            }
        }
        
        while (i >= bit) {
            val <<= bit;
            val |= (*in++ & ((1 << bit) - 1));
            i -= bit;
            bit = 8;
        }
        
        val <<= i;
        val |= (*in >> (bit - i)) & ((1 << i) - 1);
        *inbits = ((in - bytes) << 3) + (8 - bit) + i;
    }
    
    return val;
}

int CountBits (Seq_p seq) {
    int result = 0, from = 0, count = 0;

    if (seq != NULL)
        while (NextBits(seq, &from, &count))
            result += count;

    return result;
}

ItemTypes BitRunsCmd_i (Item_p a) {
    int i, outsize, count = a->c.seq->count, pos = 0;
    char *data;
    struct Buffer buffer;
    Seq_p temp;

    temp = NewBitVec(count);
    
    for (i = 0; i < count; ++i) {
        if (GetColItem(i, a->c, IT_int).i)
            SetBit(&temp, i);
    }

    data = Bits2elias(temp->data[0].p, count, &outsize);

    count = (outsize + 7) / 8;
    InitBuffer(&buffer);
    
    if (count > 0) {
        ADD_INT_TO_BUF(buffer, (*data & 0x80) != 0);
        while ((i = NextElias(data, count, &pos)) != 0)
            ADD_INT_TO_BUF(buffer, i);
    }
    
    free(data);
    
    a->c = SeqAsCol(BufferAsIntVec(&buffer));
    return IT_column;
}
/* %include<buffer.c>% */
/*
 * buffer.c - Implementation of a simple temporary buffer.
 */

#include <stdlib.h>
#include <string.h>


typedef struct Overflow {
    char                b[4096];    /* must be first member */
    Overflow_p  next;
} Overflow;

void InitBuffer (Buffer_p bp) {
    bp->fill.c = bp->buf;
    bp->limit = bp->buf + sizeof bp->buf;
    bp->head = 0;
    bp->ofill = 0;
    bp->saved = 0;
    bp->result = 0;
}

void ReleaseBuffer (Buffer_p bp, int keep) {
    while (bp->head != 0) {
        Overflow_p op = bp->head;
        bp->head = op->next;
        free(op);
    }
    if (!keep && bp->result != 0)
        free(bp->result);
}

void AddToBuffer (Buffer_p bp, const void *data, Int_t len) {
    Int_t n;
    while (len > 0) {
        if (bp->fill.c >= bp->limit) {
            if (bp->head == 0 || 
                    bp->ofill >= bp->head->b + sizeof bp->head->b) {
                Overflow_p op = (Overflow_p) malloc(sizeof(Overflow));
                op->next = bp->head;
                bp->head = op;
                bp->ofill = op->b;
            }
            memcpy(bp->ofill, bp->buf, sizeof bp->buf);
            bp->ofill += sizeof bp->buf;
            bp->saved += sizeof bp->buf;
            n = bp->fill.c - bp->slack;
            memcpy(bp->buf, bp->slack, n);
            bp->fill.c = bp->buf + n;
        }
        n = len;
        if (n > bp->limit - bp->fill.c)
            n = bp->limit - bp->fill.c; /* TODO: copy big chunks to overflow */
        memcpy(bp->fill.c, data, n);
        bp->fill.c += n;
        data = (const char*) data + n;
        len -= n;
    }
}

int NextBuffer (Buffer_p bp, char **firstp, int *countp) {
    int count;
    
    if (*firstp == NULL) {
        Overflow_p p = bp->head, q = NULL;
        while (p != NULL) {
            Overflow_p t = p->next;
            p->next = q;
            q = p;
            p = t;
        }
        
        bp->head = q;
        bp->used = 0;
    } else if (*firstp == bp->head->b) {
        bp->head = bp->head->next;
        bp->used += *countp;
        free(*firstp);
    } else if (*firstp == bp->buf)
        return *countp == 0;
    
    if (bp->head != NULL) {
        *firstp = bp->head->b;
        count = bp->saved - bp->used;
        if (count > sizeof bp->head->b)
            count = sizeof bp->head->b;
    } else {
        count = bp->fill.c - bp->buf;
        *firstp = bp->buf;
    }
    
    *countp = count;
    return *countp;
}

void *BufferAsPtr (Buffer_p bp, int fast) {
    Int_t len;
    char *data, *ptr = NULL;
    int cnt;

    if (fast && bp->saved == 0)
        return bp->buf;

    len = BufferFill(bp);
    if (bp->result == 0)
        bp->result = malloc(len);
        
    for (data = bp->result; NextBuffer(bp, &ptr, &cnt); data += cnt)
        memcpy(data, ptr, cnt);

    return bp->result;
}

Seq_p BufferAsIntVec (Buffer_p bp) {
    int cnt;
    char *data, *ptr = NULL;
    Seq_p seq;
    
    seq = NewIntVec(BufferFill(bp) / sizeof(int), (void*) &data);

    for (; NextBuffer(bp, &ptr, &cnt); data += cnt)
        memcpy(data, ptr, cnt);

    return seq;
}
/* %include<column.c>% */
/*
 * column.c - Implementation of sequences, columns, and items.
 */

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* wrap_gen.h - generated code, do not edit */

View_p (BlockedView) (View_p);
View_p (CloneView) (View_p);
Column (CoerceCmd) (Object_p, const char*);
View_p (ColMapView) (View_p, Column);
View_p (ColOmitView) (View_p, Column);
View_p (ConcatView) (View_p, View_p);
View_p (DescToMeta) (const char*);
View_p (FirstView) (View_p, int);
Column (GetHashInfo) (View_p, View_p, int);
View_p (GroupCol) (View_p, Column, const char*);
View_p (GroupedView) (View_p, Column, Column, const char*);
int    (HashDoFind) (View_p, int, View_p, Column, Column, Column);
Column (HashValues) (View_p);
View_p (IjoinView) (View_p, View_p);
Column (IntersectMap) (View_p, View_p);
View_p (JoinView) (View_p, View_p, const char*);
View_p (LastView) (View_p, int);
Column (NewCountsColumn) (Column);
Column (NewIotaColumn) (int);
View_p (NoColumnView) (int);
View_p (ObjAsMetaView) (Object_p);
Column (OmitColumn) (Column, int);
View_p (OneColView) (View_p, int);
View_p (PairView) (View_p, View_p);
View_p (RemapSubview) (View_p, Column, int, int);
int    (RowCompare) (View_p, int, View_p, int);
int    (RowEqual) (View_p, int, View_p, int);
int    (RowHash) (View_p, int);
Column (SortMap) (View_p);
View_p (StepView) (View_p, int, int, int, int);
int    (StringLookup) (const char*, Column);
View_p (TagView) (View_p, const char*);
View_p (TakeView) (View_p, int);
View_p (UngroupView) (View_p, int);
Column (UniqMap) (View_p);
View_p (V_Meta) (View_p);
Column (ViewAsCol) (View_p);
Column (ViewCol) (View_p, int);
int    (ViewCompare) (View_p, View_p);
int    (ViewCompat) (View_p, View_p);
View_p (ViewReplace) (View_p, int, int, View_p);
int    (ViewSize) (View_p);
int    (ViewWidth) (View_p);

ItemTypes (AppendCmd_VV) (Item_p a);
ItemTypes (AtCmd_VIO) (Item_p a);
ItemTypes (AtRowCmd_OI) (Item_p a);
ItemTypes (BitRunsCmd_i) (Item_p a);
ItemTypes (BlockedCmd_V) (Item_p a);
ItemTypes (CloneCmd_V) (Item_p a);
ItemTypes (CoerceCmd_OS) (Item_p a);
ItemTypes (ColConvCmd_C) (Item_p a);
ItemTypes (ColMapCmd_Vn) (Item_p a);
ItemTypes (ColOmitCmd_Vn) (Item_p a);
ItemTypes (CompareCmd_VV) (Item_p a);
ItemTypes (CompatCmd_VV) (Item_p a);
ItemTypes (ConcatCmd_VV) (Item_p a);
ItemTypes (CountsCmd_VN) (Item_p a);
ItemTypes (CountsColCmd_C) (Item_p a);
ItemTypes (CountViewCmd_I) (Item_p a);
ItemTypes (DataCmd_VX) (Item_p a);
ItemTypes (DebugCmd_I) (Item_p a);
ItemTypes (DefCmd_OO) (Item_p a);
ItemTypes (DeleteCmd_VII) (Item_p a);
ItemTypes (DepsCmd_O) (Item_p a);
ItemTypes (EmitCmd_V) (Item_p a);
ItemTypes (EmitModsCmd_V) (Item_p a);
ItemTypes (ExceptCmd_VV) (Item_p a);
ItemTypes (ExceptMapCmd_VV) (Item_p a);
ItemTypes (FirstCmd_VI) (Item_p a);
ItemTypes (GetCmd_VX) (Item_p a);
ItemTypes (GetColCmd_VN) (Item_p a);
ItemTypes (GetInfoCmd_VVI) (Item_p a);
ItemTypes (GroupCmd_VnS) (Item_p a);
ItemTypes (GroupedCmd_ViiS) (Item_p a);
ItemTypes (HashColCmd_SO) (Item_p a);
ItemTypes (HashFindCmd_VIViii) (Item_p a);
ItemTypes (HashViewCmd_V) (Item_p a);
ItemTypes (IjoinCmd_VV) (Item_p a);
ItemTypes (InsertCmd_VIV) (Item_p a);
ItemTypes (IntersectCmd_VV) (Item_p a);
ItemTypes (IotaCmd_I) (Item_p a);
ItemTypes (IsectMapCmd_VV) (Item_p a);
ItemTypes (JoinCmd_VVS) (Item_p a);
ItemTypes (LastCmd_VI) (Item_p a);
ItemTypes (LoadCmd_O) (Item_p a);
ItemTypes (LoadModsCmd_OV) (Item_p a);
ItemTypes (LoopCmd_X) (Item_p a);
ItemTypes (MaxCmd_VN) (Item_p a);
ItemTypes (MdefCmd_O) (Item_p a);
ItemTypes (MdescCmd_S) (Item_p a);
ItemTypes (MetaCmd_V) (Item_p a);
ItemTypes (MinCmd_VN) (Item_p a);
ItemTypes (MutInfoCmd_V) (Item_p a);
ItemTypes (NameColCmd_V) (Item_p a);
ItemTypes (NamesCmd_V) (Item_p a);
ItemTypes (OmitMapCmd_iI) (Item_p a);
ItemTypes (OneColCmd_VN) (Item_p a);
ItemTypes (OpenCmd_S) (Item_p a);
ItemTypes (PairCmd_VV) (Item_p a);
ItemTypes (ProductCmd_VV) (Item_p a);
ItemTypes (RefCmd_OX) (Item_p a);
ItemTypes (RefsCmd_O) (Item_p a);
ItemTypes (RemapCmd_Vi) (Item_p a);
ItemTypes (RemapSubCmd_ViII) (Item_p a);
ItemTypes (RenameCmd_VO) (Item_p a);
ItemTypes (RepeatCmd_VI) (Item_p a);
ItemTypes (ReplaceCmd_VIIV) (Item_p a);
ItemTypes (ResizeColCmd_iII) (Item_p a);
ItemTypes (ReverseCmd_V) (Item_p a);
ItemTypes (RowCmpCmd_VIVI) (Item_p a);
ItemTypes (RowEqCmd_VIVI) (Item_p a);
ItemTypes (RowHashCmd_VI) (Item_p a);
ItemTypes (SaveCmd_VS) (Item_p a);
ItemTypes (SetCmd_MIX) (Item_p a);
ItemTypes (SizeCmd_V) (Item_p a);
ItemTypes (SliceCmd_VIII) (Item_p a);
ItemTypes (SortCmd_V) (Item_p a);
ItemTypes (SortMapCmd_V) (Item_p a);
ItemTypes (SpreadCmd_VI) (Item_p a);
ItemTypes (StepCmd_VIIII) (Item_p a);
ItemTypes (StrLookupCmd_Ss) (Item_p a);
ItemTypes (StructDescCmd_V) (Item_p a);
ItemTypes (StructureCmd_V) (Item_p a);
ItemTypes (SumCmd_VN) (Item_p a);
ItemTypes (TagCmd_VS) (Item_p a);
ItemTypes (TakeCmd_VI) (Item_p a);
ItemTypes (ToCmd_OO) (Item_p a);
ItemTypes (TypeCmd_O) (Item_p a);
ItemTypes (TypesCmd_V) (Item_p a);
ItemTypes (UngroupCmd_VN) (Item_p a);
ItemTypes (UnionCmd_VV) (Item_p a);
ItemTypes (UnionMapCmd_VV) (Item_p a);
ItemTypes (UniqueCmd_V) (Item_p a);
ItemTypes (UniqueMapCmd_V) (Item_p a);
ItemTypes (ViewCmd_X) (Item_p a);
ItemTypes (ViewAsColCmd_V) (Item_p a);
ItemTypes (ViewConvCmd_V) (Item_p a);
ItemTypes (WidthCmd_V) (Item_p a);
ItemTypes (WriteCmd_VO) (Item_p a);

/* end of generated code */

ItemTypes DebugCmd_I (Item_p a) {
	Shared_p sh = GetShared();
	a->i = sh->debug ^= a[0].i;
	return IT_int;
}

void *AdjustSeqRefs (void *refs, int count) {
	/*if (count > 0)*/
	if (refs != NULL) {
		Seq_p ptr = refs;
		ptr->refs += count;
		if (ptr->refs <= 0) {
			if (ptr->type != NULL) {
				int i, f;
				
				if (ptr->type->cleaner != NULL)
					ptr->type->cleaner(ptr);

				for (i = 0, f = ptr->type->cleanups; f > 0; ++i, f >>= 3)
					if (f & 1)
						DecRefCount(ptr->data[i].p);
					else if (f & 2)
						free(ptr->data[i].p);
			}
			free(refs);
			refs = NULL;
		}
	}
	return refs;
}

Seq_p LoseRef (Seq_p seq) {
	ADD_PTR_TO_BUF(*GetShared()->keeps, seq);
	return seq;
}

Column SeqAsCol (Seq_p seq) {
	Column result;
	result.seq = seq;
	result.pos = -1;
	return result;
}

Seq_p NewSequenceNoRef (int count, SeqType_p type, int bytes) {
	Seq_p seq;
	
	seq = calloc(1, sizeof(struct Sequence) + bytes);
	seq->count = count;
	seq->type = type;
	seq->getter = type->getfun;

	/* default for data[0].p is to point to the aux bytes after the sequence */
	/* default for data[1].i is to contain number of extra bytes allocated */
	seq->data[0].p = seq + 1;
	seq->data[1].i = bytes;
	
	/* make sure 64-bit values have a "natural" 8-byte alignment */
	Assert(((Int_t) seq->data[0].p) % 8 == 0);

	return seq;
}

Seq_p NewSequence (int count, SeqType_p type, int bytes) {
	return KeepRef(NewSequenceNoRef(count, type, bytes));
}

void ReleaseSequences (Buffer_p keep) {
	int cnt;
	char *ptr = NULL;
	
	while (NextBuffer(keep, &ptr, &cnt)) {
		Seq_p *refs = (Seq_p*) ptr;
		int count = cnt / sizeof(Seq_p);

		while (--count >= 0)
			DecRefCount(refs[count]);
	}
}

/* TODO: not used yet, should review refcount scenarios, might be wrong now */
Seq_p ResizeSeq (Seq_p seq, int pos, int diff, int elemsize) {
	int newcnt, olimit, nlimit = 0, bytes;
	Seq_p result = seq;
	
	Assert(seq->refs == 1);
	
	newcnt = seq->count + diff;
	bytes = seq->data[1].i;
	olimit = bytes / elemsize;
	
	if (diff > 0) { /* grow when limit is reached */
		if (newcnt > olimit) {
			nlimit = (olimit / 2) * 3 + 4;
			if (nlimit < newcnt)
				nlimit = newcnt + 4;
		}
	} else { /* shrink when less than half full */
		if (newcnt < olimit / 2) {
			nlimit = newcnt + 2;
			bytes = seq->count * elemsize;
		}
	}

	if (nlimit > 0) {
		result = IncRefCount(NewSequence(seq->count, seq->type,
		                                                nlimit * elemsize));
		result->getter = seq->getter;
		result->data[2] = seq->data[2];
		result->data[3] = seq->data[3];
		memcpy(result+1, seq+1, pos * elemsize);

		seq->type = NULL; /* release and prevent from calling cleaner */
		/*LoseRef(seq);*/
	}

	if (diff > 0) {
		memmove((char*) (result+1) + (pos + diff) * elemsize,
						(char*) (seq+1) + pos * elemsize,
						(seq->count - pos) * elemsize);
		memset((char*) (result+1) + pos * elemsize, 0, diff * elemsize);
	} else
		memmove((char*) (result+1) + pos * elemsize,
						(char*) (seq+1) + (pos - diff) * elemsize,
						(seq->count - (pos - diff)) * elemsize);
	
	result->count += diff;
	return result;
}

ItemTypes ResizeColCmd_iII (Item_p a) {
	int r, *data;
	Seq_p seq, result;
	
	seq = a[0].c.seq;
	result = NewIntVec(seq->count, &data);
	for (r = 0; r < seq->count; ++r)
		data[r] = GetSeqItem(r, seq, IT_int).i;
		
	a->c.seq = ResizeSeq(result, a[1].i, a[2].i, sizeof(int));
	return IT_column;
}

Seq_p NewBitVec (int count) {
	return NewSequence(count, PickIntGetter(1), (count + 7) / 8);
}

Seq_p NewIntVec (int count, int **dataptr) {
	const int w = sizeof(int);
	Seq_p seq = NewSequence(count, PickIntGetter(8 * w), w * count);
	if (dataptr != NULL)
		*dataptr = seq->data[0].p;
	return seq;
}

Seq_p NewPtrVec (int count, Int_t **dataptr) {
	const int w = sizeof(Int_t);
	Seq_p result = NewSequence(count, PickIntGetter(8 * w), w * count);
	if (dataptr != NULL)
		*dataptr = result->data[0].p;
	return result;
}

ItemTypes GetItem (int row, Item_p item) {
	Seq_p seq = item->c.seq;
	
	if (row < 0 || row >= seq->count) {
		item->e = EC_rioor;
		return IT_error;
	}
		
	return seq->getter(row, item);
}

Item GetColItem (int row, Column column, ItemTypes type) {
	Item item;
	ItemTypes got;
	
	item.c = column;
	got = GetItem(row, &item);
	Assert(got == type);
	return item;
}

Item GetSeqItem (int row, Seq_p seq, ItemTypes type) {
	Item item;
	ItemTypes got;
	
	item.c.seq = seq;
	item.c.pos = -1;
	got = GetItem(row, &item);
	Assert(got == type);
	return item;
}

Item GetViewItem (View_p view, int row, int col, ItemTypes type) {
	Item item;
	ItemTypes got;
	
	item.c = ViewCol(view, col);
	got = GetItem(row, &item);
	Assert(got == type);
	return item;
}

#if 0 /* unused */
const char *ItemTypeAsString (ItemTypes type) {
	static const char *typeTable[] = {
		"",	 /* IT_unknown */
		"I", /* IT_int */
		"L", /* IT_wide */
		"D", /* IT_double */
		"S", /* IT_string */
		"O", /* IT_object */
		"C", /* IT_column */
		"V", /* IT_view */
		"E", /* IT_error */
	};

	return typeTable[type];
}
#endif

ItemTypes CharAsItemType (char type) {
	switch (type) {
		case 0:		return IT_unknown;
		case 'I': return IT_int;
		case 'L': return IT_wide;
		case 'F': return IT_float;
		case 'D': return IT_double;
		case 'S': return IT_string;
		case 'B': return IT_bytes;
		case 'O': return IT_object;
		case 'C': return IT_column;
		case 'V': return IT_view;
	}
	return IT_error;
}

static ItemTypes StringGetter (int row, Item_p item) {
	char **ptrs = (char**) item->c.seq->data[0].p;
	item->s = ptrs[row];
	return IT_string;
}

static struct SeqType ST_String = { "string", StringGetter, 0102 };

static ItemTypes BytesGetter (int row, Item_p item) {
	char **ptrs = (char**) item->c.seq->data[0].p;
	item->u.ptr = (const Byte_t*) ptrs[row];
	item->u.len = ptrs[row+1] - ptrs[row];
	return IT_bytes;
}

static struct SeqType ST_Bytes = { "string", BytesGetter, 0102 };

Seq_p NewStrVec (int istext) {
	Seq_p seq;
	Buffer_p bufs;

	bufs = malloc(2 * sizeof(struct Buffer));
	InitBuffer(bufs);
	InitBuffer(bufs+1);

	seq = NewSequence(0, istext ? &ST_String : &ST_Bytes, 0);
	/* data[0] starts as two buffers, then becomes vector of string pointers */
	/* data[1] is not used */
	/* data[2] is an optional pointer to hash map sequence, see StringLookup */
	seq->data[0].p = bufs;
	
	return seq;
}

void AppendToStrVec (const void *string, int bytes, Seq_p seq) {
	Buffer_p bufs = seq->data[0].p;

	if (bytes < 0)
		bytes = strlen((const char*) string) + 1;
	
	/* TODO: consider tracking pointers i.s.o. making actual copies here */
	AddToBuffer(bufs, string, bytes);
	ADD_INT_TO_BUF(bufs[1], bytes);
	
	++seq->count;
}

Seq_p FinishStrVec (Seq_p seq) {
	int r = 0, cnt = 0, *iptr = NULL;
	char *fill, **ptrs, *cptr = NULL;
	Buffer_p bufs = seq->data[0].p;

	ptrs = malloc((seq->count+1) * sizeof(char*) + BufferFill(bufs));

	fill = (char*) (ptrs + seq->count + 1);
	while (NextBuffer(bufs+1, (void*) &iptr, &cnt)) {
		int i, n = cnt / sizeof(int);
		for (i = 0; i < n; ++i) {
			ptrs[r++] = fill;
			fill += iptr[i];
		}
	}
	ptrs[r] = fill; /* past-end mark needed to provide length of last entry */

	fill = (char*) (ptrs + seq->count + 1);
	while (NextBuffer(bufs, &cptr, &cnt)) {
		memcpy(fill, cptr, cnt);
		fill += cnt;
	}
	
	ReleaseBuffer(bufs, 0);
	ReleaseBuffer(bufs+1, 0);
	free(seq->data[0].p);
	
	seq->data[0].p = ptrs;
	return seq;
}

Column ForceStringColumn (Column column) {
	int r, rows;
	Seq_p result;
	
	/* this code needed to always make name column of meta views string cols */
	
	if (column.seq == NULL || column.seq->getter == StringGetter)
		return column;
		
	rows = column.seq->count;
	result = NewStrVec(1);
	
	for (r = 0; r < rows; ++r)
		AppendToStrVec(GetColItem(r, column, IT_string).s, -1, result);
	
	return SeqAsCol(FinishStrVec(result));
}

static ItemTypes IotaGetter (int row, Item_p item) {
	item->i = row;
	return IT_int;
}

static struct SeqType ST_Iota = { "iota", IotaGetter };

Column NewIotaColumn (int count) {
	return SeqAsCol(NewSequence(count, &ST_Iota, 0));
}

static void SequenceCleaner (Seq_p seq) {
	int i;
	View_p *items = seq->data[0].p;

	for (i = 0; i < seq->count; ++i)
		DecRefCount(items[i]);
}

static ItemTypes SequenceGetter (int row, Item_p item) {
	void **items = (void**) item->c.seq->data[0].p;
	ItemTypes type = item->c.seq->data[1].i;

	switch (type) {

		case IT_view:
			item->v = items[row];
			return IT_view;

		case IT_column:
			item->c.seq = items[row];
			item->c.pos = -1;
			return IT_column;

		default:
			return IT_unknown;
	}
}

static struct SeqType ST_Sequence = {
	"sequence", SequenceGetter, 0, SequenceCleaner
};

Seq_p NewSeqVec (ItemTypes type, const Seq_p *items, int count) {
	int i, bytes;
	Seq_p seq;

	bytes = count * sizeof(View_p);
	seq = NewSequence(count, &ST_Sequence, bytes);
	/* data[0] points to a list of pointers */
	/* data[1] is the type of the returned items */
	seq->data[1].i = type;

	if (items != NULL) {
		memcpy(seq->data[0].p, items, bytes);
		for (i = 0; i < count; ++i)
			IncRefCount(items[i]);
	}

	return seq;
}

static ItemTypes CountsGetter (int row, Item_p item) {
	const View_p *items = (const View_p*) item->c.seq->data[0].p;

	item->i = ViewSize(items[row]);
	return IT_int;
}

Column NewCountsColumn (Column column) {
	int r, rows;
	View_p *items;
	Seq_p seq;
	
	rows = column.seq->count;	 
	seq = NewSeqVec(IT_view, NULL, rows);
	seq->getter = CountsGetter;

	items = seq->data[0].p;
	for (r = 0; r < rows; ++r)
		items[r] = IncRefCount(GetColItem(r, column, IT_view).v);

	return SeqAsCol(seq);
}

Column OmitColumn (Column omit, int size) {
	int i, *data, j = 0, outsize;
	Seq_p seq, map;
	
	if (omit.seq == NULL)
		return omit;
		
	outsize = size - omit.seq->count;
	
	/* FIXME: wasteful for large views, consider sorting input col instead */
	map = NewBitVec(size);
	for (i = 0; i < omit.seq->count; ++i)
		SetBit(&map, GetColItem(i, omit, IT_int).i);

	seq = NewIntVec(outsize, &data);
	
	for (i = 0; i < size; ++i)
		if (!TestBit(map, i)) {
			Assert(j < outsize);
			data[j++] = i;
		}
		
	return SeqAsCol(seq);
}

#if 0
static ItemTypes UnknownGetter (int row, Item_p item) {
	return IT_unknown;
}

static struct SeqType ST_Unknown = { "unknown", UnknownGetter };

static Seq_p NewUnknownVec (int count) {
	return NewSequence(count, &ST_Unknown, 0);
}
#endif

Column CoerceColumn (ItemTypes type, Object_p obj) {
	int i, n;
	void *data;
	Seq_p seq;
	Item item;
	Column out, column = ObjAsColumn(obj);

	if (column.seq == NULL)
		return column;
		
	/* determine the input column type by fetching one item */
	item.c = column;
	n = column.seq->count;

	if (GetItem(0, &item) == type)
		return column;
		
	switch (type) {
		case IT_int:	seq = NewIntVec(n, (void*) &data); break;
		case IT_wide:	seq = NewSequence(n, PickIntGetter(64), 8 * n); break;
		case IT_float:	seq = NewSequence(n, FixedGetter(4,1,1,0), 4 * n);
		                break;
		case IT_double: seq = NewSequence(n, FixedGetter(8,1,1,0), 8 * n);
		                break;
		case IT_string: seq = NewStrVec(1); break;
		case IT_bytes:	seq = NewStrVec(0); break;
		case IT_view:	seq = NewSeqVec(IT_view, NULL, n); break;
		default:		Assert(0);
	}

	out = SeqAsCol(seq);
	data = out.seq->data[0].p;

	for (i = 0; i < n; ++i) {
		item.o = GetColItem(i, column, IT_object).o;
		if (!ObjToItem(type, &item)) {
			out.seq = NULL;
			out.pos = i;
			break;
		}
	
		switch (type) {
			case IT_int:	((int*) data)[i] = item.i; break;
			case IT_wide:	((int64_t*) data)[i] = item.w; break;
			case IT_float:	((float*) data)[i] = item.f; break;
			case IT_double: ((double*) data)[i] = item.d; break;
			case IT_string: AppendToStrVec(item.s, -1, out.seq); break;
			case IT_bytes:	AppendToStrVec(item.u.ptr, item.u.len, out.seq);
			                break;
			case IT_view:	((Seq_p*) data)[i] = IncRefCount(item.v); break;
			default:		Assert(0);
		}
	}

	if (type == IT_string || type == IT_bytes)
		FinishStrVec(seq);

	return out;
}

Column CoerceCmd (Object_p obj, const char *str) {
	return CoerceColumn(CharAsItemType(str[0]), obj);
}

int CastObjToItem (char type, Item_p item) {
	switch (type) {

		case 'M':
			item->o = NeedMutable(item->o);
			return (void*) item->o != NULL;
			
		case 'N':
			item->i = ColumnByName(V_Meta(item[-1].v), item->o);
			return item->i >= 0;
		
		case 'n': {
			int *data, r, rows;
			View_p meta;
			Column column;
			Seq_p seq;

			column = ObjAsColumn(item->o);
			if (column.seq == NULL)
				return 0;
				
			rows = column.seq->count;
			seq = NewIntVec(rows, &data);
			meta = V_Meta(item[-1].v);
			
			for (r = 0; r < rows; ++r) {
				/* FIXME: this only works if input is a list, not a column! */
				data[r] = ColumnByName(meta, GetColItem(r, column,
				                                            IT_object).o);
				if (data[r] < 0)
					return 0;
			}
			
			item->c = SeqAsCol(seq);
			break;
		}
		
		default:
			if (type < 'a' || type > 'z')
				return ObjToItem(CharAsItemType(type), item);

			item->c = CoerceColumn(CharAsItemType(type + 'A'-'a'), item->o);
			break;
	}

	return 1; /* cast succeeded */
}
/* %include<emit.c>% */
/*
 * emit.c - Implementation of file output commands.
 */

#include <stdlib.h>
#include <string.h>
#include <unistd.h>


typedef enum EmitTypes {
    ET_mem
} EmitTypes;

typedef struct EmitItem {
    EmitTypes type;
    Int_t size;
    const void *data;
} EmitItem, *EmitItem_p;

typedef struct EmitInfo {
    int64_t         position;   /* emit offset, track >2 Gb even on 32b arch */
    struct Buffer  *itembuf;    /* item buffer */
    struct Buffer  *colbuf;     /* column buffer */
    int             diff;       /* true if emitting differences */
} EmitInfo, *EmitInfo_p;

static void EmitView (EmitInfo_p eip, View_p view, int describe); /* forward */

static Int_t EmitBlock (EmitInfo_p eip, const void* data, int size) {
    EmitItem item;
    Int_t pos = eip->position;

    if (size > 0) {
        item.type = ET_mem;
        item.size = size;
        item.data = data;
        AddToBuffer(eip->itembuf, &item, sizeof item);

        eip->position += size;
    } else
        free((void*) data);
    
    return pos;
}

static void *EmitCopy (EmitInfo_p eip, const void* data, int size) {
    void *buf = NULL;
    if (size > 0) {
        buf = memcpy(malloc(size), data, size);
        EmitBlock(eip, buf, size);
    }
    return buf;
}

static Int_t EmitBuffer (EmitInfo_p eip, Buffer_p buf) {
    Int_t pos = EmitBlock(eip, BufferAsPtr(buf, 0), BufferFill(buf));
    ReleaseBuffer(buf, 1);
    return pos;
}

static void EmitAlign (EmitInfo_p eip) {
    if (eip->position >= 1024 * 1024)
        EmitCopy(eip, "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0", 15 & - eip->position);
}

void MetaAsDesc (View_p meta, Buffer_p buffer) {
    int r, rows = (int)ViewSize(meta);
    Column names, types, subvs;
    char type;
    const char *name;
    View_p subv;

    names = ViewCol(meta, MC_name);
    types = ViewCol(meta, MC_type);
    subvs = ViewCol(meta, MC_subv);

    for (r = 0; r < rows; ++r) {
        if (r > 0)
            AddToBuffer(buffer, ",", 1);

        name = GetColItem(r, names, IT_string).s;
        type = *GetColItem(r, types, IT_string).s;
        subv = GetColItem(r, subvs, IT_view).v;

        AddToBuffer(buffer, name, strlen(name));
        if (type == 'V' && ViewSize(subv) > 0) {
            AddToBuffer(buffer, "[", 1);
            MetaAsDesc(subv, buffer);
            AddToBuffer(buffer, "]", 1);
        } else {
            AddToBuffer(buffer, ":", 1);
            AddToBuffer(buffer, &type, 1);
        }
    }
}

static void EmitVarInt (EmitInfo_p eip, Int_t value) {
    int n;

#if 0
    if (value < 0) {
        ADD_ONEC_TO_BUF(*eip->colbuf, 0);
        value = ~value;
    }
#endif

    Assert(value >= 0);
    for (n = 7; (value >> n) > 0; n += 7)
        ;
    while ((n -= 7) > 0)
        ADD_ONEC_TO_BUF(*eip->colbuf, (value >> n) & 0x7F);
    ADD_CHAR_TO_BUF(*eip->colbuf, (value & 0x7F) | 0x80);
}

static void EmitPair (EmitInfo_p eip, Int_t offset) {
    Int_t size;

    size = eip->position - offset;
    EmitVarInt(eip, size);
    if (size > 0)
        EmitVarInt(eip, offset);
}

static int MinWidth (int lo, int hi) {
    lo = lo > 0 ? 0 : "444444445555555566666666666666666"[TopBit(~lo)+1] & 7;
    hi = hi < 0 ? 0 : "012334445555555566666666666666666"[TopBit(hi)+1] & 7;
    return lo > hi ? lo : hi;
}

static int *PackedIntVec (const int *data, int rows, Int_t *outsize) {
    int r, width, lo = 0, hi = 0, bits, *result;
    const int *limit;
    Int_t bytes;

    for (r = 0; r < rows; ++r) {
        if (data[r] < lo) lo = data[r];
        if (data[r] > hi) hi = data[r];
    }

    width = MinWidth(lo, hi);

    if (width >= 6) {
        bytes = rows * (1 << (width-4));
        result = malloc(bytes);
        memcpy(result, data, bytes);
    } else if (rows > 0 && width > 0) {
        if (rows < 5 && width < 4) {
            static char fudges[3][4] = {    /* n:    1:  2:  3:  4: */
                {1,1,1,1},          /* 1-bit entries:    1b  2b  3b  4b */
                {1,1,1,1},          /* 2-bit entries:    2b  4b  6b  8b */
                {1,1,2,2},          /* 4-bit entries:    4b  8b 12b 16b */
            };
            static char widths[3][4] = {    /* n:    1:  2:  3:  4: */
                {3,3,2,2},          /* 1-bit entries:    4b  4b  2b  2b */
                {3,3,2,2},          /* 2-bit entries:    4b  4b  2b  2b */
                {3,3,3,3},          /* 4-bit entries:    4b  4b  4b  4b */
            };
            bytes = fudges[width-1][rows-1];
            width = widths[width-1][rows-1];
        } else
            bytes = (((Int_t) rows << width) + 14) >> 4; /* round up */
            
        result = malloc(bytes);
        if (width < 4)
            memset(result, 0, bytes);

        limit = data + rows;
        bits = 0;

        switch (width) {

            case 1: { /* 1 bit, 8 per byte */
                char* q = (char*) result;
                while (data < limit) {
                    *q |= (*data++ & 1) << bits; ++bits; q += bits >> 3; 
                    bits &= 7;
                }
                break;
            }

            case 2: { /* 2 bits, 4 per byte */
                char* q = (char*) result;
                while (data < limit) {
                    *q |= (*data++ & 3) << bits; bits += 2; q += bits >> 3; 
                    bits &= 7;
                }
                break;
            }

            case 3: { /* 4 bits, 2 per byte */
                char* q = (char*) result;
                while (data < limit) {
                    *q |= (*data++ & 15) << bits; bits += 4; q += bits >> 3;
                    bits &= 7;
                }
                break;
            }

            case 4: { /* 1-byte (char) */
                char* q = (char*) result;
                while (data < limit)
                    *q++ = (char) *data++;
                break;
            }

            case 5: { /* 2-byte (short) */
                short* q = (short*) result;
                while (data < limit)
                    *q++ = (short) *data++;
                break;
            }
        }
    } else {
        bytes = 0;
        result = NULL;
    }

    *outsize = bytes;
    return result;
}

static int EmitFixCol (EmitInfo_p eip, Column column, ItemTypes type) {
    int r, rows, *tempvec;
    void *buffer;
    Int_t bufsize;

    rows = column.seq->count;

    switch (type) {

        case IT_int:
            bufsize = rows * sizeof(int);
            tempvec = malloc(bufsize);
            for (r = 0; r < rows; ++r)
                tempvec[r] = GetColItem(r, column, type).i;
            buffer = PackedIntVec(tempvec, rows, &bufsize);
            free((char*) tempvec); /* TODO: avoid extra copy & malloc */
            
            /* try to compress the bitmap, but only in diff save mode */
            if (eip->diff && rows >= 128 && rows == bufsize * 8) {
                int ebits;

                tempvec = (void*) Bits2elias(buffer, rows, &ebits);

                /* only keep compressed form if under 80% of plain bitmap */
                if (ebits + ebits/4 < rows) {
                    free(buffer);
                    buffer = tempvec;
                    bufsize = (ebits + 7) / 8;
                } else
                    free((char*) tempvec);
            }
            
            break;

        case IT_wide:
            bufsize = rows * sizeof(int64_t);
            buffer = malloc(bufsize);
            for (r = 0; r < rows; ++r)
                ((int64_t*) buffer)[r] = GetColItem(r, column, type).w;
            break;

        case IT_float:
            bufsize = rows * sizeof(float);
            buffer = malloc(bufsize);
            for (r = 0; r < rows; ++r)
                ((float*) buffer)[r] = GetColItem(r, column, type).f;
            break;

        case IT_double:
            bufsize = rows * sizeof(double);
            buffer = malloc(bufsize);
            for (r = 0; r < rows; ++r)
                ((double*) buffer)[r] = GetColItem(r, column, type).d;
            break;

        default: Assert(0); return 0;
    }

    /* start using 16-byte alignment once the emitted data reaches 1 Mb */
    /* only do this for vectors >= 128 bytes, worst-case waste is under 10% */
    
    Assert(!(bufsize > 0 && rows == 0));
    if (bufsize >= 128 && bufsize / rows >= 2) 
        EmitAlign(eip);
        
    EmitPair(eip, EmitBlock(eip, buffer, bufsize));
    
    return bufsize != 0;
}

static void EmitVarCol (EmitInfo_p eip, Column column, int istext) {
    int r, rows, bytes, *sizevec;
    Int_t buflen;
    Item item;
    struct Buffer buffer;
    Seq_p sizes;

    InitBuffer(&buffer);
    rows = column.seq->count;
    sizes = NewIntVec(rows, &sizevec);

    if (istext)
        for (r = 0; r < rows; ++r) {
            item = GetColItem(r, column, IT_string);
            bytes = strlen(item.s);
            if (bytes > 0)
                AddToBuffer(&buffer, item.s, ++bytes);
            sizevec[r] = bytes;
        }
    else
        for (r = 0; r < rows; ++r) {
            item = GetColItem(r, column, IT_bytes);
            AddToBuffer(&buffer, item.u.ptr, item.u.len);
            sizevec[r] = item.u.len;
        }

    buflen = BufferFill(&buffer);
    EmitPair(eip, EmitBuffer(eip, &buffer));    
    if (buflen > 0)
        EmitFixCol(eip, SeqAsCol(sizes), 1);

    EmitVarInt(eip, 0); /* no memos */
}

static void EmitSubCol (EmitInfo_p eip, Column column, int describe) {
    int r, rows;
    View_p view;
    struct Buffer newcolbuf;
    Buffer_p origcolbuf;

    origcolbuf = eip->colbuf;
    eip->colbuf = &newcolbuf;
    InitBuffer(eip->colbuf);

    rows = column.seq->count;

    for (r = 0; r < rows; ++r) {
        view = GetColItem(r, column, IT_view).v;
        EmitView(eip, view, describe);
    }

    eip->colbuf = origcolbuf;

    EmitPair(eip, EmitBuffer(eip, &newcolbuf));  
}

static void EmitCols (EmitInfo_p eip, View_p view, View_p maps) {
    int rows = ViewSize(view);
    
    EmitVarInt(eip, rows);

    if (rows > 0) {
        int i, r, c, *rowptr = NULL;
        ItemTypes type;
        Column column;
        Seq_p rowmap;
        View_p v, subv, meta = V_Meta(view);

        rowmap = NULL;
        
        for (c = 0; c < ViewWidth(view); ++c) {          
            if (maps != NULL) {
                Column mapcol = ViewCol(maps, c);
                if (!EmitFixCol(eip, mapcol, IT_int))
                    continue;

                if (rowmap == NULL)
                    rowmap = NewIntVec(rows, &rowptr);

                i = 0;
                for (r = 0; r < rows; ++r)
                    if (GetColItem(r, mapcol, IT_int).i)
                        rowptr[i++] = r;
                        
                rowmap->count = i;
                        
                v = RemapSubview(view, SeqAsCol(rowmap), 0, -1);
            } else
                v = view;
                
            column = ViewCol(v,c);
            type = ViewColType(view, c);
            switch (type) {
                
                case IT_int:
                case IT_wide:
                case IT_float:
                case IT_double:
                    EmitFixCol(eip, column, type); 
                    break;
                    
                case IT_string:
                    EmitVarCol(eip, column, 1);
                    break;
                    
                case IT_bytes:
                    EmitVarCol(eip, column, 0);
                    break;
                    
                case IT_view:
                    subv = GetViewItem(meta, c, MC_subv, IT_view).v;
                    EmitSubCol(eip, column, ViewSize(subv) == 0);
                    break;
                    
                default: Assert(0);
            }
        }
    }
}

static void EmitView (EmitInfo_p eip, View_p view, int describe) {
    EmitVarInt(eip, 0);

    if (eip->diff) {
        Seq_p mods = MutPrepare(view), *seqs = (void*) (mods + 1);

        EmitVarInt(eip, 0);
        EmitFixCol(eip, SeqAsCol(seqs[MP_delmap]), IT_int);

        if (EmitFixCol(eip, SeqAsCol(seqs[MP_adjmap]), IT_int))
            EmitCols(eip, seqs[MP_adjdat], seqs[MP_usemap]);

        EmitCols(eip, seqs[MP_insdat], NULL);
        if (ViewSize(seqs[MP_insdat]) > 0)
            EmitFixCol(eip, SeqAsCol(seqs[MP_insmap]), IT_int);
    } else {
        if (describe) {
            int cnt;
            char *ptr = NULL;
            struct Buffer desc;

            InitBuffer(&desc);
            MetaAsDesc(V_Meta(view), &desc);

            EmitVarInt(eip, BufferFill(&desc));
            
            while (NextBuffer(&desc, &ptr, &cnt))
                AddToBuffer(eip->colbuf, ptr, cnt);
        }

        EmitCols(eip, view, NULL);
    }
}

static void SetBigEndian32 (char* dest, Int_t value) {
    dest[0] = (char) (value >> 24);
    dest[1] = (char) (value >> 16);
    dest[2] = (char) (value >> 8);
    dest[3] = (char) value;
}

static Int_t EmitComplete (EmitInfo_p eip, View_p view) {
    int overflow;
    Int_t rootpos, tailpos, endpos;
    char tail[16];
    struct Head { short a; char b; char c; char d[4]; } head, *fixup;
    struct Buffer newcolbuf;

    if (eip->diff && !IsMutable(view))
        return 0;
        
    eip->position = 0;

    head.a = 'J' + ('L' << 8);
    head.b = 0x1A;
    head.c = 0;
    fixup = EmitCopy(eip, &head, sizeof head);

    eip->colbuf = &newcolbuf;
    InitBuffer(eip->colbuf);

    EmitView(eip, view, 1);

    EmitAlign(eip);
    rootpos = EmitBuffer(eip, eip->colbuf);  
    eip->colbuf = NULL;

    EmitAlign(eip);
    
    if (eip->position != (Int_t) eip->position)
        return -1; /* fail if >= 2 Gb on machines with a 32-bit address space */
    
    /* large files will have bit 8 of head[3] and bit 8 of tail[12] set */
    tailpos = eip->position;
    endpos = tailpos + sizeof tail;
    overflow = endpos >> 31;

    /* for file sizes > 2 Gb, store bits 41..31 in tail[2..3] */
    SetBigEndian32(tail, 0x80000000 + overflow);
    SetBigEndian32(tail+4, tailpos);
    SetBigEndian32(tail+8, 
                    ((eip->diff ? 0x90 : 0x80) << 24) + tailpos - rootpos);
    SetBigEndian32(tail+12, rootpos);
    if (overflow)
        tail[12] |= 0x80;
    
    EmitCopy(eip, tail, sizeof tail);
    
    if (overflow) {
        /* store bits 41..36 in head[3], and bits 35..4 in head[4..7] */
        Assert((endpos & 15) == 0);
        fixup->c = 0x80 | ((endpos >> 16) >> 20);
        SetBigEndian32(fixup->d, endpos >> 4);
    } else
        SetBigEndian32(fixup->d, endpos);
        
    return endpos;
}

Int_t ViewSave (View_p view, void *aux, SaveInitFun initfun, SaveDataFun datafun, int diff) {
    int i, numitems;
    Int_t bytes;
    struct Buffer buffer;
    EmitItem_p items;
    EmitInfo einfo;
    
    InitBuffer(&buffer);
    
    einfo.itembuf = &buffer;
    einfo.diff = diff;

    bytes = EmitComplete(&einfo, view);

    numitems = BufferFill(&buffer) / sizeof(EmitItem);
    items = BufferAsPtr(&buffer, 1);

    if (initfun != NULL)
        aux = initfun(aux, bytes);

    for (i = 0; i < numitems; ++i) {
        if (aux != NULL)
            aux = datafun(aux, items[i].data, items[i].size);
        free((void*) items[i].data);
    }
    
    if (aux == NULL)
        bytes = 0;
    
    ReleaseBuffer(&buffer, 0);
    return bytes;
}
/* %include<file.c>% */
/*
 * file.c - Implementation of memory-mapped file access.
 */

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#endif


static View_p MapSubview (MappedFile_p, Int_t, View_p, View_p); /* forward */

#define MF_Data(map) ((const char*) ((map)->data[0].p))
#define MF_Length(map) ((Int_t) (map)->data[1].p)

static void MappedCleaner (MappedFile_p map) {
    Cleaner fun = *((Cleaner*) (map + 1));
    if (fun != NULL)
        fun(map);
}

static struct SeqType ST_Mapped = { "mapped", NULL, 0, MappedCleaner };

MappedFile_p InitMappedFile (const char *data, Int_t length, Cleaner fun) {
    MappedFile_p map;
    
    map = NewSequence(0, &ST_Mapped, sizeof(Cleaner));
    /* data[0] points to the current start of the mapped area */
    /* data[1] is the current length of the mapped area */
    /* data[2] points to the original mapped area */
    /* data[3] is available for storing an Object_p, see ext_tcl.c */
    map->data[0].p = (void*) data;
    map->data[1].p = (void*) length;
    map->data[2].p = (void*) data;
    
    *((Cleaner*) (map + 1)) = fun; /* TODO: get rid of this ugly hack */
    
    return map;
}

static void MappedFileCleaner (MappedFile_p map) {
#if WIN32+0
    UnmapViewOfFile(map->data[2].p);
#else
    int offset = MF_Data(map) - (const char*) map->data[2].p;
    munmap(map->data[2].p, MF_Length(map) + offset);
#endif
}

#if WIN32+0
/*
 * If we are opening a Windows PE executable with an attached metakit
 * then we must check for the presence of an Authenticode certificate
 * and reduce the length of our mapped region accordingly
 */

static DWORD
AuthenticodeOffset(LPBYTE pData, DWORD cbData)
{
    if (pData[0] == 'M' && pData[1] == 'Z')              /* IMAGE_DOS_SIGNATURE */
    {
        LPBYTE pNT = pData + *(LONG *)(pData + 0x3c);    /* e_lfanew */
        if (pNT[0] == 'P' && pNT[1] == 'E' && pNT[2] == 0 && pNT[3] == 0)
        {                                                /* IMAGE_NT_SIGNATURE */
            DWORD dwCheckSum = 0, dwDirectories = 0;
            LPBYTE pOpt = pNT + 0x18;                    /* OptionalHeader */
            LPDWORD pCertDir = NULL;
            if (pOpt[0] == 0x0b && pOpt[1] == 0x01) {    /* IMAGE_NT_OPTIONAL_HDR_MAGIC */
                dwCheckSum = *(DWORD *)(pOpt + 0x40);    /* Checksum */
                dwDirectories = *(DWORD *)(pOpt + 0x5c); /* NumberOfRvaAndSizes */
                if (dwDirectories > 4) {                 /* DataDirectory[] */
                    pCertDir = (DWORD *)(pOpt + 0x60 + (4 * 8));
                }
            } else {
                dwCheckSum = *(DWORD *)(pOpt + 0x40);    /* Checksum */
                dwDirectories = *(DWORD *)(pOpt + 0x6c); /* NumberOfRvaAndSizes */
                if (dwDirectories > 4) {                 /* DataDirectory[] */
                    pCertDir = (DWORD *)(pOpt + 0x70 + (4 * 8));
                }
            }

            if (pCertDir && pCertDir[1] > 0) {
                int n = 0;
                cbData = pCertDir[0];
                /* need to eliminate any zero padding - up to 8 bytes */
                while (pData[cbData - 16] != 0x80 && pData[cbData-1] == 0 && n < 16) {
                    --cbData, ++n;
                }
            }
        }
    }
    return cbData;
}
#endif /* WIN32 */

static MappedFile_p OpenMappedFile (const char *filename) {
    const char *data = NULL;
    Int_t length = -1;
        
#if WIN32+0
    {
        DWORD n;
        HANDLE h, f;
        OSVERSIONINFO os;

        memset(&os, 0, sizeof(os));
        os.dwOSVersionInfoSize = sizeof(os);
	os.dwPlatformId = VER_PLATFORM_WIN32_WINDOWS;
        GetVersionEx(&os);
        if (os.dwPlatformId < VER_PLATFORM_WIN32_NT) {
            f = CreateFileA(filename, GENERIC_READ, FILE_SHARE_READ, 0,
                OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
        } else {
            f = CreateFileW((LPCWSTR)filename, GENERIC_READ, FILE_SHARE_READ, 0,
                OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
        }
        if (f != INVALID_HANDLE_VALUE) {
            h = CreateFileMapping(f, 0, PAGE_READONLY, 0, 0, 0);
            if (h != INVALID_HANDLE_VALUE) {
                n = GetFileSize(f, 0);
                data = MapViewOfFile(h, FILE_MAP_READ, 0, 0, n);
                if (data != NULL) {
                    length = AuthenticodeOffset((LPBYTE)data, n);
                }
                CloseHandle(h);
            }
            CloseHandle(f);
        }
    }
#else
    {
        struct stat sb;
        int fd = open(filename, O_RDONLY);
        if (fd != -1) {
            if (fstat(fd, &sb) == 0) {
                data = mmap(0, sb.st_size, PROT_READ, MAP_SHARED, fd, 0);

                /* On HP-UX mmap does not work multiple times on the same
                   file, so try a private mmap before giving up on it */
                if (data == MAP_FAILED)
                  data = mmap(0, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0);

                if (data != MAP_FAILED)
                    length = sb.st_size;
            }
            close(fd);
        }
    }
#endif

    if (length < 0)
        return NULL;
        
    return InitMappedFile(data, length, MappedFileCleaner);
}

static void AdjustMappedFile (MappedFile_p map, int offset) {
    map->data[0].p = (void*) (MF_Data(map) + offset);
    map->data[1].p = (void*) (MF_Length(map) - offset);
}

static int IsReversedEndian(MappedFile_p map) {
#if _BIG_ENDIAN+0
    return *MF_Data(map) == 'J';
#else
    return *MF_Data(map) == 'L';
#endif
}

static Int_t GetVarInt (const char **cp) {
    int8_t b;
    Int_t v = 0;
    do {
        b = *(*cp)++;
        v = (v << 7) + b;
    } while (b >= 0);
    return v + 128;
}

static Int_t GetVarPair(const char** cp) {
    Int_t n = GetVarInt(cp);
    if (n > 0 && GetVarInt(cp) == 0)
        *cp += n;
    return n;
}

#define MM_cache        data[0].p
#define MM_offvec       data[1].q
#define MM_mapf         data[2].q
#define MM_meta         data[3].q

#define MM_offsets  MM_offvec->data[0].p
#define MM_base         MM_offvec->data[1].q


static void MappedViewCleaner (Seq_p seq) {
    int i, count;
    const View_p *subviews = seq->MM_cache;
    
    count = seq->MM_offvec->count;
    for (i = 0; i < count; ++i)
        DecRefCount(subviews[i]);
    
    DecRefCount(seq->MM_base);
}

static ItemTypes MappedViewGetter (int row, Item_p item) {
    Seq_p seq = item->c.seq;
    View_p *subviews = seq->MM_cache;
    
    if (subviews[row] == NULL) {
        Seq_p base = seq->MM_base;
        const Int_t *offsets = seq->MM_offsets;
        
        if (base != NULL)
            base = GetViewItem(base, row, item->c.pos, IT_view).v;
        
        subviews[row] = IncRefCount(MapSubview(seq->MM_mapf, offsets[row],
                                                        seq->MM_meta, base));
    }
    
    item->v = subviews[row];
    return IT_view;
}

static struct SeqType ST_MappedView = {
    "mappedview", MappedViewGetter, 01110, MappedViewCleaner
};

static Seq_p MappedViewCol (MappedFile_p map, int rows, const char **nextp, View_p meta, View_p base) {
    int r, c, cols, subcols;
    Int_t colsize, colpos, *offsets;
    const char *next;
    Seq_p offseq, result;
    
    offseq = NewPtrVec(rows, &offsets);
    
    cols = ViewSize(meta);
    
    colsize = GetVarInt(nextp);
    colpos = colsize > 0 ? GetVarInt(nextp) : 0;
    next = MF_Data(map) + colpos;
    
    for (r = 0; r < rows; ++r) {
        offsets[r] = next - MF_Data(map);
        GetVarInt(&next);
        if (cols == 0) {
            Int_t desclen = GetVarInt(&next);
            const char *desc = next;
            next += desclen;
            meta = DescAsMeta(&desc, next);
        }
        if (GetVarInt(&next) > 0) {
            subcols = ViewSize(meta);
            for (c = 0; c < subcols; ++c)
                switch (GetColType(meta, c)) {
                    case 'B': case 'S': if (GetVarPair(&next))
                                            GetVarPair(&next);
                                        /* fall through */
                    default:            GetVarPair(&next);
                }
        }
    }
    
    result = NewSequence(rows, &ST_MappedView, rows * sizeof(View_p));
    /* data[0] points to a subview cache */
    /* data[1] points to a sequence owning the offest vector */
    /* data[2] points to the mapped file */
    /* data[3] points to the meta view */
    result->MM_offvec = IncRefCount(offseq);
    result->MM_mapf = IncRefCount(map);
    result->MM_meta = IncRefCount(cols > 0 ? meta : EmptyMetaView());
    /* offseq->data[1] points to the base view if there is one */
    result->MM_base = IncRefCount(base);
    
    /* TODO: could combine subview cache and offsets vector */
    
    return result;
}

static struct SeqType ST_MappedFix = { "mappedfix", NULL, 010 };

static Seq_p MappedFixedCol (MappedFile_p map, int rows, const char **nextp, int isreal) {
    Int_t colsize, colpos;
    const char *data;
    Seq_p result;

    colsize = GetVarInt(nextp);
    colpos = colsize > 0 ? GetVarInt(nextp) : 0;
    data = MF_Data(map) + colpos;
    
    /* if bit count is too low-for single-bit vectors then it's compressed */
    if (rows >= 128 && 0 < colsize && colsize < rows/8) {
        int from, pos = 0;
        
        result = NewBitVec(rows);
        from = *data & 0x80 ? 0 : NextElias(data, colsize, &pos);
            
        for (;;) {
            int count = NextElias(data, colsize, &pos);
            if (count == 0)
                break;
            SetBitRange(result, from, count);
            from += count + NextElias(data, colsize, &pos);
        }
    } else {    
        int rev = IsReversedEndian(map);
    
        result = NewSequence(rows, &ST_MappedFix, 0);
        result->getter = FixedGetter(colsize, rows, isreal, rev)->getfun;
        /* data[0] points to the mapped data */
        /* data[1] points to the mapped file */
        result->data[0].p = (void*) data;
        result->data[1].p = IncRefCount(map);
    }
    
    return result;
}

#define MS_offvec       data[0].q
#define MS_offptr       data[1].p
#define MS_mapf         data[2].q
#define MS_sizes        data[3].q

static ItemTypes MappedStringGetter (int row, Item_p item) {
    const Int_t *offsets = item->c.seq->MS_offptr;
    const char *data = MF_Data(item->c.seq->MS_mapf);

    if (offsets[row] == 0)
        item->s = "";
    else if (offsets[row] > 0)
        item->s = data + offsets[row];
    else {
        const char *next = data - offsets[row];
        if (GetVarInt(&next) > 0)
            item->s = data + GetVarInt(&next);
        else
            item->s = "";
    }

    return IT_string;
}

static struct SeqType ST_MappedString = {
    "mappedstring", MappedStringGetter, 01101
};

static ItemTypes MappedBytesGetter (int row, Item_p item) {
    Seq_p seq = item->c.seq;
    const Int_t *offsets = seq->MS_offptr;
    const char *data = MF_Data(seq->MS_mapf);
    
    item->u.len = GetSeqItem(row, seq->MS_sizes, IT_int).i;
    
    if (offsets[row] >= 0)
        item->u.ptr = (const Byte_t*) data + offsets[row];
    else {
        const char *next = data - offsets[row];
        item->u.len = GetVarInt(&next);
        item->u.ptr = (const Byte_t*) data + GetVarInt(&next);
    }

    return IT_bytes;
}

static struct SeqType ST_MappedBytes = {
    "mappedstring", MappedBytesGetter, 01101
};

static Seq_p MappedStringCol (MappedFile_p map, int rows, const char **nextp, int istext) {
    int r, len, memopos;
    Int_t colsize, colpos, *offsets;
    const char *next, *limit;
    Seq_p offseq, result, sizes;

    offseq = NewPtrVec(rows, &offsets);

    colsize = GetVarInt(nextp);
    colpos = colsize > 0 ? GetVarInt(nextp) : 0;

    if (colsize > 0) {
        sizes = MappedFixedCol(map, rows, nextp, 0);
        for (r = 0; r < rows; ++r) {
            len = GetSeqItem(r, sizes, IT_int).i;
            if (len > 0) {
                offsets[r] = colpos;
                colpos += len;
            }
        }
    } else
        sizes = NewSequence(rows, FixedGetter(0, rows, 0, 0), 0);

    colsize = GetVarInt(nextp);
    memopos = colsize > 0 ? GetVarInt(nextp) : 0;
    next = MF_Data(map) + memopos;
    limit = next + colsize;
    
    /* negated offsets point to the size/pos pair in the map */
    for (r = 0; next < limit; ++r) {
        r += (int) GetVarInt(&next);
        offsets[r] = MF_Data(map) - next; /* always < 0 */
        GetVarPair(&next);
    }

    result = NewSequence(rows, istext ? &ST_MappedString : &ST_MappedBytes, 0);
    /* data[0] points to a sequence owning the offset vector */
    /* data[1] points to that vector of subview offsets */
    /* data[2] points to the mapped file */
    /* data[3] points to sizes seq if binary or is null if zero-terminated */
    result->MS_offvec = IncRefCount(offseq);
    result->MS_offptr = offsets;
    result->MS_mapf = IncRefCount(map);
    result->MS_sizes = istext ? NULL : IncRefCount(sizes);

    return result;
}

static Seq_p MappedBits (MappedFile_p map, int rows, const char **nextp) {
    Seq_p seq;
    
    if ((**nextp & 0xFF) == 0x80) {
        ++*nextp;
        return NULL; /* zero-sized column */
    }
        
    seq = MappedFixedCol(map, rows, nextp, 0);

    /* if this does not use 1-bit encoding, then we need to convert back */
    if (rows < 8 && seq->getter != PickIntGetter(1)->getfun) {
        int i;
        Seq_p result = NULL;

        for (i = 0; i < rows; ++i)
            if (GetSeqItem(i, seq, IT_int).i)
                SetBit(&result, i);
                
        return result;
    }
    
    return seq;
}

static View_p MapCols (MappedFile_p map, const char **nextp, View_p meta, View_p base, Seq_p adjseq) {
    int i, c, r, rows, *rowptr = NULL;
    View_p result, subview;
    Seq_p seq, usedmap = NULL, rowmap;
    Item item;
    
    rows = (int) GetVarInt(nextp);
    
    if (ViewSize(meta) == 0)
        return NoColumnView(rows);
    
    if (base != NULL) {
        int pos = 0, from = 0, count = 0;
        
        rowmap = NewIntVec(rows, &rowptr);
        while (NextBits(adjseq, &from, &count))
            for (i = 0; i < count; ++i)
                rowptr[pos++] = from + i;
                
        result = base;
    } else
        result = NewView(meta);
    
    if (rows > 0)
        for (c = 0; c < ViewWidth(result); ++c) {
            if (base != NULL) {
                usedmap = MappedBits(map, rows, nextp);
                r = CountBits(usedmap);
            } else
                r = rows;
                
            switch (ViewColType(result, c)) {
                
                case IT_int:
                case IT_wide:
                    seq = MappedFixedCol(map, r, nextp, 0); 
                    break;

                case IT_float:
                case IT_double:
                    seq = MappedFixedCol(map, r, nextp, 1);
                    break;
                    
                case IT_string:
                    seq = MappedStringCol(map, r, nextp, 1); 
                    break;
                    
                case IT_bytes:
                    seq = MappedStringCol(map, r, nextp, 0);
                    break;
                    
                case IT_view:
                    subview = GetViewItem(meta, c, MC_subv, IT_view).v;
                    seq = MappedViewCol(map, r, nextp, subview, base); 
                    break;
                    
                default:
                    Assert(0);
                    return result;
            }
            
            if (base != NULL) {
                i = 0;
                for (r = 0; r < usedmap->count; ++r)
                    if (TestBit(usedmap, r)) {
                        item.c.seq = seq;
                        item.c.pos = -1;
                        GetItem(i++, &item);
                        result = ViewSet(result, rowptr[r], c, &item);
                    }
                Assert(i == seq->count);
            } else
                SetViewSeqs(result, c, 1, seq);
        }

    return result;
}

static View_p MapSubview (MappedFile_p map, Int_t offset, View_p meta, View_p base) {
    const char *next;
    
    next = MF_Data(map) + offset;
    GetVarInt(&next);
    
    if (base != NULL) {
        int inscnt;
        View_p insview;
        Seq_p delseq, insseq, adjseq;
        
        meta = V_Meta(base);
        
        GetVarInt(&next); /* structure changes and defaults, NOTYET */

        delseq = MappedBits(map, ViewSize(base), &next);
        if (delseq != NULL) {
            int delcnt = 0, from = 0, count = 0;
            while (NextBits(delseq, &from, &count)) {
                base = ViewReplace(base, from - delcnt, count, NULL);
                delcnt += count;
            }
        }
            
        adjseq = MappedBits(map, ViewSize(base), &next);
        if (adjseq != NULL)
            base = MapCols(map, &next, meta, base, adjseq);

        insview = MapCols(map, &next, meta, NULL, NULL);
        inscnt = ViewSize(insview);
        if (inscnt > 0) {
            int shift = 0, from = 0, count = 0;
            insseq = MappedBits(map, ViewSize(base) + inscnt, &next);
            while (NextBits(insseq, &from, &count)) {
                View_p newdata = StepView(insview, count, shift, 1, 1);
                base = ViewReplace(base, from, 0, newdata);
                shift += count;
            }
        }
    
        return base;
    }
    
    if (ViewSize(meta) == 0) {
        Int_t desclen = GetVarInt(&next);
        const char *desc = next;
        next += desclen;
        meta = DescAsMeta(&desc, next);
    }

    return MapCols(map, &next, meta, NULL, NULL);
}

static int BigEndianInt32 (const char *p) {
    const Byte_t *up = (const Byte_t*) p;
    return (p[0] << 24) | (up[1] << 16) | (up[2] << 8) | up[3];
}

View_p MappedToView (MappedFile_p map, View_p base) {
    int i, t[4];
    Int_t datalen, rootoff;
    
    if (MF_Length(map) <= 24 || *(MF_Data(map) + MF_Length(map) - 16) != '\x80')
        return NULL;
        
    for (i = 0; i < 4; ++i)
        t[i] = BigEndianInt32(MF_Data(map) + MF_Length(map) - 16 + i * 4);
        
    datalen = t[1] + 16;
    rootoff = t[3];

    if (rootoff < 0) {
        const Int_t mask = 0x7FFFFFFF; 
        datalen = (datalen & mask) + ((Int_t) ((t[0] & 0x7FF) << 16) << 15);
        rootoff = (rootoff & mask) + (datalen & ~mask);
        /* FIXME: rollover at 2 Gb, prob needs: if (rootoff > datalen) ... */
    }
    
    AdjustMappedFile(map, MF_Length(map) - datalen);
    return MapSubview(map, rootoff, EmptyMetaView(), base);
}

View_p OpenDataFile (const char *filename) {
    MappedFile_p map;
    
    map = OpenMappedFile(filename);
    if (map == NULL)
        return NULL;
        
    return MappedToView(map, NULL);
}
/* %include<getters.c>% */
/*
 * getters.c - Implementation of several simple getter functions.
 */

#include <stdlib.h>


static ItemTypes Getter_i0 (int row, Item_p item) {
    item->i = 0;
    return IT_int;
}

static ItemTypes Getter_i1 (int row, Item_p item) {
    const char *ptr = item->c.seq->data[0].p;
    item->i = (ptr[row>>3] >> (row&7)) & 1;
    return IT_int;
}

static ItemTypes Getter_i2 (int row, Item_p item) {
    const char *ptr = item->c.seq->data[0].p;
    item->i = (ptr[row>>2] >> 2*(row&3)) & 3;
    return IT_int;
}

static ItemTypes Getter_i4 (int row, Item_p item) {
    const char *ptr = item->c.seq->data[0].p;
    item->i = (ptr[row>>1] >> 4*(row&1)) & 15;
    return IT_int;
}

static ItemTypes Getter_i8 (int row, Item_p item) {
    const char *ptr = item->c.seq->data[0].p;
    item->i = (int8_t) ptr[row];
    return IT_int;
}

#if VALUES_MUST_BE_ALIGNED+0

static ItemTypes Getter_i16 (int row, Item_p item) {
    const Byte_t *ptr = (const Byte_t*) item->c.seq->data[0].p + row * 2;
#if _BIG_ENDIAN+0
    item->i = (((int8_t) ptr[0]) << 8) | ptr[1];
#else
    item->i = (((int8_t) ptr[1]) << 8) | ptr[0];
#endif
    return IT_int;
}

static ItemTypes Getter_i32 (int row, Item_p item) {
    const char *ptr = (const char*) item->c.seq->data[0].p + row * 4;
    int i;
    for (i = 0; i < 4; ++i)
        item->b[i] = ptr[i];
    return IT_int;
}

static ItemTypes Getter_i64 (int row, Item_p item) {
    const char *ptr = (const char*) item->c.seq->data[0].p + row * 8;
    int i;
    for (i = 0; i < 8; ++i)
        item->b[i] = ptr[i];
    return IT_wide;
}

static ItemTypes Getter_f32 (int row, Item_p item) {
    Getter_i32(row, item);
    return IT_float;
}

static ItemTypes Getter_f64 (int row, Item_p item) {
    Getter_i64(row, item);
    return IT_double;
}

#else

static ItemTypes Getter_i16 (int row, Item_p item) {
    const char *ptr = item->c.seq->data[0].p;
    item->i = ((short*) ptr)[row];
    return IT_int;
}

static ItemTypes Getter_i32 (int row, Item_p item) {
    const char *ptr = item->c.seq->data[0].p;
    item->i = ((const int*) ptr)[row];
    return IT_int;
}

static ItemTypes Getter_i64 (int row, Item_p item) {
    const char *ptr = item->c.seq->data[0].p;
    item->w = ((const int64_t*) ptr)[row];
    return IT_wide;
}

static ItemTypes Getter_f32 (int row, Item_p item) {
    const char *ptr = item->c.seq->data[0].p;
    item->f = ((const float*) ptr)[row];
    return IT_float;
}

static ItemTypes Getter_f64 (int row, Item_p item) {
    const char *ptr = item->c.seq->data[0].p;
    item->d = ((const double*) ptr)[row];
    return IT_double;
}

#endif

static ItemTypes Getter_i16r (int row, Item_p item) {
    const Byte_t *ptr = (const Byte_t*) item->c.seq->data[0].p + row * 2;
#if _BIG_ENDIAN+0
    item->i = (((int8_t) ptr[1]) << 8) | ptr[0];
#else
    item->i = (((int8_t) ptr[0]) << 8) | ptr[1];
#endif
    return IT_int;
}

static ItemTypes Getter_i32r (int row, Item_p item) {
    const char *ptr = (const char*) item->c.seq->data[0].p + row * 4;
    int i;
    for (i = 0; i < 4; ++i)
        item->b[i] = ptr[3-i];
    return IT_int;
}

static ItemTypes Getter_i64r (int row, Item_p item) {
    const char *ptr = (const char*) item->c.seq->data[0].p + row * 8;
    int i;
    for (i = 0; i < 8; ++i)
        item->b[i] = ptr[7-i];
    return IT_wide;
}

static ItemTypes Getter_f32r (int row, Item_p item) {
    Getter_i32r(row, item);
    return IT_float;
}

static ItemTypes Getter_f64r (int row, Item_p item) {
    Getter_i64r(row, item);
    return IT_double;
}

static struct SeqType ST_Get_i0   = { "get_i0"  , Getter_i0   };
static struct SeqType ST_Get_i1   = { "get_i1"  , Getter_i1   };
static struct SeqType ST_Get_i2   = { "get_i2"  , Getter_i2   };
static struct SeqType ST_Get_i4   = { "get_i4"  , Getter_i4   };
static struct SeqType ST_Get_i8   = { "get_i8"  , Getter_i8   };
static struct SeqType ST_Get_i16  = { "get_i16" , Getter_i16  };
static struct SeqType ST_Get_i32  = { "get_i32" , Getter_i32  };
static struct SeqType ST_Get_i64  = { "get_i64" , Getter_i64  };
static struct SeqType ST_Get_i16r = { "get_i16r", Getter_i16r };
static struct SeqType ST_Get_i32r = { "get_i32r", Getter_i32r };
static struct SeqType ST_Get_i64r = { "get_i64r", Getter_i64r };
static struct SeqType ST_Get_f32  = { "get_f32" , Getter_f32  };
static struct SeqType ST_Get_f64  = { "get_f64" , Getter_f64  };
static struct SeqType ST_Get_f32r = { "get_f32r", Getter_f32r };
static struct SeqType ST_Get_f64r = { "get_f64r", Getter_f64r };

SeqType_p PickIntGetter (int bits) {
    switch (bits) {
        default:    Assert(0); /* fall through */
        case 0:     return &ST_Get_i0;
        case 1:     return &ST_Get_i1;
        case 2:     return &ST_Get_i2;
        case 4:     return &ST_Get_i4;
        case 8:     return &ST_Get_i8;
        case 16:    return &ST_Get_i16;
        case 32:    return &ST_Get_i32;
        case 64:    return &ST_Get_i64;
    }
}

SeqType_p FixedGetter (int bytes, int rows, int real, int flip) {
    int bits;

    static char widths[8][7] = {
        {0,-1,-1,-1,-1,-1,-1},
        {0, 8,16, 1,32, 2, 4},
        {0, 4, 8, 1,16, 2,-1},
        {0, 2, 4, 8, 1,-1,16},
        {0, 2, 4,-1, 8, 1,-1},
        {0, 1, 2, 4,-1, 8,-1},
        {0, 1, 2, 4,-1,-1, 8},
        {0, 1, 2,-1, 4,-1,-1},
    };

    bits = rows < 8 && bytes < 7 ? widths[rows][bytes] : (bytes << 3) / rows;

    switch (bits) {
        case 16:    return flip ? &ST_Get_i16r : &ST_Get_i16;
        case 32:    return real ? flip ? &ST_Get_f32r : &ST_Get_f32
                                : flip ? &ST_Get_i32r : &ST_Get_i32;
        case 64:    return real ? flip ? &ST_Get_f64r : &ST_Get_f64
                                : flip ? &ST_Get_i64r : &ST_Get_i64;
    }

    return PickIntGetter(bits);
}
/* %include<hash.c>% */
/*
 * hash.c - Implementation of hashing functions.
 */

#include <stdlib.h>
#include <string.h>


typedef struct HashInfo *HashInfo_p;

typedef struct HashInfo {
    View_p  view;       /* input view */
    int     prime;      /* prime used for hashing */
    int     fill;       /* used to fill map */
    int    *map;        /* map of unique rows */
    Column  mapcol;     /* owner of map */
    int    *hvec;       /* hash probe vector */
    Column  veccol;     /* owner of hvec */
    int    *hashes;     /* hash values, one per view row */
    Column  hashcol;    /* owner of hashes */
} HashInfo;

static int StringHash (const char *s, int n) {
    /* similar to Python's stringobject.c */
    int i, h = (*s * 0xFF) << 7;
    if (n < 0)
        n = strlen(s);
    for (i = 0; i < n; ++i)
        h = (1000003 * h) ^ s[i];
    return h ^ i;
}

static Column HashCol (ItemTypes type, Column column) {
    int i, count, *data;
    Seq_p seq;
    Item item;

/* the following is not possible: the result may be xor-ed into!
    if (type == IT_int && column.seq->getter == PickIntGetter(32))
        return column;
*/

    count = column.seq->count;
    seq = NewIntVec(count, &data);

    switch (type) {
        
        case IT_int:
            for (i = 0; i < count; ++i)
                data[i] = GetColItem(i, column, IT_int).i;
            break;
                
        case IT_wide:
            for (i = 0; i < count; ++i) {
                item = GetColItem(i, column, IT_wide);
                data[i] = item.q[0] ^ item.q[1];
            }
            break;
                
        case IT_float:
            for (i = 0; i < count; ++i)
                data[i] = GetColItem(i, column, IT_float).i;
            break;
                
        case IT_double:
            for (i = 0; i < count; ++i) {
                item = GetColItem(i, column, IT_double);
                data[i] = item.q[0] ^ item.q[1];
            }
            break;
                
        case IT_string:
            for (i = 0; i < count; ++i) {
                item = GetColItem(i, column, IT_string);
                data[i] = StringHash(item.s, -1);
            }
            break;
                
        case IT_bytes:
            for (i = 0; i < count; ++i) {
                item = GetColItem(i, column, IT_bytes);
                data[i] = StringHash((const char*) item.u.ptr, item.u.len);
            }
            break;
                
        case IT_view:
            for (i = 0; i < count; ++i) {
                int j, hcount, hval = 0;
                const int *hvec;
                Column hashes;
                
                item = GetColItem(i, column, IT_view);
                hashes = HashValues(item.v);
                hvec = (const int*) hashes.seq->data[0].p;
                hcount = hashes.seq->count;
                
                for (j = 0; j < hcount; ++j)
                    hval ^= hvec[j];
                /* TODO: release hashes right now */
            
                data[i] = hval ^ hcount;
            }
            break;
                
        default: Assert(0);
    }

    return SeqAsCol(seq);
}

ItemTypes HashColCmd_SO (Item args[]) {
    ItemTypes type;
    Column column;
    
    type = CharAsItemType(args[0].s[0]);
    column = CoerceColumn(type, args[1].o);
    if (column.seq == NULL)
        return IT_unknown;
        
    args->c = HashCol(type, column);
    return IT_column;
}

int RowHash (View_p view, int row) {
    int c, hash = 0;
    Item item;

    for (c = 0; c < ViewWidth(view); ++c) {
        item.c = ViewCol(view, c);
        switch (GetItem(row, &item)) {

            case IT_int:
                hash ^= item.i;
                break;

            case IT_string:
                hash ^= StringHash(item.s, -1);
                break;

            case IT_bytes:
                hash ^= StringHash((const char*) item.u.ptr, item.u.len);
                break;

            default: {
                View_p rview = StepView(view, 1, row, 1, 1);
                Column rcol = HashValues(rview);
                hash = *(const int*) rcol.seq->data[0].p;
                break;
            }
        }
    }

    return hash;
}

static void XorWithIntCol (Column src, Column_p dest) {
    const int *srcdata = (const int*) src.seq->data[0].p;
    int i, count = src.seq->count, *destdata = (int*) dest->seq->data[0].p;

    for (i = 0; i < count; ++i)
        destdata[i] ^= srcdata[i];
}

Column HashValues (View_p view) {
    int c;
    Column result;

    if (ViewWidth(view) == 0 || ViewSize(view) == 0)
        return SeqAsCol(NewIntVec(ViewSize(view), NULL));
        
    result = HashCol(ViewColType(view, 0), ViewCol(view, 0));
    for (c = 1; c < ViewWidth(view); ++c) {
        Column auxcol = HashCol(ViewColType(view, c), ViewCol(view, c));
        /* TODO: get rid of the separate xor step by xoring in HashCol */
        XorWithIntCol(auxcol, &result);
    }
    return result;
}

static int HashFind (View_p keyview, int keyrow, int keyhash, HashInfo_p data) {
    int probe, datarow, mask, step;

    mask = data->veccol.seq->count - 1;
    probe = ~keyhash & mask;

    step = (keyhash ^ (keyhash >> 3)) & mask;
    if (step == 0)
        step = mask;

    for (;;) {
        probe = (probe + step) & mask;
        if (data->hvec[probe] == 0)
            break;

        datarow = data->map[data->hvec[probe]-1];
        if (keyhash == data->hashes[datarow] &&
                RowEqual(keyview, keyrow, data->view, datarow))
            return data->hvec[probe] - 1;

        step <<= 1;
        if (step > mask)
            step ^= data->prime;
    }

    if (keyview == data->view) {
        data->hvec[probe] = data->fill + 1;
        data->map[data->fill++] = keyrow;
    }

    return -1;
}

static int StringHashFind (const char *key, Seq_p hseq, Column values) {
    int probe, datarow, mask, step, keyhash;
    const int *hvec = (const int*) hseq->data[0].p;
    int prime = hseq->data[2].i;
    
    keyhash = StringHash(key, -1);
    mask = hseq->count - 1;
    probe = ~keyhash & mask;

    step = (keyhash ^ (keyhash >> 3)) & mask;
    if (step == 0)
        step = mask;

    for (;;) {
        probe = (probe + step) & mask;
        datarow = hvec[probe] - 1;
        if (datarow < 0)
            break;

        /* These string hashes are much simpler than the standard HashFind:
             no hashes vector, no indirect map, compute all hashes on-the-fly */
                
        if (strcmp(key, GetColItem(datarow, values, IT_string).s) == 0)
            return datarow;

        step <<= 1;
        if (step > mask)
            step ^= prime;
    }

    return ~probe;
}

static int Log2bits (int n) {
    int bits = 0;
    while ((1 << bits) < n)
        ++bits;
    return bits;
}

static Column HashVector (int rows) {
    int bits = Log2bits((4 * rows) / 3);
    if (bits < 2)
        bits = 2;
    return SeqAsCol(NewIntVec(1 << bits, NULL));
}

static void InitHashInfo (HashInfo_p info, View_p view, Column hmap, Column hvec, Column hashes) {
    int size = hvec.seq->count;

    static char slack [] = {
        0, 0, 3, 3, 3, 5, 3, 3, 29, 17, 9, 5, 83, 27, 43, 3,
        45, 9, 39, 39, 9, 5, 3, 33, 27, 9, 71, 39, 9, 5, 83, 0
    };

    info->view = view;
    info->prime = size + slack[Log2bits(size-1)];
    info->fill = 0;
    
    info->mapcol = hmap;
    info->map = (int*) hmap.seq->data[0].p;

    info->veccol = hvec;
    info->hvec = (int*) hvec.seq->data[0].p;

    info->hashcol = hashes;
    info->hashes = (int*) hashes.seq->data[0].p;
}

int StringLookup (const char *key, Column values) {
    int h, r, rows, *hptr;
    const char *string;
    Column hvec;
    HashInfo info;
    
    /* adjust data[2], this assumes values is a string column */
    
    if (values.seq->data[2].p == NULL) {
        rows = values.seq->count;
        hvec = HashVector(rows);
        hptr = (int*) hvec.seq->data[0].p;
        
        /* use InitHashInfo to get at the prime number, bit of a hack */
        InitHashInfo(&info, NULL, hvec, hvec, hvec);
        hvec.seq->data[2].i = info.prime;
        
        for (r = 0; r < rows; ++r) {
            string = GetColItem(r, values, IT_string).s;
            h = StringHashFind(string, hvec.seq, values);
            if (h < 0) /* silently ignore duplicates */
                hptr[~h] = r + 1;
        }
        
        values.seq->data[2].p = IncRefCount(hvec.seq);
    }
    
    h = StringHashFind(key, (Seq_p) values.seq->data[2].p, values);
    return h >= 0 ? h : -1;
}

static void FillHashInfo (HashInfo_p info, View_p view) {
    int r, rows;
    Column mapcol;
    
    rows = ViewSize(view);
    mapcol = SeqAsCol(NewIntVec(rows, NULL)); /* worst-case, dunno #groups */

    InitHashInfo(info, view, mapcol, HashVector(rows), HashValues(view));

    for (r = 0; r < rows; ++r)
        HashFind(view, r, info->hashes[r], info);

    /* TODO: reclaim unused entries at end of map */
    mapcol.seq->count = info->fill;
}

static void ChaseLinks (HashInfo_p info, int count, const int *hmap, const int *lmap) {
    int groups = info->fill, *smap = info->hvec, *gmap = info->hashes;
    
    while (--groups >= 0) {
        int head = hmap[groups] - 1;
        smap[groups] = count;
        while (head >= 0) {
            gmap[--count] = head;
            head = lmap[head];
        }
    }
    /* assert(count == 0); */
}

void FillGroupInfo (HashInfo_p info, View_p view) {
    int g, r, rows, *hmap, *lmap;
    Column mapcol, headmap, linkmap;
    
    rows = ViewSize(view);
    mapcol = SeqAsCol(NewIntVec(rows, NULL)); /* worst-case, dunno #groups */
    headmap = SeqAsCol(NewIntVec(rows, &hmap)); /* same: don't know #groups */
    linkmap = SeqAsCol(NewIntVec(rows, &lmap));

    InitHashInfo(info, view, mapcol, HashVector(rows), HashValues(view));

    for (r = 0; r < rows; ++r) {
        g = HashFind(view, r, info->hashes[r], info);
        if (g < 0)
            g = info->fill - 1;
        lmap[r] = hmap[g] - 1;
        hmap[g] = r + 1;
    }
    
    /* TODO: reclaim unused entries at end of map and hvec */
    info->mapcol.seq->count = info->veccol.seq->count = info->fill;
    
    ChaseLinks(info, rows, hmap, lmap);

    /* TODO: could release headmap and linkmap but that's a no-op here */
    
    /*  There's probably an opportunity to reduce space usage further,
        since the grouping map points to the starting row of each group:
            map[i] == gmap[smap[i]]
        Perhaps "map" (which starts out with #rows entries) can be re-used
        to append the gmap entries (if we can reduce it by #groups items).
        Or just release map[x] for grouping views, and use gmap[smap[x]].
    */
}

View_p GroupCol (View_p view, Column cols, const char *name) {
    View_p vkey, vres, gview;
    HashInfo info;
    
    vkey = ColMapView(view, cols);
    vres = ColMapView(view, OmitColumn(cols, ViewWidth(view)));

    FillGroupInfo(&info, vkey);
    gview = GroupedView(vres, info.veccol, info.hashcol, name);
    return PairView(RemapSubview(vkey, info.mapcol, 0, -1), gview);
}

static void FillJoinInfo (HashInfo_p info, View_p left, View_p right) {
    int g, r, gleft, nleft, nright, nused = 0, *hmap, *lmap, *jmap;
    Column mapcol, headmap, linkmap, joincol;
    
    nleft = ViewSize(left);
    mapcol = SeqAsCol(NewIntVec(nleft, NULL)); /* worst-case dunno #groups */
    joincol = SeqAsCol(NewIntVec(nleft, &jmap));
    
    InitHashInfo(info, left, mapcol, HashVector(nleft), HashValues(left));

    for (r = 0; r < nleft; ++r) {
        g = HashFind(left, r, info->hashes[r], info);
        if (g < 0)
            g = info->fill - 1;
        jmap[r] = g;
    }

    /* TODO: reclaim unused entries at end of map */
    mapcol.seq->count = info->fill;

    gleft = info->mapcol.seq->count;
    nleft = info->hashcol.seq->count;
    nright = ViewSize(right);
    
    headmap = SeqAsCol(NewIntVec(gleft, &hmap)); /* don't know #groups */
    linkmap = SeqAsCol(NewIntVec(nright, &lmap));

    for (r = 0; r < nright; ++r) {
        g = HashFind(right, r, RowHash(right, r), info);
        if (g >= 0) {
            lmap[r] = hmap[g] - 1;
            hmap[g] = r + 1;
            ++nused;
        }
    }

    /* we're reusing veccol, but it might not be large enough to start with */
    /* TODO: reclaim unused entries at end of hvec */
    if (info->veccol.seq->count < nused)
        info->veccol = SeqAsCol(NewIntVec(nused, &info->hvec));
    else 
        info->veccol.seq->count = nused;

    /* reorder output to match results from FillHashInfo and FillGroupInfo */
    info->hashcol = info->veccol;
    info->hashes = info->hvec;
    info->veccol = info->mapcol;
    info->hvec = info->map;
    info->mapcol = joincol;
    info->map = jmap;
    
    ChaseLinks(info, nused, hmap, lmap);
    
    /*  As with FillGroupInfo, this is most likely not quite optimal yet.
        All zero-length groups in smap (info->map, now info->hvec) could be 
        coalesced into one, and joinmap indices into it adjusted down a bit.
        Would reduce the size of smap when there are lots of failed matches.
        Also: FillJoinInfo needs quite a lot of temp vector space right now.
     */
}

Column IntersectMap (View_p keys, View_p view) {
    int r, rows;
    HashInfo info;
    struct Buffer buffer;
    
    if (!ViewCompat(keys, view)) 
        return SeqAsCol(NULL);
        
    FillHashInfo(&info, view);
    InitBuffer(&buffer);
    rows = ViewSize(keys);
    
    /* these ints are added in increasing order, could have used a bitmap */
    for (r = 0; r < rows; ++r)
        if (HashFind(keys, r, RowHash(keys, r), &info) >= 0)
            ADD_INT_TO_BUF(buffer, r);

    return SeqAsCol(BufferAsIntVec(&buffer));
}

/* ReverseIntersectMap returns RHS indices, instead of IntersectMap's LHS */
static Column ReverseIntersectMap (View_p keys, View_p view) {
    int r, rows;
    HashInfo info;
    struct Buffer buffer;
    
    FillHashInfo(&info, view);
    InitBuffer(&buffer);
    rows = ViewSize(keys);
    
    for (r = 0; r < rows; ++r) {
        int f = HashFind(keys, r, RowHash(keys, r), &info);
        if (f >= 0)
            ADD_INT_TO_BUF(buffer, f);
    }
    
    return SeqAsCol(BufferAsIntVec(&buffer));
}

View_p JoinView (View_p left, View_p right, const char *name) {
    View_p lmeta, rmeta, lkey, rkey, rres, gview;
    Column lmap, rmap;
    HashInfo info;
    
    lmeta = V_Meta(left);
    rmeta = V_Meta(right);
    
    lmap = IntersectMap(lmeta, rmeta);
    /* TODO: optimize, don't create the hash info twice */
    rmap = ReverseIntersectMap(lmeta, rmeta);

    lkey = ColMapView(left, lmap);
    rkey = ColMapView(right, rmap);
    rres = ColMapView(right, OmitColumn(rmap, ViewWidth(right)));

    FillJoinInfo(&info, lkey, rkey);
    gview = GroupedView(rres, info.veccol, info.hashcol, name);
    return PairView(left, RemapSubview(gview, info.mapcol, 0, -1));
}

Column UniqMap (View_p view) {
    HashInfo info;
    FillHashInfo(&info, view);
    return info.mapcol;
}

int HashDoFind (View_p view, int row, View_p w, Column a, Column b, Column c) {
    HashInfo info;
    /* TODO: avoid Log2bits call in InitHashInfo, since done on each find */
    InitHashInfo(&info, w, a, b, c);
    return HashFind(view, row, RowHash(view, row), &info);
}

Column GetHashInfo (View_p left, View_p right, int type) {
    HashInfo info;
    Seq_p seqvec[3];

    switch (type) {
        case 0:     FillHashInfo(&info, left); break;
        case 1:     FillGroupInfo(&info, left); break;
        default:    FillJoinInfo(&info, left, right); break;
    }
        
    seqvec[0] = info.mapcol.seq;
    seqvec[1] = info.veccol.seq;
    seqvec[2] = info.hashcol.seq;

    return SeqAsCol(NewSeqVec(IT_column, seqvec, 3));
}

View_p IjoinView (View_p left, View_p right) {
    View_p view = JoinView(left, right, "?");
    return UngroupView(view, ViewWidth(view)-1);
}

static struct SeqType ST_HashMap = { "hashmap", NULL, 02 };

Seq_p HashMapNew (void) {
    Seq_p result;
    
    result = NewSequence(0, &ST_HashMap, 0);
    /* data[0] is the key + value + hash vector */
    /* data[1] is the allocated count */
    result->data[0].p = NULL;
    result->data[1].i = 0;
    
    return result;
}

/* FIXME: dumb linear scan instead of hashing for now, for small tests only */

static int HashMapLocate(Seq_p hmap, int key, int *pos) {
    const int *data = hmap->data[0].p;
    
    for (*pos = 0; *pos < hmap->count; ++*pos)
        if (key == data[*pos])
            return 1;
        
    return 0;
}

int HashMapAdd (Seq_p hmap, int key, int value) {
    int pos, *data = hmap->data[0].p;
    int allocnt = hmap->data[1].i;

    if (HashMapLocate(hmap, key, &pos)) {
        data[pos+allocnt] = value;
        return 0;
    }

    Assert(pos == hmap->count);
    if (pos >= allocnt) {
        int newlen = (allocnt / 2) * 3 + 10;
        hmap->data[0].p = data = realloc(data, newlen * 2 * sizeof(int));
        memmove(data+newlen, data+allocnt, allocnt * sizeof(int));
        allocnt = hmap->data[1].i = newlen;
    }
    
    data[pos] = key;
    data[pos+allocnt] = value;
    ++hmap->count;
    return allocnt;
}

int HashMapLookup (Seq_p hmap, int key, int defval) {
    int pos;

    if (HashMapLocate(hmap, key, &pos)) {
        int allocnt = hmap->data[1].i;
        const int *data = hmap->data[0].p;
        return data[pos+allocnt];
    }
    
    return defval;
}

#if 0 /* not yet */
int HashMapRemove (Seq_p hmap, int key) {
    int pos;

    if (HashMapLocate(hmap, key, &pos)) {
        int last = --hmap->count;
        if (pos < last) {
            int allocnt = hmap->data[1].i, *data = hmap->data[0].p;
            data[pos] = data[last];
            data[pos+allocnt] = data[last+allocnt];
            return pos;
        }
    }
    
    return -1;
}
#endif
/* %include<indirect.c>% */
/*
 * indirect.c - Implementation of some virtual views.
 */

#include <stdlib.h>
#include <string.h>


#define RV_parent   data[0].q
#define RV_map      data[1].q
#define RV_start    data[2].i

static ItemTypes RemapGetter (int row, Item_p item) {
    const int* data;
    Seq_p seq;

    Assert(item->c.pos >= 0);
    seq = item->c.seq;
    data = seq->RV_map->data[0].p;

    item->c = ViewCol(seq->RV_parent, item->c.pos);
    row += seq->RV_start;
    if (data[row] < 0)
        row += data[row];
    return GetItem(data[row], item);
}

static struct SeqType ST_Remap = { "remap", RemapGetter, 011 };

View_p RemapSubview (View_p view, Column mapcol, int start, int count) {
    Seq_p seq;

    if (mapcol.seq == NULL)
        return NULL;
        
    if (count < 0)
        count = mapcol.seq->count;
    
    if (ViewWidth(view) == 0)
        return NoColumnView(count);
        
    seq = NewSequence(count, &ST_Remap, 0);
    /* data[0] is the parent view to which the map applies */
    /* data[1] is the map, as a sequence */
    /* data[2] is the index offset */
    seq->RV_parent = IncRefCount(view);
    seq->RV_map = IncRefCount(mapcol.seq);
    seq->RV_start = start;

    return IndirectView(V_Meta(view), seq);
}

static View_p MakeMetaSubview (const char *name, View_p view) {
    View_p meta, result;
    Seq_p names, types, subvs;
    
    names = NewStrVec(1);
    AppendToStrVec(name, -1, names);

    types = NewStrVec(1);
    AppendToStrVec("V", -1, types);
    
    meta = V_Meta(view);
    subvs = NewSeqVec(IT_view, &meta, 1);
    
    result = NewView(V_Meta(meta));
    SetViewSeqs(result, MC_name, 1, FinishStrVec(names));
    SetViewSeqs(result, MC_type, 1, FinishStrVec(types));
    SetViewSeqs(result, MC_subv, 1, subvs);
    return result;
}

#define GV_parent       data[0].q
#define GV_start        data[1].q
#define GV_group        data[2].q
#define GV_cache        data[3].q

static ItemTypes GroupedGetter (int row, Item_p item) {
    Seq_p seq = item->c.seq;
    View_p *subviews = seq->GV_cache->data[0].p;
    
    if (subviews[row] == NULL) {
        const int *sptr = seq->GV_start->data[0].p;
        Seq_p gmap = seq->GV_group;
        int start = row > 0 ? sptr[row-1] : 0;
        
        item->c.seq = gmap;
        subviews[row] = IncRefCount(RemapSubview(seq->GV_parent, item->c,
                                                    start, sptr[row] - start));
    }
    
    item->v = subviews[row];
    return IT_view;
}

static struct SeqType ST_Grouped = { "grouped", GroupedGetter, 01111 };

View_p GroupedView (View_p view, Column startcol, Column groupcol, const char *name) {
    int groups;
    Seq_p seq, subviews;
    
    groups = startcol.seq->count;
    subviews = NewSeqVec(IT_view, NULL, groups);

    seq = NewSequence(groups, &ST_Grouped, 0);
    /* data[0] is the parent view to which the grouping applies */
    /* data[1] is the start map, as a sequence */
    /* data[2] is the group map, as a sequence */
    /* data[3] is a cache of subviews, as a pointer vector in a sequence */
    seq->GV_parent = IncRefCount(view);
    seq->GV_start = IncRefCount(startcol.seq);
    seq->GV_group = IncRefCount(groupcol.seq);
    seq->GV_cache = IncRefCount(subviews);

    return IndirectView(MakeMetaSubview(name, view), seq);
}

#define PV_parent   data[0].q
#define PV_start    data[1].i
#define PV_rate     data[2].i
#define PV_step     data[3].i

static ItemTypes StepGetter (int row, Item_p item) {
    Seq_p seq = item->c.seq;
    int rows = ViewSize(seq->PV_parent);

    item->c = ViewCol(seq->PV_parent, item->c.pos);
    row = (seq->PV_start + (row / seq->PV_rate) * seq->PV_step) % rows;
    if (row < 0)
        row += rows;
    return GetItem(row, item);
}

static struct SeqType ST_Step = { "step", StepGetter, 01 };

View_p StepView (View_p view, int count, int offset, int rate, int step) {
    Seq_p seq;

    /* prevent division by zero if input view is empty */
    if (ViewSize(view) == 0)
        return view;

    seq = NewSequence(count * rate, &ST_Step, 0);
    /* data[0] is the parent view to which the changes apply */
    /* data[1] is the starting offset */
    /* data[2] is the rate to repeat an item */
    /* data[3] is the step to the next item */
    seq->PV_parent = IncRefCount(view);
    seq->PV_start = offset;
    seq->PV_rate = rate;
    seq->PV_step = step;

    return IndirectView(V_Meta(view), seq);
}

#define CV_left     data[0].q
#define CV_right    data[1].q

static ItemTypes ConcatGetter (int row, Item_p item) {
    View_p view = item->c.seq->CV_left;
    int rows = ViewSize(view);
    
    if (row >= rows) {
        row -= rows;
        view = item->c.seq->CV_right;
    }

    item->c = ViewCol(view, item->c.pos);
    return GetItem(row, item);
}

static struct SeqType ST_Concat = { "concat", ConcatGetter, 011 };

View_p ConcatView (View_p view1, View_p view2) {
    int rows1, rows2;
    Seq_p seq;

    if (view1 == NULL || view2 == NULL || !ViewCompat(view1, view2))
        return NULL;
                    
    rows1 = ViewSize(view1);
    if (rows1 == 0)
        return view2;

    rows2 = ViewSize(view2);
    if (rows2 == 0)
        return view1;

    seq = NewSequence(rows1 + rows2, &ST_Concat, 0);
    /* data[0] is the left-hand view */
    /* data[1] is the right-hand view */
    seq->CV_left = IncRefCount(view1);
    seq->CV_right = IncRefCount(view2);

    return IndirectView(V_Meta(view1), seq);
}

#define UV_parent       data[0].q
#define UV_unmap        data[1].q
#define UV_subcol       data[2].i
#define UV_swidth       data[3].i

static ItemTypes UngroupGetter (int row, Item_p item) {
    int col, subcol, parentrow;
    const int *data;
    View_p view;
    Seq_p seq;

    col = item->c.pos;
    seq = item->c.seq;

    view = seq->UV_parent;
    data = seq->UV_unmap->data[0].p;
    subcol = seq->UV_subcol;

    parentrow = data[row];
    if (parentrow < 0) {
        parentrow = data[row+parentrow];
        row = -data[row];
    } else
        row = 0;
    
    if (subcol <= col && col < subcol + seq->UV_swidth) {
        view = GetViewItem(view, parentrow, subcol, IT_view).v;
        col -= subcol;
    } else {
        if (col >= subcol)
            col -= seq->UV_swidth - 1;
        row = parentrow;
    }
    
    item->c = ViewCol(view, col);
    return GetItem(row, item);
}

static struct SeqType ST_Ungroup = { "ungroup", UngroupGetter, 011 };

View_p UngroupView (View_p view, int col) {
    int i, n, r, rows;
    struct Buffer buffer;
    View_p subview, meta, submeta, newmeta;
    Seq_p seq, map;
    Column column;
    
    InitBuffer(&buffer);

    column = ViewCol(view, col);
    rows = column.seq->count;
    
    for (r = 0; r < rows; ++r) {
        subview = GetColItem(r, column, IT_view).v;
        n = ViewSize(subview);
        if (n > 0) {
            ADD_INT_TO_BUF(buffer, r);
            for (i = 1; i < n; ++i)
                ADD_INT_TO_BUF(buffer, -i);
        }
    }

    map = BufferAsIntVec(&buffer);
    
    /* result meta view replaces subview column with its actual meta view */
    meta = V_Meta(view);
    submeta = GetViewItem(meta, col, MC_subv, IT_view).v;
    newmeta = ConcatView(FirstView(meta, col), submeta);
    newmeta = ConcatView(newmeta, LastView(meta, ViewSize(meta) - (col + 1)));
    
    seq = NewSequence(map->count, &ST_Ungroup, 0);
    /* data[0] is the parent view */
    /* data[1] is ungroup map as a sequence */
    /* data[2] is the subview column */
    /* data[3] is the subview width */
    seq->UV_parent = IncRefCount(view);
    seq->UV_unmap = IncRefCount(map);
    seq->UV_subcol = col;
    seq->UV_swidth = ViewSize(submeta);
    
    return IndirectView(newmeta, seq);
}

#define BV_parent       data[0].q
#define BV_cumcnt       data[1].q

static ItemTypes BlockedGetter (int row, Item_p item) {
    int block;
    const int* data;
    View_p subv;
    Seq_p seq;

    seq = item->c.seq;
    data = seq->BV_cumcnt->data[0].p;

    for (block = 0; block + data[block] < row; ++block)
        ;

    if (row == block + data[block]) {
        row = block;
        block = ViewSize(seq->BV_parent) - 1;
    } else if (block > 0)
        row -= block + data[block-1];

    subv = GetViewItem(seq->BV_parent, block, 0, IT_view).v;
    item->c = ViewCol(subv, item->c.pos);    
    return GetItem(row, item);
}

static struct SeqType ST_Blocked = { "blocked", BlockedGetter, 011 };

View_p BlockedView (View_p view) {
    int r, rows, *limits, tally = 0;
    View_p submeta;
    Seq_p seq, offsets;
    Column blocks;

    /* view must have exactly one subview column */
    if (ViewWidth(view) != 1)
        return NULL;
    
    blocks = ViewCol(view, 0);
    rows = ViewSize(view);
    
    offsets = NewIntVec(rows, &limits);
    for (r = 0; r < rows; ++r) {
        tally += ViewSize(GetColItem(r, blocks, IT_view).v);
        limits[r] = tally;
    }
    
    seq = NewSequence(tally, &ST_Blocked, 0);
    /* data[0] is the parent view */
    /* data[1] is a cumulative row count, as a sequence */
    seq->BV_parent = IncRefCount(view);
    seq->BV_cumcnt = IncRefCount(offsets);
    
    submeta = GetViewItem(V_Meta(view), 0, MC_subv, IT_view).v;
    return IndirectView(submeta, seq);
}
/* %include<mutable.c>% */
/*
 * mutable.c - Implementation of mutable views.
 */

#include <stdlib.h>
#include <string.h>


#define MUT_DEBUG 0

#define S_aux(seq,typ) ((typ) ((Seq_p) (seq) + 1))

#define SV_data(seq)    S_aux(seq, Seq_p*)
#define SV_bits         data[0].q
#define SV_hash         data[1].q
#define SV_view         data[2].q

static void SettableCleaner (Seq_p seq) {
    int c, r;
    View_p view = seq->SV_view;
    
    for (c = 0; c < ViewWidth(view); ++c) {
        Seq_p dseq = SV_data(seq)[c];

        if (dseq != NULL)
            switch (ViewColType(view, c)) {

                case IT_string: {
                    char **p = S_aux(dseq, char**);
                    for (r = 0; r < dseq->count; ++r)
                        if (p[r] != NULL)
                            free(p[r]);
                    break;
                }

                case IT_bytes: {
                    const Item *p = S_aux(dseq, const Item*);
                    for (r = 0; r < dseq->count; ++r)
                        if (p[r].u.ptr != NULL)
                            free((char*) p[r].u.ptr);
                    break;
                }

                case IT_view: {
                    const View_p *p = S_aux(dseq, const View_p*);
                    for (r = 0; r < dseq->count; ++r)
                        if (p[r] != NULL)
                            DecRefCount(p[r]);
                    break;
                }
            
                default: break;
            }

        DecRefCount(dseq);
        DecRefCount(SV_data(seq)[ViewWidth(view)+c]);
    }
}

static ItemTypes SettableGetter (int row, Item_p item) {
    int col = item->c.pos;
    Seq_p seq = item->c.seq;
    View_p view = seq->SV_view;
    
    item->c = ViewCol(view, col);
    
    if (TestBit(seq->SV_bits, row)) {
        int index = HashMapLookup(seq->SV_hash, row, -1);
        Assert(index >= 0);
    
        if (TestBit(SV_data(seq)[ViewWidth(view)+col], index)) {
            row = index;
            item->c.seq = SV_data(seq)[col];
        }
    }
    
    return GetItem(row, item);
}
 
static int SettableWidth (ItemTypes type) {
    switch (type) {
        case IT_int:  case IT_float:    return 4;
        case IT_wide: case IT_double:   return 8;
        case IT_view: case IT_string:   return sizeof(void*);
        case IT_bytes:                  return sizeof(Item);
        default:                        Assert(0); return 0;
    }
}

static ItemTypes StringSetGetter (int row, Item_p item) {
    item->s = S_aux(item->c.seq, const char**)[row];
    return IT_string;
}

static ItemTypes BytesSetGetter (int row, Item_p item) {
    item->u = S_aux(item->c.seq, const Item*)[row].u;
    return IT_bytes;
}

static ItemTypes ViewSetGetter (int row, Item_p item) {
    item->v = S_aux(item->c.seq, const View_p*)[row];
    return IT_view;
}

static struct SeqType ST_StringSet = { "stringset", StringSetGetter };
static struct SeqType ST_BytesSet = { "bytesset", BytesSetGetter };
static struct SeqType ST_ViewSet = { "viewset", ViewSetGetter };

static SeqType_p PickSetGetter (ItemTypes type) {
    int w = 8;
    switch (type) {
        case IT_int:    w = 4; /* fall through */
        case IT_wide:   return FixedGetter(w, 1, 0, 0);
        case IT_float:  w = 4; /* fall through */
        case IT_double: return FixedGetter(w, 1, 1, 0);
        case IT_string: return &ST_StringSet;
        case IT_bytes:  return &ST_BytesSet;
        case IT_view:   return &ST_ViewSet;
        default:        Assert(0); return NULL;
    }
}

static struct SeqType ST_Settable = {
    "settable", SettableGetter, 0111, SettableCleaner
};

static int IsSettable(View_p view) {
    Seq_p seq = ViewCol(view, 0).seq;
    return seq != NULL && seq->type == &ST_Settable;
}

static void SettableSetter (View_p view, int row, int col, Item_p item) {
    int index, count, limit;
    ItemTypes type;
    Seq_p seq, dseq, hash;
    
    Assert(IsSettable(view));
    
    seq = ViewCol(view, col).seq;
    type = ViewColType(view, col);
    dseq = SV_data(seq)[col];
    hash = seq->SV_hash;

    if (SetBit(&seq->SV_bits, row)) {
        index = hash->count;
        HashMapAdd(hash, row, index);
    } else {
        index = HashMapLookup(hash, row, -1);
        Assert(index >= 0);
    }
    
    limit = SetBit(&SV_data(seq)[ViewWidth(view)+col], index);
    count = dseq != NULL ? dseq->count : 0;
    
    /* clumsy: resize a settable column so it can store more items if needed */
    if (limit > count) {
        int w = SettableWidth(type);
        Seq_p nseq;
        nseq = IncRefCount(NewSequence(limit, PickSetGetter(type), limit * w));
        memcpy(nseq + 1, LoseRef(dseq) + 1, count * w);
        SV_data(seq)[col] = dseq = nseq;
    }
    
    switch (type) {
        
        case IT_int:    S_aux(dseq, int*    )[index] = item->i; break;
        case IT_wide:   S_aux(dseq, int64_t*)[index] = item->w; break;
        case IT_float:  S_aux(dseq, float*  )[index] = item->f; break;                                  
        case IT_double: S_aux(dseq, double* )[index] = item->d; break;

        case IT_string: {
            char **p = S_aux(dseq, char**) + index;
            free(*p);
            *p = strdup(item->s);
            break;
        }
        
        case IT_bytes: {
            Item *p = S_aux(dseq, Item*) + index;
            free((char*) p->u.ptr);
            p->u.len = item->u.len;
            p->u.ptr = memcpy(malloc(p->u.len), item->u.ptr, p->u.len);
            break;
        }
        
        case IT_view: {
            View_p *p = S_aux(dseq, View_p*) + index;
            LoseRef(*p);
            *p = IncRefCount(item->v);
            break;
        }
        
        default: Assert(0); break;
    }
}

static View_p SettableView (View_p view) {
    Int_t bytes = 2 * ViewWidth(view) * sizeof(Seq_p*);
    Seq_p seq;
    
    /* room for a set of new data columns, then a set of "used" bitmaps */
    seq = NewSequence(ViewSize(view), &ST_Settable, bytes);
    /* data[0] is the adjusted bitmap */
    /* data[1] points to hash row map */
    /* data[2] points to the original view */
    seq->SV_bits = NULL;
    seq->SV_hash = IncRefCount(HashMapNew());
    seq->SV_view = IncRefCount(view);

    return IndirectView(V_Meta(view), seq);
}    

#define MV_parent       data[0].q
#define MV_mutmap       data[1].q

    typedef struct MutRange {
        int start;
        int shift;
        View_p added;
    } *MutRange_p;

static void MutableCleaner (Seq_p seq) {
    int i;
    Seq_p map;
    MutRange_p range;
    
    map = seq->MV_mutmap;
    range = S_aux(map, MutRange_p);

    for (i = 0; i < map->count; ++i)
        DecRefCount(range[i].added);
}

#if MUT_DEBUG
#include <stdio.h>
static void DumpRanges (const char *msg, View_p view) {
    int i;
    Seq_p seq, map;
    MutRange_p range;

    seq = ViewCol(view, 0).seq;
    map = seq->MV_mutmap;
    range = S_aux(map, MutRange_p);

    printf("%s: view %p rc %d, has %d rows %d ranges\n",
                 msg, (void*) view, view->refs, ViewSize(view), map->count);
    for (i = 0; i < map->count; ++i) {
        View_p v = range[i].added;
        printf("    %2d: st %3d sh %3d", i, range[i].start, range[i].shift);
        if (v != NULL)
            printf(" - %p: sz %3d rc %3d\n", (void*) v, ViewSize(v), v->refs);
        printf("\n");
    }
}
#else
#define DumpRanges(a,b)
#endif

static int MutSlot (MutRange_p range, int pos) {
    int i = -1;
    /* TODO: use binary i.s.o. linear search */
    while (pos > range[++i].start)
        ;
    return pos < range[i].start ? i-1 : i;
}

static int ChooseMutView (Seq_p seq, int *prow) {
    int slot, row = *prow;
    MutRange_p range;
    
    range = S_aux(seq->MV_mutmap, MutRange_p);
    
    slot = MutSlot(range, row);
    row -= range[slot].start;
    Assert(row >= 0);

    *prow = range[slot].added != NULL ? slot : -1;
    return row + range[slot].shift;
}

static ItemTypes MutableGetter (int row, Item_p item) {
    Seq_p seq = item->c.seq;
    View_p view = seq->MV_parent;
    int index = ChooseMutView(seq, &row);

    if (row >= 0) {
        MutRange_p range = S_aux(seq->MV_mutmap, MutRange_p);
        view = range[row].added;
    }

    item->c = ViewCol(view, item->c.pos);
    return GetItem(index, item);
}

static MutRange_p MutResize (Seq_p *seqp, int pos, int diff) {
    int i, limit = 0, count = 0;
    MutRange_p range = NULL;
    Seq_p map;
    
    map = *seqp;
    if (map != NULL) {
        count = map->count;
        limit = map->data[1].i / sizeof(struct MutRange);
        range = S_aux(map, MutRange_p);
    }
    
    /* TODO: this only grows the ranges, should also shrink when possible */
    if (diff > 0 && count + diff > limit) {
        int newlimit;
        MutRange_p newrange;
        
        newlimit = (count / 2) * 3 + 5;
        Assert(pos + diff <= newlimit);
            
        *seqp = NewSequence(count, PickIntGetter(0),
                                newlimit * sizeof(struct MutRange));

        newrange = S_aux(*seqp, MutRange_p);
        memcpy(newrange, range, count * sizeof(struct MutRange));
        range = newrange;

        LoseRef(map);
        map = IncRefCount(*seqp);
    }
    
    map->count += diff;
    
    if (diff > 0) {
        for (i = count; --i >= pos; )
            range[i+diff] = range[i];
    } else if (diff < 0) {
        for (i = pos; i < pos - diff; ++i)
            LoseRef(range[i].added);
        for (i = pos - diff; i < count; ++i)
            range[i+diff] = range[i];
    }
    
    return range;
}

static struct SeqType ST_Mutable = {
    "mutable", MutableGetter, 011, MutableCleaner 
};

View_p MutableView (View_p view) {
    int rows;
    Seq_p seq;

    rows = ViewSize(view);
    seq = NewSequence(rows, &ST_Mutable, 0);            
    /* data[0] is the parent view (may be changed to a settable one later) */
    /* data[1] holds the mutable range map, as a sequence */
    seq->MV_parent = IncRefCount(view);
    seq->MV_mutmap = NULL;
    
    if (rows == 0)
        MutResize(&seq->MV_mutmap, 0, 1);
    else {
        MutRange_p range = MutResize(&seq->MV_mutmap, 0, 2);
        range[1].start = rows;
    }
    
    return IndirectView(V_Meta(view), seq);
}

int IsMutable (View_p view) {
    Seq_p seq = ViewCol(view, 0).seq;
    return seq != NULL && seq->type == &ST_Mutable;
}

View_p ViewSet (View_p view, int row, int col, Item_p item) {
    int index;
    View_p *modview;
    Seq_p mutseq;

    if (!IsMutable(view))
        view = MutableView(view);
    
    mutseq = ViewCol(view, 0).seq;
    index = ChooseMutView(mutseq, &row);

    if (row >= 0) {
        MutRange_p range = S_aux(mutseq->MV_mutmap, MutRange_p);
        modview = &range[row].added;
    } else
        modview = (View_p*) &mutseq->data[0].p;
    
    if (!IsSettable(*modview))
        *modview = IncRefCount(SettableView(LoseRef(*modview)));
        
    SettableSetter(*modview, index, col, item);

    return view;
}

View_p ViewReplace (View_p view, int offset, int count, View_p data) {
    int drows, fend, fpos, fslot, tend, tpos, tslot, grow, diff;
    Seq_p seq, map;
    MutRange_p range;
    View_p tailv;
    
    drows = data != NULL ? ViewSize(data) : 0;
    fend = offset + count;
    tend = offset + drows;

    if (drows > 0 && !ViewCompat(view, data))
        return NULL;

    if (drows == 0 && count == 0)
        return view;
        
    if (!IsMutable(view))
        view = MutableView(view);

    DumpRanges("before", view);

    seq = ViewCol(view, 0).seq;
    map = seq->MV_mutmap;
    range = S_aux(map, MutRange_p);
    
    fslot = MutSlot(range, offset);
    fpos = offset - range[fslot].start;
    
    tslot = MutSlot(range, fend);
    tpos = fend - range[tslot].start;
    
    tailv = range[tslot].added;

#if MUT_DEBUG
printf("fslot %d fpos %d tslot %d tpos %d drows %d offset %d count %d fend %d tend %d\n", fslot,fpos,tslot,tpos,drows,offset,count,fend,tend);
#endif

    /* keep head of first range, if rows remain before offset */
    if (fpos > 0)
        ++fslot;
    
    grow = fslot - tslot;
    if (drows > 0)
        ++grow;

    range = MutResize(&seq->MV_mutmap, fslot, grow);
    tslot += grow;
    
    if (fpos > 0 && grow > 0)
        range[tslot].shift = range[fslot-1].shift;
    
    if (drows > 0) {
        range[fslot].start = offset;
        range[fslot].shift = 0;
        range[fslot].added = IncRefCount(data);
    }
        
    /* keep tail of last range, if rows remain after offset */
    if (tpos > 0) {
        range[tslot].start = tend;
        range[tslot].shift += tpos;
        LoseRef(range[tslot].added);
        range[tslot].added = IncRefCount(tailv);
        ++tslot;
    }
    
    diff = drows - count;
    seq->count += diff;
    
    map = seq->MV_mutmap;
    while (tslot < map->count)
        range[tslot++].start += diff;

    DumpRanges("after", view);
    return view;
}

ItemTypes SetCmd_MIX (Item args[]) {
    int i, col, row, objc;
    const Object_p *objv;
    View_p view;
    Item item;
    
    view = ObjAsView(args[0].o);
    row = args[1].i;
    if (row < 0)
        row += ViewSize(view);
    
    objv = (const Object_p *) args[2].u.ptr;
    objc = args[2].u.len;

    if (objc % 2 != 0) {
        args->e = EC_wnoa;
        return IT_error;
    }

    for (i = 0; i < objc; i += 2) {
        col = ColumnByName(V_Meta(view), objv[i]);
        if (col < 0)
            return IT_unknown;

        item.o = objv[i+1];
        if (!ObjToItem(ViewColType(view, col), &item))
            return IT_unknown;

        view = ViewSet(view, row, col, &item);
    }

    args->v = view;
    return IT_view;
}

Seq_p MutPrepare (View_p view) {
    int i, j, k, cols, ranges, parows, delcnt, delpos, count, adjpos;
    Seq_p result, *seqs, mutseq, mutmap;
    MutRange_p range, rp;
    View_p parent, insview;
    Column column;
    
    Assert(IsMutable(view));
    
    mutseq = ViewCol(view, 0).seq;
    parent = mutseq->MV_parent;
    parows = ViewSize(parent);
    mutmap = mutseq->MV_mutmap;
    range = S_aux(mutmap, MutRange_p);
    ranges = mutmap->count - 1;
    
    cols = ViewWidth(view);
    result = NewSeqVec(IT_unknown, NULL, MP_limit);
    
    seqs = S_aux(result, Seq_p*);
        
    seqs[MP_delmap] = NewBitVec(parows);
    seqs[MP_insmap] = NewBitVec(ViewSize(view));

    delcnt = delpos = 0;
    insview = CloneView(view);
    
    for (i = 0; i < ranges; ++i) {
        rp = range + i;
        count = (rp+1)->start - rp->start;
        if (rp->added == NULL) {
            delcnt += rp->shift - delpos;
            SetBitRange(seqs[MP_delmap], delpos, rp->shift - delpos);
            delpos = rp->shift + count;
        } else {
            View_p newdata = StepView(rp->added, count, rp->shift, 1, 1);
            insview = ViewReplace(insview, ViewSize(insview), 0, newdata);
            SetBitRange(seqs[MP_insmap], rp->start, count);
        }
    }
    
    delcnt += parows - delpos;
    SetBitRange(seqs[MP_delmap], delpos, parows - delpos);
    seqs[MP_insdat] = insview;

    seqs[MP_adjmap] = NewBitVec(parows - delcnt);

    if (IsSettable(parent)) {
        Seq_p setseq, adjseq;
        
        setseq = ViewCol(parent, 0).seq;
        adjseq = setseq->SV_bits;
                
        if (adjseq != NULL && adjseq->count > 0) {
            int *revptr, *rowptr;
            Seq_p revmap, rowmap, hash = setseq->SV_hash;
            View_p bitmeta, bitview;
        
            revmap = NewIntVec(hash->count, &revptr);
            rowmap = NewIntVec(hash->count, &rowptr);

            k = adjpos = 0;
            
            for (i = 0; i < ranges; ++i) {
                rp = range + i;
                if (rp->added == NULL) {
                    count = (rp+1)->start - rp->start;
                    for (j = 0; j < count; ++j)
                        if (TestBit(adjseq, rp->start + j)) {
                            int index = HashMapLookup(hash, rp->start + j, -1);
                            Assert(index >= 0);
                            revptr[k] = index;
                            rowptr[k++] = rp->start + j;
                            SetBit(seqs+MP_adjmap, adjpos + j);
                        }
                    adjpos += count;
                }
            }
            
            Assert(adjpos == parows - delcnt);
            
            seqs[MP_adjdat] = RemapSubview(parent, SeqAsCol(rowmap), 0, -1);

            bitmeta = StepView(MakeIntColMeta("_"), 1, 0, ViewWidth(parent), 1);
            bitview = NewView(bitmeta); /* will become an N-intcol view */
            
            for (i = 0; i < cols; ++i) {
                column.seq = SV_data(setseq)[cols+i];
                column.pos = -1;
                MinBitCount(&column.seq, hash->count);
                SetViewCols(bitview, i, 1, column);
            }

            seqs[MP_revmap] = revmap;
            seqs[MP_usemap] = RemapSubview(bitview, SeqAsCol(revmap), 0, -1);
        }
    } else {
        seqs[MP_adjdat] = seqs[MP_usemap] = NoColumnView(0);
        seqs[MP_revmap] = ViewCol(seqs[MP_usemap], 0).seq;
    }
    
    for (i = 0; i < MP_limit; ++i)
        IncRefCount(seqs[i]);       
         
    return result;
}
/* %include<sorted.c>% */
/*
 * sorted.c - Implementation of sorting functions.
 */

#include <stdlib.h>
#include <string.h>


static int ItemsEqual (ItemTypes type, Item a, Item b) {
    switch (type) {

        case IT_int:
            return a.i == b.i;

        case IT_wide:
            return a.w == b.w;

        case IT_float:
            return a.f == b.f;

        case IT_double:
            return a.d == b.d;

        case IT_string:
            return strcmp(a.s, b.s) == 0;

        case IT_bytes:
            return a.u.len == b.u.len && memcmp(a.u.ptr, b.u.ptr, a.u.len) == 0;

        case IT_view:
            return ViewCompare(a.v, b.v) == 0;
            
        default: Assert(0); return -1;
    }
}

int RowEqual (View_p v1, int r1, View_p v2, int r2) {
    int c;
    ItemTypes type;
    Item item1, item2;

    for (c = 0; c < ViewWidth(v1); ++c) {
        type = ViewColType(v1, c);
        item1 = GetViewItem(v1, r1, c, type);
        item2 = GetViewItem(v2, r2, c, type);

        if (!ItemsEqual(type, item1, item2))
            return 0;
    }

    return 1;
}

/* TODO: UTF-8 comparisons, also case-sensitive & insensitive */

static int ItemsCompare (ItemTypes type, Item a, Item b, int lower) {
    switch (type) {

        case IT_int:
            return (a.i > b.i) - (a.i < b.i);

        case IT_wide:
            return (a.w > b.w) - (a.w < b.w);

        case IT_float:
            return (a.f > b.f) - (a.f < b.f);

        case IT_double:
            return (a.d > b.d) - (a.d < b.d);

        case IT_string:
            return (lower ? strcasecmp : strcmp)(a.s, b.s);

        case IT_bytes: {
            int f;

            if (a.u.len == b.u.len)
                return memcmp(a.u.ptr, b.u.ptr, a.u.len);

            f = memcmp(a.u.ptr, b.u.ptr, a.u.len < b.u.len ? a.u.len : b.u.len);
            return f != 0 ? f : a.u.len - b.u.len;
        }
        
        case IT_view:
            return ViewCompare(a.v, b.v);
            
        default: Assert(0); return -1;
    }
}

int RowCompare (View_p v1, int r1, View_p v2, int r2) {
    int c, f;
    ItemTypes type;
    Item item1, item2;

    for (c = 0; c < ViewWidth(v1); ++c) {
        type = ViewColType(v1, c);
        item1 = GetViewItem(v1, r1, c, type);
        item2 = GetViewItem(v2, r2, c, type);

        f = ItemsCompare(type, item1, item2, 0);
        if (f != 0)
            return f < 0 ? -1 : 1;
    }

    return 0;
}

static int RowIsLess (View_p v, int a, int b) {
    int c, f;
    ItemTypes type;
    Item va, vb;

    if (a != b)
        for (c = 0; c < ViewWidth(v); ++c) {
            type = ViewColType(v, c);
            va = GetViewItem(v, a, c, type);
            vb = GetViewItem(v, b, c, type);

            f = ItemsCompare(type, va, vb, 0);
            if (f != 0)
                return f < 0;
        }

    return a < b;
}

static int TestAndSwap (View_p v, int *a, int *b) {
    if (RowIsLess(v, *b, *a)) {
        int t = *a;
        *a = *b;
        *b = t;
        return 1;
    }
    return 0;
}

static void MergeSort (View_p v, int *ar, int nr, int *scr) {
    switch (nr) {
        case 2:
            TestAndSwap(v, ar, ar+1);
            break;
        case 3:
            TestAndSwap(v, ar, ar+1);
            if (TestAndSwap(v, ar+1, ar+2))
                TestAndSwap(v, ar, ar+1);
            break;
        case 4: /* TODO: optimize with if's */
            TestAndSwap(v, ar, ar+1);
            TestAndSwap(v, ar+2, ar+3);
            TestAndSwap(v, ar, ar+2);
            TestAndSwap(v, ar+1, ar+3);
            TestAndSwap(v, ar+1, ar+2);
            break;
            /* TODO: also special-case 5-item sort? */
        default: {
            int s1 = nr / 2, s2 = nr - s1;
            int *f1 = scr, *f2 = scr + s1, *t1 = f1 + s1, *t2 = f2 + s2;
            MergeSort(v, f1, s1, ar);
            MergeSort(v, f2, s2, ar+s1);
            for (;;)
                if (RowIsLess(v, *f1, *f2)) {
                    *ar++ = *f1++;
                    if (f1 >= t1) {
                        while (f2 < t2)
                            *ar++ = *f2++;
                        break;
                    }
                } else {
                    *ar++ = *f2++;
                    if (f2 >= t2) {
                        while (f1 < t1)
                            *ar++ = *f1++;
                        break;
                    }
                }
        }
    }
}

Column SortMap (View_p view) {
    int r, rows, *imap, *itmp;
    Seq_p seq;

    rows = ViewSize(view);

    if (rows <= 1 || ViewWidth(view) == 0)
        return NewIotaColumn(rows);

    seq = NewIntVec(rows, &imap);
    NewIntVec(rows, &itmp);
    
    for (r = 0; r < rows; ++r)
        imap[r] = itmp[r] = r;

    MergeSort(view, imap, rows, itmp);

    return SeqAsCol(seq);
}

static ItemTypes AggregateMax (ItemTypes type, Column column, Item_p item) {
    int r, rows;
    Item temp;
    
    rows = column.seq->count;
    if (rows == 0) {
        item->e = EC_nalor;
        return IT_error;
    }
    
    *item = GetColItem(0, column, type);
    for (r = 1; r < rows; ++r) {
        temp = GetColItem(r, column, type);
        if (ItemsCompare (type, temp, *item, 0) > 0)
            *item = temp;
    }
    
    return type;
}

ItemTypes MaxCmd_VN (Item args[]) {
    Column column = ViewCol(args[0].v, args[1].i);
    return AggregateMax(ViewColType(args[0].v, args[1].i), column, args);
}

static ItemTypes AggregateMin (ItemTypes type, Column column, Item_p item) {
    int r, rows;
    Item temp;
    
    rows = column.seq->count;
    if (rows == 0) {
        item->e = EC_nalor;
        return IT_error;
    }
    
    *item = GetColItem(0, column, type);
    for (r = 1; r < rows; ++r) {
        temp = GetColItem(r, column, type);
        if (ItemsCompare (type, temp, *item, 0) < 0)
            *item = temp;
    }
    
    return type;
}

ItemTypes MinCmd_VN (Item args[]) {
    Column column = ViewCol(args[0].v, args[1].i);
    return AggregateMin(ViewColType(args[0].v, args[1].i), column, args);
}

static ItemTypes AggregateSum (ItemTypes type, Column column, Item_p item) {
    int r, rows = column.seq->count;
    
    switch (type) {
        
        case IT_int:
            item->w = 0; 
            for (r = 0; r < rows; ++r)
                item->w += GetColItem(r, column, IT_int).i;
            return IT_wide;
            
        case IT_wide:
            item->w = 0;
            for (r = 0; r < rows; ++r)
                item->w += GetColItem(r, column, IT_wide).w;
            return IT_wide;
            
        case IT_float:
            item->d = 0; 
            for (r = 0; r < rows; ++r)
                item->d += GetColItem(r, column, IT_float).f;
            return IT_double;

        case IT_double:     
            item->d = 0; 
            for (r = 0; r < rows; ++r)
                item->d += GetColItem(r, column, IT_double).d;
            return IT_double;
            
        default:
            return IT_unknown;
    }
}

ItemTypes SumCmd_VN (Item args[]) {
    Column column = ViewCol(args[0].v, args[1].i);
    return AggregateSum(ViewColType(args[0].v, args[1].i), column, &args[0]);
}
/* %include<view.c>% */
/*
 * view.c - Implementation of views.
 */

#include <stdlib.h>
#include <string.h>


int ViewSize (View_p view) {
    Seq_p seq = ViewCol(view, 0).seq;
    return seq != NULL ? seq->count : 0;
}

char GetColType (View_p meta, int col) {
    /* TODO: could be a #define */
    return *GetViewItem(meta, col, MC_type, IT_string).s;
}

static void ViewCleaner (View_p view) {
    int c;
    for (c = 0; c <= ViewWidth(view); ++c)
        DecRefCount(ViewCol(view, c).seq);
}

static ItemTypes ViewGetter (int row, Item_p item) {
    item->c = ViewCol(item->c.seq, row);
    return IT_column;
}

static struct SeqType ST_View = { "view", ViewGetter, 010, ViewCleaner };

static View_p ForceMetaView(View_p meta) {
    View_p newmeta;
    Column orignames, names;
    
    /* this code is needed so we can always do a hash lookup on name column */
    
    orignames = ViewCol(meta, MC_name);
    names = ForceStringColumn(orignames);
    
    if (names.seq == orignames.seq)
        return meta;
        
    /* need to construct a new meta view with the adjusted names column */
    newmeta = NewView(V_Meta(meta));

    SetViewCols(newmeta, MC_name, 1, names);
    SetViewCols(newmeta, MC_type, 1, ViewCol(meta, MC_type));
    SetViewCols(newmeta, MC_subv, 1, ViewCol(meta, MC_subv));

    return newmeta;
}

View_p NewView (View_p meta) {
    int c, cols, bytes;
    View_p view;

    /* a view must always be able to store at least one column */
    cols = ViewSize(meta);
    bytes = (cols + 1) * (sizeof(Column) + 1);

    view = NewSequence(cols, &ST_View, bytes);
    /* data[0] points to types string (allocated inline after the columns) */
    /* data[1] is the meta view */
    V_Types(view) = (char*) (V_Cols(view) + cols + 1);
    V_Meta(view) = IncRefCount(ForceMetaView(meta));

    /* TODO: could optimize by storing the types with the meta view instead */
    for (c = 0; c < cols; ++c)
        V_Types(view)[c] = CharAsItemType(GetColType(meta, c));

    return view;
}

void SetViewCols (View_p view, int first, int count, Column src) {
    int c;

    AdjustSeqRefs(src.seq, count);
    for (c = first; c < first + count; ++c) {
        DecRefCount(ViewCol(view, c).seq);
        ViewCol(view, c) = src;
    }
}

void SetViewSeqs (View_p view, int first, int count, Seq_p src) {
    SetViewCols(view, first, count, SeqAsCol(src));
}

View_p IndirectView (View_p meta, Seq_p seq) {
    int c, cols;
    View_p result;

    cols = ViewSize(meta);
    if (cols == 0)
        ++cols;
        
    result = NewView(meta);
    SetViewSeqs(result, 0, cols, seq);

    for (c = 0; c < cols; ++c)
        ViewCol(result, c).pos = c;

    return result;
}

View_p EmptyMetaView (void) {
    Shared_p sh = GetShared();
    
    if (sh->empty == NULL) {
        int bytes;
        View_p meta, subs [MC_limit];
        Seq_p tempseq;

        /* meta is recursively defined, so we can't use NewView here */
        bytes = (MC_limit + 1) * (sizeof(Column) + 1);
        meta = NewSequence(MC_limit, &ST_View, bytes);
        V_Meta(meta) = IncRefCount(meta); /* circular */
    
        V_Types(meta) = (char*) (V_Cols(meta) + MC_limit + 1);
        V_Types(meta)[0] = V_Types(meta)[1] = IT_string;
        V_Types(meta)[2] = IT_view;

        /* initialize all but last columns of meta with static ptr contents */
        tempseq = NewStrVec(1);
        AppendToStrVec("name", -1, tempseq);
        AppendToStrVec("type", -1, tempseq);
        AppendToStrVec("subv", -1, tempseq);
        SetViewSeqs(meta, MC_name, 1, FinishStrVec(tempseq));

        tempseq = NewStrVec(1);
        AppendToStrVec("S", -1, tempseq);
        AppendToStrVec("S", -1, tempseq);
        AppendToStrVec("V", -1, tempseq);
        SetViewSeqs(meta, MC_type, 1, FinishStrVec(tempseq));

        /* same structure as meta but no rows */
        sh->empty = NewView(meta);

        /* initialize last column of meta, now that empty exists */
        subs[MC_name] = subs[MC_type] = subs[MC_subv] = sh->empty;
        SetViewSeqs(meta, MC_subv, 1, NewSeqVec(IT_view, subs, MC_limit));
    }
    
    return sh->empty;
}

View_p NoColumnView (int rows) {
    View_p result = NewView(EmptyMetaView());
    SetViewSeqs(result, 0, 1, NewSequence(rows, PickIntGetter(0), 0));
    return result;
}

View_p ColMapView (View_p view, Column mapcol) {
    int c, cols;
    const int *map;
    View_p result;

    map = (const int*) mapcol.seq->data[0].p;
    cols = mapcol.seq->count;

    if (cols == 0)
        return NoColumnView(ViewSize(view));

    result = NewView(RemapSubview(V_Meta(view), mapcol, 0, cols));

    for (c = 0; c < cols; ++c)
        SetViewCols(result, c, 1, ViewCol(view, map[c]));

    return result;
}

View_p ColOmitView (View_p view, Column omitcol) {
    int c, d, *map;
    Seq_p mapcol;
    
    mapcol = NewIntVec(ViewWidth(view), &map);

    /* TODO: make this robust, may have to add bounds check */
    for (c = 0; c < omitcol.seq->count; ++c)
        map[GetColItem(c, omitcol, IT_int).i] = 1;
    
    for (c = d = 0; c < mapcol->count; ++c)
        if (!map[c])
            map[d++] = c;
            
    mapcol->count = d; /* TODO: truncate unused area */
    return ColMapView(view, SeqAsCol(mapcol));
}

View_p OneColView (View_p view, int col) {
    View_p result;

    result = NewView(StepView(V_Meta(view), 1, col, 1, 1));
    SetViewCols(result, 0, 1, ViewCol(view, col));

    return result;
}

View_p FirstView (View_p view, int count) {
    if (count >= ViewSize(view))
        return view;
    return StepView(view, count, 0, 1, 1);
}

View_p LastView (View_p view, int count) {
    int rows = ViewSize(view);
    if (count >= rows)
        return view;
    return StepView(view, count, rows - count, 1, 1);
}

View_p TakeView (View_p view, int count) {
    if (count >= 0)
        return StepView(view, count, 0, 1, 1);
    else
        return StepView(view, -count, -1, 1, -1);
}

View_p CloneView (View_p view) {
    return NewView(V_Meta(view));
}

View_p PairView (View_p view1, View_p view2) {
    int c, rows1 = ViewSize(view1), rows2 = ViewSize(view2);
    View_p meta, result;

    if (rows2 < rows1)
        view1 = FirstView(view1, rows2);
    else if (rows1 < rows2)
        view2 = FirstView(view2, rows1);
    else if (ViewWidth(view2) == 0)
        return view1;
    else if (ViewWidth(view1) == 0)
        return view2;

    meta = ConcatView(V_Meta(view1), V_Meta(view2));
    result = NewView(meta);
    
    for (c = 0; c < ViewWidth(view1); ++c)
        SetViewCols(result, c, 1, ViewCol(view1, c));
    for (c = 0; c < ViewWidth(view2); ++c)
        SetViewCols(result, ViewWidth(view1) + c, 1, ViewCol(view2, c));
        
    return result;
}

static void ViewStructure (View_p meta, Buffer_p buffer) {
    int c, cols = ViewSize(meta);
    
    for (c = 0; c < cols; ++c) {
        char type = GetColType(meta, c);
        
        if (type == 'V') {
            View_p submeta = GetViewItem(meta, c, MC_subv, IT_view).v;
            
            if (ViewSize(submeta) > 0) {
                AddToBuffer(buffer, "(", 1);
                ViewStructure(submeta, buffer);
                AddToBuffer(buffer, ")", 1);
                continue;
            }
        }
        AddToBuffer(buffer, &type, 1);
    }
}

int MetaIsCompatible (View_p meta1, View_p meta2) {
    if (meta1 != meta2) {
        int c, cols = ViewSize(meta1);

        if (cols != ViewSize(meta2))
            return 0;
        
        for (c = 0; c < cols; ++c) {
            char type = GetColType(meta1, c);
        
            if (type != GetColType(meta2, c))
                return 0;
            
            if (type == 'V') {
                View_p v1 = GetViewItem(meta1, c, MC_subv, IT_view).v;
                View_p v2 = GetViewItem(meta2, c, MC_subv, IT_view).v;

                if (!MetaIsCompatible(v1, v2))
                    return 0;
            }
        }
    }
    
    return 1;
}

int ViewCompare (View_p view1, View_p view2) {
    int f, r, rows1, rows2;
    
    if (view1 == view2)
        return 0;
    
    f = ViewCompare(V_Meta(view1), V_Meta(view2));
    if (f != 0)
        return f;
        
    rows1 = ViewSize(view1);
    rows2 = ViewSize(view2);
    
    for (r = 0; r < rows1; ++r) {
        if (r >= rows2)
            return 1;
    
        f = RowCompare(view1, r, view2, r);
        if (f != 0)
            return f < 0 ? -1 : 1;
    }
        
    return rows1 < rows2 ? -1 : 0;
}

View_p MakeIntColMeta (const char *name) {
    View_p result, subv;
    Seq_p names, types, subvs;
    
    names = NewStrVec(1);
    AppendToStrVec(name, -1, names);

    types = NewStrVec(1);
    AppendToStrVec("I", -1, types);

    subv = EmptyMetaView();
    subvs = NewSeqVec(IT_view, &subv, 1);
    
    result = NewView(V_Meta(EmptyMetaView()));
    SetViewSeqs(result, MC_name, 1, FinishStrVec(names));
    SetViewSeqs(result, MC_type, 1, FinishStrVec(types));
    SetViewSeqs(result, MC_subv, 1, subvs);
    return result;
}

View_p TagView (View_p view, const char* name) {
    View_p tagview;
    
    tagview = NewView(MakeIntColMeta(name));
    SetViewCols(tagview, 0, 1, NewIotaColumn(ViewSize(view)));
    
    return PairView(view, tagview);
}

View_p DescAsMeta (const char** desc, const char* end) {
    int c, len, cols = 0, namebytes = 0;
    char sep, *buf, *ptr, **strings;
    Seq_p *views, tempseq;
    struct Buffer names, types, subvs;
    View_p result, empty;

    InitBuffer(&names);
    InitBuffer(&types);
    InitBuffer(&subvs);
    
    len = end - *desc;
    buf = memcpy(malloc(len+1), *desc, len);
    buf[len] = ',';
    
    empty = EmptyMetaView();
    result = NewView(V_Meta(empty));
    
    for (ptr = buf; ptr < buf + len; ++ptr) {
        const char *s = ptr;
        while (strchr(":,[]", *ptr) == 0)
            ++ptr;
        sep = *ptr;
        *ptr = 0;

        ADD_PTR_TO_BUF(names, s);
        namebytes += ptr - s + 1;
         
        switch (sep) {

            case '[':
                ADD_PTR_TO_BUF(types, "V");
                ++ptr;
                ADD_PTR_TO_BUF(subvs, DescAsMeta((const char**) &ptr, buf + len));
                sep = *++ptr;
                break;

            case ':':
                ptr += 2;
                sep = *ptr;
                *ptr = 0;
                ADD_PTR_TO_BUF(types, ptr - 1);
                ADD_PTR_TO_BUF(subvs, empty);
                break;

            default:
                ADD_PTR_TO_BUF(types, "S");
                ADD_PTR_TO_BUF(subvs, empty);
                break;
        }
        
        ++cols;
        if (sep != ',')
            break;
    }

    strings = BufferAsPtr(&names, 1);
    tempseq = NewStrVec(1);
    for (c = 0; c < cols; ++c)
        AppendToStrVec(strings[c], -1, tempseq);
    SetViewSeqs(result, MC_name, 1, FinishStrVec(tempseq));
    ReleaseBuffer(&names, 0);
    
    strings = BufferAsPtr(&types, 1);
    tempseq = NewStrVec(1);
    for (c = 0; c < cols; ++c)
        AppendToStrVec(strings[c], -1, tempseq);
    SetViewSeqs(result, MC_type, 1, FinishStrVec(tempseq));
    ReleaseBuffer(&types, 0);
    
    views = BufferAsPtr(&subvs, 1);
    tempseq = NewSeqVec(IT_view, views, cols);
    SetViewSeqs(result, MC_subv, 1, tempseq);
    ReleaseBuffer(&subvs, 0);
    
    free(buf);
    *desc += ptr - buf;
    return result;
}

View_p DescToMeta (const char *desc) {
    int len;
    const char *end;
    View_p meta;

    len = strlen(desc);
    end = desc + len;
    
    meta = DescAsMeta(&desc, end);
    if (desc < end)
        return NULL;

    return meta;
}

ItemTypes DataCmd_VX (Item args[]) {
    View_p meta, view;
    int c, cols, objc;
    const Object_p *objv;
    Column column;

    meta = args[0].v;
    cols = ViewSize(meta);
    
    objv = (const Object_p *) args[1].u.ptr;
    objc = args[1].u.len;

    if (objc != cols) {
        args->e = EC_wnoa;
        return IT_error;
    }

    view = NewView(meta);
    for (c = 0; c < cols; ++c) {
        column = CoerceColumn(ViewColType(view, c), objv[c]);
        if (column.seq == NULL)
            return IT_unknown;
            
        SetViewCols(view, c, 1, column);
    }

    args->v = view;
    return IT_view;
}

ItemTypes StructDescCmd_V (Item args[]) {
    struct Buffer buffer;
    Item item;
    
    InitBuffer(&buffer);
    MetaAsDesc(V_Meta(args[0].v), &buffer);
    AddToBuffer(&buffer, "", 1);
    item.s = BufferAsPtr(&buffer, 1);
    args->o = ItemAsObj(IT_string, &item);
    ReleaseBuffer(&buffer, 0);
    return IT_object;
}

ItemTypes StructureCmd_V (Item args[]) {
    struct Buffer buffer;
    Item item;
    
    InitBuffer(&buffer);
    ViewStructure(V_Meta(args[0].v), &buffer);
    AddToBuffer(&buffer, "", 1);
    item.s = BufferAsPtr(&buffer, 1);
    args->o = ItemAsObj(IT_string, &item);
    ReleaseBuffer(&buffer, 0);
    return IT_object;
}
/* %include<wrap_gen.c>% */
/* wrap_gen.c - generated code, do not edit */

#include <stdlib.h>

  
ItemTypes BlockedCmd_V (Item_p a) {
  a[0].v = BlockedView(a[0].v);
  return IT_view;
}

ItemTypes CloneCmd_V (Item_p a) {
  a[0].v = CloneView(a[0].v);
  return IT_view;
}

ItemTypes CoerceCmd_OS (Item_p a) {
  a[0].c = CoerceCmd(a[0].o, a[1].s);
  return IT_column;
}

ItemTypes ColMapCmd_Vn (Item_p a) {
  a[0].v = ColMapView(a[0].v, a[1].c);
  return IT_view;
}

ItemTypes ColOmitCmd_Vn (Item_p a) {
  a[0].v = ColOmitView(a[0].v, a[1].c);
  return IT_view;
}

ItemTypes CompareCmd_VV (Item_p a) {
  a[0].i = ViewCompare(a[0].v, a[1].v);
  return IT_int;
}

ItemTypes CompatCmd_VV (Item_p a) {
  a[0].i = ViewCompat(a[0].v, a[1].v);
  return IT_int;
}

ItemTypes ConcatCmd_VV (Item_p a) {
  a[0].v = ConcatView(a[0].v, a[1].v);
  return IT_view;
}

ItemTypes CountsColCmd_C (Item_p a) {
  a[0].c = NewCountsColumn(a[0].c);
  return IT_column;
}

ItemTypes CountViewCmd_I (Item_p a) {
  a[0].v = NoColumnView(a[0].i);
  return IT_view;
}

ItemTypes FirstCmd_VI (Item_p a) {
  a[0].v = FirstView(a[0].v, a[1].i);
  return IT_view;
}

ItemTypes GetColCmd_VN (Item_p a) {
  a[0].c = ViewCol(a[0].v, a[1].i);
  return IT_column;
}

ItemTypes GetInfoCmd_VVI (Item_p a) {
  a[0].c = GetHashInfo(a[0].v, a[1].v, a[2].i);
  return IT_column;
}

ItemTypes GroupCmd_VnS (Item_p a) {
  a[0].v = GroupCol(a[0].v, a[1].c, a[2].s);
  return IT_view;
}

ItemTypes GroupedCmd_ViiS (Item_p a) {
  a[0].v = GroupedView(a[0].v, a[1].c, a[2].c, a[3].s);
  return IT_view;
}

ItemTypes HashFindCmd_VIViii (Item_p a) {
  a[0].i = HashDoFind(a[0].v, a[1].i, a[2].v, a[3].c, a[4].c, a[5].c);
  return IT_int;
}

ItemTypes HashViewCmd_V (Item_p a) {
  a[0].c = HashValues(a[0].v);
  return IT_column;
}

ItemTypes IjoinCmd_VV (Item_p a) {
  a[0].v = IjoinView(a[0].v, a[1].v);
  return IT_view;
}

ItemTypes IotaCmd_I (Item_p a) {
  a[0].c = NewIotaColumn(a[0].i);
  return IT_column;
}

ItemTypes IsectMapCmd_VV (Item_p a) {
  a[0].c = IntersectMap(a[0].v, a[1].v);
  return IT_column;
}

ItemTypes JoinCmd_VVS (Item_p a) {
  a[0].v = JoinView(a[0].v, a[1].v, a[2].s);
  return IT_view;
}

ItemTypes LastCmd_VI (Item_p a) {
  a[0].v = LastView(a[0].v, a[1].i);
  return IT_view;
}

ItemTypes MdefCmd_O (Item_p a) {
  a[0].v = ObjAsMetaView(a[0].o);
  return IT_view;
}

ItemTypes MdescCmd_S (Item_p a) {
  a[0].v = DescToMeta(a[0].s);
  return IT_view;
}

ItemTypes MetaCmd_V (Item_p a) {
  a[0].v = V_Meta(a[0].v);
  return IT_view;
}

ItemTypes OmitMapCmd_iI (Item_p a) {
  a[0].c = OmitColumn(a[0].c, a[1].i);
  return IT_column;
}

ItemTypes OneColCmd_VN (Item_p a) {
  a[0].v = OneColView(a[0].v, a[1].i);
  return IT_view;
}

ItemTypes PairCmd_VV (Item_p a) {
  a[0].v = PairView(a[0].v, a[1].v);
  return IT_view;
}

ItemTypes RemapSubCmd_ViII (Item_p a) {
  a[0].v = RemapSubview(a[0].v, a[1].c, a[2].i, a[3].i);
  return IT_view;
}

ItemTypes ReplaceCmd_VIIV (Item_p a) {
  a[0].v = ViewReplace(a[0].v, a[1].i, a[2].i, a[3].v);
  return IT_view;
}

ItemTypes RowCmpCmd_VIVI (Item_p a) {
  a[0].i = RowCompare(a[0].v, a[1].i, a[2].v, a[3].i);
  return IT_int;
}

ItemTypes RowEqCmd_VIVI (Item_p a) {
  a[0].i = RowEqual(a[0].v, a[1].i, a[2].v, a[3].i);
  return IT_int;
}

ItemTypes RowHashCmd_VI (Item_p a) {
  a[0].i = RowHash(a[0].v, a[1].i);
  return IT_int;
}

ItemTypes SizeCmd_V (Item_p a) {
  a[0].i = ViewSize(a[0].v);
  return IT_int;
}

ItemTypes SortMapCmd_V (Item_p a) {
  a[0].c = SortMap(a[0].v);
  return IT_column;
}

ItemTypes StepCmd_VIIII (Item_p a) {
  a[0].v = StepView(a[0].v, a[1].i, a[2].i, a[3].i, a[4].i);
  return IT_view;
}

ItemTypes StrLookupCmd_Ss (Item_p a) {
  a[0].i = StringLookup(a[0].s, a[1].c);
  return IT_int;
}

ItemTypes TagCmd_VS (Item_p a) {
  a[0].v = TagView(a[0].v, a[1].s);
  return IT_view;
}

ItemTypes TakeCmd_VI (Item_p a) {
  a[0].v = TakeView(a[0].v, a[1].i);
  return IT_view;
}

ItemTypes UngroupCmd_VN (Item_p a) {
  a[0].v = UngroupView(a[0].v, a[1].i);
  return IT_view;
}

ItemTypes UniqueMapCmd_V (Item_p a) {
  a[0].c = UniqMap(a[0].v);
  return IT_column;
}

ItemTypes ViewAsColCmd_V (Item_p a) {
  a[0].c = ViewAsCol(a[0].v);
  return IT_column;
}

ItemTypes WidthCmd_V (Item_p a) {
  a[0].i = ViewWidth(a[0].v);
  return IT_int;
}

/* : append ( VV-V ) over size swap insert ; */
ItemTypes AppendCmd_VV (Item_p a) {
  ItemTypes t;
  /* over 0 */ a[2] = a[0];
  t = SizeCmd_V(a+2);
  /* swap 1 */ a[3] = a[1]; a[1] = a[2]; a[2] = a[3];
  t = InsertCmd_VIV(a);
  return IT_view;
}

/* : colconv ( C-C )   ; */
ItemTypes ColConvCmd_C (Item_p a) {
  return IT_column;
}

/* : counts ( VN-C ) getcol countscol ; */
ItemTypes CountsCmd_VN (Item_p a) {
  ItemTypes t;
  t = GetColCmd_VN(a);
  t = CountsColCmd_C(a);
  return IT_column;
}

/* : delete ( VII-V ) 0 countview replace ; */
ItemTypes DeleteCmd_VII (Item_p a) {
  ItemTypes t;
  a[3].i = 0;
  t = CountViewCmd_I(a+3);
  t = ReplaceCmd_VIIV(a);
  return IT_view;
}

/* : except ( VV-V ) over swap exceptmap remap ; */
ItemTypes ExceptCmd_VV (Item_p a) {
  ItemTypes t;
  /* over 0 */ a[2] = a[0];
  /* swap 1 */ a[3] = a[1]; a[1] = a[2]; a[2] = a[3];
  t = ExceptMapCmd_VV(a+1);
  t = RemapCmd_Vi(a);
  return IT_view;
}

/* : exceptmap ( VV-C ) over swap isectmap swap size omitmap ; */
ItemTypes ExceptMapCmd_VV (Item_p a) {
  ItemTypes t;
  /* over 0 */ a[2] = a[0];
  /* swap 1 */ a[3] = a[1]; a[1] = a[2]; a[2] = a[3];
  t = IsectMapCmd_VV(a+1);
  /* swap 0 */ a[2] = a[0]; a[0] = a[1]; a[1] = a[2];
  t = SizeCmd_V(a+1);
  t = OmitMapCmd_iI(a);
  return IT_column;
}

/* : insert ( VIV-V ) 0 swap replace ; */
ItemTypes InsertCmd_VIV (Item_p a) {
  ItemTypes t;
  a[3].i = 0;
  /* swap 2 */ a[4] = a[2]; a[2] = a[3]; a[3] = a[4];
  t = ReplaceCmd_VIIV(a);
  return IT_view;
}

/* : intersect ( VV-V ) over swap isectmap remap ; */
ItemTypes IntersectCmd_VV (Item_p a) {
  ItemTypes t;
  /* over 0 */ a[2] = a[0];
  /* swap 1 */ a[3] = a[1]; a[1] = a[2]; a[2] = a[3];
  t = IsectMapCmd_VV(a+1);
  t = RemapCmd_Vi(a);
  return IT_view;
}

/* : namecol ( V-V ) meta 0 onecol ; */
ItemTypes NameColCmd_V (Item_p a) {
  ItemTypes t;
  t = MetaCmd_V(a);
  a[1].i = 0;
  t = OneColCmd_VN(a);
  return IT_view;
}

/* : names ( V-C ) meta 0 getcol ; */
ItemTypes NamesCmd_V (Item_p a) {
  ItemTypes t;
  t = MetaCmd_V(a);
  a[1].i = 0;
  t = GetColCmd_VN(a);
  return IT_column;
}

/* : product ( VV-V ) over over size spread rrot swap size repeat pair ; */
ItemTypes ProductCmd_VV (Item_p a) {
  ItemTypes t;
  /* over 0 */ a[2] = a[0];
  /* over 1 */ a[3] = a[1];
  t = SizeCmd_V(a+3);
  t = SpreadCmd_VI(a+2);
  /* rrot 0 */ a[3] = a[2]; a[2] = a[1]; a[1] = a[0]; a[0] = a[3];
  /* swap 1 */ a[3] = a[1]; a[1] = a[2]; a[2] = a[3];
  t = SizeCmd_V(a+2);
  t = RepeatCmd_VI(a+1);
  t = PairCmd_VV(a);
  return IT_view;
}

/* : remap ( Vi-V ) 0 -1 remapsub ; */
ItemTypes RemapCmd_Vi (Item_p a) {
  ItemTypes t;
  a[2].i = 0;
  a[3].i = -1;
  t = RemapSubCmd_ViII(a);
  return IT_view;
}

/* : repeat ( VI-V ) over size imul 0 1 1 step ; */
ItemTypes RepeatCmd_VI (Item_p a) {
  ItemTypes t;
  /* over 0 */ a[2] = a[0];
  t = SizeCmd_V(a+2);
  /* imul 1 */ a[1].i *=  a[2].i;
  a[2].i = 0;
  a[3].i = 1;
  a[4].i = 1;
  t = StepCmd_VIIII(a);
  return IT_view;
}

/* : reverse ( V-V ) dup size -1 1 -1 step ; */
ItemTypes ReverseCmd_V (Item_p a) {
  ItemTypes t;
  /* dup 0 */ a[1] = a[0];
  t = SizeCmd_V(a+1);
  a[2].i = -1;
  a[3].i = 1;
  a[4].i = -1;
  t = StepCmd_VIIII(a);
  return IT_view;
}

/* : slice ( VIII-V ) rrot 1 swap step ; */
ItemTypes SliceCmd_VIII (Item_p a) {
  ItemTypes t;
  /* rrot 1 */ a[4] = a[3]; a[3] = a[2]; a[2] = a[1]; a[1] = a[4];
  a[4].i = 1;
  /* swap 3 */ a[5] = a[3]; a[3] = a[4]; a[4] = a[5];
  t = StepCmd_VIIII(a);
  return IT_view;
}

/* : sort ( V-V ) dup sortmap remap ; */
ItemTypes SortCmd_V (Item_p a) {
  ItemTypes t;
  /* dup 0 */ a[1] = a[0];
  t = SortMapCmd_V(a+1);
  t = RemapCmd_Vi(a);
  return IT_view;
}

/* : spread ( VI-V ) over size 0 rot 1 step ; */
ItemTypes SpreadCmd_VI (Item_p a) {
  ItemTypes t;
  /* over 0 */ a[2] = a[0];
  t = SizeCmd_V(a+2);
  a[3].i = 0;
  /* rot 1 */ a[4] = a[1]; a[1] = a[2]; a[2] = a[3]; a[3] = a[4];
  a[4].i = 1;
  t = StepCmd_VIIII(a);
  return IT_view;
}

/* : types ( V-C ) meta 1 getcol ; */
ItemTypes TypesCmd_V (Item_p a) {
  ItemTypes t;
  t = MetaCmd_V(a);
  a[1].i = 1;
  t = GetColCmd_VN(a);
  return IT_column;
}

/* : union ( VV-V ) over except concat ; */
ItemTypes UnionCmd_VV (Item_p a) {
  ItemTypes t;
  /* over 0 */ a[2] = a[0];
  t = ExceptCmd_VV(a+1);
  t = ConcatCmd_VV(a);
  return IT_view;
}

/* : unionmap ( VV-C ) swap exceptmap ; */
ItemTypes UnionMapCmd_VV (Item_p a) {
  ItemTypes t;
  /* swap 0 */ a[2] = a[0]; a[0] = a[1]; a[1] = a[2];
  t = ExceptMapCmd_VV(a);
  return IT_column;
}

/* : unique ( V-V ) dup uniquemap remap ; */
ItemTypes UniqueCmd_V (Item_p a) {
  ItemTypes t;
  /* dup 0 */ a[1] = a[0];
  t = UniqueMapCmd_V(a+1);
  t = RemapCmd_Vi(a);
  return IT_view;
}

/* : viewconv ( V-V )   ; */
ItemTypes ViewConvCmd_V (Item_p a) {
  return IT_view;
}

/* end of generated code */

/* %include<cmds.c>% */
/*
 * cmds.c - Implementation of Tcl-specific operators.
 */

/*
 * ext_tcl.h - Definitions needed across the Tcl-specific code.
 */

#include <stdlib.h>
#include <string.h>


#include <tcl.h>

#if 10 * TCL_MAJOR_VERSION + TCL_MINOR_VERSION < 86
#define Tcl_GetErrorLine(interp) (interp)->errorLine
#define CONST86
#endif

/* colobj.c */

extern Tcl_ObjType f_colObjType;

Object_p  (ColumnAsObj) (Column column);

/* ext_tcl.c */

typedef struct SharedInfo {
    Tcl_Interp *interp;
    const struct CmdDispatch *cmds;
} SharedInfo;

#define Interp() (GetShared()->info->interp)
#define DecObjRef   Tcl_DecrRefCount

Object_p  (BufferAsTclList) (Buffer_p bp);
Object_p  (IncObjRef) (Object_p objPtr);

/* viewobj.c */

extern Tcl_ObjType f_viewObjType;

int       (AdjustCmdDef) (Object_p cmd);
Object_p  (ViewAsObj) (View_p view);

ItemTypes DefCmd_OO (Item args[]) {
    int argc, c, r, cols, rows;
    View_p meta, result;
    Object_p obj, *argv;
    Column column;
    struct Buffer buffer;
    
    if (MdefCmd_O(args) != IT_view)
        return IT_error;
    if (args[0].v == NULL)
        return IT_view;
    meta = args[0].v;
        
    if (Tcl_ListObjGetElements(Interp(), args[1].o, &argc, &argv) != TCL_OK)
        return IT_unknown;
        
    cols = ViewSize(meta);
    if (cols == 0) {
        if (argc != 0) {
            args->e = EC_cizwv;
            return IT_error;
        }
        
        args->v = NoColumnView(0);
        return IT_view;
    }
    
    if (argc % cols != 0) {
        args->e = EC_nmcw;
        return IT_error;
    }

    result = NewView(meta);
    rows = argc / cols;
    
    for (c = 0; c < cols; ++c) {
        InitBuffer(&buffer);

        for (r = 0; r < rows; ++r)
            ADD_PTR_TO_BUF(buffer, argv[c + cols * r]);

        obj = BufferAsTclList(&buffer);
        column = CoerceColumn(ViewColType(result, c), obj);
        DecObjRef(obj);
        
        if (column.seq == NULL)
            return IT_unknown;
            
        SetViewCols(result, c, 1, column);
    }
    
    args->v = result;
    return IT_view;
}

static Object_p GetViewRows (View_p view, int row, int rows, int tags) {
    int r, c, cols;
    View_p meta;
    Item item;
    struct Buffer buf;
    
    meta = V_Meta(view);
    cols = ViewSize(meta);
    InitBuffer(&buf);
    
    for (r = 0; r < rows; ++r)
        for (c = 0; c < cols; ++c) {
            if (tags) {
                item.c = ViewCol(meta, 0);
                ADD_PTR_TO_BUF(buf, ItemAsObj(GetItem(c, &item), &item));
            }
            item.c = ViewCol(view, c);
            ADD_PTR_TO_BUF(buf, ItemAsObj(GetItem(r + row, &item), &item));
        }
        
    return BufferAsTclList(&buf);
}

static int ColumnNumber(Object_p obj, View_p meta) {
    int col;
    const char *name = Tcl_GetString(obj);
    
    switch (*name) {

        case '#': return -2;
        case '*': return -1;
        case '-': case '0': case '1': case '2': case '3': case '4':
                  case '5': case '6': case '7': case '8': case '9':
            if (Tcl_GetIntFromObj(Interp(), obj, &col) != TCL_OK)
                return -3;

            if (col < 0)
                col += ViewSize(meta);
                
            if (col < 0 || col >= ViewSize(meta))
                return -4;
            break;

        default:
            col = StringLookup(name, ViewCol(meta, MC_name));
            if (col < 0)
                return -3;
    }

    return col;
}

ItemTypes GetCmd_VX (Item args[]) {
    View_p view;
    int i, objc, r, row, col, rows;
    const Object_p *objv;
    ItemTypes currtype;

    view = args[0].v;
    currtype = IT_view;
    
    objv = (const Object_p *) args[1].u.ptr;
    objc = args[1].u.len;

    if (objc == 0) {
        args->o = GetViewRows(view, 0, ViewSize(view), 0);
        return IT_object;
    }
    
    for (i = 0; i < objc; ++i) {
        if (currtype == IT_view)
            view = args[0].v;
        else /* TODO: make sure the object does not leak if newly created */
            view = ObjAsView(ItemAsObj(currtype, args));
        
        if (view == NULL)
            return IT_unknown;

        rows = ViewSize(view);

        if (Tcl_GetIntFromObj(0, objv[i], &row) != TCL_OK)
            switch (*Tcl_GetString(objv[i])) {
                case '@':   args->v = V_Meta(view);
                            currtype = IT_view;
                            continue;
                case '#':   args->i = rows;
                            currtype = IT_int;
                            continue;
                case '*':   row = -1;
                            break;
                default:    Tcl_GetIntFromObj(Interp(), objv[i], &row);
                            return IT_unknown;
            }
        else if (row < 0) {
            row += rows;
            if (row < 0) {
                args->e = EC_rioor;
                return IT_error;
            }
        }

        if (++i >= objc) {
            if (row >= 0)
                args->o = GetViewRows(view, row, 1, 1); /* one tagged row */
            else {
                struct Buffer buf;
                InitBuffer(&buf);
                for (r = 0; r < rows; ++r)
                    ADD_PTR_TO_BUF(buf, GetViewRows(view, r, 1, 0));
                args->o = BufferAsTclList(&buf);
            }
            return IT_object;
        }

        col = ColumnNumber(objv[i], V_Meta(view));

        if (row >= 0)
            switch (col) {
                default:    args->c = ViewCol(view, col);
                            currtype = GetItem(row, args);
                            break;
                case -1:    args->o = GetViewRows(view, row, 1, 0);
                            currtype = IT_object;
                            break;
                case -2:    args->i = row;
                            currtype = IT_int;
                            break;
                case -3:    return IT_unknown;
                case -4:    args->e = EC_cioor;
                            return IT_error;
            }
        else
            switch (col) {
                default:    args->c = ViewCol(view, col);
                            currtype = IT_column;
                            break;
                case -1:    args->o = GetViewRows(view, 0, rows, 0);
                            currtype = IT_object;
                            break;
                case -2:    args->c = NewIotaColumn(rows);
                            currtype = IT_column;
                            break;
                case -3:    return IT_unknown;
                case -4:    args->e = EC_cioor;
                            return IT_error;
            }
    }
    
    return currtype;
}

ItemTypes MutInfoCmd_V (Item_p a) {
    int i;
    Seq_p mods, *seqs;
    Object_p result;
    
    if (!IsMutable(a[0].v))
        return IT_unknown;
        
    mods = MutPrepare(a[0].v);
    seqs = (void*) (mods + 1);
    
    result = Tcl_NewListObj(0, NULL);

    for (i = 0; i < MP_delmap; ++i)
        Tcl_ListObjAppendElement(NULL, result, ViewAsObj(seqs[i]));
    for (i = MP_delmap; i < MP_limit; ++i)
        Tcl_ListObjAppendElement(NULL, result, ColumnAsObj(SeqAsCol(seqs[i])));
    
    a->o = result;
    return IT_object;
}

ItemTypes RefsCmd_O (Item args[]) {
    args->i = args[0].o->refCount;
    return IT_int;
}

ItemTypes RenameCmd_VO (Item args[]) {
    int i, c, objc;
    View_p meta, view, newmeta, newview;
    Object_p *objv;
    Item item;

    if (Tcl_ListObjGetElements(Interp(), args[1].o, &objc, &objv) != TCL_OK)
        return IT_unknown;
        
    if (objc % 2) {
        args->e = EC_rambe;
        return IT_error;
    }

    view = args[0].v;
    meta = V_Meta(view);
    newmeta = meta;
    
    for (i = 0; i < objc; i += 2) {
        c = ColumnByName(meta, objv[i]);
        if (c < 0)
            return IT_unknown;
        item.s = Tcl_GetString(objv[i+1]);
        newmeta = ViewSet(newmeta, c, MC_name, &item);
    }
    
    newview = NewView(newmeta);
    
    for (c = 0; c < ViewWidth(view); ++c)
        SetViewCols(newview, c, 1, ViewCol(view, c));

    args->v = newview;
    return IT_view;
}

ItemTypes ToCmd_OO (Item args[]) {
    Object_p result;

    result = Tcl_ObjSetVar2(Interp(), args[1].o, 0, args[0].o,
                                                    TCL_LEAVE_ERR_MSG);
    if (result == NULL)
        return IT_unknown;

    args->o = result;
    return IT_object;
}

ItemTypes TypeCmd_O (Item args[]) {
    args->s = args[0].o->typePtr != NULL ? args[0].o->typePtr->name : "";
    return IT_string;
}
/* %include<colobj.c>% */
/*
 * colobj.c - Implementation of column objects in Tcl.
 */


static const char *ErrorMessage(ErrorCodes code) {
	switch (code) {
		case EC_cioor: return "column index out of range";
		case EC_rioor: return "row index out of range";
		case EC_cizwv: return "cannot insert in zero-width view";
		case EC_nmcw:	 return "item count not a multiple of column width";
		case EC_rambe: return "rename arg count must be even";
		case EC_nalor: return "nead at least one row";
		case EC_wnoa:	 return "wrong number of arguments";
	}
	return "?";
}

Object_p ItemAsObj (ItemTypes type, Item_p item) {
	switch (type) {

		case IT_int:
			return Tcl_NewIntObj(item->i);

		case IT_wide:
			return Tcl_NewWideIntObj(item->w);

		case IT_float:
			return Tcl_NewDoubleObj(item->f);

		case IT_double:
			return Tcl_NewDoubleObj(item->d);

		case IT_string:
			return Tcl_NewStringObj(item->s, -1);
			
		case IT_bytes:
			return Tcl_NewByteArrayObj(item->u.ptr, item->u.len);

		case IT_object:
			return item->o;

		case IT_column:
			return ColumnAsObj(item->c);

		case IT_view:
			if (item->v == NULL) {
				Tcl_SetResult(Interp(), "invalid view", TCL_STATIC);
				return NULL;
			}
			return ViewAsObj(item->v);

		case IT_error:
			Tcl_SetResult(Interp(), (char*) ErrorMessage(item->e), TCL_STATIC);
			return NULL;

		default:
			Assert(0);
			return NULL;
	}
}

int ColumnByName (View_p meta, Object_p obj) {
	int colnum;
	const char *str = Tcl_GetString(obj);
	
	switch (*str) {

		case '#':
			return -2;

		case '*':
			return -3;

		case '-': case '0': case '1': case '2': case '3': case '4':
				  case '5': case '6': case '7': case '8': case '9':
			if (Tcl_GetIntFromObj(NULL, obj, &colnum) != TCL_OK)
				return -4;
			if (colnum < 0)
				colnum += ViewSize(meta);
			if (colnum < 0 || colnum >= ViewSize(meta))
				return -5;
			return colnum;
			
		default:
			return StringLookup(str, ViewCol(meta, MC_name));
	}
}

int ObjToItem (ItemTypes type, Item_p item) {
	switch (type) {
		
		case IT_int:
			return Tcl_GetIntFromObj(Interp(), item->o, &item->i) == TCL_OK;

		case IT_wide:
			return Tcl_GetWideIntFromObj(Interp(), item->o,
											(Tcl_WideInt*) &item->w) == TCL_OK;

		case IT_float:
			if (Tcl_GetDoubleFromObj(Interp(), item->o, &item->d) != TCL_OK)
				return 0;
			item->f = (float) item->d;
			break;

		case IT_double:
			return Tcl_GetDoubleFromObj(Interp(), item->o, &item->d) == TCL_OK;

		case IT_string:
			item->s = Tcl_GetString(item->o);
			break;

		case IT_bytes:
			item->u.ptr = Tcl_GetByteArrayFromObj(item->o, &item->u.len);
			break;

		case IT_object:
			break;

		case IT_column:
			item->c = ObjAsColumn(item->o);
			return item->c.seq != NULL && item->c.seq->count >= 0;

		case IT_view:
			item->v = ObjAsView(item->o);
			return item->v != NULL;

		default:
			Assert(0);
			return 0;
	}
	
	return 1;
}

#define OBJ_TO_COLUMN_P(o) ((Column*) &(o)->internalRep.twoPtrValue)

static void FreeColIntRep (Tcl_Obj *obj) {
	PUSH_KEEP_REFS
	DecRefCount(OBJ_TO_COLUMN_P(obj)->seq);
	POP_KEEP_REFS
}

static void DupColIntRep (Tcl_Obj *src, Tcl_Obj *dup) {
	Column_p column = OBJ_TO_COLUMN_P(src);
	IncRefCount(column->seq);
	*OBJ_TO_COLUMN_P(dup) = *column;
	dup->typePtr = &f_colObjType;
}

static void UpdateColStrRep (Tcl_Obj *obj) {
	int r;
	Object_p list;
	Item item;
	struct Buffer buf;
	Column column;
	PUSH_KEEP_REFS
	
	InitBuffer(&buf);
	column = *OBJ_TO_COLUMN_P(obj);
	
	if (column.seq != NULL)
		for (r = 0; r < column.seq->count; ++r) {
			item.c = column;
			ADD_PTR_TO_BUF(buf, ItemAsObj(GetItem(r, &item), &item));
		}

	list = BufferAsTclList(&buf);

	/* this hack avoids creating a second copy of the string rep */
	obj->bytes = Tcl_GetStringFromObj(list, &obj->length);
	list->bytes = NULL;
	list->length = 0;
	
	DecObjRef(list);
	
	POP_KEEP_REFS
}

#define LO_list		data[0].o
#define LO_objv		data[1].p

static void ListObjCleaner (Seq_p seq) {
	DecObjRef(seq->LO_list);
}

static ItemTypes ListObjGetter (int row, Item_p item) {
	const Object_p *objv = item->c.seq->LO_objv;
	item->o = objv[row];
	return IT_object;
}

static struct SeqType ST_ListObj = {
	"listobj", ListObjGetter, 0, ListObjCleaner
};

static int SetColFromAnyRep (Tcl_Interp *interp, Tcl_Obj *obj) {
	int objc;
	Object_p dup, *objv;
	Seq_p seq;

	/* must duplicate the list because it'll be kept by ObjAsColumn */
	dup = Tcl_DuplicateObj(obj);
	
	if (Tcl_ListObjGetElements(interp, dup, &objc, &objv) != TCL_OK) {
		DecObjRef(dup);
		return TCL_ERROR;
	}

	PUSH_KEEP_REFS
	
	seq = IncRefCount(NewSequence(objc, &ST_ListObj, 0));
	seq->LO_list = IncObjRef(dup);
	seq->LO_objv = objv;

	if (obj->typePtr != NULL && obj->typePtr->freeIntRepProc != NULL)
		obj->typePtr->freeIntRepProc(obj);

	*OBJ_TO_COLUMN_P(obj) = SeqAsCol(seq);
	obj->typePtr = &f_colObjType;
	
	POP_KEEP_REFS
	return TCL_OK;
}

Tcl_ObjType f_colObjType = {
	"column", FreeColIntRep, DupColIntRep, UpdateColStrRep, SetColFromAnyRep
};

Column ObjAsColumn (Object_p obj) {
	if (Tcl_ConvertToType(Interp(), obj, &f_colObjType) != TCL_OK) {
		Column column;
		column.seq = NULL;
		column.pos = -1;
		return column;
	}
	
	return *OBJ_TO_COLUMN_P(obj);
}

Object_p ColumnAsObj (Column column) {
	Object_p result;

	result = Tcl_NewObj();
	Tcl_InvalidateStringRep(result);

	IncRefCount(column.seq);
	*OBJ_TO_COLUMN_P(result) = column;
	result->typePtr = &f_colObjType;
	return result;
}
/* %include<loop.c>% */
/*
 * loop.c - Implementation of the Tcl-specific "loop" operator.
 */

/* TODO: code was copied from v3, needs a lot of cleanup */


    typedef struct LoopInfo {
        int row;
        Object_p view;
        View_p v;
        Object_p cursor;
    } LoopInfo;

    typedef struct ColTrace {
        LoopInfo* lip;
        int col;
        Object_p name;
        int lastRow;
    } ColTrace;

static char* cursor_tracer(ClientData cd, Tcl_Interp* ip, const char *name1, const char *name2, int flags) {
    ColTrace* ct = (ColTrace*) cd;
    int row = ct->lip->row;
    if (row != ct->lastRow) {
        Object_p o;
        if (ct->col < 0)
            o = Tcl_NewIntObj(row);
        else {
            Item item;
            item.c = ViewCol(ct->lip->v, ct->col);
            o = ItemAsObj(GetItem(row, &item), &item);
        }
        if (Tcl_ObjSetVar2(ip, ct->lip->cursor, ct->name, o, 0) == 0)
            return "cursor_tracer?";
        ct->lastRow = row;
    }
    return 0;
}

static int loop_vop(int oc, Tcl_Obj* const* ov) {
    LoopInfo li;
    const char *cur;
    struct Buffer result;
    int type, e = TCL_OK;
    static const char *options[] = { "-where", "-index", "-collect", 0 };
    enum { eLOOP = -1, eWHERE, eINDEX, eCOLLECT };

    if (oc < 2 || oc > 4) {
        Tcl_WrongNumArgs(Interp(), 1, ov, "view ?arrayName? ?-type? body");
        return TCL_ERROR;
    }

    li.v = ObjAsView(ov[0]);
    if (li.v == NULL)
        return TCL_ERROR;

    --oc; ++ov;
    if (oc > 1 && *Tcl_GetString(*ov) != '-') {
        li.cursor = *ov++; --oc;
    } else
        li.cursor = Tcl_NewObj();
    cur = Tcl_GetString(IncObjRef(li.cursor));

    if (Tcl_GetIndexFromObj(0, *ov, options, "", 0, &type) == TCL_OK) {
        --oc; ++ov;
    } else
        type = eLOOP;

    {
        int i, nr = ViewSize(li.v), nc = ViewWidth(li.v);
        ColTrace* ctab = (ColTrace*) ckalloc((nc + 1) * sizeof(ColTrace));
        InitBuffer(&result);
        for (i = 0; i <= nc; ++i) {
            ColTrace* ctp = ctab + i;
            ctp->lip = &li;
            ctp->lastRow = -1;
            if (i < nc) {
                ctp->col = i;
                ctp->name = Tcl_NewStringObj(GetColItem(i,
                              ViewCol(V_Meta(li.v), MC_name), IT_string).s, -1);
            } else {
                ctp->col = -1;
                ctp->name = Tcl_NewStringObj("#", -1);
            }
            e = Tcl_TraceVar2(Interp(), cur, Tcl_GetString(ctp->name),
                              TCL_TRACE_READS, cursor_tracer, (ClientData) ctp);
        }
        if (e == TCL_OK)
            switch (type) {
                case eLOOP:
                    for (li.row = 0; li.row < nr; ++li.row) {
                        e = Tcl_EvalObj(Interp(), *ov);
                        if (e == TCL_CONTINUE)
                            e = TCL_OK;
                        else if (e != TCL_OK) {
                            if (e == TCL_BREAK)
                                e = TCL_OK;
                            else if (e == TCL_ERROR) {
                                char msg[50];
                                sprintf(msg, "\n    (\"loop\" body line %d)",
				    Tcl_GetErrorLine(Interp()));
                                Tcl_AddObjErrorInfo(Interp(), msg, -1);
                            }
                            break;
                        }
                    }
                    break;
                case eWHERE:
                case eINDEX:
                    for (li.row = 0; li.row < nr; ++li.row) {
                        int f;
                        e = Tcl_ExprBooleanObj(Interp(), *ov, &f);
                        if (e != TCL_OK)
                            break;
                        if (f)
                            ADD_INT_TO_BUF(result, li.row);
                    }
                    break;
                case eCOLLECT:
                    for (li.row = 0; li.row < nr; ++li.row) {
                        Object_p o;
                        e = Tcl_ExprObj(Interp(), *ov, &o);
                        if (e != TCL_OK)
                            break;
                        ADD_PTR_TO_BUF(result, o);
                    }
                    break;
                default: Assert(0); return TCL_ERROR;
            }
        for (i = 0; i <= nc; ++i) {
            ColTrace* ctp = ctab + i;
            Tcl_UntraceVar2(Interp(), cur, Tcl_GetString(ctp->name),
                            TCL_TRACE_READS, cursor_tracer, (ClientData) ctp);
            DecObjRef(ctp->name);
        }
        ckfree((char*) ctab);
        if (e == TCL_OK)
            switch (type) {
                case eWHERE: case eINDEX: {
                    Column map = SeqAsCol(BufferAsIntVec(&result));
                    if (type == eWHERE)
                        Tcl_SetObjResult(Interp(), 
                                ViewAsObj(RemapSubview(li.v, map, 0, -1)));
                    else
                        Tcl_SetObjResult(Interp(), ColumnAsObj(map));
                    break;
                }
                case eCOLLECT: {
                    Tcl_SetObjResult(Interp(), BufferAsTclList(&result));
                    break;
                }
                default:
                    ReleaseBuffer(&result, 0);
            }
    }
    
    DecObjRef(li.cursor);
    return e;
}

ItemTypes LoopCmd_X (Item args[]) {
    int objc;
    const Object_p *objv;

    objv = (const Object_p *) args[0].u.ptr;
    objc = args[0].u.len;

    if (loop_vop(objc, objv) != TCL_OK)
        return IT_unknown;
        
    args->o = Tcl_GetObjResult(Interp());
    return IT_object;
}
/* %include<viewobj.c>% */
/*
 * viewobj.c - Implementation of view objects in Tcl.
 */


#define DEP_DEBUG 0

#define OBJ_TO_VIEW_P(o) ((o)->internalRep.twoPtrValue.ptr1)
#define OBJ_TO_ORIG_P(o) ((o)->internalRep.twoPtrValue.ptr2)

#define OBJ_TO_ORIG_CNT(o)  (((Seq_p) OBJ_TO_ORIG_P(o))->OR_objc)
#define OBJ_TO_ORIG_VEC(o)  ((Object_p*) ((Seq_p) OBJ_TO_ORIG_P(o) + 1))

#define OR_depx     data[0].o
#define OR_objc     data[1].i
#define OR_depc     data[2].i
#define OR_depv     data[3].p

/* forward */
static void SetupViewObj (Object_p, View_p, int objc, Object_p *objv, Object_p);
static void ClearViewObj (Object_p obj);

static void FreeViewIntRep (Tcl_Obj *obj) {
    PUSH_KEEP_REFS

    Assert(((Seq_p) OBJ_TO_ORIG_P(obj))->refs == 1);
    ClearViewObj(obj);
    
    DecRefCount(OBJ_TO_VIEW_P(obj));
    DecRefCount(OBJ_TO_ORIG_P(obj));
    
    POP_KEEP_REFS
}

static void DupViewIntRep (Tcl_Obj *src, Tcl_Obj *dup) {
    Assert(0);
}

static void UpdateViewStrRep (Tcl_Obj *obj) {
    int length;
    const char *string;
    Object_p origobj;
    PUSH_KEEP_REFS
    
    /* TODO: avoid creating a Tcl list and copying the string, use a buffer */
    origobj = Tcl_NewListObj(OBJ_TO_ORIG_CNT(obj), OBJ_TO_ORIG_VEC(obj));
    string = Tcl_GetStringFromObj(origobj, &length);

    obj->bytes = strcpy(ckalloc(length+1), string);
    obj->length = length;

    DecObjRef(origobj);

    POP_KEEP_REFS
}

static char *RefTracer (ClientData cd, Tcl_Interp *interp, const char *name1, const char *name2, int flags) {
    Object_p obj = (Object_p) cd;
    
#if DEBUG+0
    DbgIf(DBG_trace) printf("       rt %p var: %s\n", (void*) obj, name1);
#endif

    if (flags && TCL_TRACE_WRITES) {
        Tcl_UntraceVar2(Interp(), name1, name2,
                        TCL_TRACE_WRITES | TCL_TRACE_UNSETS | TCL_GLOBAL_ONLY,
                        RefTracer, obj);
    }

    InvalidateView(obj);
    DecObjRef(obj);
    return NULL;
}

static int SetViewFromAnyRep (Tcl_Interp *interp, Tcl_Obj *obj) {
    int e = TCL_ERROR, objc, rows;
    Object_p *objv;
    View_p view;
    
    if (Tcl_ListObjGetElements(interp, obj, &objc, &objv) != TCL_OK)
        return TCL_ERROR;

    PUSH_KEEP_REFS

    switch (objc) {

        case 0:
            view = EmptyMetaView();
            SetupViewObj(obj, view, objc, objv, NULL);
            break;

        case 1:
            if (Tcl_GetIntFromObj(interp, objv[0], &rows) == TCL_OK &&
                                                                rows >= 0) {
                view = NoColumnView(rows);
                SetupViewObj(obj, view, objc, objv, NULL);
            } else {
                Object_p o;
                const char *var = Tcl_GetString(objv[0]) + 1;
                
                if (var[-1] != '@')
                    goto FAIL;
                    
#if DEBUG+0
                DbgIf(DBG_trace) printf("svfar %p var: %s\n", (void*) obj, var);
#endif
                o = (Object_p) Tcl_VarTraceInfo(Interp(), var, TCL_GLOBAL_ONLY,
                                                            RefTracer, NULL);
                if (o == NULL) {
                    o = Tcl_GetVar2Ex(interp, var, NULL,
                                        TCL_LEAVE_ERR_MSG | TCL_GLOBAL_ONLY);
                    if (o == NULL)
                        goto FAIL;

                    o = NeedMutable(o);
                    Assert(o != NULL);
                    
                    if (Tcl_TraceVar2(interp, var, 0,
                            TCL_TRACE_WRITES|TCL_TRACE_UNSETS|TCL_GLOBAL_ONLY,
                                            RefTracer, IncObjRef(o)) != TCL_OK)
                        goto FAIL;
                }
                
                view = ObjAsView(o);
                Assert(view != NULL);

                SetupViewObj(obj, view, objc, objv, /*o*/ NULL);
            }
            
            break;

        default: {
            Object_p cmd;
            Tcl_SavedResult state;

            Assert(interp != NULL);
            Tcl_SaveResult(interp, &state);

            cmd = IncObjRef(Tcl_DuplicateObj(obj));
            view = NULL;

            /* def -> cmd namespace name conv can prob be avoided in Tcl 8.5 */
            if (AdjustCmdDef(cmd) == TCL_OK) {
                int ac;
                Object_p *av;
                Tcl_ListObjGetElements(NULL, cmd, &ac, &av);
                /* don't use Tcl_EvalObjEx, it forces a string conversion */
                if (Tcl_EvalObjv(interp, ac, av, TCL_EVAL_GLOBAL) == TCL_OK) {
                    /* result to view, may call EvalIndirectView recursively */
                    view = ObjAsView(Tcl_GetObjResult(interp));
                }
            }

            DecObjRef(cmd);

            if (view == NULL) {
                Tcl_DiscardResult(&state);
                goto FAIL;
            }

            SetupViewObj(obj, view, objc, objv, NULL);
            Tcl_RestoreResult(interp, &state);
        }
    }

    e = TCL_OK;

FAIL:
    POP_KEEP_REFS
    return e;
}

Tcl_ObjType f_viewObjType = {
    "view", FreeViewIntRep, DupViewIntRep, UpdateViewStrRep, SetViewFromAnyRep
};

View_p ObjAsView (Object_p obj) {
    if (Tcl_ConvertToType(Interp(), obj, &f_viewObjType) != TCL_OK)
        return NULL;
    return OBJ_TO_VIEW_P(obj);
}

static void AddDependency (Object_p obj, Object_p child) { 
    Object_p *deps;
    Seq_p origin = OBJ_TO_ORIG_P(obj);
    
#if DEBUG+0
    DbgIf(DBG_deps) printf(" +dep %p %p\n", (void*) obj, (void*) child);
#endif

    if (origin->OR_depv == NULL) 
        origin->OR_depv = calloc(10, sizeof(Object_p));
    
    Assert(origin->OR_depc < 10);
    deps = origin->OR_depv;
    deps[origin->OR_depc++] = child;
}

static void RemoveDependency (Object_p obj, Object_p child) {    
    Object_p *deps;
    Seq_p origin = OBJ_TO_ORIG_P(obj);
    
#if DEBUG+0
    DbgIf(DBG_deps) printf(" -dep %p %p\n", (void*) obj, (void*) child);
#endif

    deps = origin->OR_depv;
    Assert(deps != NULL);
    
    if (deps != NULL) {
        int i;

        /* removed from end in case we're called from the invalidation loop */
        for (i = origin->OR_depc; --i >= 0; )
            if (deps[i] == child)
                break;
        /*printf("  -> i %d n %d\n", i, origin->OR_depc);*/
        
        Assert(i < origin->OR_depc);
        deps[i] = deps[--origin->OR_depc];
            
        if (origin->OR_depc == 0) {
            free(deps);
            origin->OR_depv = NULL;
        }
    }
}

void InvalidateView (Object_p obj) {
    Object_p *deps;
    Seq_p origin = OBJ_TO_ORIG_P(obj);
    
#if DEP_DEBUG+0
    printf("inval %p deps %d refs %d\n",
                    (void*) obj, origin->OR_depc, obj->refCount);
#endif

    if (obj->typePtr != &f_viewObjType)
        return;
    
    deps = origin->OR_depv;
    if (deps != NULL) {
        /* careful, RemoveDependency will remove the item during iteration */
        while (origin->OR_depc > 0) {
            int last = origin->OR_depc - 1;
            InvalidateView(deps[last]);
            Assert(origin->OR_depc <= last); /* make sure count went down */
        }
        Assert(origin->OR_depv == NULL);
    }

#if 1
    Assert(obj->typePtr == &f_viewObjType);
    Tcl_GetString(obj);
    FreeViewIntRep(obj);
    obj->typePtr = NULL;
#else
    /* FIXME: hack to allow changes to list, even when the object is shared! */
    { Object_p nobj = Tcl_NewListObj(OBJ_TO_ORIG_CNT(obj),
                                        OBJ_TO_ORIG_VEC(obj));
        DecRefCount(OBJ_TO_VIEW_P(obj));
        DecRefCount(origin);
        obj->typePtr = nobj->typePtr;
        obj->internalRep = nobj->internalRep;
        nobj->typePtr = NULL;
        DecObjRef(nobj);
        /*Tcl_InvalidateStringRep(obj);*/
    }
#endif
}

#if 0
static void OriginCleaner (Seq_p seq) {
    int i, items = seq->OR_objc;
    const Object_p *objv = (const void*) (seq + 1);

    for (i = 0; i < items; ++i)
        DecObjRef(objv[i]);

    Assert(seq->OR_depc == 0);
    Assert(seq->OR_depv == NULL);
}

static ItemTypes OriginGetter (int row, Item_p item) {
    const Object_p *objv = (const void*) (item->c.seq + 1);
    item->o = objv[row];
    return IT_object;
}
#endif

static struct SeqType ST_Origin = { "origin", NULL, 0, NULL };

static void SetupViewObj (Object_p obj, View_p view, int objc, Object_p *objv, Object_p extradep) {
    int i;
    Object_p *vals;
    Seq_p origin;
    
    origin = NewSequenceNoRef(objc, &ST_Origin, objc * sizeof(Object_p));
    /* data[0] is a pointer to the reference for "@..." references */
    /* data[1] has the number of extra bytes, i.e. objc * sizeof (Seq_p) */
    /* data[2] is the dependency count */
    /* data[3] is the dependency vector if data[2].i > 0 */
    origin->OR_depx = extradep;
    origin->OR_objc = objc;
    vals = (void*) (origin + 1);
    
    for (i = 0; i < objc; ++i)
        vals[i] = IncObjRef(objv[i]);

    if (obj->typePtr != NULL && obj->typePtr->freeIntRepProc != NULL)
        obj->typePtr->freeIntRepProc(obj);
    
    OBJ_TO_VIEW_P(obj) = IncRefCount(view);
    OBJ_TO_ORIG_P(obj) = IncRefCount(origin);
    obj->typePtr = &f_viewObjType;
    
    for (i = 0; i < objc; ++i)
        if (vals[i]->typePtr == &f_viewObjType)
            AddDependency(vals[i], obj);
            
    if (extradep != NULL)
        AddDependency(extradep, obj);
}

static void ClearViewObj (Object_p obj) {
    int i, objc;
    Object_p *objv;
    Seq_p origin = OBJ_TO_ORIG_P(obj);
    
    /* can't have any child dependencies at this stage */
    Assert(origin->OR_depc == 0);
    Assert(origin->OR_depv == NULL);

    objc = OBJ_TO_ORIG_CNT(obj);
    objv = OBJ_TO_ORIG_VEC(obj);
    
    for (i = 0; i < objc; ++i) {
        if (objv[i]->typePtr == &f_viewObjType)
            RemoveDependency(objv[i], obj);
        DecObjRef(objv[i]);
    }
        
    if (origin->OR_depx != NULL)
        RemoveDependency(origin->OR_depx, obj);
}

static Object_p MetaViewAsObj (View_p meta) {
    char typeCh;
    int width, rowNum;
    Object_p result, fieldobj;
    View_p subv;
    Column names, types, subvs;

    names = ViewCol(meta, MC_name);
    types = ViewCol(meta, MC_type);
    subvs = ViewCol(meta, MC_subv);

    result = Tcl_NewListObj(0, NULL);

    width = ViewSize(meta);
    for (rowNum = 0; rowNum < width; ++rowNum) {
        fieldobj = Tcl_NewStringObj(GetColItem(rowNum, names, IT_string).s, -1);

        /* this ignores all but the first character of the type */
        typeCh = *GetColItem(rowNum, types, IT_string).s;

        switch (typeCh) {

            case 'S':
                break;

            case 'V':
                subv = GetColItem(rowNum, subvs, IT_view).v;
                fieldobj = Tcl_NewListObj(1, &fieldobj);
                Tcl_ListObjAppendElement(NULL, fieldobj, MetaViewAsObj(subv));
                break;

            default:
                Tcl_AppendToObj(fieldobj, ":", 1);
                Tcl_AppendToObj(fieldobj, &typeCh, 1);
                break;
        }

        Tcl_ListObjAppendElement(NULL, result, fieldobj);
    }

    return result;
}

Object_p ViewAsObj (View_p view) {
    View_p meta = V_Meta(view);
    int c, rows = ViewSize(view), cols = ViewSize(meta);
    Object_p result;
    struct Buffer buffer;
    
    InitBuffer(&buffer);

    if (meta == V_Meta(meta)) {
        if (rows > 0) {
            ADD_PTR_TO_BUF(buffer, Tcl_NewStringObj("mdef", 4));
            ADD_PTR_TO_BUF(buffer, MetaViewAsObj(view));
        }
    } else {
        if (cols == 0) {
            ADD_PTR_TO_BUF(buffer, Tcl_NewIntObj(rows));
        } else {
            ADD_PTR_TO_BUF(buffer, Tcl_NewStringObj("data", 4));
            ADD_PTR_TO_BUF(buffer, ViewAsObj(meta));
            for (c = 0; c < cols; ++c)
                ADD_PTR_TO_BUF(buffer, ColumnAsObj(ViewCol(view, c)));
        }
    }

    result = Tcl_NewObj();
    Tcl_InvalidateStringRep(result);
    
    SetupViewObj(result, view, BufferFill(&buffer) / sizeof(Object_p),
                                            BufferAsPtr(&buffer, 1), NULL);
    
    ReleaseBuffer(&buffer, 0);
    return result;
}

View_p ObjAsMetaView (Object_p obj) {
    int r, rows, objc;
    Object_p names, types, subvs, *objv, entry, nameobj, subvobj;
    const char *name, *sep, *type;
    View_p view = NULL;

    if (Tcl_ListObjLength(Interp(), obj, &rows) != TCL_OK)
        return NULL;

    names = Tcl_NewListObj(0, 0);
    types = Tcl_NewListObj(0, 0);
    subvs = Tcl_NewListObj(0, 0);

    for (r = 0; r < rows; ++r) {
        Tcl_ListObjIndex(NULL, obj, r, &entry);
        if (Tcl_ListObjGetElements(Interp(), entry, &objc, &objv) != TCL_OK ||
                objc < 1 || objc > 2)
            goto DONE;

        name = Tcl_GetString(objv[0]);
        sep = strchr(name, ':');
        type = objc > 1 ? "V" : "S";

        if (sep != NULL) {
            if (sep[1] != 0) {
                if (strchr("BDFLISV", sep[1]) == NULL)
                    goto DONE;
                type = sep+1;
            }
            nameobj= Tcl_NewStringObj(name, sep - name);
        } else
            nameobj = objv[0];

        if (objc > 1) {
            view = ObjAsMetaView(objv[1]);
            if (view == NULL)
                goto DONE;
            subvobj = ViewAsObj(view);
        } else
            subvobj = Tcl_NewObj();

        Tcl_ListObjAppendElement(NULL, names, nameobj);
        Tcl_ListObjAppendElement(NULL, types, Tcl_NewStringObj(type, -1));
        Tcl_ListObjAppendElement(NULL, subvs, subvobj);
    }

    view = NewView(V_Meta(EmptyMetaView()));
    SetViewCols(view, MC_name, 1, CoerceColumn(IT_string, names));
    SetViewCols(view, MC_type, 1, CoerceColumn(IT_string, types));
    SetViewCols(view, MC_subv, 1, CoerceColumn(IT_view, subvs));
    
DONE:
    DecObjRef(names);
    DecObjRef(types);
    DecObjRef(subvs);
    return view;
}

Object_p NeedMutable (Object_p obj) {
    int objc;
    Object_p *objv;
    View_p view;

    view = ObjAsView(obj);
    if (view == NULL)
        return NULL;
    
    if (IsMutable(view))
        return obj;
    
    objc = OBJ_TO_ORIG_CNT(obj);
    objv = OBJ_TO_ORIG_VEC(obj);
    
    if (objc == 1 && *Tcl_GetString(objv[0]) == '@') {
        const char *var = Tcl_GetString(objv[0]) + 1;
        Object_p o = (Object_p) Tcl_VarTraceInfo(Interp(), var,
                                            TCL_GLOBAL_ONLY, RefTracer, NULL);
        Assert(o != NULL);
        return o;
    }
    
    if (objc > 1 && strcmp(Tcl_GetString(objv[0]), "get") == 0) {
        Object_p origdef, tmp = NeedMutable(objv[1]); /* recursive */
        Assert(tmp != NULL);
        
        origdef = Tcl_NewListObj(objc, objv);
        Tcl_ListObjReplace(NULL, origdef, 1, 1, 1, &tmp);
        return origdef;
    }
    
    return obj;
}

static void ListOneDep (Buffer_p buf, Object_p obj) {
    Object_p o, vec[3];
    
    if (obj->typePtr == &f_viewObjType) {
        View_p view = OBJ_TO_VIEW_P(obj);
        Seq_p seq = ViewCol(view, 0).seq;
        vec[0] = Tcl_NewStringObj(seq != NULL ? seq->type->name : "", -1);
        vec[1] = MetaViewAsObj(V_Meta(view));
        vec[2] = Tcl_NewIntObj(ViewSize(view));
        o = Tcl_NewListObj(3, vec);
    } else if (Tcl_ListObjIndex(NULL, obj, 0, &o) != TCL_OK)
        o = Tcl_NewStringObj("?", 1);

    ADD_PTR_TO_BUF(*buf, o);
}

ItemTypes DepsCmd_O (Item_p a) {
    int i;
    Object_p obj, *objv;
    Seq_p origin;
    struct Buffer buffer;

    obj = a[0].o;
    ObjAsView(obj);
    
    if (obj->typePtr != &f_viewObjType)
        return IT_unknown;
    
    InitBuffer(&buffer);
    
    origin = OBJ_TO_ORIG_P(obj);
    objv = OBJ_TO_ORIG_VEC(obj);
    
    if (origin->OR_depx != NULL)
        ListOneDep(&buffer, origin->OR_depx);
    else
        ADD_PTR_TO_BUF(buffer, Tcl_NewStringObj("-", 1));
    
    for (i = 0; i < origin->OR_depc; ++i)
        ListOneDep(&buffer, objv[i]);
    
    a->o = BufferAsTclList(&buffer);
    return IT_object;
}
/* %include<ext_tcl.c>% */
/*
 * ext_tcl.c - Interface to the Tcl scripting language.
 */


#ifndef PACKAGE_VERSION
#define PACKAGE_VERSION "4"
#endif

/* stub interface code, removes the need to link with libtclstub*.a */
#if STATIC_BUILD+0
#define MyInitStubs(x) 1
#else
#ifdef FAKE_TCL_STUBS
/*
 * stubs.h - Internal stub code, adapted from CritLib
 */

CONST86 TclStubs               *tclStubsPtr        = NULL;
CONST86 TclPlatStubs           *tclPlatStubsPtr    = NULL;
struct TclIntStubs     *tclIntStubsPtr     = NULL;
struct TclIntPlatStubs *tclIntPlatStubsPtr = NULL;

static int MyInitStubs (Tcl_Interp *ip) {

    typedef struct HeadOfInterp {
        char                 *result;
        Tcl_FreeProc *freeProc;
        int                     errorLine;
        TclStubs         *stubTable;
    } HeadOfInterp;

    HeadOfInterp *hoi = (HeadOfInterp*) ip;

    if (hoi->stubTable == NULL || hoi->stubTable->magic != TCL_STUB_MAGIC) {
        ip->result = "This extension requires stubs-support.";
        ip->freeProc = TCL_STATIC;
        return 0;
    }

    tclStubsPtr = hoi->stubTable;

    if (Tcl_PkgRequire(ip, "Tcl", "8.1", 0) == NULL) {
        tclStubsPtr = NULL;
        return 0;
    }

    if (tclStubsPtr->hooks != NULL) {
            tclPlatStubsPtr = tclStubsPtr->hooks->tclPlatStubs;
            tclIntStubsPtr = tclStubsPtr->hooks->tclIntStubs;
            tclIntPlatStubsPtr = tclStubsPtr->hooks->tclIntPlatStubs;
    }

    return 1;
}
#else /* !FAKE_TCL_STUBS */
#define MyInitStubs(ip) Tcl_InitStubs((ip), "8.4", 0)
#endif /* FAKE_TCL_STUBS */
#endif /* !STATIC_BUILD */

#if NO_THREAD_CALLS+0
Shared_p GetShared (void) {
    static struct Shared data;
    return &data;
}
#define UngetShared Tcl_CreateExitHandler
#else
static Tcl_ThreadDataKey tls_data;
Shared_p GetShared (void) {
    return (Shared_p) Tcl_GetThreadData(&tls_data, sizeof(struct Shared));
}
#define UngetShared Tcl_CreateThreadExitHandler
#endif

typedef struct CmdDispatch {
    const char *name, *args;
    ItemTypes (*proc) (Item_p);
} CmdDispatch;

static CmdDispatch f_commands[] = {
/* opdefs_gen.h - generated code, do not edit */

/* 94 definitions generated for Tcl:

  # name       in     out    inline-code
  : drop       U      {}     {}
  : dup        U      UU     {$1 = $0;}
  : imul       II     I      {$0.i *=  $1.i;}
  : nip        UU     U      {$0 = $1;}
  : over       UU     UUU    {$2 = $0;}
  : rot        UUU    UUU    {$3 = $0; $0 = $1; $1 = $2; $2 = $3;}
  : rrot       UUU    UUU    {$3 = $2; $2 = $1; $1 = $0; $0 = $3;}
  : swap       UU     UU     {$2 = $0; $0 = $1; $1 = $2;}
  : tuck       UU     UUU    {$1 = $2; $0 = $1; $2 = $0;}
  
  tcl {
    # name       in    out
    : Def        OO     V
    : Deps       O      O
    : Emit       V      O
    : EmitMods   V      O
    : Get        VX     U
    : Load       O      V
    : LoadMods   OV     V
    : Loop       X      O
    : MutInfo    V      O
    : Ref        OX     O
    : Refs       O      I
    : Rename     VO     V
    : Save       VS     I
    : To         OO     O
    : Type       O      O
    : View       X      O
  }
  
  python {
    # name       in    out
  }
  
  ruby {
    # name       in    out
    : AtRow      OI     O
  }
  
  lua {
    # name       in    out
  }
  
  objc {
    # name       in    out
    : At         VIO    O
  }
  
  # name       in    out
  : BitRuns    i      C
  : Data       VX     V
  : Debug      I      I
  : HashCol    SO     C
  : Max        VN     O
  : Min        VN     O
  : Open       S      V
  : ResizeCol  iII    C
  : Set        MIX    V
  : StructDesc V      S
  : Structure  V      S
  : Sum        VN     O
  : Write      VO     I

  # name       in    out  internal-name
  : Blocked    V      V   BlockedView
  : Clone      V      V   CloneView
  : ColMap     Vn     V   ColMapView
  : ColOmit    Vn     V   ColOmitView
  : Coerce     OS     C   CoerceCmd
  : Compare    VV     I   ViewCompare
  : Compat     VV     I   ViewCompat
  : Concat     VV     V   ConcatView
  : CountsCol  C      C   NewCountsColumn
  : CountView  I      V   NoColumnView
  : First      VI     V   FirstView
  : GetCol     VN     C   ViewCol
  : Group      VnS    V   GroupCol
  : HashFind   VIViii I   HashDoFind
  : Ijoin      VV     V   IjoinView
  : GetInfo    VVI    C   GetHashInfo
  : Grouped    ViiS   V   GroupedView
  : HashView   V      C   HashValues
  : IsectMap   VV     C   IntersectMap
  : Iota       I      C   NewIotaColumn
  : Join       VVS    V   JoinView
  : Last       VI     V   LastView
  : Mdef       O      V   ObjAsMetaView
  : Mdesc      S      V   DescToMeta
  : Meta       V      V   V_Meta
  : OmitMap    iI     C   OmitColumn
  : OneCol     VN     V   OneColView
  : Pair       VV     V   PairView
  : RemapSub   ViII   V   RemapSubview
  : Replace    VIIV   V   ViewReplace
  : RowCmp     VIVI   I   RowCompare
  : RowEq      VIVI   I   RowEqual
  : RowHash    VI     I   RowHash
  : Size       V      I   ViewSize
  : SortMap    V      C   SortMap
  : Step       VIIII  V   StepView
  : StrLookup  Ss     I   StringLookup
  : Tag        VS     V   TagView
  : Take       VI     V   TakeView
  : Ungroup    VN     V   UngroupView
  : UniqueMap  V      C   UniqMap
  : ViewAsCol  V      C   ViewAsCol
  : Width      V      I   ViewWidth
               
  # name       in    out  compound-definition
  : Append     VV     V   {over size swap insert}
  : ColConv    C      C   { }
  : Counts     VN     C   {getcol countscol}
  : Delete     VII    V   {0 countview replace}
  : Except     VV     V   {over swap exceptmap remap}
  : ExceptMap  VV     C   {over swap isectmap swap size omitmap}
  : Insert     VIV    V   {0 swap replace}
  : Intersect  VV     V   {over swap isectmap remap}
  : NameCol    V      V   {meta 0 onecol}
  : Names      V      C   {meta 0 getcol}
  : Product    VV     V   {over over size spread rrot swap size repeat pair}
  : Repeat     VI     V   {over size imul 0 1 1 step}
  : Remap      Vi     V   {0 -1 remapsub}
  : Reverse    V      V   {dup size -1 1 -1 step}
  : Slice      VIII   V   {rrot 1 swap step}
  : Sort       V      V   {dup sortmap remap}
  : Spread     VI     V   {over size 0 rot 1 step}
  : Types      V      C   {meta 1 getcol}
  : Unique     V      V   {dup uniquemap remap}
  : Union      VV     V   {over except concat}
  : UnionMap   VV     C   {swap exceptmap}
  : ViewConv   V      V   { }
  
  # some operators are omitted from restricted execution environments
  unsafe Open Save
*/

  { "open",       "V:S",      OpenCmd_S          },
  { "save",       "I:VS",     SaveCmd_VS         },
#define UNSAFE 2
  { "append",     "V:VV",     AppendCmd_VV       },
  { "bitruns",    "C:i",      BitRunsCmd_i       },
  { "blocked",    "V:V",      BlockedCmd_V       },
  { "clone",      "V:V",      CloneCmd_V         },
  { "coerce",     "C:OS",     CoerceCmd_OS       },
  { "colconv",    "C:C",      ColConvCmd_C       },
  { "colmap",     "V:Vn",     ColMapCmd_Vn       },
  { "colomit",    "V:Vn",     ColOmitCmd_Vn      },
  { "compare",    "I:VV",     CompareCmd_VV      },
  { "compat",     "I:VV",     CompatCmd_VV       },
  { "concat",     "V:VV",     ConcatCmd_VV       },
  { "counts",     "C:VN",     CountsCmd_VN       },
  { "countscol",  "C:C",      CountsColCmd_C     },
  { "countview",  "V:I",      CountViewCmd_I     },
  { "data",       "V:VX",     DataCmd_VX         },
  { "debug",      "I:I",      DebugCmd_I         },
  { "def",        "V:OO",     DefCmd_OO          },
  { "delete",     "V:VII",    DeleteCmd_VII      },
  { "deps",       "O:O",      DepsCmd_O          },
  { "emit",       "O:V",      EmitCmd_V          },
  { "emitmods",   "O:V",      EmitModsCmd_V      },
  { "except",     "V:VV",     ExceptCmd_VV       },
  { "exceptmap",  "C:VV",     ExceptMapCmd_VV    },
  { "first",      "V:VI",     FirstCmd_VI        },
  { "get",        "U:VX",     GetCmd_VX          },
  { "getcol",     "C:VN",     GetColCmd_VN       },
  { "getinfo",    "C:VVI",    GetInfoCmd_VVI     },
  { "group",      "V:VnS",    GroupCmd_VnS       },
  { "grouped",    "V:ViiS",   GroupedCmd_ViiS    },
  { "hashcol",    "C:SO",     HashColCmd_SO      },
  { "hashfind",   "I:VIViii", HashFindCmd_VIViii },
  { "hashview",   "C:V",      HashViewCmd_V      },
  { "ijoin",      "V:VV",     IjoinCmd_VV        },
  { "insert",     "V:VIV",    InsertCmd_VIV      },
  { "intersect",  "V:VV",     IntersectCmd_VV    },
  { "iota",       "C:I",      IotaCmd_I          },
  { "isectmap",   "C:VV",     IsectMapCmd_VV     },
  { "join",       "V:VVS",    JoinCmd_VVS        },
  { "last",       "V:VI",     LastCmd_VI         },
  { "load",       "V:O",      LoadCmd_O          },
  { "loadmods",   "V:OV",     LoadModsCmd_OV     },
  { "loop",       "O:X",      LoopCmd_X          },
  { "max",        "O:VN",     MaxCmd_VN          },
  { "mdef",       "V:O",      MdefCmd_O          },
  { "mdesc",      "V:S",      MdescCmd_S         },
  { "meta",       "V:V",      MetaCmd_V          },
  { "min",        "O:VN",     MinCmd_VN          },
  { "mutinfo",    "O:V",      MutInfoCmd_V       },
  { "namecol",    "V:V",      NameColCmd_V       },
  { "names",      "C:V",      NamesCmd_V         },
  { "omitmap",    "C:iI",     OmitMapCmd_iI      },
  { "onecol",     "V:VN",     OneColCmd_VN       },
  { "pair",       "V:VV",     PairCmd_VV         },
  { "product",    "V:VV",     ProductCmd_VV      },
  { "ref",        "O:OX",     RefCmd_OX          },
  { "refs",       "I:O",      RefsCmd_O          },
  { "remap",      "V:Vi",     RemapCmd_Vi        },
  { "remapsub",   "V:ViII",   RemapSubCmd_ViII   },
  { "rename",     "V:VO",     RenameCmd_VO       },
  { "repeat",     "V:VI",     RepeatCmd_VI       },
  { "replace",    "V:VIIV",   ReplaceCmd_VIIV    },
  { "resizecol",  "C:iII",    ResizeColCmd_iII   },
  { "reverse",    "V:V",      ReverseCmd_V       },
  { "rowcmp",     "I:VIVI",   RowCmpCmd_VIVI     },
  { "roweq",      "I:VIVI",   RowEqCmd_VIVI      },
  { "rowhash",    "I:VI",     RowHashCmd_VI      },
  { "set",        "V:MIX",    SetCmd_MIX         },
  { "size",       "I:V",      SizeCmd_V          },
  { "slice",      "V:VIII",   SliceCmd_VIII      },
  { "sort",       "V:V",      SortCmd_V          },
  { "sortmap",    "C:V",      SortMapCmd_V       },
  { "spread",     "V:VI",     SpreadCmd_VI       },
  { "step",       "V:VIIII",  StepCmd_VIIII      },
  { "strlookup",  "I:Ss",     StrLookupCmd_Ss    },
  { "structdesc", "S:V",      StructDescCmd_V    },
  { "structure",  "S:V",      StructureCmd_V     },
  { "sum",        "O:VN",     SumCmd_VN          },
  { "tag",        "V:VS",     TagCmd_VS          },
  { "take",       "V:VI",     TakeCmd_VI         },
  { "to",         "O:OO",     ToCmd_OO           },
  { "type",       "O:O",      TypeCmd_O          },
  { "types",      "C:V",      TypesCmd_V         },
  { "ungroup",    "V:VN",     UngroupCmd_VN      },
  { "union",      "V:VV",     UnionCmd_VV        },
  { "unionmap",   "C:VV",     UnionMapCmd_VV     },
  { "unique",     "V:V",      UniqueCmd_V        },
  { "uniquemap",  "C:V",      UniqueMapCmd_V     },
  { "view",       "O:X",      ViewCmd_X          },
  { "viewascol",  "C:V",      ViewAsColCmd_V     },
  { "viewconv",   "V:V",      ViewConvCmd_V      },
  { "width",      "I:V",      WidthCmd_V         },
  { "write",      "I:VO",     WriteCmd_VO        },

/* end of generated code */
    { NULL, NULL, NULL }
};

Object_p IncObjRef (Object_p objPtr) {
    Tcl_IncrRefCount(objPtr);
    return objPtr;
}

Object_p BufferAsTclList (Buffer_p bp) {
    int argc;
    Object_p result;
    
    argc = BufferFill(bp) / sizeof(Object_p);
    result = Tcl_NewListObj(argc, BufferAsPtr(bp, 1));
    ReleaseBuffer(bp, 0);
    return result;
}

void FailedAssert (const char *msg, const char *file, int line) {
    Tcl_Panic("Failed assertion at %s, line %d: %s\n", file, line, msg);
}

ItemTypes RefCmd_OX (Item args[]) {
    int objc;
    const Object_p *objv;
    
    objv = (const Object_p *) args[1].u.ptr;
    objc = args[1].u.len;

    args->o = Tcl_ObjGetVar2(Interp(), args[0].o, 0,
                                TCL_GLOBAL_ONLY | TCL_LEAVE_ERR_MSG);
    return IT_object;
}

ItemTypes OpenCmd_S (Item args[]) {
    View_p result;
    Tcl_DString ds;
    char *native;

    /*
     * When providing a database for a tclkit, we have here a "chicken
     * and egg" problem with encodings: a call to this code will
     * result from invoking the "open" script-level Vlerq command, and
     * thus it will be passed an UTF-8 encoded filename.  In the case
     * of tclkits this filename is obtained via a call to [info
     * nameofexecutable], and at the time of that call [encoding
     * system] is "identity" because the encodings subsystem can be
     * initialized only after the tclkit VFS is mounted, and its
     * mounting is what this very function is called for.
     *
     * On Windows, when converting between "system native" characters
     * and UTF-8 (and vice-versa) two encodings may be used:
     * - [encoding system] on Win9x/ME
     * - "unicode" encoding on Windows NT.
     * The "unicode" encoding is built-in, and [encoding system] will
     * only have correct value on Latin-1 and UTF-8 systems,
     * on others it will be "identity".
     *
     * This means when the tclkit is started from a folder whose name
     * contains non-latin-1 characters, on Windows NT this function
     * will receive correct UTF-8 string and the call to
     * Tcl_WinUtfToTChar() below will return proper array of WCHARs.
     * On Win95/ME and Unices the behaviour depends on whether the
     * system encoding happens to be UTF-8 or any variant of Latin-1;
     * if it is, this function will receive a proper UTF-8 string and
     * the call to Tcl_WinUtfToTChar()/Tcl_UtfToExternalDString()
     * below will produce a proper "native" string back. Otherwise,
     * [encoding system] will be "identity" and due to the way it
     * works this function will receive a string containing exactly
     * the bytes returned by some relevant system call, which [info
     * nameofexecutable] performed and the call to
     * Tcl_WinUtfToTChar()/Tcl_UtfToExternalDString() will produce an
     * array of char with the same bytes.  It this case startup from a
     * folder containing non-latin-1 characters will fail.
     */

#if WIN32+0
    native = Tcl_WinUtfToTChar(args[0].s, strlen(args[0].s), &ds);
#else
    native = Tcl_UtfToExternalDString(NULL,
         args[0].s, strlen(args[0].s), &ds);
#endif
    result = OpenDataFile(native);
    Tcl_DStringFree(&ds);
    if (result == NULL) {
        Tcl_AppendResult(Interp(), "cannot open file: ", args[0].s, NULL);
        return IT_unknown;
    }
    
    args->v = result;
    return IT_view;
}

static void LoadedStringCleaner (MappedFile_p map) {
    DecObjRef((Object_p) map->data[3].p);
}

ItemTypes LoadCmd_O (Item args[]) {
    int len;
    const char *ptr;
    View_p view;
    MappedFile_p map;
    
    ptr = (const char*) Tcl_GetByteArrayFromObj(args[0].o, &len);

    map = InitMappedFile(ptr, len, LoadedStringCleaner);
    map->data[3].p = IncObjRef(args[0].o);
    
    view = MappedToView(map, NULL);
    if (view == NULL)
        return IT_unknown;
        
    args->v = view;
    return IT_view;
}

ItemTypes LoadModsCmd_OV (Item args[]) {
    int len;
    const char *ptr;
    View_p view;
    MappedFile_p map;
    
    ptr = (const char*) Tcl_GetByteArrayFromObj(args[0].o, &len);

    map = InitMappedFile(ptr, len, LoadedStringCleaner);
    map->data[3].p = IncObjRef(args[0].o);
    
    view = MappedToView(map, args[1].v);
    if (view == NULL)
        return IT_unknown;
        
    args->v = view;
    return IT_view;
}

    static void *WriteDataFun(void *chan, const void *ptr, Int_t len) {
        Tcl_Write(chan, ptr, len);
        return chan;
    }

ItemTypes SaveCmd_VS (Item args[]) {
    Int_t bytes;
    Tcl_Channel chan;
    
    chan = Tcl_OpenFileChannel(Interp(), args[1].s, "w", 0666);
    if (chan == NULL || Tcl_SetChannelOption(Interp(), chan,
                                            "-translation", "binary") != TCL_OK)
        return IT_unknown;
        
    bytes = ViewSave(args[0].v, chan, NULL, WriteDataFun, 0);

    Tcl_Close(Interp(), chan);

    if (bytes < 0)
        return IT_unknown;
        
    args->w = bytes;
    return IT_wide;
}

ItemTypes WriteCmd_VO (Item args[]) {
    Int_t bytes;
    Tcl_Channel chan;
    
    chan = Tcl_GetChannel(Interp(), Tcl_GetString(args[1].o), NULL);
    if (chan == NULL || Tcl_SetChannelOption(Interp(), chan,
                                            "-translation", "binary") != TCL_OK)
        return IT_unknown;
        
    bytes = ViewSave(args[0].v, chan, NULL, WriteDataFun, 0);
    
    if (bytes < 0)
        return IT_unknown;
        
    args->w = bytes;
    return IT_wide;
}

#define EmitInitFun ((SaveInitFun) Tcl_SetByteArrayLength)

    static void *EmitDataFun(void *data, const void *ptr, Int_t len) {
        memcpy(data, ptr, len);
        return (char*) data + len;
    }

ItemTypes EmitCmd_V (Item args[]) {
    Object_p result = Tcl_NewByteArrayObj(NULL, 0);
        
    if (ViewSave(args[0].v, result, EmitInitFun, EmitDataFun, 0) < 0) {
        DecObjRef(result);
        return IT_unknown;
    }
        
    args->o = result;
    return IT_object;
}

ItemTypes EmitModsCmd_V (Item args[]) {
    Object_p result = Tcl_NewByteArrayObj(NULL, 0);
        
    if (ViewSave(args[0].v, result, EmitInitFun, EmitDataFun, 1) < 0) {
        DecObjRef(result);
        return IT_unknown;
    }
        
    args->o = result;
    return IT_object;
}

int AdjustCmdDef (Object_p cmd) {
    Object_p origname, newname;
    Tcl_CmdInfo cmdinfo;

    /* Use "::vlerq::blah ..." if it exists, else use "vlerq blah ...". */
    /* Could perhaps be simplified (optimized?) by using 8.5 ensembles. */
     
    if (Tcl_ListObjIndex(Interp(), cmd, 0, &origname) != TCL_OK)
        return TCL_ERROR;

    /* insert "::vlerq::" before the first list element */
    newname = Tcl_NewStringObj("::vlerq::", -1);
    Tcl_AppendObjToObj(newname, origname);
    
    if (Tcl_GetCommandInfo(Interp(), Tcl_GetString(newname), &cmdinfo))
        Tcl_ListObjReplace(NULL, cmd, 0, 1, 1, &newname);
    else {
        Object_p buf[2];
        DecObjRef(newname);
        buf[0] = Tcl_NewStringObj("vlerq", -1);
        buf[1] = origname;
        Tcl_ListObjReplace(NULL, cmd, 0, 1, 2, buf);
    }
    return TCL_OK;
}

ItemTypes ViewCmd_X (Item args[]) {
    int e = TCL_OK, i, n, index, objc;
    const Object_p *objv;
    Object_p result, buf[10], *cvec;
    Tcl_Interp *interp = Interp();
    const CmdDispatch *cmds = GetShared()->info->cmds;

    objv = (const Object_p *) args[0].u.ptr;
    objc = args[0].u.len;

    if (objc < 1) {
        Tcl_WrongNumArgs(interp, 0, objv, "view arg ?op ...? ?| ...?");
        return IT_unknown;
    }
    
    Tcl_SetObjResult(interp, objv[0]); --objc; ++objv;
    
    while (e == TCL_OK && objc > 0) {
        for (n = 0; n < objc; ++n)
            if (objv[n]->bytes != 0 && *objv[n]->bytes == '|' && 
                    objv[n]->length == 1)
                break;

        if (n > 0) {
            cvec = n > 8 ? malloc((n+2) * sizeof(Object_p)) : buf;
                
            if (Tcl_GetIndexFromObjStruct(NULL, *objv, cmds, sizeof *cmds, 
                                            "", TCL_EXACT, &index) != TCL_OK)
                index = -1;

            cvec[0] = Tcl_NewStringObj("vlerq", -1);
            cvec[1] = objv[0];
            cvec[2] = IncObjRef(Tcl_GetObjResult(interp));
            for (i = 1; i < n; ++i)
                cvec[i+2] = objv[i];
            
            result = Tcl_NewListObj(n+1, cvec+1);

            if (index < 0 || *cmds[index].args != 'V') {
                e = AdjustCmdDef(result);
                if (e == TCL_OK) {
                    int ac;
                    Object_p *av;
                    Tcl_ListObjGetElements(NULL, result, &ac, &av);
                    /* don't use Tcl_EvalObjEx, it forces a string conversion */
                    e = Tcl_EvalObjv(interp, ac, av, 0);
                }
                DecObjRef(result);
            } else
                Tcl_SetObjResult(interp, result);
            
            DecObjRef(cvec[2]);
            if (n > 8)
                free(cvec);
        }
        
        objc -= n+1; objv += n+1; /*++k;*/
    }

    if (e != TCL_OK) {
#if 0
        char msg[50];
        sprintf(msg, "\n        (\"view\" step %d)", k);
        Tcl_AddObjErrorInfo(interp, msg, -1);
#endif
        return IT_unknown;
    }
    
    args->o = Tcl_GetObjResult(interp);
    return IT_object;
}

#define MAX_STACK 20

static int VlerqObjCmd (ClientData data, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]) {
    int i, index, ok = TCL_ERROR;
    ItemTypes type;
    Object_p result;
    Item stack [MAX_STACK];
    const char *args;
    const CmdDispatch *cmdtable;
    PUSH_KEEP_REFS
    
    Interp() = interp;
    
    if (objc <= 1) {
        Tcl_WrongNumArgs(interp, 1, objv, "command ...");
        goto FAIL;
    }

    cmdtable = (const CmdDispatch*) data;
    if (Tcl_GetIndexFromObjStruct(interp, objv[1], cmdtable, sizeof *cmdtable, 
                                    "command", TCL_EXACT, &index) != TCL_OK)
        goto FAIL;

    objv += 2; objc -= 2;
    args = cmdtable[index].args + 2; /* skip return type and ':' */

    for (i = 0; args[i] != 0; ++i) {
        Assert(i < MAX_STACK);
        if (args[i] == 'X') {
            Assert(args[i+1] == 0);
            stack[i].u.ptr = (const void*) (objv+i);
            stack[i].u.len = objc-i;
            break;
        }
        if ((args[i] == 0 && i != objc) || (args[i] != 0 && i >= objc)) {
            char buf [10*MAX_STACK];
            const char* s;
            *buf = 0;
            for (i = 0; args[i] != 0; ++i) {
                if (*buf != 0)
                    strcat(buf, " ");
                switch (args[i] & ~0x20) {
                    case 'C': s = "list"; break;
                    case 'I': s = "int"; break;
                    case 'N': s = "col"; break;
                    case 'O': s = "any"; break;
                    case 'S': s = "string"; break;
                    case 'V': s = "view"; break;
                    case 'X': s = "..."; break;
                    default: Assert(0); s = "?"; break;
                }
                strcat(buf, s);
                if (args[i] & 0x20)
                    strcat(buf, "*");
            }
            Tcl_WrongNumArgs(interp, 2, objv-2, buf);
            goto FAIL;
        }
        stack[i].o = objv[i];
        if (!CastObjToItem(args[i], stack+i)) {
            if (*Tcl_GetStringResult(interp) == 0) {
                const char* s = "?";
                switch (args[i] & ~0x20) {
                    case 'C': s = "list"; break;
                    case 'I': s = "integer"; break;
                    case 'N': s = "column name"; break;
                    case 'V': s = "view"; break;
                }
                Tcl_AppendResult(interp, cmdtable[index].name, ": invalid ", s,
                                                                        NULL);
            }
            goto FAIL; /* TODO: append info about which arg is bad */
        }
    }
    
    GetShared()->info->cmds = cmdtable; /* for ViewCmd_X */
    
    type = cmdtable[index].proc(stack);
    if (type == IT_unknown)
        goto FAIL;

    result = ItemAsObj(type, stack);
    if (result == NULL)
        goto FAIL;

    Tcl_SetObjResult(interp, result);
    ok = TCL_OK;

FAIL:
    POP_KEEP_REFS
    return ok;
}

static void DoneShared (ClientData cd) {
    Shared_p sh = (Shared_p) cd;
    if (--sh->refs == 0)
        free(sh->info);
}

static void InitShared (void) {
    Shared_p sh = GetShared();
    if (sh->refs++ == 0)
        sh->info = calloc(1, sizeof(SharedInfo));
    UngetShared(DoneShared, (ClientData) sh);
}

DLLEXPORT int Vlerq_Init (Tcl_Interp *interp) {
    if (!MyInitStubs(interp) || Tcl_PkgRequire(interp, "Tcl", "8.4", 0) == NULL)
        return TCL_ERROR;

    Tcl_CreateObjCommand(interp, "vlerq", VlerqObjCmd, f_commands, NULL);
    InitShared();
    return Tcl_PkgProvide(interp, "vlerq", PACKAGE_VERSION);
}

DLLEXPORT int Vlerq_SafeInit (Tcl_Interp *interp) {
    if (!MyInitStubs(interp) || Tcl_PkgRequire(interp, "Tcl", "8.4", 0) == NULL)
        return TCL_ERROR;

    /* UNSAFE is defined in the "opdefs_gen.h" file included above */
    Tcl_CreateObjCommand(interp, "vlerq", VlerqObjCmd, f_commands + UNSAFE,
                                                                        NULL);
    InitShared();
    return Tcl_PkgProvide(interp, "vlerq", PACKAGE_VERSION);
}

/* end of generated code */
