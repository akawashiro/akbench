# misen

## OS
```
$ uname -a
Linux misen 6.14.0-33-generic #33-Ubuntu SMP PREEMPT_DYNAMIC Wed Sep 17 23:22:02 UTC 2025 x86_64 x86_64 x86_64 GNU/Linux
```

## DRAM
```
$ sudo lshw -class memory
*-firmware
       description: BIOS
       vendor: American Megatrends International, LLC.
       physical id: 0
       version: 1.0O
       date: 01/03/2024
       size: 64KiB
       capabilities: pci upgrade shadowing cdboot bootselect socketedrom edd int13floppynec int13floppytoshiba int13floppy360 int13floppy1200 int13floppy720 int13floppy2880 int5printscreen int14serial int17printer int10video acpi usb biosbootspecification uefi
  *-memory
       description: System Memory
       physical id: 26
       slot: System board or motherboard
       size: 32GiB
     *-bank:0
          description: SODIMM DDR4 Synchronous 3200 MHz (0.3 ns)
          product: CBD32D4S2S8MF-16
          vendor: Kingston
          physical id: 0
          serial: 29B42C02
          slot: Controller0-ChannelA-DIMM0
          size: 16GiB
          width: 64 bits
          clock: 3200MHz (0.3ns)
     *-bank:1
          description: SODIMM DDR4 Synchronous 3200 MHz (0.3 ns)
          product: CBD32D4S2S8MF-16
          vendor: Kingston
          physical id: 1
          serial: 0AF42D68
          slot: Controller1-ChannelA-DIMM0
          size: 16GiB
          width: 64 bits
          clock: 3200MHz (0.3ns)
  *-cache:0
       description: L1 cache
       physical id: 32
       slot: L1 Cache
       size: 288KiB
       capacity: 288KiB
       capabilities: synchronous internal write-back data
       configuration: level=1
  *-cache:1
       description: L1 cache
       physical id: 33
       slot: L1 Cache
       size: 192KiB
       capacity: 192KiB
       capabilities: synchronous internal write-back instruction
       configuration: level=1
  *-cache:2
       description: L2 cache
       physical id: 34
       slot: L2 Cache
       size: 7680KiB
       capacity: 7680KiB
       capabilities: synchronous internal write-back unified
       configuration: level=2
  *-cache:3
       description: L3 cache
       physical id: 35
       slot: L3 Cache
       size: 24MiB
       capacity: 24MiB
       capabilities: synchronous internal write-back unified
       configuration: level=3
  *-cache:4
       description: L1 cache
       physical id: 36
       slot: L1 Cache
       size: 256KiB
       capacity: 256KiB
       capabilities: synchronous internal write-back data
       configuration: level=1
  *-cache:5
       description: L1 cache
       physical id: 37
       slot: L1 Cache
       size: 512KiB
       capacity: 512KiB
       capabilities: synchronous internal write-back instruction
       configuration: level=1
  *-cache:6
       description: L2 cache
       physical id: 38
       slot: L2 Cache
       size: 4MiB
       capacity: 4MiB
       capabilities: synchronous internal write-back unified
       configuration: level=2
  *-cache:7
       description: L3 cache
       physical id: 39
       slot: L3 Cache
       size: 24MiB
       capacity: 24MiB
       capabilities: synchronous internal write-back unified
       configuration: level=3
  *-memory UNCLAIMED
       description: RAM memory
       product: Alder Lake PCH Shared SRAM
       vendor: Intel Corporation
       physical id: 14.2
       bus info: pci@0000:00:14.2
       version: 01
       width: 64 bits
       clock: 33MHz (30.3ns)
       capabilities: pm cap_list
       configuration: latency=0
       resources: iomemory:600-5ff iomemory:600-5ff memory:6002114000-6002117fff memory:600211a000-600211afff
```

