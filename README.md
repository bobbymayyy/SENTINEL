# SENTINEL 🛰️

**A small Linux incident-response sensor written in C, built to emit useful host telemetry without dragging an EDR-sized dependency tree behind it.**

SENTINEL watches processes, sensitive files, and TCP listeners, maintains lightweight state between scans, and emits newline-delimited JSON to stdout. It is designed for incident response, threat hunting, lab systems, containers, disconnected environments, and infrastructure where a compact operational sensor is more useful than a heavyweight endpoint platform.

> **Detect → Snapshot → Preserve → Enrich → Alert**

## Current status

**Version:** `0.2.0`

SENTINEL 0.2.0 is the first major correctness, configurability, and engineering-hardening iteration. The sensor is intentionally still small and polling-based for process and socket discovery, but the state model is substantially safer than the original scaffold and the repository now has repeatable GCC, Clang, static-analysis, and container validation through GitHub Actions.

The container smoke test used by CI is intentionally simple:

```bash
docker run --rm sentinel:ci --version
```

Expected output:

```text
SENTINEL 0.2.0
```

## Design goals

SENTINEL favors:

- **Small operational footprint** - C, libc, Linux interfaces, and no runtime YAML library.
- **Portable telemetry** - NDJSON on stdout, operational diagnostics on stderr.
- **Graceful degradation** - unavailable sensors should warn rather than invent state.
- **Explicit semantics** - polling observations are called observations, not fake real-time exec events.
- **Container awareness** - the same configuration can work natively or through `/host/...` mounts.
- **Defensive state tracking** - process and listener identity are preserved across polling cycles.
- **Reviewable engineering** - strict compiler flags, tests, static analysis, and bounded CI jobs.

## What 0.2.0 includes

### Process telemetry

SENTINEL scans `/proc` and identifies processes by **PID plus process start time** from `/proc/<pid>/stat`.

That matters because Linux eventually reuses PIDs. Tracking only the integer PID can silently hide a later process that receives the same number.

Newly observed process identities emit:

```json
{"event":"process_seen","schema_version":1,"sensor_version":"0.2.0","pid":4123,"ppid":1,"uid":0,"start_ticks":12345678,"user":"root","comm":"bash","cmd":"bash -c id","time":"2026-08-18T21:30:00Z"}
```

The event is intentionally named `process_seen`. Polling cannot guarantee capture of every `exec` that occurs between intervals.

### File integrity telemetry

SENTINEL uses inotify for file and directory watches.

Built-in paths:

- `/etc/passwd`
- `/etc/shadow`
- `/etc/group`
- `/etc/sudoers`
- `/etc/sudoers.d`
- `/etc/ssh`

Additional absolute paths can be supplied through configuration or repeated `--watch` arguments.

Example:

```json
{"event":"file_change","schema_version":1,"sensor_version":"0.2.0","path":"/etc/sudoers","action":"close_write","time":"2026-08-18T21:31:00Z"}
```

The watcher keeps desired paths separately from inotify watch descriptors. If a watched file is replaced atomically, SENTINEL detects the invalidated watch and attempts to arm it again after the path reappears.

### TCP listener telemetry

SENTINEL currently reads `/proc/net/tcp` and `/proc/net/tcp6`, snapshots active listening sockets, and diffs that state between polling cycles.

New listener:

```json
{"event":"network_listen","schema_version":1,"sensor_version":"0.2.0","proto":"tcp6","local_addr":"::1","local_port":4444,"inode":12345,"time":"2026-08-18T21:32:00Z"}
```

Listener disappearance:

```json
{"event":"network_close","schema_version":1,"sensor_version":"0.2.0","proto":"tcp6","local_addr":"::1","local_port":4444,"inode":12345,"time":"2026-08-18T21:33:00Z"}
```

The state tracker avoids re-emitting every active listener on every cycle and preserves prior protocol state if one `/proc/net` source temporarily cannot be read.

## 0.2.0 correctness fixes

The 0.2.0 line includes several fixes that materially change behavior from the original scaffold:

- Replaced permanent PID-bit tracking with PID + start-time identities so PID reuse is handled correctly.
- Corrected inotify descriptor mapping instead of assuming watch descriptors were sequential array indexes.
- Added automatic watch re-arming after atomic replacement or invalidation.
- Replaced repeated listener snapshots with `network_listen` and `network_close` transitions.
- Corrected IPv4 and IPv6 decoding from `/proc/net/tcp*`.
- Preserved previous listener state when an individual protocol scan fails, avoiding false close storms.
- Guarded empty process and listener state operations so sorting/search helpers are not invoked on absent state.
- Added structured schema and sensor version fields to emitted telemetry.
- Replaced legacy `ir-sentinel` naming internally while retaining `ir-sentinel` as a compatibility symlink.
- Removed compiled objects and binaries from source control.
- Normalized remaining legacy header guards.

