# -*- coding: utf-8 -*-
# Cette ligne indique l'encodage du fichier source.
# Ici UTF-8 permet d'écrire des commentaires avec des accents sans problème.

import multiprocessing as mp
# Le module multiprocessing permet de créer plusieurs processus Python.
# Contrairement aux threads, chaque processus a sa propre mémoire.
# Ici on l'utilise pour répartir le calcul de l'image entre plusieurs cœurs du CPU.

import os
# Le module os donne accès à des fonctions proches du système d'exploitation.
# Ici on l'utilise pour créer et écrire le fichier image en mode "bas niveau".

import math
# Le module math fournit des fonctions mathématiques : sqrt, log, pow, etc.
# Il est utilisé dans le calcul de la fractale et de la coloration.

import sys
# Le module sys permet notamment de récupérer les arguments passés en ligne de commande.
# Exemple : python3 fractale_mp.py 8
# Ici, 8 sera récupéré dans sys.argv[1].

# Taille de l'image : largeur = hauteur = 300 pixels
size = 300


def calcul(x, y, image, pixel_index):
    """
    Calcule la couleur d'un pixel situé en coordonnées (x, y),
    puis écrit directement ses 3 composantes RGB dans le tableau image
    à partir de la position pixel_index.

    Paramètres :
    - x, y : coordonnées du pixel dans l'image
    - image : tableau partagé contenant tous les pixels de l'image
    - pixel_index : position de départ du pixel dans le tableau image
                    (car chaque pixel prend 3 cases : R, G et B)
    """

    # ------------------------------------------------------------------
    # 1) Conversion des coordonnées pixel -> coordonnées "mathématiques"
    # ------------------------------------------------------------------

    # x varie entre 0 et size-1.
    # On le ramène dans l'intervalle [-1 ; 1].
    # Cela permet de travailler dans un repère centré autour de 0.
    p_x = x / size * 2.0 - 1.0

    # Même idée pour y : on le transforme aussi dans [-1 ; 1].
    p_y = y / size * 2.0 - 1.0

    # tz joue ici le rôle d'un paramètre de zoom.
    tz = 0.7

    # zoo correspond au facteur de zoom.
    # Plus tz est grand, plus pow(0.5, 13 * tz) devient petit,
    # donc plus on zoome dans une zone précise de la fractale.
    zoo = pow(0.5, 13.0 * tz)

    # cc_x et cc_y représentent le point complexe "c"
    # vers lequel on va faire converger le calcul.
    # On part d'un centre donné (-0.05 ; 0.6805)
    # puis on décale selon la position du pixel et le facteur de zoom.
    cc_x = -0.05 + p_x * zoo
    cc_y = 0.6805 + p_y * zoo

    # ------------------------------------------------------------------
    # 2) Initialisation des variables du calcul itératif
    # ------------------------------------------------------------------

    # z_x et z_y représentent les parties réelle et imaginaire du nombre complexe z.
    # On démarre à z = 0 + 0i.
    z_x = 0.0
    z_y = 0.0

    # m2 contiendra |z|² = z_x² + z_y²
    # Cela permet de tester si la suite diverge.
    m2 = 0.0

    # co va compter le nombre d'itérations effectuées.
    # C'est souvent utilisé pour colorer la fractale.
    co = 0.0

    # dz_x et dz_y représentent ici une dérivée/variation associée à l'itération.
    # Elle sert ensuite à estimer une distance pour améliorer le rendu.
    dz_x = 0.0
    dz_y = 0.0

    # ------------------------------------------------------------------
    # 3) Boucle principale d'itération
    # ------------------------------------------------------------------

    # On autorise jusqu'à 2560 itérations par pixel.
    # Plus ce nombre est grand, plus le calcul est précis,
    # mais plus le temps de calcul augmente.
    for i in range(2560):

        # Variables temporaires utiles pour ne pas perdre
        # les anciennes valeurs pendant les mises à jour.
        old_dz_x = 0.0
        old_z_x = 0.0

        # Si |z|² devient trop grand (> 1024),
        # on considère que la suite diverge
        # et on arrête les itérations pour ce pixel.
        if m2 > 1024.0:
            break

        # On sauvegarde l'ancienne valeur de dz_x
        # car on en a besoin pour calculer dz_y.
        old_dz_x = dz_x

        # Mise à jour de la dérivée (ou estimation dérivée)
        # liée à l'itération de la fractale.
        dz_x = 2.0 * z_x * dz_x - z_y * dz_y + 1.0
        dz_y = 2.0 * z_x * dz_y + z_y * old_dz_x

        # On sauvegarde l'ancienne valeur de z_x
        # car la formule de z_y en a besoin.
        old_z_x = z_x

        # Mise à jour de z selon la formule complexe :
        # z <- c + z²
        #
        # Si z = z_x + i*z_y
        # alors z² = (z_x² - z_y²) + i*(2*z_x*z_y)
        z_x = cc_x + z_x * z_x - z_y * z_y
        z_y = cc_y + 2.0 * old_z_x * z_y

        # Calcul de |z|² = z_x² + z_y²
        # pour savoir si la suite diverge.
        m2 = z_x * z_x + z_y * z_y

        # On incrémente le compteur d'itérations.
        co += 1.0

    # ------------------------------------------------------------------
    # 4) Coloration du pixel
    # ------------------------------------------------------------------

    # d sera une grandeur utilisée pour nuancer la couleur.
    d = 0.0

    # Si on est sorti avant 2560 itérations,
    # cela signifie que le point a divergé.
    if co < 2560.0:

        # dot_z = |z|²
        dot_z = z_x * z_x + z_y * z_y

        # Estimation de distance à l'ensemble fractal.
        # Ce genre de formule est utilisé pour produire des contours plus fins
        # et un rendu plus joli que la simple coloration par nombre d'itérations.
        d = math.sqrt(dot_z / (dz_x * dz_x + dz_y * dz_y)) * math.log(dot_z)

        # Composante rouge :
        # on prend le nombre d'itérations modulo 256.
        # Cela crée une variation cyclique de couleur.
        image[pixel_index + 0] = int(co % 256)

        # On remet d à l'échelle par rapport au zoom.
        d = 4.0 * d / zoo

        # On borne d entre 0 et 1 pour éviter
        # des valeurs incohérentes dans la suite du calcul.
        if d < 0.0:
            d = 0.0
        if d > 1.0:
            d = 1.0

        # Composante verte :
        # on utilise (1-d) multiplié par 76500 puis modulo 255.
        # Cela donne un effet de variation colorée assez "artistique".
        image[pixel_index + 1] = int((1.0 - d) * 76500 % 255.0)

        # On accentue fortement les petites valeurs de d
        # avec une puissance 12.5 pour modifier le contraste.
        d = pow(d, 12.5)

        # Composante bleue :
        # d est ramené sur [0 ; 255].
        image[pixel_index + 2] = int(d * 255.0)

    else:
        # Si le point ne diverge pas après 2560 itérations,
        # on considère qu'il appartient à l'ensemble
        # (ou qu'il en est très proche).
        # Dans ce cas, on le colore en noir.
        image[pixel_index + 0] = 0
        image[pixel_index + 1] = 0
        image[pixel_index + 2] = 0


