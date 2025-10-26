#pragma once

typedef int             MWCOORD;        /* device coordinates*/

/* region types */
#define MWREGION_ERROR          0
#define MWREGION_NULL           1
#define MWREGION_SIMPLE         2
#define MWREGION_COMPLEX        3

/* MWRECT used in region routines*/
typedef struct {
        MWCOORD left;
        MWCOORD top;
        MWCOORD right;
        MWCOORD bottom;
} MWRECT;

/* dynamically allocated multi-rectangle clipping region*/
typedef struct {
        int     size;           /* malloc'd # of rectangles*/
        int     numRects;       /* # rectangles in use*/
        int     type;           /* region type*/
        MWRECT *rects;          /* rectangle array*/
        MWRECT  extents;        /* bounding box of region*/
} MWCLIPREGION;

MWCLIPREGION *GdAllocRegion(void);
void GdDestroyRegion(MWCLIPREGION *rgn);
void GdSetRectRegion(MWCLIPREGION *rgn, MWCOORD left, MWCOORD top, MWCOORD right, MWCOORD bottom);
MWCLIPREGION *GdAllocRectRegion(MWCOORD left,MWCOORD top,MWCOORD right,MWCOORD bottom);
void GdSubtractRegion(MWCLIPREGION *d, MWCLIPREGION *s1, MWCLIPREGION *s2);
void GdUnionRegion(MWCLIPREGION *d, MWCLIPREGION *s1, MWCLIPREGION *s2);
bool GdPtInRegion(MWCLIPREGION *rgn, MWCOORD x, MWCOORD y);
