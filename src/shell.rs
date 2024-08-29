/// Enum representing different types of shells.
pub(crate) enum Shell {
    Powershell,
    BornAgainShell,
    Zsh,
    Fish,
    DebianAlmquistShell,
    KornShell,
    CShell,
    Unknown,
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

    /// Converts the shell type to a shell command and a command argument.
    pub fn to_shell_command_and_command_arg(&self) -> (String, String) {
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