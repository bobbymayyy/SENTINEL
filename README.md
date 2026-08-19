# SENTINEL 🛰️

*A lightweight Linux incident-response sensor that emits portable JSON telemetry without dragging an EDR-sized dependency tree behind it.*

SENTINEL is a small C-based host telemetry sensor for incident response, threat hunting, security monitoring, containers, and lab environments. It watches processes, sensitive files, and TCP listeners, preserves enough state to identify meaningful changes, and writes newline-delimited JSON to stdout for whatever pipeline you already use.

The operating idea remains simple:

> **Detect → Snapshot → Preserve → Enrich → Alert**

SENTINEL 0.2.0 is the first hardening and configurability iteration. It keeps the original polling architecture intentionally small while fixing state-tracking bugs, adding configurable watch paths and telemetry classes, and establishing real build/test CI.

## What 0.2.0 adds

- Dependency-free configuration using a deliberately small YAML subset.
- Configurable inotify watch paths, with transparent `/host` path mapping in sidecar mode.
- Per-class telemetry controls for process, network, and file events.
- Optional suppression of the initial baseline with `emit_baseline: false` / `--no-baseline`.
- One-shot collection with `--once` for fast triage and scripting.
- PID reuse-safe process tracking using `/proc/<pid>/stat` start time instead of a permanent PID bitmap.
- Stateful TCP listener tracking, including `network_listen` and `network_close` transitions rather than re-emitting every listener every poll.
- Correct IPv4 and IPv6 listener address decoding from `/proc/net/tcp*`.
- Correct inotify watch-descriptor-to-path mapping plus automatic re-arming when watched files are replaced.
- JSON schema and sensor version fields for downstream consumers.
- Hardened PIE builds, tests, cppcheck, GCC/Clang CI, and container-build CI.
- Build artifacts removed from source control and ignored going forward.
- Primary binary renamed to `sentinel`; `ir-sentinel` remains as a compatibility symlink.

## Telemetry

### Process observations

SENTINEL polls `/proc` and emits a `process_seen` event when it observes a PID/start-time pair that was not present in the previous scan. Using start time prevents PID reuse from hiding later processes.

```json
{"event":"process_seen","schema_version":1,"sensor_version":"0.2.0","pid":4123,"ppid":1,"uid":0,"start_ticks":12345678,"user":"root","comm":"bash","cmd":"bash -c id","time":"2026-08-18T21:30:00Z"}
```

This is intentionally called **process_seen**, not `process_exec`: polling can observe new processes, but it cannot guarantee capture of every exec between intervals. Netlink process events remain a later phase.

### File integrity events

Default watches cover:

- `/etc/passwd`
- `/etc/shadow`
- `/etc/group`
- `/etc/sudoers`
- `/etc/sudoers.d`
- `/etc/ssh`

Additional absolute paths can be supplied through YAML or repeated `--watch` options.

```json
{"event":"file_change","schema_version":1,"sensor_version":"0.2.0","path":"/etc/sudoers","action":"close_write","time":"2026-08-18T21:31:00Z"}
```

SENTINEL keeps the intended watch path even if a watched file is atomically replaced. Once the path reappears, the watch is armed again.

### TCP listener transitions

SENTINEL reads `/proc/net/tcp` and `/proc/net/tcp6`, maintains listener state between scans, and emits transitions.

```json
{"event":"network_listen","schema_version":1,"sensor_version":"0.2.0","proto":"tcp6","local_addr":"::1","local_port":4444,"inode":12345,"time":"2026-08-18T21:32:00Z"}
```

```json
{"event":"network_close","schema_version":1,"sensor_version":"0.2.0","proto":"tcp6","local_addr":"::1","local_port":4444,"inode":12345,"time":"2026-08-18T21:33:00Z"}
```

## Build

Requirements:

- Linux
- GCC or Clang
- GNU Make
- Kernel inotify support

```bash
make
make check
```

The primary binary is:

```bash
./sentinel
```

For compatibility with 0.1.0 workflows, `make` also creates:

```bash
./ir-sentinel -> sentinel
```

### Build hardening

The default Makefile enables stack protection, FORTIFY, PIE, RELRO, and immediate symbol binding while retaining `-Wall -Wextra -Wpedantic -Werror`.

## Configuration

Start from `sentinel.example.yaml`:

```yaml
interval_seconds: 2
monitor_processes: true
monitor_network: true
monitor_files: true
emit_baseline: true
default_watch_paths: true

watch_paths:
  - /etc/pam.d
  - /etc/cron.d
```

Validate it without starting the sensor:

```bash
./sentinel --config sentinel.example.yaml --check-config
```

