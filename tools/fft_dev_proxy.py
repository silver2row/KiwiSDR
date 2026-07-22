#!/usr/bin/env python3
"""Dev proxy: serve local FFT extension files, forward everything else to a KiwiSDR.

Usage:
  python3 tools/fft_dev_proxy.py [listen_port] [upstream_host:port]

Browse http://127.0.0.1:<port>/ so the Peaks UI comes from this repo while
SND/WF websockets still come from the upstream Kiwi.
"""

from __future__ import annotations

import asyncio
import mimetypes
import sys
from pathlib import Path

from aiohttp import ClientSession, ClientTimeout, TCPConnector, WSMsgType, web

REPO = Path(__file__).resolve().parents[1]
FFT_DIR = REPO / "web" / "extensions" / "FFT"

# URI path -> local file (tried in order; first existing wins for gzip preference)
OVERRIDES = {
    "/extensions/FFT/FFT.min.js.gz": FFT_DIR / "FFT.min.js.gz",
    "/extensions/FFT/FFT.min.js": FFT_DIR / "FFT.min.js",
    "/extensions/FFT/FFT.js": FFT_DIR / "FFT.js",
    "/extensions/FFT/FFT.min.css": FFT_DIR / "FFT.min.css",
    "/extensions/FFT/FFT.css": FFT_DIR / "FFT.css",
}


def local_file_for(path: str) -> Path | None:
    p = OVERRIDES.get(path)
    if p and p.is_file():
        return p
    # also allow other files under the FFT extension dir
    if path.startswith("/extensions/FFT/"):
        cand = FFT_DIR / path[len("/extensions/FFT/") :]
        if cand.is_file() and FFT_DIR in cand.resolve().parents:
            return cand
    return None


async def handle_http(request: web.Request) -> web.StreamResponse:
    upstream: str = request.app["upstream"]
    path = request.rel_url.path
    local = local_file_for(path)
    if local is not None:
        data = local.read_bytes()
        ctype = mimetypes.guess_type(str(local).replace(".gz", ""))[0] or "application/octet-stream"
        headers = {
            "Content-Type": ctype,
            "Cache-Control": "no-store",
            "Access-Control-Allow-Origin": "*",
        }
        if str(local).endswith(".gz"):
            headers["Content-Encoding"] = "gzip"
            headers["Content-Type"] = "application/javascript"
        print(f"LOCAL  {path} -> {local} ({len(data)} bytes)")
        return web.Response(body=data, headers=headers)

    # Memory cache for static GETs — BBB drops under parallel asset storms
    cache: dict = request.app["http_cache"]
    if request.method == "GET" and path in cache:
        status, body, out_headers = cache[path]
        print(f"CACHE  {path} ({len(body)} bytes)")
        return web.Response(status=status, body=body, headers=out_headers)

    url = f"http://{upstream}{request.rel_url.path_qs}"
    # Drop conditional cache validators: a proxied 304 with empty body breaks
    # browsers that do not reuse their cache the way a direct Kiwi hit would.
    skip = {"host", "content-length", "if-none-match", "if-modified-since", "if-range"}
    headers = {k: v for k, v in request.headers.items() if k.lower() not in skip}
    headers["Host"] = upstream.split("@")[-1]
    session: ClientSession = request.app["session"]
    sem: asyncio.Semaphore = request.app["http_sem"]
    req_body = await request.read() if request.can_read_body else None
    last_err = None
    for attempt in range(4):
        try:
            async with sem:
                async with session.request(
                    request.method,
                    url,
                    headers=headers,
                    data=req_body,
                    allow_redirects=False,
                ) as resp:
                    body = await resp.read()
                    out_headers = {
                        k: v
                        for k, v in resp.headers.items()
                        if k.lower()
                        not in (
                            "transfer-encoding",
                            "content-encoding",
                            "content-length",
                            "connection",
                        )
                    }
                    print(f"PROXY  {request.method} {path} -> {resp.status} ({len(body)} bytes)")
                    if (
                        request.method == "GET"
                        and resp.status == 200
                        and path.startswith(("/kiwi/", "/pkgs/", "/openwebrx", "/extensions/", "/config/", "/audio", "/ima_"))
                    ):
                        cache[path] = (resp.status, body, out_headers)
                    return web.Response(status=resp.status, body=body, headers=out_headers)
        except Exception as e:
            last_err = e
            print(f"ERROR  {path} (try {attempt+1}): {e}")
            await asyncio.sleep(0.25 * (attempt + 1))
    return web.Response(status=502, text=f"upstream error: {last_err}")


