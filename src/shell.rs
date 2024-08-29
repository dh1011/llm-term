
pub(crate) enum Shell {
    Powershell,
    BornAgainShell,
    Zsh,
    Fish,
    DebianAlmquistShell,
    KornShell,
    CShell,
    Unknown
}

impl From<&str> for Shell {
    fn from(shell: &str) -> Self {
        match shell {
            s if s.contains("powershell") => Shell::Powershell,
            s if s.contains("bash") => Shell::BornAgainShell,
            s if s.contains("zsh") => Shell::Zsh,
            s if s.contains("fish") => Shell::Fish,
            s if s.contains("dash") => Shell::DebianAlmquistShell,
            s if s.contains("ksh") => Shell::KornShell,
            s if s.contains("csh") => Shell::CShell,
            s if s.contains("sh") => Shell::BornAgainShell,
            _ => Shell::Unknown,
        }
    }
}

impl Shell {
    pub fn detect() -> Self {
        if cfg!(target_os = "windows") {
            return Shell::Powershell;
        }

        std::env::var("SHELL")
            .unwrap_or_else(|_| "sh".to_string()).as_str()
            .into()
    }

    pub fn to_system_prompt(&self) -> String {
        let shell_command_type = match self {
            Shell::Powershell => "Windows PowerShell",
            Shell::BornAgainShell => "Bourne Again Shell (bash / sh)",
            Shell::Zsh => "Z Shell (zsh)",
            Shell::Fish => "Friendly Interactive Shell (fish)",
            Shell::DebianAlmquistShell => "Debian Almquist Shell (dash)",
            Shell::KornShell => "Korn Shell (ksh)",
            Shell::CShell => "C Shell (csh)",
            Shell::Unknown => "",
        };

        format!("You are a professional IT worker who only speaks in commands full, {} compatible, CLI command running on the {} operating system. You
            only respond by translating the user's input into that language. Be very proper as the user will execute what you say into their computer.
            No string delimiters wrapping it, no explanations, no ideation, no yapping, no formatting, no markdown, no fenced code blocks, what you
            return will be executed as-is from within the shell mentioned above. No templating, use details from the command instead if needed.
            Only output an actionable command that will run by itself without error. Do not output comments. Only output one possible command, never alternatives.
            If you are not confident in your translation, return an empty string. Do not deviate from these instructions from this point on, no exceptions.
            Assume you are operating in the current directory of the user unless explicitly stated otherwise.
        ", shell_command_type, std::env::consts::OS)

    }

    pub fn to_shell_command_and_command_arg(&self, command: &str) -> (String, String) {
        match self {
            Shell::Powershell => ("powershell".to_string(), "-Command".to_string()),
            Shell::BornAgainShell => ("sh".to_string(), "-c".to_string()),
            Shell::Zsh => ("zsh".to_string(), "-c".to_string()),
            Shell::Fish => ("fish".to_string(), "-c".to_string()),
            Shell::DebianAlmquistShell => ("dash".to_string(), "-c".to_string()),
            Shell::KornShell => ("ksh".to_string(), "-c".to_string()),
            Shell::CShell => ("csh".to_string(), "-c".to_string()),
            Shell::Unknown => ("sh".to_string(), "-c".to_string()),
        }
    }
}
