#!/usr/bin/env python3
"""Manual real-browser contract test for the served Orator Web UI.

This tool verifies transport and browser-state mechanics only. It never assigns
semantic or speaker correctness. Playwright is a tools-only dependency and is
not imported by the C++/CUDA runtime.
"""

import argparse
import json
import os
import signal
import subprocess
import sys
import tempfile
import time
import urllib.error
import urllib.request

from playwright.sync_api import sync_playwright


def progress(stage):
    print(f"[browser] {stage}", flush=True)


def wait_http(url, process, timeout):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if process.poll() is not None:
            raise RuntimeError(f"server exited with code {process.returncode}")
        try:
            with urllib.request.urlopen(url, timeout=1) as response:
                if response.status == 200:
                    return
        except (urllib.error.URLError, TimeoutError):
            pass
        time.sleep(0.2)
    raise RuntimeError(f"server did not become ready at {url}")


def start_server(args, log_file):
    env = os.environ.copy()
    env["ORATOR_CONFIG"] = args.config
    process = subprocess.Popen(
        [args.server, str(args.port)],
        cwd=args.repo,
        env=env,
        stdout=log_file,
        stderr=subprocess.STDOUT,
        start_new_session=True,
    )
    wait_http(f"http://127.0.0.1:{args.port + 1}/", process,
              args.server_timeout)
    return process


def stop_server(process):
    if process is None or process.poll() is not None:
        return
    os.killpg(process.pid, signal.SIGINT)
    try:
        process.wait(timeout=10)
    except subprocess.TimeoutExpired:
        os.killpg(process.pid, signal.SIGTERM)
        try:
            process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            os.killpg(process.pid, signal.SIGKILL)
            process.wait(timeout=5)


def timeline_from_page(page):
    text = page.locator("#timelineView pre").inner_text()
    return json.loads(text)


def assert_terminal_contract(timeline):
    tracks = {track.get("kind"): track.get("entries", [])
              for track in timeline.get("tracks", [])}
    asr = tracks.get("asr", [])
    align = tracks.get("align", [])
    business = tracks.get("business_speaker", [])
    asr_ids = [entry.get("text_id") for entry in asr]
    align_ids = [entry.get("text_id") for entry in align]
    if len(asr_ids) != len(set(asr_ids)):
        raise AssertionError("terminal ASR IDs are not unique")
    if set(asr_ids) != set(align_ids):
        raise AssertionError("terminal ASR/alignment IDs differ")
    if business != timeline.get("comprehensive", []):
        raise AssertionError("business track differs from comprehensive alias")
    if not timeline.get("timebase_reconciled") or not timeline.get("timebase_ok"):
        raise AssertionError("terminal time-base reconciliation failed")
    if any(extent.get("gap_samples") != 0
           for extent in timeline.get("track_extents", [])):
        raise AssertionError("terminal track extent gap is nonzero")


