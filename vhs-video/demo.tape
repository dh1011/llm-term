# VHS documentation
# https://github.com/charmbracelet/vhs

# Output file
Output demo.gif

# Require llm-term to be available
Require llm-term

# Set up the terminal
Set Shell "powershell"
Set FontSize 24
Set Width 1200
Set Height 600
Set Padding 20

Type "llm-term 'List files in the current directory'"
Enter
Sleep 3s

Type "y"
Enter
Sleep 2s

Type "llm-term 'Create a new directory called test_folder'"
Enter
Sleep 3s

Type "y"
Enter
Sleep 2s

Type "llm-term 'Move into the test_folder and create a text file called hello.txt with the content Hello, LLM-Term!'"
Enter
Sleep 3s

Type "y"
Enter
Sleep 2s

Type "cat test_folder/hello.txt"
Enter
Sleep 3s

# End the demo
Type "echo 'Thanks for watching the LLM-Term demo!'"
Enter
Sleep 3s
