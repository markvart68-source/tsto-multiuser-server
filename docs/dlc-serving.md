# DLC/static payload serving

The server serves locally supplied DLC payloads from the configured `dlc_directory`.

Default configuration:

```text
dlc_directory: dlc
```

Put DLC files under that directory and request them using either route:

```text
http://127.0.0.1:8080/static/<relative-file>
http://127.0.0.1:8080/dlc/<relative-file>
```

For example, `dlc/packages/content.zip` is available at `/static/packages/content.zip`.

Security behavior:

- absolute paths are rejected;
- `..` traversal is rejected after lexical normalization;
- symlinks resolving outside the DLC root are rejected when the platform can canonicalize them;
- missing files return HTTP 404;
- unsafe paths return HTTP 403;
- binary payloads are returned without text conversion;
- common ZIP, JSON, gzip, image, JavaScript, CSS, XML, and text content types are supported.

The server creates the configured DLC directory at startup if it does not exist. Payload files are not included in this repository; add only content you are authorized to distribute.
