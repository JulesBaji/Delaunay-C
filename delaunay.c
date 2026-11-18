#include "delaunay.h"
#include <stdlib.h>
#include <stdio.h>
#define CAP_INITIALE 16

typedef struct { int a, b; } Arete;

int triangulation_init(Triangulation *tgn) {
    tgn->p = NULL;
    tgn->t = NULL;
    
    tgn->nb_p = 0;
    tgn->nb_t = 0;

    tgn->cap_p = CAP_INITIALE;
    tgn->cap_t = CAP_INITIALE;
    
    tgn->p = malloc(tgn->cap_p * sizeof *tgn->p);
    if (tgn->p == NULL) {
        return -1;  
    }

    tgn->t = malloc(tgn->cap_t * sizeof *tgn->t);
    if (tgn->t == NULL) {
        free(tgn->p);
        tgn->p = NULL;
        return -1;
    }

    return 0;
}

void triangulation_liberer (Triangulation *tgn) {
    tgn->nb_p = 0;
    tgn->nb_t = 0;

    tgn->cap_p = 0;
    tgn->cap_t = 0;

    free(tgn->p);
    tgn->p = NULL;

    free(tgn->t);
    tgn->t = NULL;
}

int triangulation_ajouter_point(Triangulation *tgn, Point p) {
    Point* tmp;
    int new_cap_p = CAP_INITIALE;
    if (tgn->cap_p != 0) new_cap_p = tgn->cap_p * 2;

    if (tgn->nb_p == tgn->cap_p) {    
        tmp = realloc(tgn->p, new_cap_p * sizeof *tgn->p);
        if (tmp == NULL) return -1;
        tgn->p = tmp;
        tgn->cap_p = new_cap_p;
    }

    tgn->p[tgn->nb_p] = p;
    tgn->nb_p++;
    return tgn->nb_p-1;
}

static int super_triangle(Triangulation *tgn) {
    if (tgn->nb_p == 0) return -1;
    int n = tgn->nb_p;

    double xmin = tgn->p[0].x;
    double ymin = tgn->p[0].y;
    double xmax = tgn->p[0].x;
    double ymax = tgn->p[0].y;

    for (int i=1; i<n; i++) {
        double ix = tgn->p[i].x;
        double iy = tgn->p[i].y;

        if (xmin > ix) xmin = ix;
        if (xmax < ix) xmax = ix;
        
        if (ymin > iy) ymin = iy;
        if (ymax < iy) ymax = iy;
    }
    
    double dmax = xmax - xmin > ymax - ymin ? xmax - xmin : ymax - ymin;
    if (dmax == 0) dmax = 1;

    double xm = (xmin + xmax) / 2;
    double ym = (ymin + ymax) / 2;

    Point s0 = {xm - 20*dmax, ym - dmax};
    Point s1 = {xm + 20*dmax, ym - dmax};
    Point s2 = {xm, ym + 20*dmax};

    if (triangulation_ajouter_point(tgn, s0) == -1) return -1;
    if (triangulation_ajouter_point(tgn, s1) == -1) return -1;
    if (triangulation_ajouter_point(tgn, s2) == -1) return -1;

    if (triangulation_ajouter_triangle(tgn, n, n + 1, n + 2) == -1) return -1;
    return 0;
}

double orientation(Point a, Point b, Point c) {
    return (b.x - a.x) * (c.y - a.y) - (c.x - a.x) * (b.y - a.y);
}

double dans_cercle(Point a, Point b, Point c, Point d) {
    double adx = a.x-d.x;
    double ady = a.y-d.y;
    double ad2 = adx*adx + ady*ady;

    double bdx = b.x-d.x;
    double bdy = b.y-d.y;
    double bd2 = bdx*bdx + bdy*bdy;

    double cdx = c.x-d.x;
    double cdy = c.y-d.y;
    double cd2 = cdx*cdx + cdy*cdy;

    return ad2 * (bdx*cdy - cdx*bdy)
    - bd2 * (adx*cdy - cdx*ady)
    + cd2 * (adx*bdy - bdx*ady);
}

