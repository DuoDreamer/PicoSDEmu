# USB CDC text protocol

This is the initial PicoSDEmu host-control and block-service protocol. It runs
over the Pico's USB CDC serial port. It is deliberately line-oriented so that a
terminal can inspect and issue commands, while the `picosd-host` program is the
normal image-serving peer.

## Framing and character set

- Each request or response is one ASCII line terminated by `\n`; a receiver may
  accept an immediately preceding `\r` as part of transport line handling.
- `parse_text_line` receives the completed line **without** the terminator. It
  rejects embedded `\r` or `\n`, control bytes, non-ASCII bytes, malformed
  fields, and more than 16 tokens.
- Tokens are separated by ASCII spaces or tabs. The first token is the command;
  remaining tokens use `key=value` form. Keys and values must be nonempty; a
  value may itself contain `=` so that padded base64 is representable. Field
  keys must be unique; duplicate keys make the entire line invalid rather than
  allowing different components to select different values.
- A line carries no implicit binary payload. Block data is one non-whitespace
  `data=` field encoded as base64 for normal use or hex for diagnostics.
- Command names, keys, values, and error codes use uppercase ASCII with digits,
  `_`, `.`, `-`, and `:` where appropriate. The generic parser permits other
  printable token characters; each command handler enforces its own schema.

Host serial reads are non-blocking. An incomplete line is retained across
polls and reported internally as `WouldBlock`; it is not treated as malformed
input or a disconnected device. This distinction allows fragmented and
coalesced USB CDC transfers to use the same line parser. An unterminated line
that exceeds the maximum length closes the host endpoint, bounding retained
input and requiring a clean reconnect rather than attempting stream recovery.

## Requests and responses

Every programmatic request includes a decimal `id` field chosen by its sender.
Responses repeat that `id`, allowing the host program to correlate requests.
IDs are positive 64-bit integers and must increase within a negotiated session.
The host rejects zero, non-decimal, overflowing, duplicate, or decreasing IDs
with `BAD_ID` or `STALE_ID`. Requests with a missing or incorrect session do
not advance the accepted ID, so unrelated traffic cannot invalidate the active
peer's next request.

```text
HELLO id=1 version=0.1
OK id=1 version=0.1 session=839201
ERR id=1 code=UNSUPPORTED_VERSION
```

`HELLO` is the only request accepted before negotiation. The host rejects an
unsupported or missing `version`; a successful response assigns an opaque
`session` value. Every later request must repeat that value, and successful
responses repeat it as well. A missing or stale value is rejected with
`MISSING_SESSION` or `BAD_SESSION`, preventing delayed requests from a previous
host process from being applied to a newly opened image.

The portable `CdcSessionClient` implements the peer-side sequencing policy: it
permits one outstanding request, correlates response IDs and session values,
and retains a pending request when unrelated or malformed input arrives. A
transport disconnect calls `reset()`, after which request numbering restarts at
one and a new `HELLO` exchange is required. The host must create a new
`SessionDispatcher` for the reopened transport so its accepted request sequence
and assigned session are reset at the same boundary.

Portable `decode_get_info_response` and `decode_read_block_response` helpers
convert correlated `OK` lines into typed metadata and 512-byte blocks. The read
decoder requires `BASE64`, a single block, an eight-digit CRC32, and exactly
512 decoded bytes before exposing the payload.

`CdcSessionClient` also provides typed builders for metadata, single-block
reads and writes, flush, and eject. The write builder always emits one 512-byte
base64 payload and calculates its uppercase eight-digit CRC32, preventing
callers from constructing inconsistent length, encoding, or checksum fields.
For a correlated `ERR`, the client exposes the required `code` field through
both a `CdcRemoteError` classification and `remote_error_code()` until the next
request begins or the session is reset. Unknown future codes map to `Unknown`
while retaining their original text for forward-compatible diagnostics.
`retry_advice()` classifies `IO_ERROR` for a bounded same-session retry,
session-state errors for renegotiation, and `NO_MEDIA` as unavailable media.
Invalid requests, unsupported operations, write-policy failures, checksum
failures, and unknown future errors are never retried automatically.

`CdcRetryController` turns that advice into a bounded exponential-backoff
schedule. Configuration supplies a maximum retry count plus initial and maximum
delays in caller-defined monotonic ticks. Backoff and target timestamps saturate
instead of overflowing; a successful operation resets the retry budget.

Transport timeout policy may call `cancel_pending_request()`. Cancellation
abandons only the outstanding correlation state, never reuses its request ID,
and leaves an established session available for the next request. A response
that arrives after cancellation is therefore rejected as mismatched. A full
disconnect still uses `reset()` and requires a new handshake.

`CdcRequestDeadline` supplies clock-agnostic timeout policy around that API.
Callers arm it with monotonic ticks and a nonzero timeout after transmitting a
request; polling at or after the saturated deadline cancels the pending request.
If a response already completed the request, polling only clears the deadline.
Retryable timeouts use bounded backoff and a fresh monotonically increasing
request ID in the existing negotiated session. A response for the cancelled ID
remains stale and cannot complete the replacement request.

CDC packet boundaries have no protocol meaning. Receivers retain incomplete
lines across reads and independently drain multiple newline-terminated lines
from one read. Native pseudo-terminal integration tests exercise fragmented
CRLF handshakes and coalesced request/response pairs through the real POSIX
transport and host dispatcher. If a line exceeds the transport limit before a
terminator arrives, the receiver reports the oversized line, discards bytes
through its next newline, and resumes parsing subsequent lines without closing
an otherwise healthy connection.

The initial command set is `HELLO`, `GET_INFO`, `MOUNT`, `READ_BLOCKS`,
`WRITE_BLOCKS`, `FLUSH`, `EJECT`, `SET_BACKEND`, `GET_STATS`, and `STATUS`.
Commands whose behavior has not yet been implemented must return an explicit
`ERR` response rather than being ignored.

`READ_BLOCKS` and `WRITE_BLOCKS` use `lba`, `count`, and `encoding` fields.
Data-bearing responses and writes include `crc32`, expressed as eight uppercase
hexadecimal digits, calculated over decoded sector bytes. The maximum block
count will be declared by `GET_INFO`; the first implementation will use a small
bounded transfer limit.

The initial host returns `RANGE` when a single-block LBA is outside the image,
`READ_ONLY` when writes are disabled, `BAD_DATA` or `BAD_CRC` before modifying
invalid write input, and `NO_MEDIA` for commands issued after `EJECT`. `FLUSH`
commits buffered image data before returning `OK`; `EJECT` flushes first and
then makes later media operations fail with `NO_MEDIA`.

## Examples

```text
GET_INFO id=2 session=839201
OK id=2 session=839201 present=1 type=SDSC blocks=131072 block_size=512 readonly=0
READ_BLOCKS id=3 session=839201 lba=0 count=1 encoding=BASE64
OK id=3 session=839201 lba=0 count=1 encoding=BASE64 crc32=00000000 data=AAAA...
FLUSH id=4 session=839201
OK id=4 session=839201
```

The example `data` value is deliberately abbreviated and is not a test vector.
All parser and encoding tests use project-owned inputs.
