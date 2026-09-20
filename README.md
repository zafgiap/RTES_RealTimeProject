# RTES Final Project

## 1. Dependencies and execution record

The program depends on the following Linux libraries:

- [libwebsockets](https://libwebsockets.org/) for the secure WebSocket connection.
- [cJSON](https://github.com/DaveGamble/cJSON) for parsing incoming JSON messages.

On Debian-based Linux systems, install the required development packages with:

```bash
sudo apt update
sudo apt install build-essential libwebsockets-dev libcjson-dev
```

The project is configured by the `Makefile` for an **ARMv6 Raspberry Pi** target and uses the **ARM sysroot** configured by `SYSROOT`. After the dependencies and cross-compilation environment are available and you move inside the downloaded project's `/RTES_finalProject` root directory, execute the provided **bash script** with:

```bash
bash readyScript.sh
```

Using `bash readyScript.sh` does not require the script to have executable permissions. Alternatively, users may make it executable once and then run it directly:

```bash
chmod +x readyScript.sh
./readyScript.sh
```
### How `readyScript.sh` works

The script asks for the Raspberry Pi target in the format `name@IP` once. After the target is entered, it performs the following operations in order:

- Cleans the previous build with `make clean`.
- Compiles the project with `make` (*\*targets the ARMv6 processor\**).
- Copies the resulting `main` executable to the Raspberry Pi with `scp` (*\*password input required\**).
- Starts the program remotely in the background and redirects its output to `main.log` (*\*password input required\**).
- Opens an SSH session to the Raspberry Pi.
    - After all these steps you will find yourself connected to your rpi with ssh. To monitor the execution of the program type the command `tail -f metrics_log.txt`. The file should show a live new record ~ every second. `main.log` contains the **stdout** & **stderr** outputs, open it with `cat main.log` to see its, debug purpose, contents.

Therefore, when the dependencies and cross-compilation environment are already configured, one execution of `readyScript.sh` performs the complete clean, compile, deployment, remote start, and connection workflow. The script requires SSH access to the Raspberry Pi and may request the corresponding password or use an existing SSH key configuration.

### **15/9** - Recorded 24-hour execution

The execution record of this day is:

| Item | Value |
| --- | :---: |
| Targeted record date | **15/09/2026** |
| Board used | **Raspberry Pi Zero W** |
| Start | **14/09/2026 23:59:02.194231** (*\*-1 minute safety factor\**)|
| Finish | **16/09/2026 00:01:01.195604** (*\*+1 minute safety factor\**)|
| Reported duration | **24 hours, 1 minute, 59 seconds** |
| Scheduling method | **Cron job** |

During the **15/09/2026** record, no SSH connection was made and no other job or work was performed on the Raspberry Pi. Consequently, the CPU usage measurements were not influenced by external SSH activity or other externally initiated workloads.

## 2. Code architecture

### **-** Core worker threads

The producer, consumer, and monitor threads are the core of the program. Together, they separate network I/O, message processing, and observability so that each responsibility can proceed independently while sharing data through synchronized structures.

- The **producer** continuously executes `lws_service(context, 0)`. This keeps the WebSocket event loop active and prevents incoming traffic from being lost while the consumer processes earlier messages. When libwebsockets delivers data, the producer-side callback copies it into the shared queue.
- The **consumer** waits for available queue data, removes one locally owned message at a time, and processes it with `JSONparse()`. After parsing, it frees the message so that queue memory is not retained unnecessarily.
- The **monitor** wakes once per second through a POSIX timer and records the parsed-message counters, circular-buffer occupancy, and CPU usage in `metrics_log.txt`. It snapshots and resets the counters under their mutex, allowing the consumer to continue processing independently between measurements.

The producer does not retain the `in` pointer received by the libwebsockets callback. That pointer belongs to the library and may be reused after the callback returns. Instead, `queueAdd()` allocates local storage, copies the received bytes, appends a null terminator, and transfers ownership of the copy to the queue. The consumer owns the dequeued pointer while parsing and frees it immediately afterwards.

### **-** Dynamic reconnection mechanism

The WebSocket callback contains a standard client reconnection procedure. When the connection is interrupted, events occur in this order:

- The callback reports the disconnection, distinguishing a connection error from a normally closed client connection, and prints the reason when libwebsockets provides one.
- The callback checks the shared `running` flag to determine whether the program is still expected to operate.
- If shutdown has already started, no reconnection is attempted and the callback reports that the WebSocket is closing gracefully.
- If the program is still running, the callback selects the current delay from the scalable backoff sequence of 1, 2, 5, 10, and 20 seconds, and prints the planned reconnection time.
- After waiting, the callback increases the backoff index up to its maximum and calls `ws_connect()` to establish a new client connection.

This mechanism is implemented for both `LWS_CALLBACK_CLIENT_CONNECTION_ERROR` and `LWS_CALLBACK_CLIENT_CLOSED`. A successful connection resets the backoff sequence so that a later disconnection starts again with the shortest delay.

### **-** Synchronization

The circular buffer is protected by its mutex and coordinated with two condition variables. The producer waits on `notFull` when the buffer is full, while the consumer waits on `notEmpty` when it is empty. The monitor uses a separate mutex and condition variable so its periodic timer callback can wake it without busy-waiting.

The code keeps mutex-protected regions short: queue operations, counter snapshots, and queue-occupancy calculations are performed while locked, while JSON parsing and other processing happen after the queue mutex has been released. This reduces the time for which other threads are blocked.

### **-** Race-condition handling

All accesses shared between threads are synchronized with mutexes and condition variables. Queue state and queue payload ownership are protected by the queue mutex, while the parsed-message counters are protected by `data_mut`. The monitor copies and resets the counters while holding `data_mut`, so each one-second measurement is consistent.

### **-** Circular-buffer size

The circular buffer can contain up to **50 message pointers**, as defined by `QUEUESIZE`. Its `head` and `tail` indices wrap around the fixed array, and the `empty` and `full` flags distinguish the buffer states. A capacity of 50 was selected after experiments with capacities of 30 and 40; the tests indicated that 50 was the most suitable size for the observed workload.

### **-** Graceful shutdown and signal handling

The shutdown behavior of `SIGINT` (normally produced by pressing **Ctrl+C**) and `SIGTERM` (normally produced by a system or `kill` command) was deliberately changed. Instead of allowing these signals to terminate the process immediately, the program now handles both signals through the same graceful shutdown mechanism.

When either signal is received, its handler sets the shared `running` flag to zero. The main thread then broadcasts to all condition-variable waiters, joins the producer, consumer, and monitor threads, deletes the queued payloads, destroys the queue and data mutex resources, and finally destroys the WebSocket context. The worker loops check `running` and exit promptly, allowing allocated memory, condition variables, mutexes, timers, and WebSocket resources to be released before the process terminates. This avoids forceful termination, reduces the risk of corrupted or unmanaged memory, and ensures that the program ends in a controlled state.

Graceful termination is notified to the user with an informative print: `"Program ended gracefully"`

### **-** Network resilience

The Raspberry Pi's connection to the local network was continuously monitored by a **WiFi watchdog** service created for the system. Every minute, the watchdog checked the following conditions:

- Whether the Raspberry Pi had an IP address assigned.
- Whether the Raspberry Pi was associated with a wireless access point (AP).

Three consecutive failed checks triggered a WiFi reconnection procedure, allowing temporary network interruptions to be recovered without manual intervention.

To further improve connection stability, WiFi power-saving mode was also disabled at every boot by a dedicated service. The service executes the following command, where `<name>` is the wireless interface name:

```bash
sudo iw dev <name> set power_save off
```

## 3. Post processing

## 4. Previous experiments and robustness

The circular-buffer capacity was evaluated with three configurations:

| Buffer capacity | Result |
| :---: | :---: |
| 30 | Tested as a smaller-capacity configuration. |
| 40 | Tested as an intermediate configuration. |
| 50 | Selected as the ideal capacity for the observed workload. |

Many tests took place before the final execution on `15/09/2026`. These tests demonstrate the robustness and reliability of the program under different execution durations, queue sizes, and network workloads. The recorded measurements are summarized below.

| Execution | Start time | Finish time | Duration | Temperature | Buffer size | Full-buffer occurrences | Total zero readings |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| Test 1 | 06/09/2026 02:52:01.065 | 06/09/2026 10:02:12.057 | 7 h, 10 m, 11 s | Maximum: 43.3 °C; average: 41.6 °C | 35 | - | 179 samples (0.693%) |
| Test 2 | 10/09/2026 00:59:02.410403 | 11/09/2026 01:01:01.409857 | 24 h, 1 m, 59 s | Maximum: 46.5 °C; average: 42.23 °C | 40 | 3 | 5,764 samples (6.846%) |
| Test 3 | 12/09/2026 00:59:02.753067 | 13/09/2026 05:00:01.754523 | 28 h, 59 s | Maximum: 46.0 °C; average: 42.59 °C | 50 | 0 | 543 samples (0.538%) |
| **Final execution** | **14/09/2026 23:59:02.194231** | **16/09/2026 00:01:01.195604** | **24 h, 1 m, 59 s** | **-** | **50** | **-** | **99 readings (0.114%)** |

***\*NOTE\**** Test 2 was affected by a router ISR issue while the Raspberry Pi remained connected to the LAN, as verified with the WiFi watchdog. No additional note was recorded for Tests 1, 3, or the final execution.
