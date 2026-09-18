# HTTP server configuration

The server now exposes:

- `GET /webpanel/` — player panel
- `GET /webpanel/index.html` — panel HTML
- `GET /webpanel/app.js` — panel JavaScript
- `GET /webpanel/style.css` — panel stylesheet
- `/v1/*` — API routes dispatched to `ApiRouter`

Start the server and open:

```text
http://127.0.0.1:8080/webpanel/
```

The adapter is intentionally small and dependency-free. It supports HTTP/1.1 request lines, `Authorization`, `Content-Length`, JSON request bodies, static panel files, and JSON API responses. Put a production reverse proxy such as nginx or Caddy in front of it for TLS, request limits, access logs, and HTTP hardening.

The current implementation handles connections sequentially. For multiple concurrent players, place it behind a reverse proxy or replace the accept loop with a worker pool/asynchronous transport before production deployment.