def run_browser(args, log_file):
    progress("starting server")
    process = start_server(args, log_file)
    console_errors = []
    page_errors = []
    planned_disconnect = False
    ui_url = f"http://127.0.0.1:{args.port + 1}/"
    page = None

    def record_console(message):
        if message.type != "error":
            return
        text = message.text
        expected_refusal = (
            planned_disconnect
            and text.startswith("WebSocket connection to")
            and "ERR_CONNECTION_REFUSED" in text
        )
        if not expected_refusal:
            console_errors.append(text)

    try:
        with sync_playwright() as playwright:
            progress("launching Chromium")
            browser = playwright.chromium.launch(
                headless=not args.headed,
                args=[
                    "--use-fake-ui-for-media-stream",
                    "--use-fake-device-for-media-stream",
                ],
            )
            context = browser.new_context(
                accept_downloads=True,
                viewport={"width": 1440, "height": 1000},
            )
            page = context.new_page()
            page.on("console", record_console)
            page.on("pageerror", lambda error: page_errors.append(str(error)))
            page.goto(ui_url, wait_until="domcontentloaded")
            page.wait_for_function(
                "() => document.querySelector('#connBadge')?.textContent"
                ".includes('Connected')",
                timeout=args.browser_timeout,
            )
            progress("connected")

            page.set_input_files("#fileInput", args.audio)
            page.locator("#transcriptList .t-item").first.wait_for(
                timeout=args.stream_timeout)
            live_rows = page.locator("#transcriptList .t-item").count()
            progress(f"live transcript populated ({live_rows} rows)")
            page.wait_for_function(
                "() => document.querySelector('#statusLabel')?.textContent === "
                "'Complete'",
                timeout=args.stream_timeout,
            )
            progress("file streaming complete")
            page.wait_for_function(
                """() => {
                  const node = document.querySelector('#timelineView pre');
                  if (!node) return false;
                  try { return JSON.parse(node.textContent).audio_sec > 0; }
                  catch (_) { return false; }
                }""",
                timeout=args.browser_timeout,
            )
            progress("flush timeline received")

            if page.locator("#endBtn").is_disabled():
                raise AssertionError("End button is disabled after file completion")
            page.locator("#endBtn").click()
            progress("end command clicked")
            page.wait_for_function(
                """() => {
                  const node = document.querySelector('#timelineView pre');
                  if (!node) return false;
                  try {
                    const value = JSON.parse(node.textContent);
                    return value.timebase_reconciled === true;
                  } catch (_) { return false; }
                }""",
                timeout=args.browser_timeout,
            )
            progress("terminal timeline received")
            terminal = timeline_from_page(page)
            assert_terminal_contract(terminal)

            if page.locator("#obsPanel .obs-card").count() < 3:
                raise AssertionError("observability cards did not populate")
            if live_rows < 1:
                raise AssertionError("live transcript did not populate")

            with page.expect_download(timeout=args.browser_timeout) as download:
                page.locator("#downloadBtn").click()
            download_path = download.value.path()
            with open(download_path, "r", encoding="utf-8") as exported:
                exported_timeline = json.load(exported)
            if exported_timeline != terminal:
                raise AssertionError("downloaded JSON differs from rendered terminal")
            progress("download matches terminal")

            page.screenshot(path=args.desktop_screenshot, full_page=True)
            page.set_viewport_size({"width": 390, "height": 844})
            page.screenshot(path=args.mobile_screenshot, full_page=True)
            page.set_viewport_size({"width": 1440, "height": 1000})
            progress("desktop and mobile screenshots captured")

            matching_session = None
            session_deadline = time.monotonic() + args.browser_timeout / 1000.0
            while time.monotonic() < session_deadline:
                page.locator("#refreshSessionsBtn").click()
                page.wait_for_timeout(250)
                candidate = page.locator(
                    "#sessionList .session-item").filter(
                        has_text=f"{terminal.get('audio_sec', 0):.1f}s").first
                if candidate.count() > 0:
                    matching_session = candidate
                    break
            if matching_session is None:
                raise AssertionError("finalized session was not persisted")
            session_id = matching_session.locator(".session-item-id").inner_text()
            progress(f"persisted session found ({session_id})")

            page.locator("#clearBtn").click()
            page.locator("#refreshSessionsBtn").click()
            saved = page.locator("#sessionList .session-item").filter(
                has_text=session_id).first
            saved.wait_for(timeout=args.browser_timeout)
            saved.locator(".session-load-btn").click()
            page.wait_for_function(
                """expected => {
                  const node = document.querySelector('#timelineView pre');
                  if (!node) return false;
                  try { return JSON.parse(node.textContent).audio_sec === expected; }
                  catch (_) { return false; }
                }""",
                arg=terminal.get("audio_sec"),
                timeout=args.browser_timeout,
            )
            loaded = timeline_from_page(page)
            if loaded != terminal:
                raise AssertionError("loaded session differs from finalized timeline")
            progress("persisted session reloaded exactly")

            planned_disconnect = True
            stop_server(process)
            process = None
            page.wait_for_function(
                "() => document.querySelector('#connBadge')?.textContent"
                ".includes('Disconnected')",
                timeout=args.browser_timeout,
            )
            process = start_server(args, log_file)
            page.wait_for_function(
                "() => document.querySelector('#connBadge')?.textContent"
                ".includes('Connected')",
                timeout=args.server_timeout * 1000,
            )
            page.wait_for_function(
                """() => document.querySelector('#mAsrSeg')?.textContent === '0' &&
                         !document.querySelector('#timelineView pre')""",
                timeout=args.browser_timeout,
            )
            planned_disconnect = False
            progress("reconnect started a clean browser session")

            page.locator("#micBtn").click()
            page.wait_for_function(
                "() => document.querySelector('#micBtn')?.textContent"
                ".includes('Stop Mic')",
                timeout=args.browser_timeout,
            )
            page.wait_for_timeout(1500)
            page.locator("#micBtn").click()
            page.wait_for_function(
                "() => document.querySelector('#micBtn')?.textContent"
                ".includes('Start Mic')",
                timeout=args.browser_timeout,
            )
            progress("fake-device microphone start/stop passed")

            browser.close()

    except Exception:
        if page is not None:
            try:
                state = page.evaluate(
                    """() => ({
                      connection: document.querySelector('#connBadge')?.textContent,
                      status: document.querySelector('#statusLabel')?.textContent,
                      endDisabled: document.querySelector('#endBtn')?.disabled,
                      timeline: !!document.querySelector('#timelineView pre'),
                      liveRows: document.querySelectorAll('#transcriptList .t-item').length,
                    })""")
                print("[browser] failure state: " + json.dumps(state),
                      file=sys.stderr, flush=True)
                page.screenshot(
                    path="/tmp/orator_web_ui_failure.png", full_page=True)
            except Exception:
                pass
        raise
    finally:
        stop_server(process)

    if console_errors or page_errors:
        raise AssertionError(
            "browser errors: " + json.dumps(
                {"console": console_errors, "page": page_errors},
                ensure_ascii=False,
            ))
    return {
        "audio_sec": terminal.get("audio_sec"),
        "live_rows": live_rows,
        "asr_entries": len(next(
            (track.get("entries", []) for track in terminal.get("tracks", [])
             if track.get("kind") == "asr"), [])),
        "session_id": session_id,
        "desktop_screenshot": args.desktop_screenshot,
        "mobile_screenshot": args.mobile_screenshot,
    }


