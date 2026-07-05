# LLM-Term

A C99 command-line tool that asks OpenAI or local Ollama models to translate plain English into a shell command, shows the generated command, and asks before running it.

The release target is a single Cosmopolitan executable:

```sh
make
```

The output is:

```text
dist/llm-term.exe
```

## Features

- OpenAI-compatible chat request format for both OpenAI and Ollama
- Interactive setup with `-c`, `--config`, or `--setup`
- JSON config and prompt-to-command cache next to the executable by default
- `--config-file PATH` for custom config location
- `--disable-cache` to force a fresh model request
- Windows PowerShell and Unix-like shell detection
- HTTPS with Mbed TLS and an embedded Mozilla CA bundle

## Build

Install the Cosmopolitan compiler so `cosmocc` is on `PATH`, then run:

```sh
make
```

The Makefile builds vendored Mbed TLS as static libraries with CMake, then links the application into `dist/llm-term.exe`.

For local unit tests that do not require network access or the TLS build:

```sh
make test
```

Clean generated build output with:

```sh
make clean
```

## Usage

Set an OpenAI API key if using OpenAI:

```sh
export OPENAI_API_KEY="sk-..."
```

On Windows PowerShell:

```powershell
$env:OPENAI_API_KEY = "sk-..."
```

Run setup:

```sh
llm-term --setup
```

Generate and optionally run a command:

```sh
llm-term "show the largest files in this directory"
```

Use a custom config path:

```sh
llm-term --config-file ./config.json "list git branches"
```

Bypass cache:

```sh
llm-term --disable-cache "find TODO comments"
```

## Configuration

`config.json` uses this schema:

```json
{
  "provider": "openai",
  "model": "gpt-5.4-mini",
  "max_tokens": 1024,
  "temperature": 0.5,
  "base_url": "https://api.openai.com/v1"
}
```

For Ollama:

```json
{
  "provider": "ollama",
  "model": "llama3.1",
  "max_tokens": 1024,
  "temperature": 0.5,
  "base_url": "http://localhost:11434/v1"
}
```

Existing Rust-era config shapes are accepted on read, including:

```json
{"model":"gpt-4o-mini","max_tokens":1024}
```

and:

```json
{"model":{"ollama":"llama3.1"},"max_tokens":1024}
```

Setup writes the new schema.

## Limits

- The executable targets platforms supported by Cosmopolitan; it is not guaranteed to run on literally every operating system.
- Proxy support is not implemented.
- The generated command is shown before execution and only runs after a `y` confirmation.
