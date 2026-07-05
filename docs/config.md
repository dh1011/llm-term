# Config Format

The Rust implementation keeps the original config format:

```json
{
  "model": "gpt-4o-mini",
  "max_tokens": 1024
}
```

For Ollama:

```json
{
  "model": {
    "ollama": "llama3.1"
  },
  "max_tokens": 1024
}
```

The C/Cosmopolitan port also reads those legacy shapes and writes this expanded shape when setup is run:

```json
{
  "provider": "openai",
  "model": "gpt-5.4-mini",
  "max_tokens": 1024,
  "temperature": 0.5,
  "base_url": "https://api.openai.com/v1"
}
```