# ----------------------------------------------------------------------
# Partie multi-processus
# ----------------------------------------------------------------------
# On ne modifie pas la logique de calcul du pixel,
# on parallélise seulement la répartition des lignes de l'image.

# Variable globale qui sera initialisée dans chaque processus worker.
# Le but est d'éviter de repasser le gros tableau partagé
# à chaque appel de fonction.
_g_image = None


def init_worker(shared_image):
    """
    Fonction appelée UNE FOIS au démarrage de chaque processus du pool.

    Son rôle :
    - récupérer le tableau partagé contenant l'image
    - le stocker dans une variable globale propre au processus

    Pourquoi faire ça ?
    - pour éviter de transmettre l'image complète à chaque tâche
    - pour que chaque worker puisse écrire directement dedans
    """
    global _g_image
    _g_image = shared_image


def worker(y_start, y_end):
    """
    Fonction exécutée par un processus.

    Elle calcule tous les pixels des lignes allant de y_start inclus
    à y_end exclu.

    Exemple :
    si y_start = 10 et y_end = 20,
    alors ce worker calcule les lignes 10 à 19.
    """
    global _g_image

    # On parcourt chaque ligne attribuée à ce processus.
    for y in range(y_start, y_end):

        # base correspond à l'indice du premier octet de la ligne y
        # dans le tableau image.
        #
        # Explication :
        # - il y a "size" pixels par ligne
        # - chaque pixel prend 3 cases (R,G,B)
        # donc une ligne prend 3 * size cases.
        base = 3 * (y * size)

        # On parcourt ensuite tous les pixels x de la ligne.
        for x in range(size):

            # Indice de départ du pixel (x, y) dans le tableau image.
            # On ajoute 3*x car chaque pixel prend 3 octets.
            pixel_index = base + 3 * x

            # On calcule la couleur du pixel et on l'écrit dans _g_image.
            calcul(x, y, _g_image, pixel_index)