## CPU
```
$ lscpu
Architecture:                            x86_64
CPU op-mode(s):                          32-bit, 64-bit
Address sizes:                           39 bits physical, 48 bits virtual
Byte Order:                              Little Endian
CPU(s):                                  20
On-line CPU(s) list:                     0-19
Vendor ID:                               GenuineIntel
Model name:                              12th Gen Intel(R) Core(TM) i7-12700H
CPU family:                              6
Model:                                   154
Thread(s) per core:                      2
Core(s) per socket:                      14
Socket(s):                               1
Stepping:                                3
CPU(s) scaling MHz:                      25%
CPU max MHz:                             4700.0000
CPU min MHz:                             400.0000
BogoMIPS:                                5376.00
Flags:                                   fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush dts acpi mmx fxsr sse sse2 ss ht tm pbe syscall nx pdpe1gb rdtscp lm constant_tsc art arch_perfmon pebs bts rep_good nopl xtopology nonstop_tsc cpuid aperfmperf tsc_known_freq pni pclmulqdq dtes64 monitor ds_cpl vmx smx est tm2 ssse3 sdbg fma cx16 xtpr pdcm pcid sse4_1 sse4_2 x2apic movbe popcnt tsc_deadline_timer aes xsave avx f16c rdrand lahf_lm abm 3dnowprefetch cpuid_fault epb cat_l2 cdp_l2 ssbd ibrs ibpb stibp ibrs_enhanced tpr_shadow flexpriority ept vpid ept_ad fsgsbase tsc_adjust bmi1 avx2 smep bmi2 erms invpcid rdt_a rdseed adx smap clflushopt clwb intel_pt sha_ni xsaveopt xsavec xgetbv1 xsaves split_lock_detect user_shstk avx_vnni dtherm ida arat pln pts hwp hwp_notify hwp_act_window hwp_epp hwp_pkg_req hfi vnmi umip pku ospke waitpkg gfni vaes vpclmulqdq rdpid movdiri movdir64b fsrm md_clear serialize arch_lbr ibt flush_l1d arch_capabilities
Virtualization:                          VT-x
L1d cache:                               544 KiB (14 instances)
L1i cache:                               704 KiB (14 instances)
L2 cache:                                11.5 MiB (8 instances)
L3 cache:                                24 MiB (1 instance)
NUMA node(s):                            1
NUMA node0 CPU(s):                       0-19
Vulnerability Gather data sampling:      Not affected
Vulnerability Ghostwrite:                Not affected
Vulnerability Indirect target selection: Not affected
Vulnerability Itlb multihit:             Not affected
Vulnerability L1tf:                      Not affected
Vulnerability Mds:                       Not affected
Vulnerability Meltdown:                  Not affected
Vulnerability Mmio stale data:           Not affected
Vulnerability Reg file data sampling:    Mitigation; Clear Register File
Vulnerability Retbleed:                  Not affected
Vulnerability Spec rstack overflow:      Not affected
Vulnerability Spec store bypass:         Mitigation; Speculative Store Bypass disabled via prctl
Vulnerability Spectre v1:                Mitigation; usercopy/swapgs barriers and __user pointer sanitization
Vulnerability Spectre v2:                Mitigation; Enhanced / Automatic IBRS; IBPB conditional; PBRSB-eIBRS SW sequence; BHI BHI_DIS_S
Vulnerability Srbds:                     Not affected
Vulnerability Tsx async abort:           Not affected
```

## Results
```
$ ./build/akbench/akbench all 
Running all latency tests:

latency_atomic: 43.128 ± 9.649 ns
latency_atomic_rel_acq: 40.417 ± 9.139 ns
latency_barrier: 3962.947 ± 1051.561 ns
latency_condition_variable: 1611.880 ± 374.936 ns
latency_semaphore: 1435.548 ± 321.478 ns
latency_statfs: 467.024 ± 104.537 ns
latency_fstatfs: 367.529 ± 82.243 ns
latency_getpid: 54.231 ± 12.151 ns

Running all bandwidth tests:

bandwidth_memcpy: 17.604 ± 0.225 GiByte/sec
bandwidth_memcpy_mt (1 threads): 17.599 ± 0.256 GiByte/sec
bandwidth_memcpy_mt (2 threads): 18.235 ± 0.124 GiByte/sec
bandwidth_memcpy_mt (3 threads): 18.617 ± 0.135 GiByte/sec
bandwidth_memcpy_mt (4 threads): 19.000 ± 0.061 GiByte/sec
bandwidth_tcp: 7.677 ± 0.043 GiByte/sec
bandwidth_uds: 7.026 ± 0.261 GiByte/sec
bandwidth_pipe: 2.482 ± 0.062 GiByte/sec
bandwidth_fifo: 2.373 ± 0.109 GiByte/sec
bandwidth_mq: 2.154 ± 0.080 GiByte/sec
bandwidth_mmap: 9.575 ± 0.575 GiByte/sec
bandwidth_shm: 9.439 ± 0.989 GiByte/sec
```

