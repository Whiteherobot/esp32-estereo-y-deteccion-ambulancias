#!/home/ing_michi/venvs/cv-ai311/bin/python3
"""
plot_depth.py — Genera gráficas de profundidad para el informe interciclo.

Uso:
  python3 tools/plot_depth.py data/measurements/depth_log_*.csv

  # Con distancia real para calcular error absoluto:
  python3 tools/plot_depth.py depth_log_30cm.csv --ground-truth 30
  python3 tools/plot_depth.py depth_log_60cm.csv --ground-truth 60
  python3 tools/plot_depth.py depth_log_90cm.csv --ground-truth 90

Salida:
  data/measurements/kalman_vs_raw.png   — serie temporal (gráfica para §5.3)
  data/measurements/error_barplot.png   — barras de error medio (gráfica para §5.2)
"""

import argparse
import sys
from pathlib import Path

import numpy as np
import pandas as pd
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import matplotlib.ticker as ticker

OUT_DIR = Path(__file__).parent.parent / "data" / "measurements"

# Colores que coinciden con el esquema del informe (azul Salesiana, verde)
C_RAW    = "#E07030"   # naranja — Z cruda
C_KALMAN = "#1A7BC4"   # azul    — Z Kalman
C_GT     = "#333333"   # gris    — valor real (ground truth)


# ---------------------------------------------------------------------------
# Gráfica 1: serie temporal Kalman vs Z cruda
# ---------------------------------------------------------------------------

def plot_time_series(df: pd.DataFrame, out_path: Path, ground_truth: float | None = None,
                     title_suffix: str = ""):
    fig, ax = plt.subplots(figsize=(8, 3.8))

    frames = df["frame"].values
    ax.plot(frames, df["z_raw_cm"],    color=C_RAW,    lw=1.2, alpha=0.7, label="Z cruda")
    ax.plot(frames, df["z_kalman_cm"], color=C_KALMAN,  lw=2.0,            label="Z Kalman")

    if ground_truth is not None:
        ax.axhline(ground_truth, color=C_GT, lw=1.5, ls="--", label=f"Real: {ground_truth} cm")

    # Banda ±3 cm alrededor de Kalman (requisito de la rúbrica)
    k = df["z_kalman_cm"].values
    ax.fill_between(frames, k - 3, k + 3, alpha=0.12, color=C_KALMAN, label="±3 cm (rúbrica)")

    ax.set_xlabel("Fotograma", fontsize=11)
    ax.set_ylabel("Profundidad Z (cm)", fontsize=11)
    title = "Estabilización de Kalman vs Lectura Cruda"
    if title_suffix:
        title += f"  [{title_suffix}]"
    ax.set_title(title, fontsize=12, fontweight="bold")
    ax.legend(fontsize=9, loc="upper right")
    ax.yaxis.set_minor_locator(ticker.AutoMinorLocator())
    ax.grid(True, which="major", ls="--", alpha=0.4)
    ax.grid(True, which="minor", ls=":",  alpha=0.2)

    # Estadísticas en recuadro
    std_raw = df["z_raw_cm"].std()
    std_kal = df["z_kalman_cm"].std()
    stats_txt = (f"σ cruda   = {std_raw:.2f} cm\n"
                 f"σ Kalman = {std_kal:.2f} cm")
    ax.text(0.02, 0.97, stats_txt, transform=ax.transAxes,
            fontsize=8, verticalalignment="top",
            bbox=dict(boxstyle="round,pad=0.4", fc="white", alpha=0.7))

    fig.tight_layout()
    fig.savefig(out_path, dpi=180)
    plt.close(fig)
    print(f"[OK] {out_path}")


# ---------------------------------------------------------------------------
# Gráfica 2: barras de error absoluto a 30 / 60 / 90 cm
# ---------------------------------------------------------------------------

