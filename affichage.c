/*
 * affichage.c — affichage OpenGL de la triangulation de Delaunay.
 *
 * La construction est animée point par point : à chaque étape, on
 * triangule les k premiers points avec delaunay.c, on envoie les sommets
 * des triangles et des arêtes au GPU (VBO), puis on les dessine avec
 * deux shaders très simples.
 *
 * Usage :
 *   ./affichage [n] [graine]
 *   ./affichage [n] [graine] --capture dossier [--clair]
 *     --capture : écrit chaque image en PPM dans « dossier » puis quitte
 *                 (sert à fabriquer l'animation du README) ;
 *     --clair   : couleurs pour fond clair.
 *
 * Touches : Espace = pause, R = recommencer, Échap = quitter.
 */

#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "delaunay.h"

#define LARGEUR 960
#define HAUTEUR 600
#define SECONDES_PAR_POINT 0.05
#define PAUSE_FINALE 2.0

/* Couleurs (r, g, b) d'un thème. */
typedef struct {
    float fond[3];
    float remplissage[3];
    float aretes[3];
    float points[3];
    float nouveau[3];
} Theme;

static const Theme THEME_SOMBRE = {
    {0.07f, 0.08f, 0.10f},
    {0.13f, 0.20f, 0.30f},
    {0.36f, 0.62f, 0.86f},
    {0.92f, 0.94f, 0.96f},
    {0.98f, 0.62f, 0.25f},
};

static const Theme THEME_CLAIR = {
    {0.97f, 0.97f, 0.96f},
    {0.84f, 0.90f, 0.97f},
    {0.16f, 0.40f, 0.68f},
    {0.10f, 0.12f, 0.16f},
    {0.86f, 0.40f, 0.10f},
};

/* ------------------------------------------------------------------ */
/* Shaders                                                              */
/* ------------------------------------------------------------------ */

/* Les points sont dans [0, rapport] x [0, 1], où rapport = largeur / hauteur
   de la fenêtre : on les ramène dans [-1, 1] (coordonnées normalisées
   d'OpenGL), avec une marge. Garder les proportions est important : un
   étirement inégal en x et en y ne conserve pas les cercles, donc la
   triangulation affichée ne serait plus de Delaunay. */
static const char *SOURCE_SOMMETS =
    "#version 330 core\n"
    "layout(location = 0) in vec2 position;\n"
    "uniform float taille_point;\n"
    "uniform float rapport;\n"
    "void main() {\n"
    "    vec2 p = position / vec2(rapport, 1.0);\n"
    "    gl_Position = vec4(p * 1.84 - 0.92, 0.0, 1.0);\n"
    "    gl_PointSize = taille_point;\n"
    "}\n";

static const char *SOURCE_FRAGMENTS =
    "#version 330 core\n"
    "uniform vec4 couleur;\n"
    "out vec4 sortie;\n"
    "void main() { sortie = couleur; }\n";

static GLuint compiler_shader(GLenum type, const char *source)
{
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &source, NULL);
    glCompileShader(s);

    GLint ok;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char journal[1024];
        glGetShaderInfoLog(s, sizeof journal, NULL, journal);
        fprintf(stderr, "Erreur de compilation du shader :\n%s\n", journal);
    }
    return s;
}

static GLuint creer_programme(void)
{
    GLuint vs = compiler_shader(GL_VERTEX_SHADER, SOURCE_SOMMETS);
    GLuint fs = compiler_shader(GL_FRAGMENT_SHADER, SOURCE_FRAGMENTS);
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint ok;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char journal[1024];
        glGetProgramInfoLog(prog, sizeof journal, NULL, journal);
        fprintf(stderr, "Erreur d'édition de liens :\n%s\n", journal);
    }
    return prog;
}

/* ------------------------------------------------------------------ */
/* Données envoyées au GPU                                              */
/* ------------------------------------------------------------------ */

/* Tampon de sommets côté CPU : des paires (x, y) en float. */
typedef struct {
    float *v;
    int nb;   /* nombre de sommets */
    int cap;  /* capacité en sommets */
} Tampon;

static int tampon_ajouter(Tampon *b, Point p)
{
    if (b->nb == b->cap) {
        int cap = b->cap ? 2 * b->cap : 1024;
        float *tmp = realloc(b->v, (size_t)cap * 2 * sizeof *tmp);
        if (tmp == NULL) return -1;
        b->v = tmp;
        b->cap = cap;
    }
    b->v[2 * b->nb] = (float)p.x;
    b->v[2 * b->nb + 1] = (float)p.y;
    b->nb++;
    return 0;
}

