import http.server
import socketserver
import json
import subprocess
from pathlib import Path
from urllib.parse import urlparse


PORT = 8000

SCRIPT_DIR = Path(__file__).resolve().parent
PROCESSOR_PATH = SCRIPT_DIR / "processor"
HTML_PATH = SCRIPT_DIR / "index.html"


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
    if not PROCESSOR_PATH.exists():
        build_processor()

    result = subprocess.run(
        [str(PROCESSOR_PATH)],
        input=text,
        capture_output=True,
        text=True,
        timeout=5
    )
    return json.loads(result.stdout)


class RequestHandler(http.server.SimpleHTTPRequestHandler):
    def do_GET(self):
        parsed = urlparse(self.path)

        if parsed.path == "/":
            self.send_response(200)
            self.send_header("Content-type", "text/html")
            self.end_headers()
            with open(HTML_PATH, "r") as f:
                html = f.read()
            self.wfile.write(html.encode())
        else:
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
                result = process_text(text)

                self.send_response(200)
                self.send_header("Content-type", "application/json")
                self.end_headers()
                self.wfile.write(json.dumps(result).encode())

            except Exception as e:
                self.send_response(500)
                self.send_header("Content-type", "application/json")
                self.end_headers()
                self.wfile.write(json.dumps({"error": str(e)}).encode())
        else:
            self.send_response(404)
            self.end_headers()

    def log_message(self, format, *args):
        print(f"[{self.log_date_time_string()}] {args[0]}")


if __name__ == "__main__":
    with socketserver.TCPServer(("", PORT), RequestHandler) as httpd:
        print(f"Server running at http://localhost:{PORT}")
        print(f"Processor: {PROCESSOR_PATH}")
        print(f"HTML: {HTML_PATH}")
        print("Press Ctrl+C to stop")
        httpd.serve_forever()
