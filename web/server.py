import http.server
import socketserver
import json
import re
import subprocess
from pathlib import Path
from urllib.parse import urlparse


PORT = 8000
MAX_INPUT_BYTES = 200_000
PROCESS_TIMEOUT = 60  # seconds (first call also builds the C++ processor)

SCRIPT_DIR = Path(__file__).resolve().parent
PROCESSOR_PATH = SCRIPT_DIR / "processor"
HTML_PATH = SCRIPT_DIR / "index.html"
SAMPLES_DIR = SCRIPT_DIR / "samples"


def ensure_samples_dir():
    SAMPLES_DIR.mkdir(parents=True, exist_ok=True)


def safe_sample_name(name):
    """Turn a user-provided name into a safe filename stem."""
    stem = re.sub(r'[^A-Za-z0-9_\-]+', '_', name.strip()).strip('_')
    return stem or "sample"


def list_samples():
    ensure_samples_dir()
    out = []
    for p in sorted(SAMPLES_DIR.glob("*.json")):
        try:
            data = json.loads(p.read_text(encoding="utf-8"))
        except Exception:
            continue
        out.append({
            "id": p.stem,
            "name": data.get("name", p.stem),
            "description": data.get("description", ""),
        })
    return out


def load_sample(sample_id):
    ensure_samples_dir()
    stem = safe_sample_name(sample_id)
    path = SAMPLES_DIR / f"{stem}.json"
    if not path.exists():
        raise FileNotFoundError(f"Sample '{sample_id}' not found")
    data = json.loads(path.read_text(encoding="utf-8"))
    return {
        "id": stem,
        "name": data.get("name", stem),
        "description": data.get("description", ""),
        "code": data.get("code", ""),
    }


def save_sample(name, description, code):
    ensure_samples_dir()
    if not name or not name.strip():
        raise ValueError("Sample name is required")
    stem = safe_sample_name(name)

    base = stem
    counter = 2
    while (SAMPLES_DIR / f"{stem}.json").exists():
        stem = f"{base}_{counter}"
        counter += 1

    payload = {
        "name": name.strip(),
        "description": description.strip(),
        "code": code,
    }
    (SAMPLES_DIR / f"{stem}.json").write_text(
        json.dumps(payload, indent=2, ensure_ascii=False),
        encoding="utf-8",
    )
    return {"id": stem, "name": payload["name"], "description": payload["description"]}


def build_processor():
    result = subprocess.run(
        ["make"],
        cwd=SCRIPT_DIR.parent,
        capture_output=True,
        text=True
    )
    if result.returncode != 0:
        raise RuntimeError(f"Build failed: {result.stderr}")
    if not PROCESSOR_PATH.exists():
        raise RuntimeError("Build succeeded but processor executable not found")


def process_text(text):
    """Send C++ source to the C++ processor and return its JSON response."""
    if not PROCESSOR_PATH.exists():
        build_processor()

    result = subprocess.run(
        [str(PROCESSOR_PATH)],
        input=text,
        capture_output=True,
        text=True,
        timeout=PROCESS_TIMEOUT
    )
    try:
        return json.loads(result.stdout)
    except json.JSONDecodeError:
        raise RuntimeError(
            f"C++ processor returned invalid JSON. stderr: {result.stderr[:500]}"
        )


class RequestHandler(http.server.SimpleHTTPRequestHandler):
    def _send_json(self, status, payload):
        self.send_response(status)
        self.send_header("Content-type", "application/json")
        self.end_headers()
        self.wfile.write(json.dumps(payload).encode("utf-8"))

    def do_GET(self):
        parsed = urlparse(self.path)

        if parsed.path == "/":
            self.send_response(200)
            self.send_header("Content-type", "text/html")
            self.end_headers()
            with open(HTML_PATH, "r", encoding="utf-8") as f:
                html = f.read()
            self.wfile.write(html.encode("utf-8"))
            return

        # Static assets served straight from SCRIPT_DIR by extension.
        STATIC_TYPES = {
            ".css": "text/css",
            ".js":  "application/javascript",
            ".ico": "image/x-icon",
            ".png": "image/png",
            ".svg": "image/svg+xml",
            ".gif": "image/gif",
            ".webp":"image/webp",
            ".woff":"font/woff",
            ".woff2":"font/woff2",
        }
        suffix = Path(parsed.path).suffix.lower()
        if suffix in STATIC_TYPES:
            rel = parsed.path.lstrip("/")
            file_path = (SCRIPT_DIR / rel).resolve()
            if SCRIPT_DIR in file_path.parents and file_path.is_file():
                self.send_response(200)
                self.send_header("Content-type", STATIC_TYPES[suffix])
                self.send_header("Content-Length", str(file_path.stat().st_size))
                self.end_headers()
                self.wfile.write(file_path.read_bytes())
                return

        if parsed.path == "/samples":
            try:
                self._send_json(200, {"samples": list_samples()})
            except Exception as e:
                self._send_json(500, {"ok": False, "error": str(e)})
            return

        if parsed.path.startswith("/samples/"):
            sample_id = parsed.path[len("/samples/"):]
            try:
                self._send_json(200, load_sample(sample_id))
            except FileNotFoundError as e:
                self._send_json(404, {"ok": False, "error": str(e)})
            except Exception as e:
                self._send_json(500, {"ok": False, "error": str(e)})
            return

        self.send_response(404)
        self.end_headers()

    def do_POST(self):
        parsed = urlparse(self.path)

        if parsed.path == "/process":
            content_length = int(self.headers.get('Content-Length', 0))
            post_data = self.rfile.read(content_length)

            try:
                data = json.loads(post_data)
                text = data.get('text', '')
                if not isinstance(text, str):
                    raise ValueError("'text' must be a string")

                if len(text) > MAX_INPUT_BYTES:
                    self._send_json(413, {
                        "ok": False,
                        "error": f"Input too large (limit {MAX_INPUT_BYTES} bytes)."
                    })
                    return

                result = process_text(text)
                self._send_json(200, result)

            except Exception as e:
                self._send_json(500, {"ok": False, "error": str(e)})
            return

        if parsed.path == "/samples":
            content_length = int(self.headers.get('Content-Length', 0))
            post_data = self.rfile.read(content_length)
            try:
                data = json.loads(post_data)
                name = data.get("name", "")
                description = data.get("description", "")
                code = data.get("code", "")
                if not isinstance(name, str) or not isinstance(description, str) or not isinstance(code, str):
                    raise ValueError("name, description, and code must be strings")
                result = save_sample(name, description, code)
                self._send_json(201, {"ok": True, "sample": result})
            except Exception as e:
                self._send_json(400, {"ok": False, "error": str(e)})
            return

        self.send_response(404)
        self.end_headers()

    def log_message(self, format, *args):
        print(f"[{self.log_date_time_string()}] {args[0]}")


if __name__ == "__main__":
    ensure_samples_dir()
    with socketserver.TCPServer(("", PORT), RequestHandler) as httpd:
        print(f"Server running at http://localhost:{PORT}")
        print(f"Processor: {PROCESSOR_PATH}")
        print(f"HTML: {HTML_PATH}")
        print(f"Samples: {SAMPLES_DIR}")
        print("Press Ctrl+C to stop")
        httpd.serve_forever()