#!/usr/bin/env python3
import json
import os
import select
import subprocess
import sys
import time

RUNTIME_DIR = os.environ.get("XDG_RUNTIME_DIR", f"/run/user/{os.getuid()}")
STATE_FILE = os.path.join(RUNTIME_DIR, "navi_seen_count")

def get_swaync_pid():
    try:
        out = subprocess.check_output(["pgrep", "-x", "swaync"], text=True).strip()
        return out.splitlines()[0] if out else None
    except Exception:
        return None

def load_seen_count():
    try:
        with open(STATE_FILE, "r") as f:
            return int(f.read().strip())
    except Exception:
        return None

def save_seen_count(val):
    try:
        with open(STATE_FILE, "w") as f:
            f.write(str(val))
    except Exception:
        pass

def emit(css_class, tooltip):
    out = {
        "text": " ",
        "alt": css_class,
        "tooltip": tooltip,
        "class": css_class,
    }
    print(json.dumps(out), flush=True)

def main():
    seen_count = load_seen_count()
    blink_bright = True

    while True:
        swaync_pid = get_swaync_pid()
        if not swaync_pid:
            emit("none", "Navi (aguardando swaync)")
            time.sleep(1.0)
            continue

        proc = None
        try:
            proc = subprocess.Popen(
                ["swaync-client", "-s"],
                stdout=subprocess.PIPE,
                stderr=subprocess.DEVNULL,
                text=True,
                bufsize=1,
            )
            poller = select.poll()
            poller.register(proc.stdout, select.POLLIN | select.POLLHUP | select.POLLERR)

            count = 0
            dnd = False
            visible = False

            while True:
                # Se o swaync reiniciou (mudou de PID), reconecta imediatamente
                current_pid = get_swaync_pid()
                if current_pid != swaync_pid:
                    break

                events = poller.poll(450)
                if events:
                    line = proc.stdout.readline()
                    if not line:
                        break
                    line = line.strip()
                    if line:
                        try:
                            data = json.loads(line)
                            count = int(data.get("count", 0))
                            dnd = bool(data.get("dnd", False))
                            visible = bool(data.get("visible", False))
                        except json.JSONDecodeError:
                            pass

                disk_seen = load_seen_count()
                if disk_seen is not None:
                    seen_count = disk_seen

                if seen_count is None:
                    seen_count = count
                    save_seen_count(seen_count)

                if visible or count < seen_count:
                    seen_count = count
                    save_seen_count(seen_count)

                unread = max(0, count - seen_count)

                if dnd:
                    emit("dnd", f"Navi silenciada ({count} notificações)")
                elif visible:
                    emit("open", f"Ouvindo a Navi ({count} notificações)")
                elif unread > 0:
                    blink_bright = not blink_bright
                    cls = "notification-bright" if blink_bright else "notification-dim"
                    emit(cls, f"Hey! Listen! ({unread} nova(s), {count} no total)")
                else:
                    emit("none", f"Navi ({count} notificações)" if count > 0 else "Navi (sem notificações)")

        except Exception:
            pass
        finally:
            if proc is not None:
                try:
                    proc.terminate()
                    proc.wait(timeout=0.5)
                except Exception:
                    try:
                        proc.kill()
                    except Exception:
                        pass

        time.sleep(0.3)

if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        sys.exit(0)
