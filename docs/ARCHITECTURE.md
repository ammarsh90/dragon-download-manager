# Architecture

## Boundaries

- `MainWindow` owns navigation, categories, search and queue actions. `mainwindowdownloads.cpp` contains this download-focused interaction layer.
- `AddUrlsDialog` validates input, resolves name conflicts against disk and unfinished jobs, and appends a `JobFile`.
- `DownloadProgressDialog` observes an `AbstractJob`. Its QPointer and the job destruction connection prevent stale job references.
- `Scheduler` owns jobs, concurrency and session snapshots. `Session` retains the selected connection count and full destination name.
- `JobFile` bridges a `SegmentedTransfer` to the existing scheduler API and atomically commits the final file using `File` / QSaveFile.
- `SegmentedTransfer` handles the HTTP probe, validated ranges, retries, per-connection progress and bounded reply buffers. `transfercheckpoint.cpp` handles durable metadata.
- `NetworkManager` supplies configured user agent, proxy, TLS, referrer, redirect policy and request timeout to all requests.

## Data safety

A SHA-256 of the original URL and exact destination names a private application-data cache directory. It contains `payload.part` plus an atomically saved `state.json`. Each segment records its start, end and successfully written length. Payload is flushed before its checkpoint is committed. Resume probes the remote representation first and verifies size and validator before trusting cached ranges. Response Content-Range and validators are checked before writing bytes. A full HTTP 200 body is never appended to a range.

The final destination uses a separate QSaveFile. Copying occurs in 1 MiB event-loop steps to keep the UI responsive. Only after commit succeeds are the two transfer cache files removed. The original destination is retained until successful replacement. Final copying temporarily requires disk space for both cache and destination.

## Concurrency

Qt network requests and disk writes run in the GUI event loop; there are no shared writable files across worker threads. Each response writes only its assigned range. QPointer guards reply lifetime. Generation counters invalidate delayed retries and assembly callbacks when paused or restarted.

## Known limitations

Fixed segments, no dynamic redistribution. Cross-request recovery requires strong ETag or Last-Modified. Single-stream servers restart from the beginning. Cache files may remain after removing an unfinished job; they are intentionally retained rather than deleted recursively. The basic scheduled start requires the app to remain running.

## Compact interface and notification area (1.2.1)

Progress details are collapsed initially. Long names and URLs are elided with full text in tooltips. MainWindow closeEvent hides the main window and owned progress windows only when a configured, available QSystemTrayIcon can restore them. SystemTray synchronizes quitOnLastWindowClosed with availability and user preference. Explicit Exit uses QCoreApplication::quit; aboutToQuit saves settings. Without an available notification area, normal close/quit behavior remains enabled.

## Browser integration (1.3)

The Chromium MV3 worker observes direct GET downloads, pauses a browser download and sends a bounded JSON request through DragonBrowserHost. The host uses Native Messaging length-prefixed JSON over stdin/stdout, then a user-scoped QLocalSocket. BrowserBridge validates the request, reserves a unique destination, appends a paused JobFile and atomically saves Session before acknowledgement. Browser request IDs are persisted for idempotent retries. Snapshot retains unfinished browser-owned jobs even when legacy auto-remove settings are enabled.

Only after durable acknowledgement does the extension cancel its browser download. Failed handoffs try browser resume and bypass that download for the current worker lifetime. A persisted pending ledger helps service-worker recovery. Queued forms are serialized to avoid nested browser dialogs. Existing jobs are edited by AddUrlsDialog rather than appended twice.

Each JobFile owns its NetworkManager, cookie jar and browser User-Agent override. Cookies are scoped by domain/path through QNetworkCookieJar and remain in memory. Session stores only the browser request ID, URL, file metadata and ordinary job state. The new native-host name and stable public extension key isolate Dragon from original upstream registration.
