"""Audit-only complex algebra; no device access and no physical accuracy claim."""
import argparse
import json
import math
from pathlib import Path


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    errors = []
    for freq in (10.0, 1000.0, 10000.0):
        omega = 2.0 * math.pi * freq
        zs = complex(0.42, omega * 0.7e-6)
        yo = complex(2e-10, omega * 80e-12)
        for rref in (10.0, 100.0, 1000.0, 10000.0, 100000.0, 1000000.0):
            k = complex(rref, rref * 0.001)
            ts = complex(0.001, -0.0002)
            to = complex(0.985, 0.003)
            for dut in (complex(10.0), complex(1000.0), complex(996000.0),
                        complex(1.0, omega * 470e-6),
                        complex(0.2, -1.0 / (omega * 10e-9)),
                        complex(0.1, -1.0 / (omega * 4700e-6))):
                fixture = zs + dut / (1.0 + yo * dut)
                t = (fixture * to - k * ts) / (fixture - k)
                a = k - zs
                b = zs * to - k * ts
                c = 1.0 - yo * a
                d = -to - yo * b
                corrected = (a * t + b) / (c * t + d)
                error = abs(corrected - dut) / max(abs(dut), 1e-12)
                assert error < 1e-6, (freq, rref, dut, corrected, error)
                errors.append(error)

    # Purely resistive special case: moving SHORT without transforming gain is wrong.
    rref, lead_r, dut_r = 10.0, 0.5, 10.0
    t = (dut_r + lead_r) / (rref + dut_r + lead_r)
    new_short = lead_r / (rref + lead_r)
    naive = -rref * (t - new_short) / (t - 1.0)
    assert abs(naive - 9.523809523809524) < 1e-12
    result = {
        "kind": "SYNTHETIC_MATH_ONLY_NOT_BENCH",
        "precision": "Python complex double; not production float/noisy endpoint qualification",
        "cases": len(errors),
        "maximum_relative_error": max(errors),
        "composition": "A=k-Zs; B=Zs*to-k*ts; C=1-Yo*A; D=-to-Yo*B; Zcorr=(A*t+B)/(C*t+D)",
        "lead_parameters": {"Rs_ohm": 0.42, "Ls_h": 0.7e-6, "G_s": 2e-10, "Cp_f": 80e-12},
        "naive_pole_only_update_example": {"rref_ohm": rref, "lead_r_ohm": lead_r,
                                           "true_dut_ohm": dut_r,
                                           "incorrect_unchanged_k_result_ohm": naive},
        "warning": "Algebra only. Physical accuracy, noise, endpoint observability and fixture model coverage remain unqualified."
    }
    encoded = json.dumps(result, indent=2) + "\n"
    if args.output:
        args.output.write_text(encoded, encoding="utf-8")
    print(encoded, end="")


if __name__ == "__main__":
    main()
