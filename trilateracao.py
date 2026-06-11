import numpy as np
from scipy.optimize import least_squares


def trilateracao3d(posicoes, distancias):
    ### OBS: Toda a fundamentacao teorica pode ser encontrada no pdf:
   # Determina a posicao 3d do beacon usando minimos quadrados

    posicoes = np.asarray(posicoes, dtype=float)
    distancias = np.asarray(distancias, dtype=float)

    n = len(posicoes)

    if n < 4:
        raise ValueError("São necessários pelo menos 4 pontos.")


    # Escolhemos o primeiro ESP como referência (ponto A)
    A = posicoes[0]

    M = [] # Matriz M do sistema linear ME = b
    b = [] # Vetor b do sistema linear

    # Constroi as linhas da matriz
    for i in range(1, n):
        P = posicoes[i]

        linha = 2 * (P - A)

        ki = (
            np.dot(P, P)
            - np.dot(A, A)
            - (distancias[i]**2 - distancias[0]**2)
        )

        M.append(linha)
        b.append(ki)

    M = np.array(M)
    b = np.array(b)

    # Resolve o sistema por minimos quadrados
    chute_inicial, *_ = np.linalg.lstsq(M, b, rcond=None)

    def residuos(E):
        # Essencialmente calcula os residuos (erros) --> e_i = E - dist_medidada
        Ex, Ey, Ez = E

        return (
            np.sqrt(
                (Ex - posicoes[:, 0])**2 +
                (Ey - posicoes[:, 1])**2 +
                (Ez - posicoes[:, 2])**2
            )
            - distancias
        )

    resultado = least_squares(
        residuos,
        x0=chute_inicial
    )

    return resultado.x

# Matriz de posicoes conhecidas
# esp32 = np.array([
#     [0, 0, 0],
#     [10, 0, 0],
#     [0, 10, 0],
#     [0, 0, 10]
# ])

# Distancias medidadas por cada esp
# distancias = np.array([
#     1,
#     2,
#     3,
#     4,
# ])


# Para testar :

# beacon_real = np.array([0, 0, 0])

# distancias = np.linalg.norm(
#     esp32 - beacon_real,
#     axis=1
# )