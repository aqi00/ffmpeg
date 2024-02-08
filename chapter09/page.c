
#include "page.h"

PointF up_paths[PATH_LEN+1]; // 上边的路径
PointF left_paths[PATH_LEN+1]; // 左边的路径

PointF a,f,g,e,h,c,j,b,k,d,i; // 贝塞尔曲线的各个关联点坐标
int mViewWidth, mViewHeight;

// 计算上边的路径
void calcUpPaths(PointF begin, PointF control, PointF end)
{ 
    float t,xt,yt;
    int rate=PATH_LEN,i=0;
  
    for (t=0; t<=1; t+=1.0/rate,i++)
    { 
        yt=1-t;
        xt = begin.x*yt*yt + control.x*2*yt*t + end.x*t*t;
        yt = begin.y*yt*yt + control.y*2*yt*t + end.y*t*t;
        PointF dot;
        dot.x = xt;
        dot.y = yt;
        up_paths[i] = dot;
    }
    up_paths[PATH_LEN] = end;
}

// 计算左边的路径
void calcLeftPaths(PointF begin, PointF control, PointF end)
{ 
    float t,xt,yt;
    int rate=PATH_LEN,i=0;
  
    for (t=0; t<=1; t+=1.0/rate,i++)
    { 
        yt=1-t;
        xt = begin.x*yt*yt + control.x*2*yt*t + end.x*t*t;
        yt = begin.y*yt*yt + control.y*2*yt*t + end.y*t*t;
        PointF dot;
        dot.x = xt;
        dot.y = yt;
        left_paths[i] = dot;
    }
    left_paths[PATH_LEN] = end;
}

// 如果C点的x坐标小于0，就重新测量A点的坐标
void calcPointA(void) {
    float w0 = mViewWidth - c.x;
    float w1 = abs(f.x - a.x);
    float w2 = mViewWidth * w1 / w0;
    float h1 = abs(f.y - a.y);
    float h2 = w2 * h1 / w1;
    a.x = abs(f.x - w2);
    a.y = abs(f.y - h2);
}

// 计算两条线段的交点坐标
PointF getCrossPoint(PointF firstP1, PointF firstP2, PointF secondP1, PointF secondP2) {
    float dxFirst = firstP1.x - firstP2.x, dyFirst = firstP1.y - firstP2.y;
    float dxSecond = secondP1.x - secondP2.x, dySecond = secondP1.y - secondP2.y;
    float gapCross = dxSecond*dyFirst - dxFirst*dySecond;
    float firstCross = firstP1.x * firstP2.y - firstP2.x * firstP1.y;
    float secondCross = secondP1.x * secondP2.y - secondP2.x * secondP1.y;
    float pointX = (dxFirst*secondCross - dxSecond*firstCross) / gapCross;
    float pointY = (dyFirst*secondCross - dySecond*firstCross) / gapCross;
    PointF cross;
    cross.x = pointX;
    cross.y = pointY;
    return cross;
}

// 计算各点的坐标
void calcEachPoint(PointF a, PointF f) {
    g.x = (a.x + f.x) / 2;
    g.y = (a.y + f.y) / 2;
    e.x = g.x - (f.y - g.y) * (f.y - g.y) / (f.x - g.x);
    e.y = f.y;
    h.x = f.x;
    h.y = g.y - (f.x - g.x) * (f.x - g.x) / (f.y - g.y);
    c.x = e.x - (f.x - e.x) / 2;
    c.y = f.y;
    j.x = f.x;
    j.y = h.y - (f.y - h.y) / 2;
    b = getCrossPoint(a,e,c,j); // 计算线段AE与CJ的交点坐标
    k = getCrossPoint(a,h,c,j); // 计算线段AH与CJ的交点坐标
    d.x = (c.x + 2 * e.x + b.x) / 4;
    d.y = (2 * e.y + c.y + b.y) / 4;
    i.x = (j.x + 2 * h.x + k.x) / 4;
    i.y = (2 * h.y + j.y + k.y) / 4;
}

// 计算C点的x坐标
float calcPointCX(PointF a, PointF f) {
    PointF g, e;
    g.x = (a.x + f.x) / 2;
    g.y = (a.y + f.y) / 2;
    e.x = g.x - (f.y - g.y) * (f.y - g.y) / (f.x - g.x);
    e.y = f.y;
    return e.x - (f.x - e.x) / 2;
}

