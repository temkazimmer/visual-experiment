"""Run Python inside the open Unreal Editor via the folder bridge
(Content/Python/init_unreal.py).

  python3 Tools/ue.py -c "import unreal; print(unreal.SystemLibrary.get_engine_version())"
  python3 Tools/ue.py Tools/some_script.py
"""
import os
import sys
import time
import uuid

BRIDGE = os.path.join(os.path.dirname(os.path.abspath(__file__)), ".bridge")


def main():
    code = sys.argv[2] if sys.argv[1] == "-c" else open(sys.argv[1]).read()
    name = "job_%d_%s" % (time.time(), uuid.uuid4().hex[:6])
    os.makedirs(os.path.join(BRIDGE, "in"), exist_ok=True)
    tmp = os.path.join(BRIDGE, "in", name + ".tmp")
    with open(tmp, "w") as f:
        f.write(code)
    os.replace(tmp, os.path.join(BRIDGE, "in", name + ".py"))
    out = os.path.join(BRIDGE, "out", name + ".out")
    for _ in range(int(os.environ.get("UE_WAIT", "1200"))):
        if os.path.exists(out):
            with open(out) as f:
                text = f.read()
            os.remove(out)
            print(text.rstrip())
            sys.exit(1 if "BRIDGE_ERROR" in text else 0)
        time.sleep(0.1)
    sys.exit("no reply in time — is the bridge running in the editor?")


if __name__ == "__main__":
    main()
