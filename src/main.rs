use std::io::{self, Write};
use std::fs;
use std::process::Command as ProcessCommand;
use serde::{Deserialize, Serialize};
use openai_api_rust::*;
use openai_api_rust::chat::*;
use clap::{Command, Arg};
use colored::*;
use std::path::PathBuf;

#[derive(Serialize, Deserialize)]
struct Config {
    model: String,
    max_tokens: i32,
}

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let matches = Command::new("llm-term")
        .version("1.0")
        .author("Your Name")
        .about("Generate terminal commands using OpenAI models")
        .arg(Arg::new("prompt")
            .help("The prompt describing the desired command")
            .required(true)
            .index(1))
        .arg(Arg::new("config")
            .short('c')
            .long("config")
            .value_name("FILE")
            .help("Sets a custom config file"))
        .get_matches();

    let config_path = if let Some(custom_config) = matches.get_one::<String>("config") {
        PathBuf::from(custom_config)
    } else {
        get_default_config_path()?
    };
    let config = load_or_create_config(&config_path)?;

    let auth = Auth::from_env().expect("OPENAI_API_KEY environment variable not set");
    let client = OpenAI::new(auth, "https://api.openai.com/v1/");

    let prompt = matches.get_one::<String>("prompt").expect("Prompt is required");
    
    let shell = detect_shell();
    let system_prompt = if shell == "powershell" {
        "Return only the PowerShell command to be executed as a raw string, no string delimiters wrapping it, no yapping, no markdown, no fenced code blocks, what you return will be passed to subprocess.check_output() directly."
    } else {
        "Return only the command to be executed as a raw string, no string delimiters wrapping it, no yapping, no markdown, no fenced code blocks, what you return will be passed to subprocess.check_output() directly."
    };

    let body = ChatBody {
        model: config.model.clone(),
        max_tokens: Some(config.max_tokens),
        temperature: Some(0.7),
        top_p: None,
        n: None,
        stream: None,
        stop: None,
        presence_penalty: None,
        frequency_penalty: None,
        logit_bias: None,
        user: None,
        messages: vec![
            Message { role: Role::System, content: system_prompt.to_string() },
            Message { role: Role::User, content: prompt.to_string() }
        ],
    };

    match client.chat_completion_create(&body) {
        Ok(response) => {
            if let Some(choice) = response.choices.first() {
                if let Some(message) = &choice.message {
                    let command = message.content.trim();
                    println!("{}", command.cyan().bold());
                    println!("{}", "Do you want to execute this command? (y/n)".yellow());
                    
                    let mut user_input = String::new();
                    io::stdin().read_line(&mut user_input)?;
                    
                    if user_input.trim().to_lowercase() == "y" {
                        let (shell_cmd, shell_arg) = if shell == "powershell" {
                            ("powershell", "-Command")
                        } else {
                            ("sh", "-c")
                        };
                        match ProcessCommand::new(shell_cmd).arg(shell_arg).arg(command).output() {
                            Ok(output) => {
                                println!("{}", "Command output:".green().bold());
                                io::stdout().write_all(&output.stdout)?;
                                io::stderr().write_all(&output.stderr)?;
                            }
                            Err(e) => eprintln!("{}", format!("Failed to execute command: {}", e).red()),
                        }
                    } else {
                        println!("{}", "Command execution cancelled.".yellow());
                    }
                }
            }
        }
        Err(e) => eprintln!("{}", format!("Error: {}", e).red()),
    }

    Ok(())
}

fn get_default_config_path() -> Result<PathBuf, Box<dyn std::error::Error>> {
    let exe_path = std::env::current_exe()?;
    let exe_dir = exe_path.parent().ok_or("Failed to get executable directory")?;
    Ok(exe_dir.join("config.json"))
}

fn load_or_create_config(path: &PathBuf) -> Result<Config, Box<dyn std::error::Error>> {
    if let Ok(content) = fs::read_to_string(path) {
        Ok(serde_json::from_str(&content)?)
    } else {
        let config = create_config()?;
        let content = serde_json::to_string_pretty(&config)?;
        fs::write(path, content)?;
        Ok(config)
    }
}

fn create_config() -> Result<Config, io::Error> {
    let model = loop {
        print!("{}", "Select model (1 for gpt-4o-mini, 2 for gpt-4o): ".cyan());
        io::stdout().flush()?;
        let mut choice = String::new();
        io::stdin().read_line(&mut choice)?;
        match choice.trim() {
            "1" => break "gpt-4o-mini".to_string(),
            "2" => break "gpt-4o".to_string(),
            _ => println!("{}", "Invalid choice. Please try again.".red()),
        }
    };

    let max_tokens = loop {
        print!("{}", "Enter max tokens (1-4096): ".cyan());
        io::stdout().flush()?;
        let mut input = String::new();
        io::stdin().read_line(&mut input)?;
        if let Ok(tokens) = input.trim().parse::<i32>() {
            if tokens > 0 && tokens <= 4096 {
                break tokens;
            }
        }
        println!("{}", "Invalid input. Please enter a number between 1 and 4096.".red());
    };

    Ok(Config {
        model,
        max_tokens,
    })
}

fn detect_shell() -> String {
    if cfg!(target_os = "windows") {
        // On Windows, we'll assume PowerShell as the default
        "powershell".to_string()
    } else {
        // On Unix-like systems, we'll check the SHELL environment variable
        std::env::var("SHELL").unwrap_or_else(|_| "sh".to_string())
    }
}