## masumi

## CPU
```
$ lscpu
Architecture:                         x86_64
CPU op-mode(s):                       32-bit, 64-bit
Address sizes:                        48 bits physical, 48 bits virtual
Byte Order:                           Little Endian
CPU(s):                               32
On-line CPU(s) list:                  0-31
Vendor ID:                            AuthenticAMD
Model name:                           AMD Ryzen 9 5950X 16-Core Processor
CPU family:                           25
Model:                                33
Thread(s) per core:                   2
Core(s) per socket:                   16
Socket(s):                            1
Stepping:                             0
Frequency boost:                      enabled
CPU(s) scaling MHz:                   50%
CPU max MHz:                          5086.0000
CPU min MHz:                          550.0000
BogoMIPS:                             6786.93
Flags:                                fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush mmx fxsr sse sse2 ht syscall nx mmxext fxsr_opt pdpe1gb rdtscp lm constant_tsc rep_good nopl xtopology nonstop_tsc cpuid extd_apicid aperfmperf rapl pni pclmulqdq monitor ssse3 fma cx16 sse4_1 sse4_2 movbe popcnt aes xsave avx f16c rdrand lahf_lm cmp_legacy svm extapic cr8_legacy abm sse4a misalignsse 3dnowprefetch osvw ibs skinit wdt tce topoext perfctr_core perfctr_nb bpext perfctr_llc mwaitx cpb cat_l3 cdp_l3 hw_pstate ssbd mba ibrs ibpb stibp vmmcall fsgsbase bmi1 avx2 smep bmi2 invpcid cqm rdt_a rdseed adx smap clflushopt clwb sha_ni xsaveopt xsavec xgetbv1 xsaves cqm_llc cqm_occup_llc cqm_mbm_total cqm_mbm_local user_shstk clzero irperf xsaveerptr rdpru wbnoinvd arat npt lbrv svm_lock nrip_save tsc_scale vmcb_clean flushbyasid decodeassists pausefilter pfthreshold avic v_vmsave_vmload vgif v_spec_ctrl umip pku ospke vaes vpclmulqdq rdpid overflow_recov succor smca debug_swap
Virtualization:                       AMD-V
L1d cache:                            512 KiB (16 instances)
L1i cache:                            512 KiB (16 instances)
L2 cache:                             8 MiB (16 instances)
L3 cache:                             64 MiB (2 instances)
NUMA node(s):                         1
NUMA node0 CPU(s):                    0-31
Vulnerability Gather data sampling:   Not affected
Vulnerability Ghostwrite:             Not affected
Vulnerability Itlb multihit:          Not affected
Vulnerability L1tf:                   Not affected
Vulnerability Mds:                    Not affected
Vulnerability Meltdown:               Not affected
Vulnerability Mmio stale data:        Not affected
Vulnerability Reg file data sampling: Not affected
Vulnerability Retbleed:               Not affected
Vulnerability Spec rstack overflow:   Mitigation; Safe RET
Vulnerability Spec store bypass:      Mitigation; Speculative Store Bypass disabled via prctl
Vulnerability Spectre v1:             Mitigation; usercopy/swapgs barriers and __user pointer sanitization
Vulnerability Spectre v2:             Mitigation; Retpolines; IBPB conditional; IBRS_FW; STIBP always-on; RSB filling; PBRSB-eIBRS Not affected; BHI Not affected
Vulnerability Srbds:                  Not affected
Vulnerability Tsx async abort:        Not affected
```

## OS
```
$ uname -a
Linux masumi 6.14.0-24-generic #24~24.04.3-Ubuntu SMP PREEMPT_DYNAMIC Mon Jul  7 16:39:17 UTC 2 x86_64 x86_64 x86_64 GNU/Linux
```

