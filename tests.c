#include "delaunay.h"
#include <stdio.h>
#include <stdlib.h>

static void test_triangulation(void) {
    Triangulation tgn;
    if (triangulation_init(&tgn) == -1) {
        printf("Erreur initialisation\n");
        triangulation_liberer(&tgn);
        return;
    }

    for (int i = 0; i < 20; i++) {
        Point q = {i, i*.5};

        if (triangulation_ajouter_point(&tgn, q) == -1) {
            printf("Erreur ajout\n");
            triangulation_liberer(&tgn);
            return;
        }
        printf ("%d : (%f, %f)\n", i, tgn.p[i].x, tgn.p[i].y);
    }

    printf("nb points : %d, capacite points : %d\n", tgn.nb_p, tgn.cap_p);
    triangulation_liberer(&tgn);
}

static void test_orientation(const char *nom, Point a, Point b, Point c, int signe_attendu) {
    double d = orientation(a,b,c);
    int signe = (d>0) - (d<0);
    printf("%s : D = %f -> %s\n", nom, d, signe == signe_attendu ? "OK" : "ECHEC");
}

static void test_dans_cercle(const char *nom, Point a, Point b, Point c, Point d, int signe_attendu) {
    double det = dans_cercle(a,b,c,d);
    int signe = (det > 0) - (det < 0);
    printf("%s : D = %f -> %s\n", nom, det, signe == signe_attendu ? "OK" : "ECHEC");
}

static void test_construire(void) {
    Triangulation tgn;
    if (triangulation_init(&tgn) == -1) {
        printf("Erreur initialisation\n");
        return;
    }

    Point pts[] = {{0, 0}, {2, 0}, {1, 2}, {1, 0.5}};
    for (int i = 0; i < (int)(sizeof pts / sizeof pts[0]); i++) {
        if (triangulation_ajouter_point(&tgn, pts[i]) == -1) {
            printf("Erreur ajout\n");
            triangulation_liberer(&tgn);
            return;
        }
    }

    if (triangulation_construire(&tgn) == -1) {
        printf("Erreur construction\n");
        triangulation_liberer(&tgn);
        return;
    }

    printf("nombre de points : %d\n", tgn.nb_p);
    printf("nombre de triangles : %d\n", tgn.nb_t);
    for (int i = 0; i < tgn.nb_t; i++) {
        printf("Triangle %d : (%d, %d, %d)\n", i, tgn.t[i].s[0], tgn.t[i].s[1], tgn.t[i].s[2]);
    }
    triangulation_liberer(&tgn);
}

static int est_delaunay(const Triangulation *tgn)
{
    for (int i = 0; i < tgn->nb_t; i++) {
        Point a = tgn->p[tgn->t[i].s[0]];
        Point b = tgn->p[tgn->t[i].s[1]];
        Point c = tgn->p[tgn->t[i].s[2]];

        if (orientation(a, b, c) <= 0) {
            printf("  triangle %d mal orienté\n", i);
            return 0;
        }
        for (int k = 0; k < tgn->nb_p; k++) {
            double det = dans_cercle(a, b, c, tgn->p[k]);
            if (det > 0) {
                printf("  point %d dans le cercle du triangle %d (det = %g)\n",
                       k, i, det);
                return 0;
            }
        }
    }
    return 1;
}

static void test_aleatoire(int n, unsigned graine)
{
    Triangulation tgn;
    if (triangulation_init(&tgn) == -1) {
        printf("Erreur initialisation\n");
        return;
    }

    srand(graine);
    for (int i = 0; i < n; i++) {
        Point q = {100.0 * rand() / RAND_MAX, 100.0 * rand() / RAND_MAX};
        if (triangulation_ajouter_point(&tgn, q) == -1) {
            printf("Erreur ajout\n");
            triangulation_liberer(&tgn);
            return;
        }
    }

    if (triangulation_construire(&tgn) == -1) {
        printf("Erreur construction\n");
        triangulation_liberer(&tgn);
        return;
    }

    printf("aleatoire n = %d (graine %u) : %d triangles -> %s\n",
           n, graine, tgn.nb_t, est_delaunay(&tgn) ? "OK" : "ECHEC");
    triangulation_liberer(&tgn);
}

static void test_cas(const char *nom, const Point *pts, int n, int nb_t_attendu)
{
    Triangulation tgn;
    if (triangulation_init(&tgn) == -1) {
        printf("%s : erreur initialisation\n", nom);
        return;
    }

    for (int i = 0; i < n; i++) {
        if (triangulation_ajouter_point(&tgn, pts[i]) == -1) {
            printf("%s : erreur ajout\n", nom);
            triangulation_liberer(&tgn);
            return;
        }
    }

    int ret = triangulation_construire(&tgn);
    int ok = ret == 0 && tgn.nb_t == nb_t_attendu && est_delaunay(&tgn);
    printf("%s : retour %d, %d triangles (attendu %d) -> %s\n",
           nom, ret, tgn.nb_t, nb_t_attendu, ok ? "OK" : "ECHEC");
    triangulation_liberer(&tgn);
}

static void test_cas_limites(void)
{
    Point un[]        = {{1, 1}};
    Point deux[]      = {{0, 0}, {1, 0}};
    Point alignes[]   = {{0, 0}, {1, 1}, {2, 2}};
    Point alignes5[]  = {{0, 0}, {3, 0}, {1, 0}, {4, 0}, {2, 0}};
    Point doublon[]   = {{0, 0}, {2, 0}, {1, 2}, {1, 0.5}, {1, 0.5}};
    Point doublon_s[] = {{0, 0}, {2, 0}, {1, 2}, {0, 0}};

    test_cas("0 point",            NULL,      0, 0);
    test_cas("1 point",            un,        1, 0);
    test_cas("2 points",           deux,      2, 0);
    test_cas("3 points alignes",   alignes,   3, 0);
    test_cas("5 points alignes",   alignes5,  5, 0);
    test_cas("point interieur x2", doublon,   5, 3);
    test_cas("sommet x2",          doublon_s, 4, 1);
}

int main(void) {
    test_triangulation();    

    test_orientation("gauche",  (Point){0,0}, (Point){1,0}, (Point){0,1},  1);
    test_orientation("droite",  (Point){0,0}, (Point){0,1}, (Point){1,0}, -1);
    test_orientation("alignes", (Point){0,0}, (Point){1,1}, (Point){2,2},  0);

    test_dans_cercle("intérieur", (Point){1,0}, (Point){0,1}, (Point){-1,0}, (Point){0,0}, 1);
    test_dans_cercle("extérieur", (Point){1,0}, (Point){0,1}, (Point){-1,0}, (Point){2,0}, -1);
    test_dans_cercle("sur le cercle", (Point){1,0}, (Point){0,1}, (Point){-1,0}, (Point){0,-1}, 0);

    test_construire();

    test_aleatoire(10, 1);
    test_aleatoire(100, 2);
    test_aleatoire(1000, 3);

    test_cas_limites();

    return 0;
}