def split_rows(nproc, height):
    """
    Découpe les lignes de l'image en blocs contigus,
    un bloc par processus.

    Paramètres :
    - nproc : nombre de processus
    - height : nombre total de lignes de l'image

    Retour :
    - une liste de couples (start, end)

    Exemple :
    1000 lignes, 4 processus
    -> [(0,250), (250,500), (500,750), (750,1000)]

    L'idée :
    chaque processus reçoit un morceau de l'image à calculer.
    """

    # On force nproc à être au moins égal à 1,
    # pour éviter un découpage invalide.
    nproc = max(1, nproc)

    # Nombre de lignes "de base" par processus.
    # // signifie division entière.
    step = height // nproc

    # Liste des intervalles de lignes.
    ranges = []

    # Début du bloc courant.
    start = 0

    # On construit nproc blocs.
    for i in range(nproc):

        # Fin théorique du bloc courant.
        end = start + step

        # Le dernier processus récupère toutes les lignes restantes,
        # pour être sûr de couvrir toute l'image.
        if i == nproc - 1:
            end = height

        # On enregistre le bloc [start, end[
        ranges.append((start, end))

        # Le bloc suivant commencera à end.
        start = end

    return ranges


def main():
    """
    Fonction principale du programme.

    Étapes :
    1) déterminer le nombre de processus
    2) créer la mémoire partagée pour l'image
    3) répartir les lignes entre les processus
    4) lancer le calcul parallèle
    5) écrire l'image finale dans un fichier PPM
    """

    # --------------------------------------------------------------
    # 1) Détermination du nombre de processus
    # --------------------------------------------------------------

    # Si un argument est donné en ligne de commande,
    # on l'utilise comme nombre de processus.
    #
    # Exemple :
    # python3 fractale_mp.py 8
    # => nproc = 8
    #
    # Sinon, on prend automatiquement le nombre de cœurs CPU disponibles.
    nproc = int(sys.argv[1]) if len(sys.argv) >= 2 else mp.cpu_count()

    # --------------------------------------------------------------
    # 2) Création de l'image en mémoire partagée
    # --------------------------------------------------------------

    # On crée un tableau brut (RawArray) de type 'B'
    # c'est-à-dire un tableau d'octets non signés (unsigned byte),
    # donc des valeurs de 0 à 255.
    #
    # Taille totale :
    # 3 * size * size
    # car :
    # - size * size pixels
    # - 3 octets par pixel (R, G, B)
    shared_image = mp.RawArray('B', 3 * size * size)

    # --------------------------------------------------------------
    # 3) Répartition du travail
    # --------------------------------------------------------------

    # On découpe l'image en blocs de lignes.
    # Chaque bloc sera traité par un processus.
    jobs = split_rows(nproc, size)

    # --------------------------------------------------------------
    # 4) Exécution parallèle
    # --------------------------------------------------------------

    # Création d'un pool de processus.
    #
    # processes=nproc :
    #   on crée exactement nproc processus
    #
    # initializer=init_worker :
    #   fonction appelée une seule fois au démarrage de chaque worker
    #
    # initargs=(shared_image,) :
    #   argument passé à init_worker
    #
    # Le "with" garantit que le pool sera correctement fermé à la fin.
    with mp.Pool(processes=nproc, initializer=init_worker, initargs=(shared_image,)) as pool:

        # starmap appelle worker(y_start, y_end)
        # pour chaque couple contenu dans jobs.
        #
        # Exemple :
        # jobs = [(0,75), (75,150), (150,225), (225,300)]
        #
        # Cela lancera :
        # worker(0,75)
        # worker(75,150)
        # worker(150,225)
        # worker(225,300)
        pool.starmap(worker, jobs)

    # --------------------------------------------------------------
    # 5) Écriture du fichier image au format PPM P6
    # --------------------------------------------------------------

    # On ouvre/crée le fichier image1.ppm en écriture.
    #
    # Flags utilisés :
    # - os.O_CREAT  : crée le fichier s'il n'existe pas
    # - os.O_WRONLY : ouverture en écriture seule
    # - os.O_TRUNC  : vide le fichier s'il existe déjà
    #
    # 0o644 = droits du fichier :
    # - propriétaire : lecture + écriture
    # - groupe       : lecture
    # - autres       : lecture
    fd = os.open("image1.ppm", os.O_CREAT | os.O_WRONLY | os.O_TRUNC, 0o644)

    # Écriture de l'en-tête PPM binaire (format P6).
    #
    # P6           -> format PPM binaire
    # size size    -> largeur et hauteur
    # 255          -> valeur max d'une composante RGB
    #
    # .encode() convertit la chaîne en bytes pour pouvoir l'écrire.
    os.write(fd, f"P6\n{size} {size}\n255\n".encode())

    # On écrit ensuite toutes les données RGB de l'image.
    # shared_image se comporte comme un buffer d'octets.
    os.write(fd, shared_image)

    # Fermeture du fichier.
    os.close(fd)

    # Message de confirmation.
    print(f"OK: image1.ppm générée avec {nproc} process.")


# Point d'entrée du programme.
# Ce test permet d'exécuter main() seulement si le fichier est lancé directement,
# et non s'il est importé comme module.
#
# C'est très important avec multiprocessing, surtout sous Windows,
# pour éviter que le code principal se relance dans chaque processus fils.
if __name__ == "__main__":
    main()