Run with it:

```bash
./sentinel --config sentinel.example.yaml
```

The parser intentionally supports only the configuration SENTINEL needs: top-level scalar keys plus the `watch_paths` sequence. It rejects unknown keys and malformed values instead of silently guessing. YAML anchors, tags, nested mappings, escape processing, and multiline scalars are intentionally out of scope so configuration does not require a YAML library.

### CLI overrides

CLI options are applied after the config file, so they override file settings:

```text
--config FILE
--check-config
--host-roots
--interval SEC
--watch PATH
--no-default-watches
--processes / --no-processes
--network / --no-network
--files / --no-files
--baseline / --no-baseline
--once
--version
```

Examples:

```bash
# Snapshot current processes/listeners once and exit
./sentinel --once --no-files

# Watch only file changes, with no built-in paths
./sentinel --no-processes --no-network --no-default-watches \
  --watch /etc/passwd --watch /etc/systemd/system

# Establish state silently, then emit only later process/listener changes
./sentinel --no-baseline
```

## Container deployment

The container build uses Alpine 3.24 and runs as an unprivileged `sentinel` user.

```bash
docker build -t sentinel .

docker run --rm \
  --pid=host \
  --network=host \
  --read-only \
  --cap-add SYS_PTRACE \
  --cap-add DAC_READ_SEARCH \
  -v /proc:/host/proc:ro \
  -v /etc:/host/etc:ro \
  -v /var/log:/host/var/log:ro \
  sentinel --host-roots
```

Or:

```bash
docker compose up --build
```

In `--host-roots` mode, configured paths beginning with `/etc`, `/var/log`, or `/proc` are transparently translated to their `/host/...` mounts. That lets the same config file work natively and in the sidecar container.

## Output and integrations

Telemetry is newline-delimited JSON on stdout. Logs about startup, configuration, permissions, and degraded sensors go to stderr, so collectors can keep telemetry and operational diagnostics separate.

SENTINEL can feed:

- Splunk
- Security Onion
- Wazuh
- Loki
- ELK / OpenSearch pipelines
- Fluent Bit
- Syslog bridges
- Custom collectors and webhooks

## Architecture

```text
                  ┌──────────────────┐
                  │     SENTINEL     │
                  └────────┬─────────┘
                           │
          ┌────────────────┼────────────────┐
          │                │                │
          ▼                ▼                ▼
   /proc process      inotify file     /proc TCP
     state diff         watchers       state diff
          │                │                │
          └────────────────┼────────────────┘
                           ▼
                   NDJSON event stream
                           │
                           ▼
                         stdout
```

0.2.0 deliberately remains a polling sensor for process and socket discovery. Linux documents `/proc/net/tcp` and `/proc/net/tcp6` as deprecated in favor of `tcp_diag`; future netlink work should replace polling where the deployment environment permits it rather than layering a second pretend-real-time path on top.

## CI

The repository now has an actual GitHub Actions pipeline rather than only a roadmap sketch:

```text
Pull request / push
        │
        ├── GCC build + tests
        ├── Clang build + tests
        ├── cppcheck
        └── container build
```

Future release automation can add image scanning, SBOM generation, signed artifacts, GHCR publishing, and multi-architecture release builds when the release model is ready for them.

## Roadmap

### Phase 1 — sensor scaffold

- [x] Process discovery
- [x] File monitoring
- [x] Listening port detection
- [x] JSON logging
- [x] Container deployment

### Phase 2 — correctness and operational control

- [x] PID reuse-safe process state
- [x] Stateful TCP listener transitions
- [x] IPv6 listener decoding
- [x] Configurable watch paths
- [x] YAML-subset configuration
- [x] Telemetry-class filtering
- [x] Baseline suppression
- [x] One-shot collection
- [x] Build/test/static-analysis CI
- [ ] Netlink process events
- [ ] Netlink socket / `sock_diag` events

### Phase 3 — detection and evidence

- [ ] Rule engine
- [ ] Detection signatures
- [ ] Alert enrichment
- [ ] Process tree reconstruction
- [ ] Evidence preservation
- [ ] Artifact hashing and collection manifests

### Phase 4 — deployment and integration

- [ ] Distributed deployment
- [ ] Webhook integrations
- [ ] SIEM-specific outputs
- [ ] Ansible deployment role
- [ ] DIP / DIPx integration
- [ ] Signed release artifacts and SBOMs

## Project boundary

SENTINEL remains the lean C operational sensor. GARGOYLE can share schemas, tests, and behavioral expectations, but it remains a separate Rust security-engineering project rather than a stealth rewrite of SENTINEL.

## License

See `LICENSE.md`.
