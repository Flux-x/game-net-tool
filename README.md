# tcp-stress-tool

A high-performance Windows C++ utility for TCP network stress testing, throughput benchmarking, and connection latency probing.

---

## Features

- **Multi-Threaded Engine**: Scalable worker thread pool (1 to 64 threads) leveraging native Windows Winsock2 sockets.
- **Multiple Test Modes**:
  - `null` (default): Streams zero-filled payloads at maximum wire throughput.
  - `rand`: Streams randomized binary payloads to test link compression & filtering.
  - `connect`: Rapid connection & disconnection cycles to test TCP socket acceptance limits.
  - `probe`: Measures single-connection round-trip latency and host reachability.
- **Live Performance Telemetry**: Calculates real-time transfer rates (`MiB/s`, `KiB/s`), total packets sent, connections completed, and socket errors.
- **Configurable Constraints**: Adjust payload size (1–65,536 bytes), per-packet delay, and auto-stop duration limits.

---

## Usage

Run `bin/tcp_stress.exe` from a Command Prompt or PowerShell:

### 1. Basic Throughput Benchmark
```cmd
tcp_stress.exe --host 192.168.1.100:8080
```

### 2. High-Throughput Multi-Threaded Test
```cmd
:: 8 worker threads, 16 KiB payload size, 30-second duration limit
tcp_stress.exe --host 192.168.1.100:8080 --threads 8 --size 16384 --duration 30
```

### 3. Latency Probe
```cmd
tcp_stress.exe --host 192.168.1.100:8080 --mode probe
```

### 4. Rapid Connection Cycle Test
```cmd
tcp_stress.exe --host 192.168.1.100:8080 --mode connect --threads 16
```

---

## Command Reference

| Option | Default | Description |
| :--- | :--- | :--- |
| `--host <ip:port>` | *Required* | Target IPv4 address and port number. |
| `--mode <name>` | `null` | Test mode: `null`, `rand`, `connect`, `probe`. |
| `--threads <n>` | `4` | Number of concurrent worker threads (1–64). |
| `--size <bytes>` | `4096` | Packet payload size in bytes (1–65,536). |
| `--delay <ms>` | `0` | Delay in milliseconds between consecutive packet sends. |
| `--duration <sec>` | `0` | Test duration in seconds (0 = run until stopped with Ctrl+C). |
| `--quiet` | `false` | Suppresses live statistics output and prints only final summary. |

---

## Building from Source

### Prerequisites
- **OS**: Windows 10 / 11 (x64)
- **Compiler**: Visual Studio 2019 / 2022 / 2026 (MSVC C++ Build Tools)

### Build Instructions
Run `build.bat` from a Developer Command Prompt or double-click it:

```cmd
build.bat
```

The compiled binary will be generated at `bin/tcp_stress.exe`.

---

## Ethical Use Notice

This utility is intended exclusively for network diagnostics, local socket benchmarking, and authorized infrastructure stress testing. Do not use this tool against networks or systems without explicit permission.

---

## License

Distributed under the [MIT License](LICENSE).
