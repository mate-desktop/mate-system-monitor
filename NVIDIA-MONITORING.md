# Optional NVIDIA monitoring

The Resources page gains a bold **NVIDIA GPU History** section directly below
CPU History, containing one 60-second utilization graph per physical GPU,
with name, current utilization percentage, VRAM used/total in MiB, and
temperature in degrees Celsius. VRAM also has a level bar. Per-process GPU
accounting is outside this patch.
The Resources page scrolls so multiple adapters do not make the window grow
without limit. The GPU UUID is available as the row tooltip.

## Architecture and failure handling

* `src/nvidia-smi.{h,cpp}`: GTK-independent sample model and parser. Each metric
  can be unavailable independently. Rejects invalid/out-of-range numeric input,
  tolerates unsupported fields and bad rows, and deduplicates UUIDs. UUIDs keep
  widget identity stable when enumeration order changes.
* `src/gpu-monitor.{h,cpp}`: GTK presentation, timestamped bounded history,
  Cairo graph rendering, and asynchronous GIO controller.
  Executes one read-only query for all adapters, with an argument vector (no
  shell), the C locale, and discarded diagnostic stderr. No CUDA/NVML headers,
  library linkage, driver installation, or elevated privileges are required.
* `src/interface.cpp`: attaches the controller to the Resources container.
  `src/interface.ui` makes Resources scrollable for small screens and large CPU
  counts. Widget destruction removes timers, terminates/cancels pending work, and
  asynchronous shared ownership keeps callback state alive until completion.
* `src/meson.build`, `src/Makefile.am`: add the implementation and the tests.
  `po/POTFILES.in` registers new UI strings. Root `Makefile.am` distributes this
  document; `README` points to it. `tests/` contains parser and UI/lifecycle tests.

Detection happens only while Resources is mapped. Missing `nvidia-smi`, missing
hardware, uninitialized/incompatible drivers, permission failures, empty output,
and query errors all hide the section without dialogs or console errors.
Failures clear old readings. Valid rows with unsupported metrics display
“unavailable” rather than claiming zero usage. Removed GPUs are dropped; new
GPUs and recovery are detected on subsequent queries.

Successful queries are followed by a two-second minimum delay; failures use a
30-second delay. A one-second timer checks eligibility, so actual timing is
approximate. Only one asynchronous query is active at a time. A five-second
watchdog force-exits and cancels a hung query. No synchronous driver operation
runs in the GTK event loop. Hidden Resources pages do not start new queries;
an already pending query may finish. Polling can wake a discrete GPU and use
some CPU, so completely disable it when desired with:

```sh
MATE_SYSTEM_MONITOR_DISABLE_NVIDIA=1 mate-system-monitor
```

The backend is deliberately small and optional. NVIDIA warns that `nvidia-smi`
output is not guaranteed stable across driver releases and recommends NVML for
long-term compatibility. This implementation uses explicit selective CSV
columns, validates readings, and fails closed if that interface changes. A
future dynamically loaded NVML backend could replace collection while retaining
the per-GPU UI. MIG instance monitoring and AMD/Intel GPUs are outside this patch.
See [NVIDIA's official nvidia-smi documentation](https://docs.nvidia.com/deploy/nvidia-smi/index.html).

## Build and test (Debian/Ubuntu)

From a checkout containing this feature:

```sh
sudo apt install build-essential pkg-config meson ninja-build gettext itstool \
  libgtkmm-3.0-dev libgtop2-dev librsvg2-dev libxml2-dev libsystemd-dev \
  libpolkit-gobject-1-dev xvfb xauth
meson setup build -Dsystemd=false
meson compile -C build
xvfb-run -a meson test -C build --print-errorlogs
```

`-Dsystemd=false` is optional and matches the validation configuration. No NVIDIA
SDK or driver is needed to build or run the tests. Ubuntu may require the
Universe repository for some build packages. UI tests return skip status 77
without a display; use `xvfb-run` to actually exercise them.

To run from the checkout without installing, first generate the schema enum:

```sh
meson compile -C build
# The enum XML is a generated build target included by the full build.
glib-compile-schemas build/src
GSETTINGS_SCHEMA_DIR="$PWD/build/src" ./build/src/mate-system-monitor
```

Alternatively install with `meson install -C build` using suitable privileges.
Close other System Monitor instances before testing the new executable. Running
without installation can produce existing missing-icon warnings because upstream
loads some icons from the configured installation prefix; install into a local
prefix for a complete visual test.

The existing Autotools build is also wired up:

```sh
# Additionally needs autoconf, automake, libtool, autoconf-archive,
# mate-common and yelp-tools, plus the development packages above.
./autogen.sh --disable-systemd
make -j2
xvfb-run -a make check
```

Standalone parser test, requiring only a C++11 compiler:

```sh
c++ -std=c++11 -Wall -Wextra -Werror -Isrc \
  src/nvidia-smi.cpp tests/nvidia-smi-test.cpp -o /tmp/nvidia-smi-test
/tmp/nvidia-smi-test
```

## Tests and manual checks

The parser test covers multiple GPUs, no devices/driver, malformed rows,
unsupported values, zero utilization, invalid/range/overflow values, duplicate
identities, CRLF output, and names containing commas.

The UI test supplies a fake executable in a temporary PATH. It checks hidden UI
before detection, missing executable, two adapters, stable widgets after reorder,
unsupported metrics, device removal, driver failure, retry backoff, recovery,
no new polling when hidden, no overlapping queries, the actual five-second
timeout, destruction during a pending query, and the runtime opt-out.

On NVIDIA hardware, open Resources and compare readings with:

```sh
nvidia-smi --query-gpu=uuid,utilization.gpu,memory.used,memory.total,temperature.gpu,name --format=csv,noheader,nounits
```

Also inspect a small window with many adapters, confirm all rows can be reached
by scrolling, switch away from Resources, and exercise the runtime opt-out.
Test on the driver versions and hardware that you support; simulated output
cannot validate individual driver versions or physical sensor accuracy.

## Redraw correction

The Resources scroller exposed existing direct-to-window rendering in the CPU
color buttons and load graphs. `src/gsm_color_button.c` and `src/load-graph.cpp`
now draw using GTK's supplied Cairo context, preserving viewport clipping,
translation, and buffering. Realization schedules drawing rather than painting
directly. The color-button regression test renders into a supplied image surface
before and after scrolling: it fails on the old implementation (transparent
instead of red) and passes on the corrected implementation.

## GPU utilization history

Each UUID owns a separate 60-second history, plotted against monotonic time
with 0–100% grid lines and a seconds axis. The graph redraws each second while
Resources is mapped; collection retains the existing two-second minimum delay.
Samples expire by timestamp and the buffer is additionally capped at 128 points.
Unavailable readings and sampling gaps longer than five seconds break the line.
There is no invented zero history at startup, and histories remain associated
with the same GPU after enumeration reorder. The GPU UI tests cover placement
between CPU and memory, independent histories, expiry, bounded storage, and
history retention after device reorder.