int triangulation_ajouter_triangle(Triangulation *tgn, int i, int j, int k) {
    Triangle* tmp;
    int new_cap_t = CAP_INITIALE;
    if (tgn->cap_t != 0) new_cap_t = tgn->cap_t * 2;

    if (tgn->nb_t == tgn->cap_t) {
        tmp = realloc(tgn->t, new_cap_t * sizeof *tgn->t);
        if (tmp == NULL) return -1;
        tgn->t = tmp;
        tgn->cap_t = new_cap_t;
    }

    double orient = orientation(tgn->p[i], tgn->p[j], tgn->p[k]);
    if (orient < 0) {
        int e = j;
        j = k;
        k = e;
    }

    tgn->t[tgn->nb_t].s[0] = i;
    tgn->t[tgn->nb_t].s[1] = j;
    tgn->t[tgn->nb_t].s[2] = k;
    tgn->nb_t++;

    return tgn->nb_t-1;
}

static int contient_arete(Triangle t, int a, int b) {
    return (a == t.s[0] || a == t.s[1] || a == t.s[2]) &&
           (b == t.s[0] || b == t.s[1] || b == t.s[2]);
}

static int inserer_point(Triangulation *tgn, int k) {
    int nt = tgn->nb_t;
    int *mauvais = calloc(nt, sizeof *mauvais);
    if (mauvais == NULL) {
        return -1;  
    }

    int nb_mauvais = 0;
    for (int i=0; i<nt; i++) {
        Point a = tgn->p[tgn->t[i].s[0]];
        Point b = tgn->p[tgn->t[i].s[1]];
        Point c = tgn->p[tgn->t[i].s[2]];

        double r = dans_cercle(a,b,c,tgn->p[k]);
        if (r >0) {
            mauvais[i] = 1;
            nb_mauvais++;
        }
    }

    if (nb_mauvais == 0) {
        free(mauvais);
        return 0;  
    }

    Arete *bord = malloc(3 * nb_mauvais * sizeof *bord);
    if (bord == NULL) {
        free(mauvais);
        return -1;  
    }

    int nb_bord = 0; 
    for (int i=0; i<nt; i++) {
        if (!mauvais[i]) continue;
        Arete s0s1 = {tgn->t[i].s[0], tgn->t[i].s[1]};
        Arete s1s2 = {tgn->t[i].s[1], tgn->t[i].s[2]};
        Arete s2s0 = {tgn->t[i].s[2], tgn->t[i].s[0]};
        int contients0s1 = 0;
        int contients1s2 = 0;
        int contients2s0 = 0;

        for (int j=0; j<nt; j++) {
            if (contients0s1 == 1 && contients1s2 == 1 && contients2s0 == 1) break;
            if (j == i) continue;
            if (!mauvais[j]) continue;

            if (contient_arete(tgn->t[j], s0s1.a, s0s1.b)) contients0s1 = 1;
            if (contient_arete(tgn->t[j], s1s2.a, s1s2.b)) contients1s2 = 1;
            if (contient_arete(tgn->t[j], s2s0.a, s2s0.b)) contients2s0 = 1;
        }

        if (contients0s1 == 0) {
            bord[nb_bord++] =  s0s1;
        }
        if (contients1s2 == 0) {
            bord[nb_bord++] =  s1s2;
        }
        if (contients2s0 == 0) {
            bord[nb_bord++] =  s2s0;
        }
    }

    // Suppression triangles cercle circonscrit
    for (int i=nt-1; i>=0; i--) {   
        if (!mauvais[i]) continue; 
        tgn->t[i] = tgn->t[tgn->nb_t - 1];
        tgn->nb_t--;
    }

    // Ajout triangles cavite-nouveau point
    for (int i=0; i<nb_bord; i++) {
        if (triangulation_ajouter_triangle(tgn, bord[i].a, bord[i].b, k) == -1) {
            free(bord);
            free(mauvais);
            return -1;
        } 
    }

    free(bord);
    free(mauvais);
    return 0;
}

static void retirer_super_triangle(Triangulation *tgn, int n) { 
    for (int i=tgn->nb_t-1; i>=0; i--) {
        if (tgn->t[i].s[0] >= n || tgn->t[i].s[1] >= n || tgn->t[i].s[2] >= n) {
            tgn->t[i] = tgn->t[tgn->nb_t - 1];
            tgn->nb_t--;
        }
    }
    tgn->nb_p = n;
}

int triangulation_construire(Triangulation *tgn) {
    int n = tgn->nb_p;
    if (!n) return 0;

    tgn->nb_t = 0;
    if (super_triangle(tgn) == -1) {
        tgn->nb_p = n;
        tgn->nb_t = 0;
        return -1;
    }

    for (int k=0; k<n; k++) {
        if (inserer_point(tgn, k) == -1) {
            tgn->nb_p = n;
            tgn->nb_t = 0;
            return -1;
        }
    }

    retirer_super_triangle(tgn, n);   
    return 0;
}
