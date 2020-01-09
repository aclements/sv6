### Spectre

##### v1 / Bounds Check Bypass
- [ ] `nospec` macro
- [ ] lfence when swapping gs

##### v2 / Branch Target Injection 
- [ ] Enhanced IBRS on Cascade Lake+ / `-mindirect-branch=thunk` for indirect
  branches / "optpolines" for software branch prediction
- [ ] `-mfunction-return=thunk` on Skylake+ for return stack underflows
- [ ] Indirect branch predictor barrier (IBPB) instruction on context switch between
  user processes.

##### v3 / Meltdown
- [ ] KAISER / kernel page table isolation

##### SpectreRSB / Return Mispredict
- [ ] RSB on context switch (+other times?) to prevent Return Stack Underflow

### Spectre-NG
##### v3a / Rogue System Register Read
- [ ] new microcode

##### v4 / Speculative Store bypass
- [ ] Set SSBD bit in the IA32_SPEC_CTRL MSR
    - In Ubuntu, SSBD is OFF by default because it is not needed by most
      programs and carries a notable performance impact.

##### Lazy FP State Restore
- [ ] TODO

##### v1.1 / Bounds Check Bypass Store
- [ ] TODO

##### v1.2 / Read-only Protection Bypass (RBP)
- [ ] TODO

### L1 Terminal Fault (L1TF)
##### Foreshadow-OS / L1TF-OS
- [ ] Either present bit set or PTE=0 in all page tables
##### Foreshadow / L1TF-SGX
##### Foreshadow-VMM / L1FT-VMM

### Microarchitectural Data Sampling
- [ ] Clear buffers on kernel exit with VERW instruction and disable HT
- [ ] Fixed in “select 8th+ gen hardware”
##### Fallout / Microarchitectural Store Buffer Data Sampling (MSBDS)
- [ ] VERW + disable HT
##### RIDL / Microarchitectural Load Port Data Sampling (MLPDS)
- [ ] VERW + disable HT
##### Zombie Load / Microarchitectural Fill Buffer Data Sampling (MFBDS)
- [ ] VERW + disable HT
##### RIDL / Microarchitectural Data Sampling Uncacheable Memory (MSBDS)
- [ ] VERW + disable HT
##### RIDL / ZombieLoad v2 / Transactional Asynchronous Abort (TAA)
- [ ] Disable HT or hardware transactional memory

### SMoTherSpectre
- [ ] Disable HT
- [ ] Retpolines in userspace
