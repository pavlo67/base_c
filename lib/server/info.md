# Server adapters

`server.h` defines shared request/response/callback types.

## Mongoose

`mongoose/mngs.h` exposes `startServer()` (bool), HTTP/WebSocket registration,
`stopServer()` and `waitForServerStopped()`. One internal event-loop thread owns
the listener and executes handlers. IPv4 input is in host byte order; start waits
for listener success/failure. Register handlers before starting.

HTTP routing is exact-path, unknown routes return 404, and handlers fill a response
from a copied request. WebSocket handlers receive text and send a reply only when
the response string is nonempty. Blocking handlers block the event loop.

Stop requests exit without joining; the owner must subsequently wait/join and reset
server state. This two-phase shutdown permits another thread to request stop while
integration waits for completion. Stopping an inactive server is harmless.
`server_http_mngs_test` covers HTTP/WebSocket behavior and scoped cleanup.

## cpp-httplib

`cpp-httplib/http.h` offers the simpler HTTP-only `startServerHTTP()`, `addHandler()`
and `stopServerHTTP()`. Start waits for bind and launches an internal listener;
stop joins it. IPv4 is host-order. GET handlers also serve HEAD without a body.
Examples live in each adapter's `_example/`; `http_test` exercises loopback HTTP.