/* Prépare trois listes de sommets à partir de la triangulation :
   - tri  : 3 sommets par triangle (dessin plein, GL_TRIANGLES) ;
   - ar   : 2 sommets par arête (GL_LINES) ;
   - neuf : les triangles qui touchent le dernier point inséré. */
static int remplir_tampons(const Triangulation *tgn, int dernier,
                           Tampon *tri, Tampon *ar, Tampon *neuf)
{
    tri->nb = ar->nb = neuf->nb = 0;
    for (int i = 0; i < tgn->nb_t; i++) {
        const int *s = tgn->t[i].s;
        int touche = s[0] == dernier || s[1] == dernier || s[2] == dernier;
        for (int j = 0; j < 3; j++) {
            Point a = tgn->p[s[j]];
            Point b = tgn->p[s[(j + 1) % 3]];
            if (tampon_ajouter(tri, a) == -1) return -1;
            if (touche && tampon_ajouter(neuf, a) == -1) return -1;
            if (tampon_ajouter(ar, a) == -1) return -1;
            if (tampon_ajouter(ar, b) == -1) return -1;
        }
    }
    return 0;
}

/* Triangule les k premiers points de pts dans tgn (réutilisée d'une
   étape à l'autre pour éviter des allocations inutiles). */
static int trianguler_prefixe(Triangulation *tgn, const Point *pts, int k)
{
    tgn->nb_p = 0;
    tgn->nb_t = 0;
    for (int i = 0; i < k; i++)
        if (triangulation_ajouter_point(tgn, pts[i]) == -1) return -1;
    return triangulation_construire(tgn);
}

/* Envoie un tampon dans le VBO puis le dessine. */
static void dessiner(GLuint vbo, const Tampon *b, GLenum mode,
                     GLint loc_couleur, const float c[3], float alpha)
{
    if (b->nb == 0) return;
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)b->nb * 2 * sizeof(float),
                 b->v, GL_STREAM_DRAW);
    glUniform4f(loc_couleur, c[0], c[1], c[2], alpha);
    glDrawArrays(mode, 0, b->nb);
}

/* ------------------------------------------------------------------ */
/* Capture d'images (pour l'animation du README)                       */
/* ------------------------------------------------------------------ */

static int ecrire_ppm(const char *dossier, int numero, int w, int h)
{
    unsigned char *pixels = malloc((size_t)w * h * 3);
    if (pixels == NULL) return -1;
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, pixels);

    char nom[512];
    snprintf(nom, sizeof nom, "%s/image_%04d.ppm", dossier, numero);
    FILE *f = fopen(nom, "wb");
    if (f == NULL) {
        free(pixels);
        return -1;
    }
    fprintf(f, "P6\n%d %d\n255\n", w, h);
    /* OpenGL lit de bas en haut : on écrit les lignes dans l'autre sens. */
    for (int y = h - 1; y >= 0; y--)
        fwrite(pixels + (size_t)y * w * 3, 1, (size_t)w * 3, f);
    fclose(f);
    free(pixels);
    return 0;
}

/* ------------------------------------------------------------------ */
/* Programme principal                                                 */
/* ------------------------------------------------------------------ */

