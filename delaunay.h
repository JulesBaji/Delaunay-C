#ifndef DELAUNAY_H
#define DELAUNAY_H

typedef struct {
    double x;
    double y;
} Point;

typedef struct {
    int s[3];
} Triangle;

typedef struct {
    Point *p;
    int nb_p;
    int cap_p;

    Triangle *t;
    int nb_t;
    int cap_t;
} Triangulation;

int triangulation_init(Triangulation *tgn);
int triangulation_ajouter_point(Triangulation *tgn, Point p);
void triangulation_liberer(Triangulation *tgn);

double orientation(Point a, Point b, Point c);

double dans_cercle(Point a, Point b, Point c, Point d);

int triangulation_ajouter_triangle(Triangulation *tgn, int i, int j, int k);
int triangulation_construire(Triangulation *tgn);

#endif