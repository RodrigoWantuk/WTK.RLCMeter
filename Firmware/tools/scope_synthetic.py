"""Reproducible synthetic CSV fixtures, never captured PCB/scope evidence."""
import argparse
import json
import math
from pathlib import Path
import random

from scope_waveform import Channel, ImportConfig
from dataclasses import asdict


def generate(out, kind="excitation", frequency=1000, rate=None, samples=None):
    out=Path(out)
    if out.exists():
        raise ValueError("use a new synthetic directory")
    rate=rate or (10_000_000 if kind=="carrier" else frequency*100 if kind in ("excitation","ret","saturated","noise") else 100000)
    samples=samples or (20000 if kind=="carrier" else 2000)
    rng=random.Random(43)
    if kind in ("powerup","simultaneous","overshoot","powerdown"):
        nets=("+5V_A","+3V3"); preset="supply"; parameters={"supply_state":"SYNTHETIC current-limited source","current_limit_a":.1,"adapters":"none synthetic","population":"SYNTHETIC"}
        def signals(t):
            ramp=lambda start: min(1.,max(0.,(t-start)/.003))
            a,b=5*ramp(.002),3.3*ramp(.002 if kind=="simultaneous" else .004)
            if kind=="overshoot": a+=.5*math.exp(-((t-.006)/.0002)**2)
            if kind=="powerdown": a,b=5-a,3.3-b
            return a,b
    elif kind in ("clamp","lower_clamp"):
        nets=("U5.7","ADC_RET_HG"); preset="clamp"
        parameters={"series_resistor_ohms":1000,"component":"SYNTHETIC U5.7 / ADC_RET_HG; verify fitted resistor", "mapping_evidence":"SYNTHETIC analytic model","resistor_tolerance_pct":1,"resistor_evidence":"SYNTHETIC 1% interval","measured_3v3_v":3.3,"rail_evidence":"SYNTHETIC independent rail"}
        def signals(t):
            a=1.65+3*math.cos(math.tau*1000*t)
            return a,max(-.2,min(3.5,a))
    else:
        nets=("RET_1X","RET_HG") if kind in ("ret","saturated") else ("VEXC","VMID")
        preset="ret" if nets[0]=="RET_1X" else "excitation"; parameters={"frequency_hz":frequency}
        def signals(t):
            x=math.cos(math.tau*frequency*t+.3)
            if kind in ("ret","saturated"):
                a=1.65+.08*x
                b=1.65+.08*(1+68000/4700)*math.cos(math.tau*frequency*t+.5)
                if kind=="saturated": b=max(.4,min(2.8,b))
                return a,b
            return 1.65+.5*x+( .01*math.cos(math.tau*450000*t) if kind=="carrier" else rng.gauss(0,.002) if kind=="noise" else 0),1.65
    channels=tuple(Channel(i+1,n,"V",1.,0.,10.,"tip","DC",1e6,displayed_limits_v=(.4,2.8) if kind=="saturated" and i==1 else None) for i,n in enumerate(nets))
    config=ImportConfig(0,"s",channels,simultaneous=True,metadata=dict(synthetic=True,instrument="SYNTHETIC mathematical generator",operator="TEST ONLY",capture_date="SYNTHETIC",acquisition="analytic samples; not Hantek export"))
    out.mkdir(parents=True)
    with (out/"waveform.csv").open("w",encoding="utf-8",newline="") as stream:
        stream.write("time(s),CH1(V),CH2(V)\n")
        for i in range(samples):
            t=i/rate; a,b=signals(t)
            stream.write(f"{t:.15g},{a:.15g},{b:.15g}\n")
    (out/"config.json").write_text(json.dumps(asdict(config),indent=2)+"\n",encoding="utf-8")
    (out/"parameters.json").write_text(json.dumps(parameters,indent=2)+"\n",encoding="utf-8")
    return out/"waveform.csv",config,preset,parameters


if __name__=="__main__":
    p=argparse.ArgumentParser(description=__doc__); p.add_argument("--out",type=Path,required=True)
    p.add_argument("--kind",choices=("powerup","powerdown","simultaneous","overshoot","excitation","carrier","ret","saturated","clamp","lower_clamp","noise"),default="excitation")
    p.add_argument("--frequency",type=int,choices=(10,100,1000,10000),default=1000)
    a=p.parse_args(); generate(a.out,a.kind,a.frequency)