def parse_args():
    parser = argparse.ArgumentParser(
        description="Manual real-browser Orator Web UI contract test")
    parser.add_argument("--repo", default=os.getcwd())
    parser.add_argument("--server", default="./build/orator_ws")
    parser.add_argument("--config", default="orator.toml")
    parser.add_argument("--audio", required=True)
    parser.add_argument("--port", type=int, default=8875)
    parser.add_argument("--server-timeout", type=float, default=60.0)
    parser.add_argument("--browser-timeout", type=float, default=60000.0)
    parser.add_argument("--stream-timeout", type=float, default=180000.0)
    parser.add_argument("--headed", action="store_true")
    parser.add_argument(
        "--desktop-screenshot",
        default="/tmp/orator_web_ui_desktop.png")
    parser.add_argument(
        "--mobile-screenshot",
        default="/tmp/orator_web_ui_mobile.png")
    return parser.parse_args()


def main():
    args = parse_args()
    args.repo = os.path.abspath(args.repo)
    args.server = os.path.abspath(args.server)
    args.config = os.path.abspath(args.config)
    args.audio = os.path.abspath(args.audio)
    with tempfile.NamedTemporaryFile(
            prefix="orator_web_ui_server_", suffix=".log", delete=False) as log:
        log_path = log.name
        result = run_browser(args, log)
    result["server_log"] = log_path
    print(json.dumps(result, ensure_ascii=False, indent=2))
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except Exception as error:
        print(f"ERROR: {error}", file=sys.stderr)
        sys.exit(1)