async def handle_ws(request: web.Request) -> web.WebSocketResponse:
    upstream: str = request.app["upstream"]
    path_qs = request.rel_url.path_qs
    protos = request.headers.getall("Sec-WebSocket-Protocol", [])
    ws_server = web.WebSocketResponse(protocols=protos)
    await ws_server.prepare(request)

    session: ClientSession = request.app["session"]
    url = f"http://{upstream}{path_qs}"
    headers = {k: v for k, v in request.headers.items() if k.lower() in ("origin", "cookie", "user-agent")}
    print(f"WS     open {path_qs}")

    try:
        async with session.ws_connect(url, headers=headers, protocols=protos) as ws_client:

            async def client_to_server():
                async for msg in ws_server:
                    if msg.type == WSMsgType.TEXT:
                        await ws_client.send_str(msg.data)
                    elif msg.type == WSMsgType.BINARY:
                        await ws_client.send_bytes(msg.data)
                    elif msg.type in (WSMsgType.CLOSE, WSMsgType.ERROR):
                        break

            async def server_to_client():
                async for msg in ws_client:
                    if msg.type == WSMsgType.TEXT:
                        await ws_server.send_str(msg.data)
                    elif msg.type == WSMsgType.BINARY:
                        await ws_server.send_bytes(msg.data)
                    elif msg.type in (WSMsgType.CLOSE, WSMsgType.ERROR):
                        break

            await asyncio.gather(client_to_server(), server_to_client())
    except Exception as e:
        print(f"WS ERR {path_qs}: {e}")
    finally:
        if not ws_server.closed:
            await ws_server.close()
        print(f"WS     close {path_qs}")
    return ws_server


async def dispatch(request: web.Request) -> web.StreamResponse:
    if request.headers.get("Upgrade", "").lower() == "websocket":
        return await handle_ws(request)
    return await handle_http(request)


async def on_startup(app: web.Application):
    # Serialize upstream HTTP: Beagle/Mongoose drops parallel GETs
    app["http_sem"] = asyncio.Semaphore(1)
    app["http_cache"] = {}
    connector = TCPConnector(limit=8, limit_per_host=8, ttl_dns_cache=60, force_close=True)
    timeout = ClientTimeout(total=60, connect=15, sock_read=60)
    app["session"] = ClientSession(
        connector=connector, timeout=timeout, auto_decompress=True
    )


async def on_cleanup(app: web.Application):
    await app["session"].close()


def main():
    listen = int(sys.argv[1]) if len(sys.argv) > 1 else 8073
    upstream = sys.argv[2] if len(sys.argv) > 2 else "fdvtest.kiwisdr.com:8073"
    for p in OVERRIDES.values():
        if not p.is_file():
            print(f"warning: missing {p}")
    app = web.Application()
    app["upstream"] = upstream
    app.router.add_route("*", "/{tail:.*}", dispatch)
    app.on_startup.append(on_startup)
    app.on_cleanup.append(on_cleanup)
    print(f"FFT dev proxy  http://127.0.0.1:{listen}/  ->  http://{upstream}/")
    print(f"Local FFT dir  {FFT_DIR}")
    web.run_app(app, host="127.0.0.1", port=listen, print=None)


if __name__ == "__main__":
    main()
