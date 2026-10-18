import json
import os
import signal
import socket
import subprocess
import time

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
BINARY = os.path.join(REPO_ROOT, "build", "src", "newsdesk")


def free_port():
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.bind(("127.0.0.1", 0))
    port = s.getsockname()[1]
    s.close()
    return port


def write_config(
    path,
    journal_path,
    threshold=0.75,
    k=3,
    half_life_hours=6,
    clients=None,
    outbox_max_size=1000,
):
    config = {
        "threshold": threshold,
        "congratulated_k": k,
        "half_life_hours": half_life_hours,
        "listen": "127.0.0.1:0",
        "journal_path": journal_path,
        "outbox_max_size": outbox_max_size,
        "clients": clients or [],
    }
    with open(path, "w") as f:
        json.dump(config, f)


def start_server(config_path, port):
    proc = subprocess.Popen(
        [BINARY, "server", "--config", config_path, "--listen", f"127.0.0.1:{port}"],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    )
    wait_for_port("127.0.0.1", port)
    return proc


def wait_for_port(host, port, timeout=5):
    deadline = time.time() + timeout
    while time.time() < deadline:
        try:
            s = socket.create_connection((host, port), timeout=0.2)
            s.close()
            return
        except OSError:
            time.sleep(0.05)
    raise TimeoutError(f"server did not start listening on {host}:{port}")


def stop_server(proc):
    if proc.poll() is not None:
        return
    proc.send_signal(signal.SIGTERM)
    try:
        proc.wait(timeout=5)
    except subprocess.TimeoutExpired:
        proc.kill()
        proc.wait()


class Connection:
    def __init__(self, host, port):
        self.sock = socket.create_connection((host, port), timeout=5)
        self.sock.settimeout(5)
        self.buffer = b""

    def send(self, obj):
        self.sock.sendall((json.dumps(obj) + "\n").encode())

    def recv_line(self, timeout=5):
        self.sock.settimeout(timeout)
        while b"\n" not in self.buffer:
            chunk = self.sock.recv(4096)
            if not chunk:
                raise ConnectionError("connection closed by the server")
            self.buffer += chunk
        line, self.buffer = self.buffer.split(b"\n", 1)
        return json.loads(line.decode())

    def hello(self, client_id, last_seq=0):
        self.send({"type": "hello", "client_id": client_id, "last_seq": last_seq})
        return self.recv_line()

    def note(self, note_id, rubric, topic, event_time_ms=None):
        if event_time_ms is None:
            event_time_ms = int(time.time() * 1000)
        self.send(
            {
                "type": "note",
                "note_id": note_id,
                "rubric": rubric,
                "topic": topic,
                "event_time": event_time_ms,
            }
        )
        return self.recv_line()

    def ack(self, seq):
        self.send({"type": "ack", "seq": seq})

    def stats(self, client_id=None):
        message = {"type": "stats"}
        if client_id:
            message["client_id"] = client_id
        self.send(message)
        return self.recv_line()

    def bye(self):
        self.send({"type": "bye"})

    def no_more_messages(self, timeout=0.3):
        if self.buffer:
            return False
        self.sock.settimeout(timeout)
        try:
            chunk = self.sock.recv(4096)
        except socket.timeout:
            return True
        if not chunk:
            return True
        self.buffer += chunk
        return False

    def close(self):
        self.sock.close()
