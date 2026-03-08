import multiprocessing as mp
import random
import time






# ---- Couleurs ANSI ----
RESET = "\033[0m"
BOLD = "\033[1m"

GREEN = "\033[92m"
BLUE = "\033[94m"
YELLOW = "\033[93m"
CYAN = "\033[96m"
RED = "\033[91m"
MAGENTA = "\033[95m"


# -------------------------
# Fonction "calculateur"
# -------------------------
def calculateur(calc_id, req_recv, rep_send, recv_lock, send_lock):
    """
    calc_id   : identifiant du calculateur (0..n-1)
    req_recv  : extrémité du pipe sur laquelle on REÇOIT les demandes
    rep_send  : extrémité du pipe sur laquelle on ENVOIE les réponses
    recv_lock : lock pour éviter que 2 calculateurs fassent recv() en même temps
    send_lock : lock pour éviter que 2 calculateurs fassent send() en même temps
    """

    while True:
        # ---- 1) Récupérer une demande ----
        # On verrouille le recv, car plusieurs calculateurs partagent req_recv
        with recv_lock:
            demande = req_recv.recv()  # BLOQUANT : attend une demande

        # Convention : None = message "stop"
        if demande is None:
            # On quitte proprement
            print(f"[CALC {calc_id}] Stop reçu, je termine.")
            return

        # La demande est un tuple : (id_demande, op, a, b)
        demande_id, op, a, b = demande

        # ---- 2) Effectuer le calcul ----
        try:
            if op == '+':
                res = a + b
            elif op == '-':
                res = a - b
            elif op == '*':
                res = a * b
            elif op == '/':
                # petite gestion d'erreur possible : division par zéro
                res = a / b
            else:
                raise ValueError(f"Opérateur inconnu: {op}")

            ok = True
            err = None

        except Exception as e:
            res = None
            ok = False
            err = str(e)

        # (Optionnel) simulation de temps de calcul variable
        time.sleep(random.uniform(0.1, 0.6))

        # ---- 3) Envoyer la réponse ----
        # Réponse : (id_demande, ok, result, calc_id, err)
        reponse = (demande_id, ok, res, calc_id, err)

        # Plusieurs calculateurs partagent rep_send => lock
        with send_lock:
            rep_send.send(reponse)


# -------------------------
# Fonction "demandeur"
# -------------------------
def demandeur(req_send, rep_recv, nb_calculateurs, nb_demandes):
    """
    req_send        : extrémité du pipe sur laquelle on ENVOIE les demandes
    rep_recv        : extrémité du pipe sur laquelle on REÇOIT les réponses
    nb_calculateurs : nombre de workers (pour envoyer les N "stop")
    nb_demandes     : nombre de calculs à demander
    """

    # On génère des demandes au format (id, op, a, b)
    print("\n" + "="*60)
    print(f"{MAGENTA}🚀 SERVEUR DE CALCUL PARALLÈLE{RESET}")
    print("="*60 + "\n")

    ops = ['+', '-', '*', '/']
    demandes = []
    for i in range(nb_demandes):
        op = random.choice(ops)
        a = random.randint(1, 20)
        b = random.randint(1, 20)

        demande_id = f"REQ-{i+1:03d}"  # 001, 002, 003...
        demandes.append((demande_id, op, a, b))
        # (Optionnel) pour tester division par zéro
        # if op == '/' and random.random() < 0.2:
        #     b = 0
        

    print(f"[DEMANDEUR] J'envoie {nb_demandes} demandes...")

    # ---- 1) Envoyer toutes les demandes ----
    for d in demandes:
        demande_id, op, a, b = d
        req_send.send(d)
        print(f"{BLUE}📤 [DEMANDEUR]{RESET} "
            f"{BOLD}{demande_id}{RESET} ➜ {a} {op} {b}")
    # ---- 2) Recevoir toutes les réponses ----
    # On attend exactement nb_demandes réponses
    resultats = {}  # id_demande -> reponse
    for _ in range(nb_demandes):
        rep = rep_recv.recv()  # BLOQUANT
        demande_id, ok, res, calc_id, err = rep
        resultats[demande_id] = {
            "worker": f"CALC-{calc_id}",
            "result": res,
            "status": "OK" if ok else "ERR"
        }

        if ok:
            print(f"{GREEN}✅ [RÉSULTAT]{RESET} "
                f"{BOLD}{demande_id}{RESET} "
                f"traité par {CYAN}CALC-{calc_id}{RESET} ➜ {res}")
        else:
            print(f"{RED}❌ [ERREUR]{RESET} "
                f"{BOLD}{demande_id}{RESET} "
                f"par CALC-{calc_id} ➜ {err}")
    # ---- 3) Dire aux calculateurs de s'arrêter ----
    # Comme ils sont n et qu'ils attendent potentiellement sur recv(),
    # on envoie n messages "None" pour que chacun puisse sortir.
    for _ in range(nb_calculateurs):
        req_send.send(None)

    print("\n" + "="*70)
    print("📊 RÉCAPITULATIF DES CALCULS")
    print("="*70)

    print(f"{'ID':<10} | {'WORKER':<10} | {'RESULTAT':<15} | {'STATUS':<10}")
    print("-"*70)

    for demande_id in sorted(resultats.keys()):
        data = resultats[demande_id]
        print(f"{demande_id:<10} | "
            f"{data['worker']:<10} | "
            f"{str(data['result']):<15} | "
            f"{data['status']:<10}")

    print("="*70 + "\n")

    return resultats


# -------------------------
# MAIN
# -------------------------
if __name__ == "__main__":
    mp.set_start_method("fork")  # Sur Linux c'est OK (Ubuntu). Sur Windows => "spawn"

    nb_calculateurs = 3
    nb_demandes = 10

    # Pipe 1 : demandeur -> calculateurs
    # duplex=False : un sens unique
    # IMPORTANT :
    # - le 1er objet est "recv-only"
    # - le 2ème objet est "send-only"
    req_recv, req_send = mp.Pipe(duplex=False)

    # Pipe 2 : calculateurs -> demandeur
    rep_recv, rep_send = mp.Pipe(duplex=False)

    # Locks pour protéger l'accès concurrent
    recv_lock = mp.Lock()
    send_lock = mp.Lock()

    # ---- 1) Lancer les calculateurs ----
    procs = []
    for cid in range(nb_calculateurs):
        p = mp.Process(
            target=calculateur,
            args=(cid, req_recv, rep_send, recv_lock, send_lock)
        )
        p.start()
        procs.append(p)

    # ---- 2) Lancer le demandeur (ici dans le main, mais ça pourrait être un Process aussi) ----
    resultats = demandeur(req_send, rep_recv, nb_calculateurs, nb_demandes)

    # ---- 3) Attendre la fin des calculateurs ----
    for p in procs:
        p.join()

    print("\nRésumé trié par id de demande :")
    for k in sorted(resultats.keys()):
        print(k, "=>", resultats[k])