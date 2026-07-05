# Cache Format

The cache file is `cache.json` next to the executable by default.

The stable format is a JSON object mapping prompt text to command text:

```json
{
  "show files": "ls"
}
```

Implementations may add cache-management prompts, but they should continue reading this object shape.
