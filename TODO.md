- [ ] Implement assignment operators
- [ ] Implement swap
- [ ] Migrate scuffed tests to GTest
- [ ] Benchmarks

- Hash set and hash map
    - Currently, `insert()` automatically grows the hash table when curr_size + 1 exceeds
    the maximum available size, even when the current `insert()` is not known to actually
    require an insert (maybe the current value or key already exists in the hash set/table).
    Fix this.