#ifndef AVFILTER_XFADE_PAGE_H
#define AVFILTER_XFADE_PAGE_H


#include <stdlib.h>

typedef struct PointF {
    float x;
    float y;
} PointF;

#define PATH_LEN 10
extern PointF up_paths[PATH_LEN+1];
extern PointF left_paths[PATH_LEN+1];

extern PointF a,f,g,e,h,c,j,b,k,d,i; // 贝塞尔曲线的各个关联点坐标

void calcUpPaths(PointF begin, PointF control, PointF end);
void calcLeftPaths(PointF begin, PointF control, PointF end);
void calcPointA(void);
PointF getCrossPoint(PointF firstP1, PointF firstP2, PointF secondP1, PointF secondP2);
void calcEachPoint(PointF a, PointF f);
float calcPointCX(PointF a, PointF f);
float calcXiea(PointF p, PointF q);
void calcSlope(int width, int height, float x, float y);
int onLineRight(float x, float y, PointF dot1, PointF dot2);
int onLineDown(float x, float y, PointF dot1, PointF dot2);
int onPathRight(float x, float y, PointF* paths, int path_len);
int onPathDown(float x, float y, PointF* paths, int path_len);
int calcShowType(float x, float y);


#endif /* AVFILTER_PAGE_H */