## DRAM
```
$ sudo lshw -class memory
  *-firmware
       description: BIOS
       vendor: American Megatrends International, LLC.
       physical id: 0
       version: P2.10
       date: 08/04/2021
       size: 64KiB
       capacity: 16MiB
       capabilities: pci upgrade shadowing cdboot bootselect socketedrom edd int13floppynec int13floppytoshiba int13floppy360 int13floppy1200 int13floppy720 int13floppy2880 int5printscreen int9keyboard int14serial int17printer int10video acpi usb biosbootspecification uefi
  *-memory
       description: System Memory
       physical id: 10
       slot: System board or motherboard
       size: 64GiB
     *-bank:0
          description: [empty]
          product: Unknown
          vendor: Unknown
          physical id: 0
          serial: Unknown
          slot: DIMM 0
     *-bank:1
          description: DIMM DDR4 Synchronous Unbuffered (Unregistered) 3200 MHz (0.3 ns)
          product: CT32G4DFD832A.C16FE
          vendor: Unknown
          physical id: 1
          serial: E60BBC35
          slot: DIMM 1
          size: 32GiB
          width: 64 bits
          clock: 3200MHz (0.3ns)
     *-bank:2
          description: [empty]
          product: Unknown
          vendor: Unknown
          physical id: 2
          serial: Unknown
          slot: DIMM 0
     *-bank:3
          description: DIMM DDR4 Synchronous Unbuffered (Unregistered) 3200 MHz (0.3 ns)
          product: CT32G4DFD832A.C16FE
          vendor: Unknown
          physical id: 3
          serial: E60BC28D
          slot: DIMM 1
          size: 32GiB
          width: 64 bits
          clock: 3200MHz (0.3ns)
  *-cache:0
       description: L1 cache
       physical id: 13
       slot: L1 - Cache
       size: 1MiB
       capacity: 1MiB
       clock: 1GHz (1.0ns)
       capabilities: pipeline-burst internal write-back unified
       configuration: level=1
  *-cache:1
       description: L2 cache
       physical id: 14
       slot: L2 - Cache
       size: 8MiB
       capacity: 8MiB
       clock: 1GHz (1.0ns)
       capabilities: pipeline-burst internal write-back unified
       configuration: level=2
  *-cache:2
       description: L3 cache
       physical id: 15
       slot: L3 - Cache
       size: 64MiB
       capacity: 64MiB
       clock: 1GHz (1.0ns)
       capabilities: pipeline-burst internal write-back unified
       configuration: level=3
```

## Results
```bash
$ ./build/akbench/akbench all
Running all latency tests:

latency_atomic: 30.603 ± 7.456 ns
latency_atomic_rel_acq: 28.425 ± 6.766 ns
latency_barrier: 1207.463 ± 501.146 ns
latency_condition_variable: 3154.424 ± 707.784 ns
latency_semaphore: 2962.199 ± 681.565 ns
latency_statfs: 1008.512 ± 225.512 ns
latency_fstatfs: 664.642 ± 148.630 ns
latency_getpid: 95.559 ± 21.436 ns

Running all bandwidth tests:

bandwidth_memcpy: 17.957 ± 0.440 GiByte/sec
bandwidth_memcpy_mt (1 threads): 17.916 ± 0.144 GiByte/sec
bandwidth_memcpy_mt (2 threads): 18.394 ± 0.189 GiByte/sec
bandwidth_memcpy_mt (3 threads): 19.081 ± 0.356 GiByte/sec
bandwidth_memcpy_mt (4 threads): 18.730 ± 0.563 GiByte/sec
bandwidth_tcp: 4.914 ± 0.395 GiByte/sec
bandwidth_uds: 6.684 ± 0.731 GiByte/sec
bandwidth_pipe: 2.143 ± 0.021 GiByte/sec
bandwidth_fifo: 2.100 ± 0.035 GiByte/sec
bandwidth_mq: 1.758 ± 0.021 GiByte/sec
bandwidth_mmap: 10.238 ± 0.120 GiByte/sec
bandwidth_shm: 10.470 ± 0.185 GiByte/sec
```

# LAPTOP-7LEF3C0M

## OS
```
$ uname -a
Linux LAPTOP-7LEF3C0M 6.6.87.2-microsoft-standard-WSL2 #1 SMP PREEMPT_DYNAMIC Thu Jun  5 18:30:46 UTC 2025 x86_64 x86_64 x86_64 GNU/Linux
```

