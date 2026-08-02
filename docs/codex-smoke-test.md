# Codex review smoke test

Throwaway file to verify the @codex review bot responds on this repository.
The snippet below contains a deliberate off-by-one bug as review bait:

```cpp
// Returns the last element of a non-empty vector.
int last(const std::vector<int> &v) {
    return v[v.size()];
}
```

This file will be deleted once the review loop is confirmed working.
