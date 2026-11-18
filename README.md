# Triangulation de Delaunay en C

Triangulation de Delaunay d'un nuage de points 2D, écrite en C11 sans bibliothèque de géométrie, avec un affichage OpenGL qui montre la construction point par point.

![Construction de la triangulation, point par point](images/construction.gif)

*À chaque insertion, les triangles en orange sont ceux qui touchent le nouveau point.*

## Principe

Une triangulation est **de Delaunay** si le cercle circonscrit de chaque triangle ne contient aucun autre point à l'intérieur. Parmi toutes les triangulations d'un même nuage, c'est celle qui maximise le plus petit angle : elle évite au mieux les triangles plats, qui dégradent les calculs sur un maillage.

L'algorithme utilisé est **Bowyer-Watson**, incrémental :

1. un **super-triangle** très grand contient tous les points ;
2. pour chaque point, on cherche les triangles dont le cercle circonscrit le contient : ils forment la **cavité** ;
3. le bord de la cavité est formé des arêtes qui n'appartiennent qu'à un seul de ses triangles ;
4. on supprime la cavité et on relie chaque arête du bord au nouveau point ;
5. à la fin, on retire les triangles qui touchent le super-triangle.

À chaque insertion, la cavité de *m* triangles est remplacée par *m* + 2 triangles : le nombre de triangles augmente toujours de 2.

## Prédicats géométriques

Toute la géométrie repose sur deux tests de signe :

- **orientation(a, b, c)** : signe du produit vectoriel AB × AC (c à gauche, à droite ou sur la droite ab) ;
- **dans_cercle(a, b, c, d)** : signe d'un déterminant 3 × 3. En relevant les points sur le paraboloïde z = x² + y², « d est dans le cercle de abc » devient « le relevé de d est sous le plan des relevés de a, b, c ».

Les triangles sont toujours stockés dans le sens inverse des aiguilles d'une montre, ce qui donne un sens fixe au signe de `dans_cercle`.

## Structures de données

- `Point` : deux `double` ;
- `Triangle` : trois **indices** dans le tableau de points (et non des pointeurs, qu'un `realloc` du tableau rendrait invalides) ;
- `Triangulation` : deux tableaux dynamiques dont la capacité double avec `realloc`.

Les sommets du super-triangle sont ajoutés à la fin du tableau de points : les points réels gardent leurs indices, et le nettoyage final se réduit à un test `indice >= n`.

## Compiler et lancer

Sous Linux (ou WSL), avec gcc et GLFW :

```sh
sudo apt install build-essential libglfw3-dev
make            # compile ./tests et ./affichage
./tests         # tests dans le terminal
make valgrind   # tests sous valgrind
./affichage     # fenêtre OpenGL : 150 points
./affichage 400 7   # 400 points, graine 7
```

Dans la fenêtre : `Espace` met en pause, `R` recommence, `Échap` quitte.

Le chargeur de fonctions OpenGL [glad](https://github.com/Dav1dde/glad) (OpenGL 3.3 core) est inclus dans `glad/`.

## Tests

`tests.c` vérifie :

- les prédicats sur des cas connus (gauche, droite, alignés ; intérieur, extérieur, sur le cercle) ;
- la propriété de Delaunay sur 10, 100 et 1 000 points aléatoires, en testant chaque point contre chaque cercle circonscrit ;
- les cas limites : 0, 1 et 2 points, points alignés, points en double.

Tous les tests passent, et valgrind ne signale ni fuite ni erreur mémoire.

## Limites et pistes

- **Complexité** : chaque insertion examine tous les triangles, soit O(n²) en moyenne. Une structure de voisinage permettrait de localiser le point par marche et de construire la cavité de proche en proche, pour environ O(n log n) en moyenne avec un ordre d'insertion aléatoire.
- **Précision** : les prédicats sont calculés en `double`. Pour des points presque alignés ou presque cocycliques, l'arrondi peut donner un mauvais signe ; des prédicats exacts adaptatifs (Shewchuk) régleraient ce problème.

## Fichiers

| Fichier | Contenu |
|---|---|
| `delaunay.h`, `delaunay.c` | la triangulation, sans dépendance |
| `tests.c` | les tests dans le terminal |
| `affichage.c` | l'affichage OpenGL (GLFW, glad, VBO, shaders) |
| `Makefile` | compilation |
