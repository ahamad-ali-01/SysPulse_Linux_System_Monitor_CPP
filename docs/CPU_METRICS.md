# CPU metrics

Linux `/proc/stat` counters are cumulative jiffies. Utilization is the non-idle delta divided by the total delta between samples. Idle includes iowait for the simple busy/idle split. The first sample is a baseline. Per-process CPU is delta(user+system ticks) / clock ticks / wall seconds × 100, normalized to one logical CPU, so multi-threaded processes can exceed 100%.