## Configuration

SENTINEL uses a deliberately small, dependency-free YAML subset.

Example `sentinel.example.yaml`:

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

Validate configuration without starting the sensor:

```bash
./sentinel --config sentinel.example.yaml --check-config
```

Run with configuration:

```bash
./sentinel --config sentinel.example.yaml
```

Supported configuration keys:

| Key | Purpose |
|---|---|
| `interval_seconds` | Polling interval from 1 to 86400 seconds |
| `monitor_processes` | Enable process observations |
| `monitor_network` | Enable TCP listener observations |
| `monitor_files` | Enable inotify file observations |
| `emit_baseline` | Emit the first process/listener snapshot |
| `default_watch_paths` | Enable built-in `/etc` watches |
| `watch_paths` | Additional absolute watch paths |

Unknown keys and malformed values are rejected. YAML anchors, tags, nested mappings, multiline scalars, and general-purpose YAML features are intentionally outside this parser's scope.

## CLI

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
-h / --help
```

CLI options are applied after the configuration file and therefore override file settings.

Useful patterns:

```bash
# One-shot process and listener snapshot
./sentinel --once --no-files

# File-only sensor with explicitly selected paths
./sentinel --no-processes --no-network --no-default-watches \
  --watch /etc/passwd \
  --watch /etc/systemd/system

# Establish process/listener state silently, then report changes
./sentinel --no-baseline
```

## Build and test

Requirements:

- Linux
- GCC or Clang
- GNU Make
- Kernel inotify support

Build:

```bash
make
```

Build and run tests:

```bash
make check
```

Outputs:

```text
./sentinel
./ir-sentinel -> sentinel
```

The default build enables:

- `-Wall -Wextra -Wpedantic -Werror`
- stack protector
- `_FORTIFY_SOURCE=2`
- PIE
- RELRO
- immediate symbol binding

## Container deployment

The production image uses Alpine 3.24 and runs as an unprivileged `sentinel` user.

Build:

```bash
docker build -t sentinel .
```

Run directly:

```bash
docker run --rm \
  --pid=host \
  --network=host \
  --read-only \
  --cap-drop ALL \
  --cap-add SYS_PTRACE \
  --cap-add DAC_READ_SEARCH \
  --security-opt no-new-privileges:true \
  -v /proc:/host/proc:ro \
  -v /etc:/host/etc:ro \
  -v /var/log:/host/var/log:ro \
  sentinel --host-roots
```

Or use Compose:

```bash
docker compose up --build
```

The Compose definition drops the default capability set, adds only the two capabilities currently requested by the host-observation deployment, enables `no-new-privileges`, uses a read-only container filesystem, and mounts host data read-only.

In `--host-roots` mode, configured paths under `/etc`, `/var/log`, and `/proc` are transparently mapped to `/host/etc`, `/host/var/log`, and `/host/proc`.

A `.dockerignore` keeps Git metadata and local build artifacts out of the Docker build context.

## Event model

| Event | Source | Meaning |
|---|---|---|
| `process_seen` | `/proc` state diff | A PID/start-time identity was not present in the previous scan |
| `file_change` | inotify | A watched file or directory produced a tracked change event |
| `network_listen` | TCP state diff | A listener appeared |
| `network_close` | TCP state diff | A previously observed listener disappeared |

All telemetry is NDJSON on **stdout**. Startup messages, warnings, permission failures, and degraded sensor diagnostics go to **stderr**.

That split is intentional so a collector can ingest stdout without mixing operational logs into the event stream.

## Architecture

```text
                       ┌──────────────────┐
                       │     SENTINEL     │
                       └────────┬─────────┘
                                │
               ┌────────────────┼────────────────┐
               │                │                │
               ▼                ▼                ▼
        /proc process       inotify file      /proc TCP
          state diff          watchers        state diff
               │                │                │
               └────────────────┼────────────────┘
                                ▼
                        NDJSON event stream
                                │
                                ▼
                              stdout
```

## GitHub Actions CI

The current Actions pipeline is intentionally CI-focused. It does **not** publish releases yet.

Runs occur:

- once for pull requests targeting `latest`
- once against the exact merged state after a push lands on `latest`
- manually through `workflow_dispatch`

Jobs:

```text
ci
├── build-test (gcc)
├── build-test (clang)
├── static-analysis
└── container-build
    ├── Dockerfile validation
    ├── production image build
    └── sentinel --version smoke test
