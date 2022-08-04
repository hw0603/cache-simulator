# cache-simulator
Simple set-associative cache simulator written in C

## Usage
```
./cachesim -s=<cache size(in Bytes)> -a=<set size> -b=<block size(in Bytes)> -f=<trace file name>
```

## Trace file format
```
<insType> <Non-memory access insCnt> <Memory address>

insType: 0(LOAD), 1(STORE)
insCnt: The number of non-memory-access instructions executed after accessing the address specified by the third argument
Address: 64-Bit physical memory address in decimal

ex)
0 16 12521612112
1 11 82423849574
#eof
```
trace file should include `#eof` mark at the end of the file

## Test Environment
Ubuntu 20.04 (WSL2, Windows 10 x64)  
gcc version 9.4.0 (Ubuntu 9.4.0-1ubuntu1~20.04.1)
