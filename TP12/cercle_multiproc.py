import multiprocessing as mp
import random
import time
import os
import math


def monte_carlo_chunk(n_points: int, seed: int) -> int:
    """
    Calcule le nombre de points "inside" (dans le quart de cercle) sur n_points tirages.
    On renvoie juste un int (inside) => léger à envoyer au parent.
    """
    rng = random.Random(seed)
    inside = 0

    for _ in range(n_points):
        x = rng.random()
        y = rng.random()
        if x*x + y*y <= 1.0:
            inside += 1

    return inside


def worker(worker_id: int, n_points: int, out_q: mp.Queue) -> None:
    """
    Process worker : calcule un chunk et renvoie inside dans une Queue.
    """
    # Seed différent par process (important pour ne pas tirer les mêmes points)
    seed = int(time.time() * 1e6) ^ (os.getpid() << 16) ^ (worker_id << 8)

    inside = monte_carlo_chunk(n_points, seed)
    out_q.put(inside)


def estimate_pi_single(N: int) -> float:
    """
    Version monoprocess.
    """
    seed = int(time.time() * 1e6)
    inside = monte_carlo_chunk(N, seed)
    return 4.0 * inside / N


def estimate_pi_multi(N: int, n_proc: int) -> float:
    """
    Version multiprocess :
    - on découpe N en n_proc morceaux
    - chaque worker calcule son inside
    - le parent somme et calcule pi
    """
    out_q = mp.Queue()

    # Répartition : on split N sur n_proc (le dernier prend le reste)
    base = N // n_proc
    chunks = [base] * n_proc
    chunks[-1] += N - base * n_proc

    procs = []
    for i in range(n_proc):
        p = mp.Process(target=worker, args=(i, chunks[i], out_q))
        p.start()
        procs.append(p)

    # Récupérer les résultats
    inside_total = 0
    for _ in range(n_proc):
        inside_total += out_q.get()

    # Attendre la fin des workers
    for p in procs:
        p.join()

    return 4.0 * inside_total / N


def main():
    N = 1_000_000

    # Nombre de process : en général = nb de coeurs logiques (ou un peu moins)
    n_proc = os.cpu_count() or 4

    print("\n" + "=" * 60)
    print(" Estimation de π par Monte-Carlo (mono vs multiprocess)")
    print("=" * 60)
    print(f"N = {N}")
    print(f"CPU count = {n_proc}")
    print("=" * 60)

    # ---- MONO ----
    t0 = time.perf_counter()
    pi_single = estimate_pi_single(N)
    t1 = time.perf_counter()
    mono_time = t1 - t0

    # ---- MULTI ----
    t2 = time.perf_counter()
    pi_multi = estimate_pi_multi(N, n_proc)
    t3 = time.perf_counter()
    multi_time = t3 - t2

    # ---- Résultats ----
    speedup = mono_time / multi_time if multi_time > 0 else float("inf")

    print("\n--- Résultats ---")
    print(f"π (mono)  = {pi_single:.8f} | erreur = {abs(pi_single - math.pi):.8f} | temps = {mono_time:.3f} s")
    print(f"π (multi) = {pi_multi:.8f} | erreur = {abs(pi_multi - math.pi):.8f} | temps = {multi_time:.3f} s")
    print(f"Accélération (mono/multi) = x{speedup:.2f}")

    print("\nNote : le gain dépend de ton CPU, de N, et du coût d'overhead des process.")
    print("=" * 60 + "\n")


if __name__ == "__main__":
    # Sur Ubuntu/Linux => fork OK. (Sur Windows ce serait spawn)
    mp.set_start_method("fork")
    main()