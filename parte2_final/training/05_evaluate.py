"""
05_evaluate.py — Evaluación del detector clásico (lo que exige la rúbrica del informe).

Carga el SVM entrenado y el conjunto de test, y calcula:
  - Matriz de confusión
  - Precisión, Sensibilidad (Recall), Especificidad, F1, Exactitud
  - Curva Precisión-Recall (usando la distancia al hiperplano como score)

Guarda las figuras en docs/figs/ para incrustarlas en el informe.

Uso:
  python 05_evaluate.py
"""
import numpy as np
import cv2
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

import config as C

FIGS = C.ROOT / "docs" / "figs"


def decision_scores(w, b, X):
    """Distancia con signo al hiperplano (score continuo para la curva PR).
    Mayor score = más 'ambulancia'."""
    return (X @ w + b).ravel()


def metrics(y_true, y_pred):
    tp = int(np.sum((y_pred == 1) & (y_true == 1)))
    tn = int(np.sum((y_pred == 0) & (y_true == 0)))
    fp = int(np.sum((y_pred == 1) & (y_true == 0)))
    fn = int(np.sum((y_pred == 0) & (y_true == 1)))
    prec = tp / (tp + fp) if tp + fp else 0.0
    rec = tp / (tp + fn) if tp + fn else 0.0
    spec = tn / (tn + fp) if tn + fp else 0.0
    f1 = 2 * prec * rec / (prec + rec) if prec + rec else 0.0
    acc = (tp + tn) / len(y_true)
    return dict(tp=tp, tn=tn, fp=fp, fn=fn, precision=prec, recall=rec,
                specificity=spec, f1=f1, accuracy=acc)


def plot_confusion(m):
    cm = np.array([[m["tn"], m["fp"]], [m["fn"], m["tp"]]])
    fig, ax = plt.subplots(figsize=(4, 4))
    ax.imshow(cm, cmap="Blues")
    for i in range(2):
        for j in range(2):
            ax.text(j, i, cm[i, j], ha="center", va="center", fontsize=14)
    ax.set_xticks([0, 1], ["No amb.", "Ambulancia"])
    ax.set_yticks([0, 1], ["No amb.", "Ambulancia"])
    ax.set_xlabel("Predicción")
    ax.set_ylabel("Real")
    ax.set_title("Matriz de confusión — detector HOG+SVM")
    fig.tight_layout()
    fig.savefig(FIGS / "cm_hog_svm.png", dpi=150)
    print(f"  → {FIGS / 'cm_hog_svm.png'}")


def plot_pr(y_true, scores):
    order = np.argsort(-scores)
    yt = y_true[order]
    tp = np.cumsum(yt == 1)
    fp = np.cumsum(yt == 0)
    total_pos = int(np.sum(y_true == 1))
    precision = tp / np.maximum(tp + fp, 1)
    recall = tp / max(total_pos, 1)
    fig, ax = plt.subplots(figsize=(5, 4))
    ax.plot(recall, precision, color="#0054A6")
    ax.set_xlabel("Sensibilidad (Recall)")
    ax.set_ylabel("Precisión")
    ax.set_title("Curva Precisión-Recall")
    ax.grid(True, ls="--", alpha=0.4)
    fig.tight_layout()
    fig.savefig(FIGS / "pr_hog_svm.png", dpi=150)
    print(f"  → {FIGS / 'pr_hog_svm.png'}")


def main():
    FIGS.mkdir(parents=True, exist_ok=True)
    model = np.load(C.MODELS / "hog_svm.npz")
    w, b = model["w"], model["b"]
    X = np.load(C.MODELS / "hog_test_X.npy")
    y = np.load(C.MODELS / "hog_test_y.npy").astype(np.int32)

    scores = decision_scores(w, b, X)
    y_pred = (scores > 0).astype(np.int32)
    m = metrics(y, y_pred)

    print("== Métricas en test ==")
    for k in ("accuracy", "precision", "recall", "specificity", "f1"):
        print(f"  {k:12s}: {m[k]:.4f}")
    print(f"  TP={m['tp']} TN={m['tn']} FP={m['fp']} FN={m['fn']}")

    plot_confusion(m)
    plot_pr(y, scores)


if __name__ == "__main__":
    main()
