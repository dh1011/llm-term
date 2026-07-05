# Behavior Contract

Both implementations should preserve the same core workflow:

1. Read a prompt from the CLI.
2. Resolve configuration next to the executable unless a custom path is supported.
3. Check the prompt cache unless cache is disabled.
4. Request one shell command from the selected provider.
5. Print the command.
6. Ask for confirmation before execution.
7. Execute through the detected shell with inherited terminal behavior where possible.

The generated model response should be treated as a command string, not an explanation or Markdown document.
