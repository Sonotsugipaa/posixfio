# Posixfio

**Posixfio** is a C++ library that wraps around POSIX functions for reading files,
in addition to a few utility functions (such as `readAll`, `readLeast`,
`writeAll`, `writeLeast`) and I/O buffers (both array-based and dynamically
allocated).

Posixfio is written with Linux in mind, but it does have an incomplete and
non-identical MSVC implementation that works for simple applications  
(like, for example, *open -> read -> write -> close* chains).

The main advantages over STL streams are the smaller overhead and the greater
control over file operations (and the buffers themselves).  
The biggest disadvantages are:
- input needs type-agnostic parsing, due to `::read`, `::write` and `::mmap`
  using `void*`;
- formatted output requires some labor;
- `::open` takes a C-style string for file names, requiring a null-character
  terminator (which is less of a big disadvantage and more of a massive,
  seemingly avoidable inconvenience).