int main(int argc, char **argv)
{
    int n = 150;
    unsigned graine = 1;
    const char *dossier_capture = NULL;
    const Theme *theme = &THEME_SOMBRE;

    int position = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--capture") == 0 && i + 1 < argc)
            dossier_capture = argv[++i];
        else if (strcmp(argv[i], "--clair") == 0)
            theme = &THEME_CLAIR;
        else if (position == 0) { n = atoi(argv[i]); position++; }
        else if (position == 1) { graine = (unsigned)atoi(argv[i]); position++; }
    }
    if (n < 1) n = 1;

    /* Points aléatoires dans [0, rapport] x [0, 1], graine fixe. */
    const double rapport = (double)LARGEUR / HAUTEUR;
    Point *pts = malloc((size_t)n * sizeof *pts);
    if (pts == NULL) return 1;
    srand(graine);
    for (int i = 0; i < n; i++) {
        pts[i].x = rapport * rand() / RAND_MAX;
        pts[i].y = (double)rand() / RAND_MAX;
    }

    /* Fenêtre et contexte OpenGL 3.3 core. */
    if (!glfwInit()) {
        fprintf(stderr, "glfwInit a échoué\n");
        free(pts);
        return 1;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 4);
    GLFWwindow *fen = glfwCreateWindow(LARGEUR, HAUTEUR,
                                       "Triangulation de Delaunay", NULL, NULL);
    if (fen == NULL) {
        fprintf(stderr, "Impossible de créer la fenêtre\n");
        glfwTerminate();
        free(pts);
        return 1;
    }
    glfwMakeContextCurrent(fen);
    glfwSwapInterval(1);
    if (!gladLoadGL(glfwGetProcAddress)) {
        fprintf(stderr, "gladLoadGL a échoué\n");
        glfwTerminate();
        free(pts);
        return 1;
    }

    GLuint prog = creer_programme();
    GLint loc_couleur = glGetUniformLocation(prog, "couleur");
    GLint loc_taille = glGetUniformLocation(prog, "taille_point");
    GLint loc_rapport = glGetUniformLocation(prog, "rapport");

    /* Un VAO décrit le format des sommets : 2 float par sommet. */
    GLuint vao, vbo;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(0);

    glEnable(GL_MULTISAMPLE);
    glEnable(GL_PROGRAM_POINT_SIZE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    Triangulation tgn;
    if (triangulation_init(&tgn) == -1) {
        fprintf(stderr, "Erreur d'initialisation\n");
        glfwTerminate();
        free(pts);
        return 1;
    }
    Tampon tri = {0}, ar = {0}, neuf = {0}, sommets = {0};

    int k = 0;              /* nombre de points déjà triangulés */
    int k_affiche = -1;     /* valeur de k pour laquelle les tampons sont à jour */
    double temps = 0.0;     /* temps écoulé dans l'animation */
    double precedent = glfwGetTime();
    int pause = 0, image = 0, erreur = 0;
    const double pas_capture = 1.0 / 30.0;  /* 30 images par seconde */

    while (!glfwWindowShouldClose(fen) && !erreur) {
        /* Avance du temps : réel à l'écran, fixe en capture. */
        double maintenant = glfwGetTime();
        double dt = dossier_capture ? pas_capture : maintenant - precedent;
        precedent = maintenant;
        if (!pause) temps += dt;

        /* Nombre de points à afficher à cet instant. */
        k = (int)(temps / SECONDES_PAR_POINT) + 1;
        if (k > n) k = n;
        if (temps > n * SECONDES_PAR_POINT + PAUSE_FINALE) {
            if (dossier_capture) break;   /* une seule boucle en capture */
            temps = 0.0;                  /* sinon on recommence */
            k = 1;
        }

        /* Recalcul seulement quand un point est ajouté. */
        if (k != k_affiche) {
            if (trianguler_prefixe(&tgn, pts, k) == -1 ||
                remplir_tampons(&tgn, k - 1, &tri, &ar, &neuf) == -1) {
                fprintf(stderr, "Erreur de triangulation\n");
                erreur = 1;
                break;
            }
            sommets.nb = 0;
            for (int i = 0; i < k; i++)
                if (tampon_ajouter(&sommets, pts[i]) == -1) erreur = 1;
            k_affiche = k;
        }

        int w, h;
        glfwGetFramebufferSize(fen, &w, &h);
        glViewport(0, 0, w, h);
        glClearColor(theme->fond[0], theme->fond[1], theme->fond[2], 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(prog);
        glBindVertexArray(vao);
        glUniform1f(loc_taille, 6.0f);
        glUniform1f(loc_rapport, (float)rapport);

        dessiner(vbo, &tri, GL_TRIANGLES, loc_couleur, theme->remplissage, 0.55f);
        dessiner(vbo, &neuf, GL_TRIANGLES, loc_couleur, theme->nouveau,
                 k < n ? 0.35f : 0.0f);
        dessiner(vbo, &ar, GL_LINES, loc_couleur, theme->aretes, 0.9f);
        dessiner(vbo, &sommets, GL_POINTS, loc_couleur, theme->points, 1.0f);

        if (dossier_capture && ecrire_ppm(dossier_capture, image++, w, h) == -1) {
            fprintf(stderr, "Impossible d'écrire dans %s\n", dossier_capture);
            erreur = 1;
        }

        glfwSwapBuffers(fen);
        glfwPollEvents();
        if (glfwGetKey(fen, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(fen, 1);
        if (glfwGetKey(fen, GLFW_KEY_R) == GLFW_PRESS) {
            temps = 0.0;
            k_affiche = -1;
        }
        static int espace_avant = 0;
        int espace = glfwGetKey(fen, GLFW_KEY_SPACE) == GLFW_PRESS;
        if (espace && !espace_avant) pause = !pause;
        espace_avant = espace;
    }

    free(tri.v);
    free(ar.v);
    free(neuf.v);
    free(sommets.v);
    triangulation_liberer(&tgn);
    free(pts);
    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);
    glDeleteProgram(prog);
    glfwDestroyWindow(fen);
    glfwTerminate();
    return erreur;
}
