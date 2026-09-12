#!/usr/bin/env python3
"""Build and record focused native evidence; never report success from stale files."""
import argparse
import datetime as dt
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--evidence-dir", type=Path, required=True)
    args = parser.parse_args()
    here = Path(__file__).resolve().parent
    repo = here.parents[2]
    workspace = Path(os.environ.get("V34_WORKSPACE", repo / "qlcplus/workspace-tools/IR4-TUBES-WASH-FOCUS-CONTROL-ONE-V34-RELIABILITY.qxw"))
    fixtures = Path(os.environ.get("V34_FIXTURES", repo / "releases/qlcplus-control-one/v32-testing/Fixtures"))
    sources = sorted((repo / "qlcplus/plugins/soundswitch").glob("*.cpp"))
    sources += sorted((repo / "qlcplus/plugins/soundswitch").glob("*.h"))
    sources += sorted(here.glob("*.cpp")) + [here / "CMakeLists.txt", here / "adapters/tardis.h"]
    sources += sorted(fixtures.glob("*.qxf"))
    source_hashes = {str(p.relative_to(repo)): sha256(p) for p in sources}
    workspace_hash = sha256(workspace)
    args.evidence_dir.mkdir(parents=True, exist_ok=True)
    build = subprocess.run(["cmake", "--build", str(args.build_dir), "--parallel", "8"],
                           stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    (args.evidence_dir / "build.log").write_text(build.stdout)
    if build.returncode:
        print(build.stdout)
        return build.returncode
    result = subprocess.run(["ctest", "--test-dir", str(args.build_dir), "--output-on-failure", "-V"],
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    (args.evidence_dir / "native-results.txt").write_text(result.stdout)
    unchanged = sha256(workspace) == workspace_hash and all(
        sha256(repo / path) == value for path, value in source_hashes.items())
    pin = re.search(r"QLC\+ source ([0-9a-f]{40}); Qt ([0-9.]+)", result.stdout)
    executables = [args.build_dir / "v34_native_contract",
                   args.build_dir / "upstream-engine/libqlcplusengine5.so"]
    record = {
        "generatedUtc": dt.datetime.now(dt.timezone.utc).isoformat(),
        "result": "passed" if result.returncode == 0 and unchanged else "failed",
        "exitCode": result.returncode,
        "sourcesUnchangedDuringBuildAndRun": unchanged,
        "qlcSourceCommit": pin.group(1) if pin else None,
        "qtVersion": pin.group(2) if pin else None,
        "workspaceFilename": workspace.name,
        "workspaceSha256": workspace_hash,
        "sourceAndFixtureSha256": source_hashes,
        "nativeBinarySha256": {p.name: sha256(p) for p in executables if p.exists()},
        "passedScenarios": re.findall(r"^1: PASS (.*)$", result.stdout, re.M),
        "scope": "Linux pinned engine, native Scene/MasterTimer/VCButton/feedback/output and candidate MIDI/effect code",
        "testAdapters": ["WinMM device I/O", "Tardis undo/network history", "output capture", "focused active-page input dispatcher"],
        "notTested": ["Windows QLC host ABI/load", "full rendered QML/page hierarchy", "physical devices", "visual lighting quality", "gig qualification"],
    }
    (args.evidence_dir / "native-evidence.json").write_text(json.dumps(record, indent=2) + "\n")
    print(result.stdout)
    if not unchanged:
        print("FAIL: workspace or source changed during build/run; repeat against an immutable candidate", file=sys.stderr)
    return result.returncode or (0 if unchanged else 1)


if __name__ == "__main__":
    raise SystemExit(main())
