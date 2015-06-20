JOS-MMAP
========

### How to run the demo

#### user/mmaptest.c

This file is used for the correctness validation of mmap and munmap.

It first creates a file and write something to it. Then we mmap the same region of the file to two different virtual addresses with MAP\_PRIVATE and MAP\_SHRAED flags respectively.

Then it tests the behavior of the two flags are correct. For more details, please refer to the comments.

```bash
$ make clean && make run-mmaptest
```

You will see all assertions pass.

#### user/benchmark.c
