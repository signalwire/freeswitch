# mod_logfile

`mod_logfile` writes selected FreeSWITCH log records to a file. The default
file is `freeswitch.log` in the FreeSWITCH log directory unless `logfile` is
configured.

## Structured prefixes

`uuid`, `log-tags`, and `channel-vars` are emitted in that order and are
prepended to every physical line. `log-tags` defaults to false and
`channel-vars` defaults to empty. `uuid` retains the historical default of
true; set it to false to omit the UUID.

The UUID is emitted as a bare token. Log tags and channel variables are
emitted as `name:value` tokens, with a space after each token. Only available
values produce tokens. For example, an enabled profile can produce:

```text
550e8400-e29b-41d4-a716-446655440000 tenant:acme callid:abc123 message
```

Log tags are captured in the `switch_log_node_t` when the log record is
created, so the captured values travel with a queued log node. The prefix is
then applied to every physical line in that record. Existing message bytes
and final-newline behavior are preserved; `mod_logfile` does not truncate the
message.

## set_log_tag

Use `<action application="set_log_tag" data="tenant=acme"/>` to set or
replace a tag. Use `tenant=` or `tenant` to remove it. Values may contain
additional `=` characters: the input is split at the first `=` only.

Tags are opt-in for the file logger with `log-tags="true"`:

```xml
<param name="log-tags" value="true"/>
```

## Channel variables

`channel-vars="callid=sip_call_id,tenant"` maps `sip_call_id` to `callid`
and uses `tenant` as both label and variable name. Values are read live while
the session exists. Use log tags for stable fields that must survive session
destruction or avoid per-log session lookup.

## Safety limits

Names are limited to 128 bytes and values to 512 bytes. Unsafe name bytes,
whitespace/control bytes in values, and brackets are replaced with `_`.
These limits apply to token names and values, not to the log message:
messages are not limited to 2048 bytes and are not truncated by this module.

## File rotation

`rollover` sets the file-size threshold in bytes; `0` disables size-based
rotation. When `maximum-rotate` is omitted, `max_rot` remains `0` and rotated
files use a timestamp and an index (`logfile.YYYY-MM-DD-HH-MM-SS.N`). A nonzero
`maximum-rotate` value of `N` keeps numbered files up to the configured count
(`logfile.1` through `logfile.N`). If `maximum-rotate` is explicitly `0` (or
parses as zero), it is normalized to `MAX_ROT` (`4096`), so numbered rotation
is kept up to `4096` files.

`rotate-on-hup` controls SIGHUP handling. When true, SIGHUP rotates each
profile; when false, it closes and reopens each file. The sample configuration
enables rotation on HUP and sets a 10 MiB rollover threshold.

## Mappings

Each `<map>` selects log levels for `all`, a source file, a function, or a
`file:function` pair. Its `value` is a comma-separated list of `debug`,
`info`, `notice`, `warning`, `err`, `crit`, `alert`, or `all`. The sample maps
all listed levels for all sources.
