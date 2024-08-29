use openai_api_rust::{Auth, Message, OpenAI, Role};
use openai_api_rust::chat::{ChatApi, ChatBody};
use serde::{Deserialize, Serialize};
use crate::Config;
use crate::shell::Shell;

#[derive(Serialize, Deserialize)]
pub enum Model {
    #[serde(rename = "gpt-4o")]
    OpenAiGpt4o,

    #[serde(rename = "gpt-4o-mini")]
    OpenAiGpt4oMini,

    #[serde(rename = "ollama")]
    Ollama(String),
}

impl Model {
    pub fn llm_get_command(&self, config: &Config, user_prompt: &str) -> Result<Option<String>, Box<dyn std::error::Error>> {
        let model_name = self.get_model_name();
        let auth = self.get_auth();
        let client = OpenAI::new(auth, self.get_openai_endpoint().as_str());

        let system_prompt = Shell::detect().to_system_prompt();

        let body = ChatBody {
            model: model_name,
            max_tokens: Some(config.max_tokens),
            temperature: Some(0.5),
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
                Message { role: Role::User, content: user_prompt.to_string() }
            ],
        };

        match client.chat_completion_create(&body) {
            Ok(response) => Ok(response.choices.first()
                .map(|choice| choice.message.as_ref())
                .flatten()
                .map(|message| message.content.clone())
            ),
            Err(e) => Err(format!("Error: {:?}", e).into()),
        }
    }

    fn get_model_name(&self) -> String {
        match self {
            Model::OpenAiGpt4o => "gpt-4o".to_string(),
            Model::OpenAiGpt4oMini => "gpt-4o-mini".to_string(),
            Model::Ollama(model_name) => model_name.to_string(),
        }
    }

    fn get_openai_endpoint(&self) -> String {
        match self {
            Model::OpenAiGpt4o => "https://api.openai.com/v1/".to_string(),
            Model::OpenAiGpt4oMini => "https://api.openai.com/v1/".to_string(),
            Model::Ollama(_) => "http://localhost:11434/v1/".to_string(),
        }
    }

    fn get_auth(&self) -> Auth {
        match self {
            Model::OpenAiGpt4o => Auth::from_env().expect("OPENAI_API_KEY environment variable not set"),
            Model::OpenAiGpt4oMini => Auth::from_env().expect("OPENAI_API_KEY environment variable not set"),
            Model::Ollama(_) => Auth::new("ollama"),
        }
    }
}
