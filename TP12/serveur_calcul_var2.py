import multiprocessing as mp
import random
import time


# -------------------------
# CALCULATEUR (worker)
# -------------------------
def calculateur(calc_id, req_recv, rep_senders, recv_lock, send_lock):
    """
    calc_id      : id du worker
    req_recv     : pipe (recv-only) commun => reçoit les requêtes des demandeurs
    rep_senders  : liste des pipes (send-only), un par demandeur => renvoyer au bon demandeur
    recv_lock    : lock pour protéger req_recv.recv() (plusieurs workers partagent le même pipe)
    send_lock    : lock pour protéger les envois (sécurité, même si pipes séparés)
    """

    while True:
        # 1) Prendre une requête de façon sûre
        with recv_lock:
            req = req_recv.recv()  # bloquant

        # Convention : None = STOP
        if req is None:
            print(f"[CALC-{calc_id}] 🛑 STOP reçu, fin.")
            return

        # req = (client_id, req_id, op, a, b)
        client_id, req_id, op, a, b = req

        # 2) Calcul
        try:
            if op == '+':
                res = a + b
            elif op == '-':
                res = a - b
            elif op == '*':
                res = a * b
            elif op == '/':
                res = a / b
            else:
                raise ValueError(f"Opérateur inconnu: {op}")

            ok = True
            err = ""

        except Exception as e:
            ok = False
            res = None
            err = str(e)

        # Simulation d'un temps de calcul variable
        time.sleep(random.uniform(0.1, 0.6))

        # 3) Répondre au BON demandeur (grâce à client_id)
        # réponse = (client_id, req_id, ok, res, calc_id, err)
        rep = (client_id, req_id, ok, res, calc_id, err)

        # On envoie sur le pipe du demandeur client_id
        with send_lock:
            rep_senders[client_id].send(rep)


# -------------------------
# DEMANDEUR (client)
# -------------------------
def demandeur(client_id, req_send, rep_recv, nb_demandes, send_lock):
    """
    client_id    : id du demandeur (0..m-1)
    req_send     : pipe (send-only) commun => envoie les requêtes aux calculateurs
    rep_recv     : pipe (recv-only) dédié => reçoit UNIQUEMENT ses réponses
    nb_demandes  : nombre de requêtes envoyées par ce demandeur
    send_lock    : lock pour protéger req_send.send() (plusieurs demandeurs partagent le même pipe)
    """
    random.seed(time.time() + client_id * 1000)

    ops = ['+', '-', '*', '/']

    # On construit les requêtes de ce demandeur
    demandes = []
    for i in range(nb_demandes):
        op = random.choice(ops)
        a = random.randint(1, 20)
        b = random.randint(1, 20)

        # ID lisible et unique "par demandeur"
        req_id = f"C{client_id}-REQ-{i+1:03d}"
        demandes.append((client_id, req_id, op, a, b))

    print(f"\n[DEMANDEUR-{client_id}] 📤 j'envoie {nb_demandes} demandes")

    # 1) Envoyer
    for (cid, req_id, op, a, b) in demandes:
        with send_lock:
            req_send.send((cid, req_id, op, a, b))
        print(f"[DEMANDEUR-{client_id}] 📤 {req_id} => {a} {op} {b}")

    # 2) Recevoir EXACTEMENT nb_demandes réponses (sur SON pipe dédié)
    results = {}  # req_id -> tuple réponse
    for _ in range(nb_demandes):
        rep = rep_recv.recv()  # bloquant
        rep_cid, rep_req_id, ok, res, calc_id, err = rep

        # sécurité : ce pipe est dédié donc rep_cid doit être == client_id
        # mais on garde le check pour être carré
        if rep_cid != client_id:
            print(f"[DEMANDEUR-{client_id}] ⚠️ réponse pas pour moi ? {rep}")
            continue

        results[rep_req_id] = rep

        if ok:
            print(f"[DEMANDEUR-{client_id}] ✅ {rep_req_id} <- CALC-{calc_id} => {res}")
        else:
            print(f"[DEMANDEUR-{client_id}] ❌ {rep_req_id} <- CALC-{calc_id} ERREUR => {err}")

    # 3) Petit tableau récap (optionnel mais lisible)
    print(f"\n[DEMANDEUR-{client_id}] 📊 RÉCAP")
    print("-" * 70)
    print(f"{'REQ_ID':<15} | {'WORKER':<8} | {'RESULTAT':<15} | {'STATUS':<6}")
    print("-" * 70)
    for req_id in sorted(results.keys()):
        _, _, ok, res, calc_id, err = results[req_id]
        status = "OK" if ok else "ERR"
        val = res if ok else err
        print(f"{req_id:<15} | {('CALC-'+str(calc_id)):<8} | {str(val):<15} | {status:<6}")
    print("-" * 70)

    return results


# -------------------------
# MAIN
# -------------------------
if __name__ == "__main__":
    mp.set_start_method("fork")  # Ubuntu OK

    # Paramètres
    m_demandeurs = 3
    n_calculateurs = 4
    nb_demandes_par_demandeur = 6

    # -------------------------
    # 1) Pipe REQUESTS (commun)
    # Demandeurs -> Calculateurs
    # duplex=False : un sens
    # -------------------------
    req_recv, req_send = mp.Pipe(duplex=False)  # req_recv: recv-only, req_send: send-only

    # -------------------------
    # 2) Pipes RESPONSES (un par demandeur)
    # Calculateurs -> Demandeur i
    # -------------------------
    rep_recvs = []   # pour les demandeurs
    rep_sends = []   # pour les calculateurs
    for _ in range(m_demandeurs):
        rep_recv_i, rep_send_i = mp.Pipe(duplex=False)
        rep_recvs.append(rep_recv_i)
        rep_sends.append(rep_send_i)

    # -------------------------
    # 3) Locks (synchronisation)
    # -------------------------
    recv_lock = mp.Lock()  # protège req_recv.recv()
    send_lock = mp.Lock()  # protège req_send.send() (plusieurs demandeurs)
    rep_send_lock = mp.Lock()  # protège les envois de réponses (option sécurité)

    # -------------------------
    # 4) Lancer les calculateurs
    # -------------------------
    calc_procs = []
    for cid in range(n_calculateurs):
        p = mp.Process(
            target=calculateur,
            args=(cid, req_recv, rep_sends, recv_lock, rep_send_lock)
        )
        p.start()
        calc_procs.append(p)

    # -------------------------
    # 5) Lancer les demandeurs
    # -------------------------
    # Ici je les lance aussi en Process pour être fidèle au "m demandeurs".
    dem_procs = []
    for did in range(m_demandeurs):
        p = mp.Process(
            target=demandeur,
            args=(did, req_send, rep_recvs[did], nb_demandes_par_demandeur, send_lock)
        )
        p.start()
        dem_procs.append(p)

    # Attendre tous les acdemandeurs
    for p in dem_procs:
        p.join()

    # -------------------------
    # 6) Stopper les calculateurs
    # On envoie n messages None pour réveiller chacun
    # -------------------------
    for _ in range(n_calculateurs):
        with send_lock:
            req_send.send(None)

    # Attendre les calculateurs
    for p in calc_procs:
        p.join()

    print("\n✅ Tous les demandeurs ont fini. ✅ Tous les calculateurs stoppés. Fin.\n")