def plot_error_bars(results: list[dict], out_path: Path):
    """
    results: lista de dict con keys: distance, mae_raw, mae_kalman
    """
    if not results:
        print("[SKIP] Sin datos suficientes para barplot de error.")
        return

    labels = [f"{r['distance']} cm" for r in results]
    mae_r  = [r["mae_raw"]    for r in results]
    mae_k  = [r["mae_kalman"] for r in results]

    x  = np.arange(len(labels))
    w  = 0.35

    fig, ax = plt.subplots(figsize=(6, 4))
    bars_r = ax.bar(x - w/2, mae_r, w, label="Z cruda",  color=C_RAW,    alpha=0.85)
    bars_k = ax.bar(x + w/2, mae_k, w, label="Z Kalman", color=C_KALMAN, alpha=0.85)

    ax.bar_label(bars_r, fmt="%.1f cm", padding=3, fontsize=8)
    ax.bar_label(bars_k, fmt="%.1f cm", padding=3, fontsize=8)

    ax.axhline(3, color=C_GT, ls="--", lw=1.5, label="Umbral rúbrica: 3 cm")
    ax.set_xticks(x)
    ax.set_xticklabels(labels, fontsize=11)
    ax.set_ylabel("Error absoluto medio (cm)", fontsize=11)
    ax.set_title("Error de Estimación a 30, 60 y 90 cm", fontsize=12, fontweight="bold")
    ax.legend(fontsize=9)
    ax.set_ylim(0, max(max(mae_r), max(mae_k)) * 1.35 + 2)
    ax.grid(True, axis="y", ls="--", alpha=0.4)

    fig.tight_layout()
    fig.savefig(out_path, dpi=180)
    plt.close(fig)
    print(f"[OK] {out_path}")


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(description="Genera gráficas de profundidad para el informe.")
    parser.add_argument("csvfiles", nargs="+", help="Archivos CSV generados por stereo_measure (tecla R)")
    parser.add_argument("--ground-truth", "-g", type=float, default=None,
                        help="Distancia real del objeto en cm (para calcular error)")
    args = parser.parse_args()

    error_results = []

    for csv_path in args.csvfiles:
        p = Path(csv_path)
        if not p.exists():
            print(f"[ERROR] No existe: {csv_path}")
            continue

        df = pd.read_csv(p)
        required = {"frame", "z_raw_cm", "z_kalman_cm"}
        if not required.issubset(df.columns):
            print(f"[ERROR] Columnas faltantes en {csv_path}: esperadas {required}")
            continue

        df = df.dropna(subset=["z_raw_cm", "z_kalman_cm"])
        if len(df) < 5:
            print(f"[SKIP] {csv_path}: solo {len(df)} filas válidas.")
            continue

        suffix = p.stem
        gt     = args.ground_truth

        # Gráfica serie temporal
        out_ts = OUT_DIR / f"kalman_vs_raw_{p.stem}.png"
        plot_time_series(df, out_ts, ground_truth=gt, title_suffix=suffix)

        # Acumular error para barplot
        if gt is not None:
            mae_raw = np.abs(df["z_raw_cm"]    - gt).mean()
            mae_kal = np.abs(df["z_kalman_cm"] - gt).mean()
            print(f"  {p.name} | GT={gt}cm | MAE cruda={mae_raw:.2f}cm | MAE Kalman={mae_kal:.2f}cm")
            error_results.append({"distance": int(gt), "mae_raw": mae_raw, "mae_kalman": mae_kal})

    # Si hay exactamente 3 archivos con ground-truth, generar barplot
    if len(error_results) == 3:
        error_results.sort(key=lambda r: r["distance"])
        plot_error_bars(error_results, OUT_DIR / "error_barplot.png")
    elif error_results:
        print(f"[INFO] Se necesitan 3 archivos con --ground-truth para el barplot. "
              f"Tienes {len(error_results)}.")

    print("\nListo. Archivos guardados en:", OUT_DIR)
    print("\nEjemplo de uso completo:")
    print("  python3 tools/plot_depth.py \\")
    print("    data/measurements/depth_log_XXXXX_30cm.csv --ground-truth 30")
    print("    data/measurements/depth_log_XXXXX_60cm.csv --ground-truth 60")
    print("    data/measurements/depth_log_XXXXX_90cm.csv --ground-truth 90")


if __name__ == "__main__":
    main()
