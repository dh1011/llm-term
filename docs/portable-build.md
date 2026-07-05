# Portable Build

The portable build lives in `ports/cosmopolitan/`.

Build:

```sh
cd ports/cosmopolitan
make
```

Test:

```sh
cd ports/cosmopolitan
make test
```

Output:

```text
ports/cosmopolitan/dist/llm-term.exe
```

The `.exe` suffix is required for normal Windows shell and Explorer launching. Unix-like systems can run the same APE binary with any filename after executable permissions are set.

The current local build is x86_64. A fat x86_64+aarch64 build can be added later when release time justifies the longer build.