```

Static analysis uses an explicit Cppcheck `2.21.0` container image tag, avoiding runtime `apt-get` installation and the Ubuntu mirror delays that previously made the job unreliable.

Additional CI safeguards include:

- fixed `ubuntu-24.04` runners
- bounded job timeouts
- `fail-fast: false` across compiler jobs
- concurrency cancellation for superseded runs
- read-only repository permissions
- checkout credentials disabled after checkout

More detail lives in `.github/CI.md`.

## Known limitations

SENTINEL 0.2.0 is useful, but deliberately not pretending to be a finished EDR.

- Process and socket discovery are polling-based. Short-lived activity can occur entirely between scans.
- `/proc/net/tcp` and `/proc/net/tcp6` are legacy kernel interfaces. A netlink `sock_diag` implementation should eventually replace them where available.
- Only TCP listeners are currently modeled. UDP and broader socket lifecycle telemetry are not implemented.
- Process telemetry does not yet include executable hashes, full parent lineage, namespace/cgroup identity, capabilities, or container metadata.
- File monitoring reports inotify activity but does not yet hash changed files or preserve evidence.
- Output is stdout only. There is no buffering, local spool, webhook, syslog, or direct SIEM sink yet.
- The YAML subset is intentionally narrow and should not be presented as a general YAML parser.
- The Cppcheck image uses an explicit version tag, not an immutable image digest yet.

## Implementation roadmap

### 0.3.x - event fidelity and sensor health

Highest-value next work:

- [ ] Add Linux process event support using a netlink-capable backend where the host kernel permits it, with polling retained as a fallback.
- [ ] Replace `/proc/net/tcp*` polling with `NETLINK_SOCK_DIAG` snapshots/events where practical.
- [ ] Add UDP listener visibility.
- [ ] Add sensor-health events and counters for scan failures, dropped/invalid events, watch re-arm failures, and degraded telemetry classes.
- [ ] Add monotonically increasing event sequence numbers and a per-process sensor instance ID.
- [ ] Add hostname, boot ID, kernel, architecture, namespace, and container/cgroup context to common event metadata.
- [ ] Add deterministic parser fixtures for process and socket data so edge cases do not depend on the CI host's live `/proc` state.

### 0.4.x - enrichment and detection

- [ ] Reconstruct process ancestry and parent lineage.
- [ ] Capture executable path, effective capabilities, namespaces, cgroup/container identity, and selected environment metadata.
- [ ] Add executable and changed-file hashing with configurable size limits.
- [ ] Introduce a small rule engine with event type, field matching, severity, tags, and allow/suppress logic.
- [ ] Add built-in detections for suspicious listeners, sensitive-file changes, unusual privilege transitions, and selected persistence locations.
- [ ] Separate observation events from alert events while retaining the original evidence fields.

### 0.5.x - evidence and resilient output

- [ ] Add evidence manifests containing hashes, timestamps, sensor version, host identity, and collection reason.
- [ ] Add bounded local spooling so downstream collector outages do not immediately discard telemetry.
- [ ] Add stdout, syslog, webhook, and optional file sinks behind one output interface.
- [ ] Add backpressure/drop policy metrics rather than silently losing events.
- [ ] Add configurable redaction for command lines or sensitive fields where required.

### 0.6.x - deployment and release engineering

- [ ] Add a hardened systemd service example.
- [ ] Add an Ansible deployment role.
- [ ] Publish multi-architecture container images to GHCR.
- [ ] Generate SBOMs and provenance metadata.
- [ ] Sign release binaries and container images.
- [ ] Pin CI container dependencies by immutable digest.
- [ ] Add release artifacts for common Linux architectures.
- [ ] Add DIP / DIPx deployment integration.

### Longer-term research

- [ ] Evaluate an optional eBPF backend for higher-fidelity process/socket telemetry while keeping a non-eBPF fallback.
- [ ] Explore filesystem evidence collection using fanotify where its semantics are a better fit than inotify.
- [ ] Add fuzzing for the config parser, `/proc` parsers, and JSON emission boundaries.
- [ ] Add richer event-schema compatibility tests that can also be shared with GARGOYLE.
- [ ] Add an optional lightweight controller/collector without turning the host sensor itself into a large agent framework.

## Suggested implementation order

If the goal is to increase operational value without losing SENTINEL's small footprint, the strongest sequence is:

1. **Sensor health + common event metadata**
2. **`sock_diag` network backend**
3. **Higher-fidelity process events with polling fallback**
4. **Process ancestry and executable enrichment**
5. **Hashing + evidence manifests**
6. **Small rule engine**
7. **Buffered outputs and integrations**
8. **Signed releases, SBOMs, GHCR, and deployment automation**

That order improves trust in the telemetry before adding increasingly sophisticated detections on top of it.

## Project boundary

SENTINEL stays the lean C operational sensor.

GARGOYLE may share schemas, tests, and behavioral expectations, but it remains a separate Rust security-engineering project rather than becoming a stealth rewrite of SENTINEL.

## License

SENTINEL is licensed under **GPL-3.0**. See `LICENSE.md` for the full license text.
