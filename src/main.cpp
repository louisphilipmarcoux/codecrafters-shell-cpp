#include <iostream>
#include <cstdlib>
#include <sstream>
#include <vector>
#include <fstream>
#include <filesystem>
#include <stdbool.h>
#include <unistd.h>   // Needed for access(), fork(), execv()
#include <sys/wait.h> // Needed for waitpid()

/**
 * @brief Tokenizes the input string, respecting single quotes.
 *
 * This function parses the input line into a vector of arguments.
 * It handles single quotes ('') to treat quoted content, including spaces,
 * as a single token. It also handles concatenation (e.g., 'a'b'c' -> "abc").
 *
 * @param input The raw input string from the user.
 * @return std::vector<std::string> A vector of tokens (arguments).
 */
std::vector<std::string> tokenize_input(const std::string &input)
{
  std::vector<std::string> tokens;
  std::string current_token;
  bool in_quote = false;
  // This tracks if we've started building a token (even an empty one like '')
  bool token_started = false;

  for (char c : input)
  {
    if (c == '\'')
    {
      in_quote = !in_quote;
      token_started = true; // A quote always starts or is part of a token
    }
    else if (c == ' ' && !in_quote)
    {
      // Space outside quote is a delimiter
      if (token_started)
      {
        tokens.push_back(current_token);
        current_token.clear();
        token_started = false;
      }
      // else, ignore multiple spaces (token_started is already false)
    }
    else
    {
      // Any other character
      current_token += c;
      token_started = true;
    }
  }

  // Add the last token if it wasn't followed by a space
  if (token_started)
  {
    tokens.push_back(current_token);
  }

  return tokens;
}

std::string checkFileInPath(std::string filename, std::vector<std::string> path_env)
{
  // ... (This function is correct, no changes needed) ...
  std::string filepath_str;
  for (int i = 0; i < path_env.size(); i++)
  {
    if (path_env[i].empty())
    {
      continue;
    }
    std::filesystem::path full_path = std::filesystem::path(path_env[i]) / filename;
    filepath_str = full_path.string();
    if (access(filepath_str.c_str(), F_OK) == 0)
    {
      if (access(filepath_str.c_str(), X_OK) == 0)
      {
        return filepath_str;
      }
    }
  }
  return "";
}

void handle_type_command(std::vector<std::string> arguments, std::vector<std::string> path)
{
  // ... (This function is correct, no changes needed) ...
  for (int i = 1; i < arguments.size(); i++)
  {
    if (arguments[i] == "echo" || arguments[i] == "exit" || arguments[i] == "type" || arguments[i] == "pwd" || arguments[i] == "cd")
    {
      std::cout << arguments[i] << " is a shell builtin\n";
    }
    else
    {
      std::string filepath = checkFileInPath(arguments[i], path);
      if (filepath.length() != 0)
        std::cout << arguments[i] << " is " << filepath << "\n";
      else
        std::cout << arguments[i] << ": not found\n";
    }
  }
}

void handle_change_directory(std::string directory)
{
  // ... (This function is correct, no changes needed) ...
  if (directory.empty() || directory == "~")
  {
    const char *home_dir = getenv("HOME");
    if (home_dir)
    {
      std::filesystem::current_path(home_dir);
    }
    else
    {
      std::cerr << "cd: HOME not set\n";
    }
  }
  else
  {
    std::filesystem::path path(directory);
    std::error_code ec;
    if (std::filesystem::is_directory(path, ec))
    {
      std::filesystem::current_path(path, ec);
      if (ec)
      {
        std::cout << "cd: " << directory << ": " << ec.message() << "\n";
      }
    }
    else if (ec)
    {
      std::cout << "cd: " << directory << ": " << ec.message() << "\n";
    }
    else
    {
      std::cout << "cd: " << directory << ": No such file or directory\n";
    }
  }
}

int main()
{
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  // We get the PATH once at the start
  // Note: We are still using ':' as the delimiter. The challenge mentions
  // OS-agnostic path handling, but for this course, ':' is fine.
  std::vector<std::string> path;
  const char *path_env_var = getenv("PATH");
  if (path_env_var != NULL)
  {
    // Use stringstream to split the path, similar to old split_string
    std::stringstream ss(path_env_var);
    std::string segment;
    while (std::getline(ss, segment, ':'))
    {
      path.push_back(segment);
    }
  }

  std::string input;
  std::vector<std::string> arguments;

  while (true)
  {
    std::cout << "$ ";
    std::getline(std::cin, input);

    // 1. THIS IS THE FIRST BIG CHANGE
    // Replace the simple split_string with our new tokenizer
    arguments = tokenize_input(input);

    // Handle empty input
    if (arguments.empty() || arguments[0].empty())
    {
      continue;
    }

    if (arguments[0] == "exit")
    {
      return 0;
    }
    // 2. THIS IS THE SECOND BIG CHANGE
    // Update 'echo' to print the processed tokens
    else if (arguments[0] == "echo")
    {
      for (size_t i = 1; i < arguments.size(); ++i)
      {
        std::cout << arguments[i];
        if (i < arguments.size() - 1)
        {
          std::cout << " "; // Add space between arguments
        }
      }
      std::cout << "\n"; // Add newline at the end
    }
    else if (arguments[0] == "type")
    {
      handle_type_command(arguments, path);
    }
    else if (arguments[0] == "pwd")
    {
      try
      {
        std::string cwd = std::filesystem::current_path();
        std::cout << cwd << "\n";
      }
      catch (std::filesystem::filesystem_error &e)
      {
        std::cerr << "pwd: " << e.what() << "\n";
      }
    }
    else if (arguments[0] == "cd")
    {
      if (arguments.size() > 2)
      {
        std::cout << "cd: too many arguments\n";
      }
      else if (arguments.size() == 1)
      {
        handle_change_directory("~");
      }
      else
      {
        handle_change_directory(arguments[1]);
      }
    }
    else
    {
      // This external command logic is now compatible with quotes
      // thanks to the new tokenize_input function.
      std::string filepath = checkFileInPath(arguments[0], path);
      if (filepath.length() != 0)
      {
        std::vector<char *> argv_c;
        for (const auto &arg : arguments)
        {
          argv_c.push_back(const_cast<char *>(arg.c_str()));
        }
        argv_c.push_back(NULL);

        pid_t child_pid = fork();

        if (child_pid == -1)
        {
          perror("fork");
        }
        else if (child_pid == 0)
        {
          if (execv(filepath.c_str(), argv_c.data()) == -1)
          {
            perror("execv");
            exit(1);
          }
        }
        else
        {
          int status;
          if (waitpid(child_pid, &status, 0) == -1)
          {
            perror("waitpid");
          }
        }
      }
      else
      {
        std::cout << arguments[0] << ": not found\n";
      }
    }
  }
}