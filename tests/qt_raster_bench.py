"""Run isolated benchmark processes; timings inform review, operation counts gate."""
import json
import os
from pathlib import Path
import platform
import statistics
import subprocess
import sys
import time

ROOT=Path(__file__).resolve().parent.parent
binary=Path(sys.argv[1]).resolve()
artifacts=ROOT/".local/artifacts/qt-performance"
artifacts.mkdir(parents=True,exist_ok=True)
(artifacts/"report.json").unlink(missing_ok=True)
cases=[]
for profile in ("ascii","unicode"):
    for scale in (1,2):
        for panes in (1,15):
            trials=[]
            for trial in range(1,4):
                name=f"{profile}-{panes}-{scale}x-trial{trial}";path=artifacts/(name+".json");path.unlink(missing_ok=True)
                with (artifacts/(name+".log")).open("w") as log:
                    child=subprocess.Popen([str(binary),str(panes),str(scale),profile,"300",str(path)],stdout=log,stderr=log)
                    try:
                        deadline=time.monotonic()+90
                        while True:
                            pid,status,usage=os.wait4(child.pid,os.WNOHANG)
                            if pid:
                                child.returncode=os.waitstatus_to_exitcode(status)
                                if child.returncode:raise RuntimeError(f"{name} failed; see {log.name}")
                                break
                            if time.monotonic()>deadline:raise TimeoutError(f"{name} timed out; see {log.name}")
                            time.sleep(.02)
                    finally:
                        if child.returncode is None:
                            child.kill();child.wait()
                case=json.loads(path.read_text())
                assert (case["panes"],case["scale"],case["profile"],case["columns"],case["rows"] )==(panes,scale,profile,80,24)
                assert case["build_type"]=="Release"
                assert {s["scenario"] for s in case["scenarios"]}=={"retained","one-row","full","one-visible","hidden"}
                for item in case["scenarios"]:
                    scenario=item["scenario"];frames=item["frames"];assert frames>=3
                    expected=0 if scenario in ("retained","hidden") else frames*(panes if scenario=="one-row" else 24 if scenario=="one-visible" else panes*24)
                    assert item["rasterized_rows"]==expected
                    assert item["row_image_bytes"]>0 and item["composite_image_bytes"]>0
                    assert item["reveal_rasterized_rows"]==(panes*24 if scenario in ("hidden","one-visible") else 0)
                # wait4 returns resources for this exact child, not compilers or earlier cases.
                case["peak_rss_bytes"]=usage.ru_maxrss*(1 if sys.platform=="darwin" else 1024)
                case["process_cpu_seconds"]=usage.ru_utime+usage.ru_stime
                trials.append(case)
                print(f"PASS: {name}; peak RSS {case['peak_rss_bytes']/1048576:.1f} MiB",flush=True)
            cases.append(dict(panes=panes,scale=scale,profile=profile,trials=trials))

comparisons=[]
for profile in ("ascii","unicode"):
    for scale in (1,2):
        one=next(c for c in cases if (c["panes"],c["profile"],c["scale"])==(1,profile,scale))
        many=next(c for c in cases if (c["panes"],c["profile"],c["scale"])==(15,profile,scale))
        for index in range(5):
            a=[trial["scenarios"][index] for trial in one["trials"]]
            b=[trial["scenarios"][index] for trial in many["trials"]]
            assert len({item["scenario"] for item in a+b})==1
            one_ms=statistics.median(item["wall_ms_per_frame"] for item in a)
            fifteen_ms=statistics.median(item["wall_ms_per_frame"] for item in b)
            comparisons.append(dict(profile=profile,scale=scale,scenario=a[0]["scenario"],one_ms=one_ms,fifteen_ms=fifteen_ms,ratio=fifteen_ms/one_ms,
                one_range_ms=[min(item["wall_ms_per_frame"] for item in a),max(item["wall_ms_per_frame"] for item in a)],
                fifteen_range_ms=[min(item["wall_ms_per_frame"] for item in b),max(item["wall_ms_per_frame"] for item in b)]))
report=dict(scope=cases[0]["trials"][0]["scope"],repeats=3,system=platform.platform(),machine=platform.machine(),logical_cpus=os.cpu_count(),cases=cases,comparisons=comparisons)
(artifacts/"report.json").write_text(json.dumps(report,indent=2)+"\n")
print("profile scale scenario       1 pane ms 15 panes ms ratio (median of 3)")
for row in comparisons:
    print(f"{row['profile']:7} {row['scale']}x {row['scenario']:12} {row['one_ms']:10.6f} {row['fifteen_ms']:11.6f} {row['ratio']:5.2f}")
print("Artifacts:",artifacts)
print("PASS: deterministic row/visibility/reveal checks; timings are observations, not thresholds")