// 计算经过两个坐标点的线段斜率
float calcXiea(PointF p, PointF q) {
    if (p.x == q.x) {
        return 0;
    } else {
        float xiea = (p.y-q.y)/(p.x-q.x);
        return xiea;
    }
}

// 计算线段斜率等初始化操作
// width和height为画布的宽高，x和y为A点的横纵坐标
void calcSlope(int width, int height, float x, float y) {
    a.x = x;
    a.y = y;
    f.x = width;
    f.y = height;
    calcEachPoint(a, f); // 计算各点的坐标
    PointF touchPoint;
    touchPoint.x = x;
    touchPoint.y = y;
    // 如果C点的x坐标小于0，就重新测量C点的坐标
    if (calcPointCX(touchPoint, f)<0) {
        calcPointA(); // 如果C点的x坐标小于0，就重新测量A点的坐标
        calcEachPoint(a, f); // 计算各点的坐标
    }
    calcUpPaths(k, h, j); // 计算上边的路径
    calcLeftPaths(c, e, b); // 计算左边的路径
}

// 指定坐标是否在线段的右边
int onLineRight(float x, float y, PointF dot1, PointF dot2) {
    int onRight = 0;
    float xiea = calcXiea(dot1, dot2);
    float xieb = dot1.y - dot1.x*xiea;
    if (xiea > 0 && y < xiea*x+xieb) {
        onRight = 1;
    }
    if (xiea < 0 && y > xiea*x+xieb) {
        onRight = 1;
    }
    if (dot1.x == dot2.x && x >= dot1.x) {
        onRight = 1;
    }
    return onRight;
}

// 指定坐标是否在线段的下边
int onLineDown(float x, float y, PointF dot1, PointF dot2) {
    int onDown = 0;
    float xiea = calcXiea(dot1, dot2);
    float xieb = dot1.y - dot1.x*xiea;
    if (y > xiea*x+xieb) {
        onDown = 1;
    }
    return onDown;
}

// 指定坐标是否在路径的右边
int onPathRight(float x, float y, PointF* paths, int path_len) {
    int onRight = 0;
    int i;
    for (i=0; i<path_len-1; i++) {
        int thisRight = onLineRight(x, y, paths[i], paths[i+1]);
        if (thisRight == 1) {
            onRight = 1;
            break;
        }
    }
    return onRight;
}

// 指定坐标是否在路径的下边
int onPathDown(float x, float y, PointF* paths, int path_len) {
    int onDown = 0;
    int i;
    for (i=0; i<path_len-1; i++) {
        int thisDown = onLineDown(x, y, paths[i], paths[i+1]);
        if (thisDown == 1) {
            onDown = 1;
            break;
        }
    }
    return onDown;
}

// 返回1表示显示背面，返回2表示显示下一页，返回3表示显示上一页
int calcShowType(float x, float y) {
    int show_type = 3;
    int right_ae = onLineRight(x, y, a, e);
    int down_ah = onLineDown(x, y, a, h);
    int down_di = onLineDown(x, y, d, i);
    int right_cj = onLineRight(x, y, c, j);
    if (right_ae==1 && down_ah==1 && down_di==0) {
        show_type = 1;
    } else if (down_di == 1) {
        show_type = 2;
    } else if (right_ae==0 && down_di==0 && right_cj==1) {
        if (y>b.y) {
          int right_cd = onLineRight(x, y, c, d);
          int right_bd = onLineRight(x, y, d, b);
          int path_right = onPathRight(x, y, left_paths, PATH_LEN+1);
          if (y>d.y && right_cd==1) { // 下方
              if (path_right == 1) {
                  show_type = 2;
              }
          } else if (y<d.y && right_bd==1) {
              if (path_right == 1) {
                  show_type = 1;
              }
          }
        }
    } else if (down_ah==0 && down_di==0 && right_cj==1) {
        if (x>k.x) {
          int down_ki = onLineDown(x, y, k, i);
          int down_ji = onLineDown(x, y, i, j);
          int path_down = onPathDown(x, y, up_paths, PATH_LEN+1);
          if (x>i.x && down_ji==1) { // 右边
              if (path_down == 1) {
                  show_type = 2;
              }
          } else if (x<i.x && down_ki==1) {
              if (path_down == 1) {
                  show_type = 1;
              }
          }
        }
    }
    
    return show_type;
}
