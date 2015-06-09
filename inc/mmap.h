// Public definitions for the POSIX-like mmap metadata

#ifndef JOS_INC_MMAP_H
#define JOS_INC_MMAP_H

struct mmap_metadata {
  void* beginva;
  void* endva;
  size_t length;
  int fd;
  int prot;
  int flags;
  off_t offset;

  int perm;

  bool avail;
};

#endif	// not JOS_INC_MMAP_H
