# fast_crc32
fast_crc32 is a trivial example program of how to use the Linux kernel crypto
API to achieve very fast CRC computation. In particular, it makes use of
hardware acceleration (if present) through means of the pclmulqdq instruction.
On a i7-5930k @ 3.50GHz it achieves around 5120 MiB/sec throughput:

```
$ dd if=/dev/zero of=/dev/shm/testfile bs=1M count=8192
8192+0 records in
8192+0 records out
8589934592 bytes (8,6 GB, 8,0 GiB) copied, 2,09219 s, 4,1 GB/s

$ time ./fast_crc32 /dev/shm/testfile
/dev/shm/testfile 48674bc7

real	0m1,576s
user	0m0,008s
sys	0m1,568s

$ time ./fast_crc32 /dev/shm/testfile
/dev/shm/testfile 48674bc7

real	0m1,582s
user	0m0,008s
sys	0m1,574s
```

# License
Public Domain.
