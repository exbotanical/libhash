# lib.hashed

Collision-free hash tables for C.

Implemented as an open-addressed, double-hashed table.

For best performance, initialize with a prime number.

## Dynamic Linking

Linking to `lib.hashed`:

```bash
# 1) include and use lib.hashed in your project
# 2) generate object file for your project
gcc -I ../path/to/lib.hashed -c main.c -o main.o
# 3) generate shared object file
make
# 4) link your project to lib.hashed
gcc -o main main.o -L../path/to/lib.hashed -lhashed
# you may need to add the lib location to your PATH
```

## TODOs

- [ ] add docs
- [ ] remove dev deps from prod compilation steps
