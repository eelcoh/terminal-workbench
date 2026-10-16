"""Fresh-process live Qt Quick measurements; count checks gate, timings do not."""
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
headless="--headless" in sys.argv[2:]
artifacts=ROOT/".local/artifacts"/("qt-scene-test" if headless else "qt-scene-performance")
artifacts.mkdir(parents=True,exist_ok=True)
(artifacts/"report.json").unlink(missing_ok=True)
try:
    revision=subprocess.check_output(["git","rev-parse","HEAD"],cwd=ROOT,text=True).strip()
    modified=bool(subprocess.check_output(["git","status","--porcelain","--untracked-files=no"],cwd=ROOT,text=True).strip())
except (OSError,subprocess.CalledProcessError):
    revision="unknown";modified=None
source=dict(revision=revision,tracked_changes=modified)
print(f"Source checkout: {revision}; tracked changes: {modified}; headless: {headless}",flush=True)
scenarios=("cached-scene","repaint","one-row","full","one-visible")
cases=[]
# Native measurements use the display's current DPR, including any user override.
# Private Xvfb has a known base DPR=1, allowing explicit 1x/2x coverage.
for scale in ((1,2) if headless else (None,)):
    for profile in ("ascii","unicode"):
        for panes in (1,15):
            trials=[]
            for trial in range(1,4):
                name=f"{profile}-{panes}-{scale or 'native'}x-trial{trial}"
                path=artifacts/(name+".json");path.unlink(missing_ok=True)
                env=dict(os.environ,QSG_INFO="1",QT_LOGGING_RULES=os.environ.get("QT_LOGGING_RULES", "")+"\nqt.scenegraph.general=true\nqt.rhi.general=true")
                if headless:
                    env.update(QT_QPA_PLATFORM="xcb",LIBGL_ALWAYS_SOFTWARE="1",QSG_RHI_BACKEND="opengl",QT_SCALE_FACTOR=str(scale))
                    for key in ("QT_QUICK_BACKEND","QT_SCREEN_SCALE_FACTORS","QT_DEVICE_PIXEL_RATIO","WAYLAND_DISPLAY"):
                        env.pop(key,None)
                with (artifacts/(name+".log")).open("w") as log:
                    child=subprocess.Popen([str(binary),str(panes),profile,"300",str(path)],env=env,stdout=log,stderr=log)
                    try:
                        deadline=time.monotonic()+75
                        while True:
                            pid,status,usage=os.wait4(child.pid,os.WNOHANG)
                            if pid:
                                child.returncode=os.waitstatus_to_exitcode(status)
                                if child.returncode:raise RuntimeError(f"{name} failed; see {log.name}")
                                break
                            if time.monotonic()>deadline:raise TimeoutError(f"{name} timed out; see {log.name}")
                            time.sleep(.02)
                    finally:
                        if child.returncode is None:child.kill();child.wait()
                data=json.loads(path.read_text())
                assert (data["panes"],data["profile"],data["columns"],data["rows"],data["build_type"])==(panes,profile,80,24,"Release")
                if headless:assert data["device_pixel_ratio"]==scale and data["graphics_api"]=="OpenGL"
                assert data["graphics_api"] not in ("unknown","null")
                assert tuple(s["scenario"] for s in data["scenarios"])==scenarios
                for item in data["scenarios"]:
                    scenario=item["scenario"];frames=item["frames"];visible=1 if scenario=="one-visible" else panes
                    assert frames>=3
                    expected_rows=frames*(visible if scenario=="one-row" else 24*visible if scenario in ("full","one-visible") else 0)
                    assert item["rasterized_rows"]==expected_rows
                    assert item["paints"]==(0 if scenario=="cached-scene" else visible*frames)
                    assert item["row_image_bytes"]>0 and item["request_to_swap_ms"]>0
                assert data["reveal_rasterized_rows"]==(panes-1)*24 and data["reveal_paints"]==panes
                assert (path.with_suffix(".json.png")).stat().st_size>0
                assert abs(data["capture_width"]-data["window_width"]*data["device_pixel_ratio"])<=1
                assert abs(data["capture_height"]-data["window_height"]*data["device_pixel_ratio"])<=1
                # Reject timing comparisons if native scale/backend/geometry changed.
                if trials:
                    for key in ("device_pixel_ratio","graphics_api","window_width","window_height","item_scale"):
                        assert data[key]==trials[0][key],f"{key} changed between trials; rerun on one display"
                data["backend_log"]=name+".log"
                data["software_gl_requested"]=env.get("LIBGL_ALWAYS_SOFTWARE")=="1"
                data["render_environment"]={key:env.get(key) for key in ("QSG_RHI_BACKEND","QT_QUICK_BACKEND","QSG_RENDER_LOOP","QSG_NO_VSYNC","QT_SCALE_FACTOR")}
                data["peak_rss_bytes"]=usage.ru_maxrss*(1 if sys.platform=="darwin" else 1024)
                data["process_cpu_seconds"]=usage.ru_utime+usage.ru_stime
                trials.append(data)
                print(f"PASS: {name}; {data['graphics_api']} {data['device_pixel_ratio']}x; RSS {data['peak_rss_bytes']/1048576:.1f} MiB",flush=True)
            cases.append(dict(profile=profile,panes=panes,scale_override=scale,trials=trials))
comparisons=[]
for scale in ((1,2) if headless else (None,)):
    for profile in ("ascii","unicode"):
        one=next(c for c in cases if (c["scale_override"],c["profile"],c["panes"])==(scale,profile,1))
        many=next(c for c in cases if (c["scale_override"],c["profile"],c["panes"])==(scale,profile,15))
        for key in ("device_pixel_ratio","graphics_api","window_width","window_height"):
            assert one["trials"][0][key]==many["trials"][0][key],f"{key} changed between 1/15 panes"
        for index,scenario in enumerate(scenarios):
            item=dict(profile=profile,scale=one["trials"][0]["device_pixel_ratio"],scenario=scenario)
            for label,case in (("one",one),("fifteen",many)):
                samples=[t["scenarios"][index] for t in case["trials"]]
                for metric in ("request_to_swap_ms","gui_update_ms","paint_ms","cpu_ms_per_frame"):
                    values=[s[metric] for s in samples]
                    item[label+"_"+metric]=statistics.median(values)
                    item[label+"_"+metric+"_range"]=[min(values),max(values)]
            comparisons.append(item)
report=dict(source=source,headless=headless,system=platform.platform(),machine=platform.machine(),repeats=3,
    scope=cases[0]["trials"][0]["scope"],cases=cases,comparisons=comparisons)
(artifacts/"report.json").write_text(json.dumps(report,indent=2)+"\n")
print("profile DPR scenario       request→swap ms (1/15)     paint ms (1/15)")
for r in comparisons:
    print(f"{r['profile']:7} {r['scale']:3} {r['scenario']:12} {r['one_request_to_swap_ms']:8.3f}/{r['fifteen_request_to_swap_ms']:8.3f}  {r['one_paint_ms']:8.3f}/{r['fifteen_paint_ms']:8.3f}")
print("Artifacts:",artifacts)
print("PASS: exact row/paint/reveal counts; swap timings include presentation pacing, not GPU completion")
