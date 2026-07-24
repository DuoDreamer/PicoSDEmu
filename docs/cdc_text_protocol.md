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
  value may itself contain `=` so that padded base64 is representable.
- A line carries no implicit binary payload. Block data is one non-whitespace
  `data=` field encoded as base64 for normal use or hex for diagnostics.
- Command names, keys, values, and error codes use uppercase ASCII with digits,
  `_`, `.`, `-`, and `:` where appropriate. The generic parser permits other
  printable token characters; each command handler enforces its own schema.

## Requests and responses

Every programmatic request includes a decimal `id` field chosen by its sender.
Responses repeat that `id`, allowing the host program to correlate requests.

```text
HELLO id=1 version=0.1
OK id=1 version=0.1
ERR id=1 code=UNSUPPORTED_VERSION
```

The initial command set is `HELLO`, `GET_INFO`, `MOUNT`, `READ_BLOCKS`,
`WRITE_BLOCKS`, `FLUSH`, `EJECT`, `SET_BACKEND`, `GET_STATS`, and `STATUS`.
Commands whose behavior has not yet been implemented must return an explicit
`ERR` response rather than being ignored.

`READ_BLOCKS` and `WRITE_BLOCKS` use `lba`, `count`, and `encoding` fields.
Data-bearing responses and writes include `crc32`, expressed as eight uppercase
hexadecimal digits, calculated over decoded sector bytes. The maximum block
count will be declared by `GET_INFO`; the first implementation will use a small
bounded transfer limit.

## Examples

```text
GET_INFO id=2
OK id=2 present=1 type=SDSC blocks=131072 block_size=512 readonly=0
READ_BLOCKS id=3 lba=0 count=1 encoding=BASE64
OK id=3 lba=0 count=1 encoding=BASE64 crc32=00000000 data=AAAA...
FLUSH id=4
OK id=4
```

The example `data` value is deliberately abbreviated and is not a test vector.
All parser and encoding tests use project-owned inputs.
