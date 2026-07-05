# Providers

LLM-Term supports OpenAI-compatible chat-completions providers.

## OpenAI

OpenAI uses `OPENAI_API_KEY` from the environment.

Default base URL:

```text
https://api.openai.com/v1
```

## Ollama

Ollama is treated as an OpenAI-compatible local endpoint.

Default base URL:

```text
http://localhost:11434/v1
```

Default local model:

```text
llama3.1
```
