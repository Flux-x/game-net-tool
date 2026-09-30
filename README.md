# game-net-tool

Simple Windows TCP connection and latency testing tool for Minecraft and Source Engine 1 servers.

## Features

- **Port & Latency Check**: Check connection latency and reachability (`--mode probe`).
- **Connection Test**: Test socket connections (`--mode connect`).
- **Packet Send**: Send test packets (`--mode null` or `--mode rand`).
- **Live Stats**: Shows sent packets, active connections, and speed.

## Usage

```cmd
# Test connection latency (Minecraft)
game_net_tool.exe --host 127.0.0.1:25565 --mode probe

# Test connection latency (Source Engine 1)
game_net_tool.exe --host 127.0.0.1:27015 --mode probe

# Test connection with 4 threads
game_net_tool.exe --host 127.0.0.1:25565 --threads 4 --duration 10
```

## Options

- `--host <ip:port>`: Target address and port (required)
- `--mode <probe|null|connect|rand>`: Test mode (default: `null`)
- `--threads <n>`: Number of worker threads (default: `4`)
- `--size <bytes>`: Packet size in bytes (default: `4096`)
- `--delay <ms>`: Delay between packets in ms (default: `0`)
- `--duration <sec>`: Duration in seconds (default: `0` = until stopped)
- `--quiet`: Only show final summary

## Building

Run `build.bat` with Visual Studio installed:

```cmd
build.bat
```

Binary is output to `bin/game_net_tool.exe`.

## License

[MIT](LICENSE)