```
$ lscpu
Architecture:                         x86_64
CPU op-mode(s):                       32-bit, 64-bit
Address sizes:                        39 bits physical, 48 bits virtual
Byte Order:                           Little Endian
CPU(s):                               8
On-line CPU(s) list:                  0-7
Vendor ID:                            GenuineIntel
Model name:                           Intel(R) Core(TM) i5-8365U CPU @ 1.60GHz
CPU family:                           6
Model:                                142
Thread(s) per core:                   2
Core(s) per socket:                   4
Socket(s):                            1
Stepping:                             12
BogoMIPS:                             3792.00
Flags:                                fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush mmx fxsr sse sse2 ss ht syscall nx pdpe1gb rdtscp lm constant_tsc arch_perfmon rep_good nopl xtopology cpuid tsc_known_freq pni pclmulqdq vmx ssse3 fma cx16 pdcm pcid sse4_1 sse4_2 movbe popcnt aes xsave avx f16c rdrand hypervisor lahf_lm abm 3dnowprefetch ssbd ibrs ibpb stibp ibrs_enhanced tpr_shadow ept vpid ept_ad fsgsbase bmi1 avx2 smep bmi2 erms invpcid rdseed adx smap clflushopt xsaveopt xsavec xgetbv1 xsaves vnmi md_clear flush_l1d arch_capabilities
Virtualization:                       VT-x
Hypervisor vendor:                    Microsoft
Virtualization type:                  full
L1d cache:                            128 KiB (4 instances)
L1i cache:                            128 KiB (4 instances)
L2 cache:                             1 MiB (4 instances)
L3 cache:                             6 MiB (1 instance)
NUMA node(s):                         1
NUMA node0 CPU(s):                    0-7
Vulnerability Gather data sampling:   Not affected
Vulnerability Itlb multihit:          KVM: Mitigation: VMX disabled
Vulnerability L1tf:                   Not affected
Vulnerability Mds:                    Not affected
Vulnerability Meltdown:               Not affected
Vulnerability Mmio stale data:        Mitigation; Clear CPU buffers; SMT Host state unknown
Vulnerability Reg file data sampling: Not affected
Vulnerability Retbleed:               Mitigation; Enhanced IBRS
Vulnerability Spec rstack overflow:   Not affected
Vulnerability Spec store bypass:      Mitigation; Speculative Store Bypass disabled via prctl
Vulnerability Spectre v1:             Mitigation; usercopy/swapgs barriers and __user pointer sanitization
Vulnerability Spectre v2:             Mitigation; Enhanced / Automatic IBRS; IBPB conditional; RSB filling; PBRSB-eIBRS SW sequence; BHI SW loop, KVM SW loop
Vulnerability Srbds:                  Unknown: Dependent on hypervisor status
Vulnerability Tsx async abort:        Mitigation; TSX disabled
```

```
$ sudo lshw -class memory
  *-memory
       description: System memory
       physical id: 1
       size: 11GiB
```

```
$ ./build/akbench/akbench all
Running all latency tests:

latency_atomic: 42.706 ± 12.833 ns
latency_atomic_rel_acq: 47.323 ± 14.577 ns
latency_barrier: 31798.330 ± 8015.647 ns
latency_condition_variable: 38019.592 ± 9726.333 ns
latency_semaphore: 25298.642 ± 6972.319 ns
latency_statfs: 737.350 ± 172.411 ns
latency_fstatfs: 435.186 ± 113.216 ns
latency_getpid: 148.462 ± 35.782 ns

Running all bandwidth tests:

bandwidth_memcpy: 5.938 ± 2.040 GiByte/sec
bandwidth_memcpy_mt (1 threads): 7.358 ± 2.000 GiByte/sec
bandwidth_memcpy_mt (2 threads): 8.990 ± 0.401 GiByte/sec
bandwidth_memcpy_mt (3 threads): 8.660 ± 0.368 GiByte/sec
bandwidth_memcpy_mt (4 threads): 8.338 ± 0.262 GiByte/sec
bandwidth_tcp: 2.367 ± 0.490 GiByte/sec
bandwidth_uds: 3.089 ± 0.217 GiByte/sec
bandwidth_pipe: 0.545 ± 0.075 GiByte/sec
bandwidth_fifo: 0.484 ± 0.093 GiByte/sec
bandwidth_mq: 0.433 ± 0.130 GiByte/sec
bandwidth_mmap: 4.066 ± 0.323 GiByte/sec
bandwidth_shm: 3.613 ± 0.904 GiByte/sec
```
