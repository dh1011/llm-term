# 🖥️ LLM-Term

A Rust-based CLI tool that generates and executes terminal commands using OpenAI's language models or local Ollama models.

## Features

- Configurable model and token limit (gpt-4o-mini, gpt-4o, or Ollama)
- Generate and execute terminal commands based on user prompts
- Works on both PowerShell and Unix-like shells (Automatically detected)

## Demo

![LLM-Term Demo](vhs-video/demo.gif)

## Installation

- Download the binary from the [Releases](https://github.com/dh1011/llm-term/releases) page

- Set PATH to the binary

    - MacOS/Linux:
    ```
    export PATH="$PATH:/path/to/llm-term"
    ```

    - Windows:
    ```
    set PATH="%PATH%;C:\path\to\llm-term"
    ```

## Development

1. Clone the repository
2. Build the project using Cargo: `cargo build --release`
3. The executable will be available in the `target/release` directory

## Usage

1. Set your OpenAI API key (if using OpenAI models):

   - MacOS/Linux:
     ```
     export OPENAI_API_KEY="sk-..."
     ```
   - Windows:
     ```
     set OPENAI_API_KEY="sk-..."
     ```

2. If using Ollama, make sure it's running locally on the default port (11434)

3. Run the application with a prompt:

   ```
   ./llm-term "your prompt here"
   ```

4. The app will generate a command based on your prompt and ask for confirmation before execution.

## Configuration

A `config.json` file will be created in the same directory as the binary on first run. You can modify this file to change the default model and token limit.

## Options

- `-c, --config <FILE>`: Specify a custom config file path

## Supported Models

- OpenAI GPT-4 (gpt-4o)
- OpenAI GPT-4 Mini (gpt-4o-mini)
- Ollama (local models, default: llama3.1)
