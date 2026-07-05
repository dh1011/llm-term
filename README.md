# LLM-Term

LLM-Term is a CLI that turns plain English prompts into shell commands, shows the generated command, and asks before execution.

This repository now carries two implementations:

- Rust: the primary implementation at the repository root.
- C/Cosmopolitan: a portable executable port under `ports/cosmopolitan/`.

The Rust implementation should remain the reference for normal development. The C port should track the same user-facing behavior where practical, with build and runtime decisions optimized for a self-contained portable binary.

## Layout

```text
.
├── Cargo.toml
├── Cargo.lock
├── src/
├── docs/
├── testdata/
└── ports/
    └── cosmopolitan/
        ├── Makefile
        ├── src/
        ├── tests/
        ├── certs/
        ├── generated/
        └── third_party/
```

## Rust Build

```sh
cargo build --release
```

The binary is written to `target/release/`.

Run:

```sh
llm-term "show the largest files in this directory"
```

Run setup:

```sh
llm-term --config
```

## Portable C Build

```sh
cd ports/cosmopolitan
make
```

The portable Windows-launchable artifact is written to:

```text
ports/cosmopolitan/dist/llm-term.exe
```

On Unix-like systems, the same file can be copied or renamed to `llm-term` and marked executable.

```sh
cp ports/cosmopolitan/dist/llm-term.exe llm-term
chmod +x llm-term
./llm-term --help
```

## Configuration And Cache

Both implementations keep user-visible `config.json` and `cache.json` files next to the executable by default.

See:

- `docs/config.md`
- `docs/cache.md`
- `docs/providers.md`
- `docs/portable-build.md`

## Safety

Generated commands are displayed before execution. A command only runs after explicit `y` confirmation.
