#include <iostream>
#include <cstdlib>
#include <sstream>
#include <vector>
#include <fstream>
#include <filesystem>
#include <stdbool.h>
#include <unistd.h>   // Needed for access(), fork(), execv()
#include <sys/wait.h> // Needed for waitpid()

std::vector<std::string> split_string(const std::string &s, char delimiter)
{
  std::stringstream ss(s);
  std::vector<std::string> return_vect;
  std::string token;
  while (getline(ss, token, delimiter))
  {
    return_vect.push_back(token);
  }
  return return_vect;
}

/**
 * @brief Checks for an executable file in the given PATH directories.
 *
 * @param filename The name of the file (command) to search for.
 * @param path_env A vector of directory paths from the PATH variable.
 * @return std::string The full path to the first executable file found, or an empty string if not found.
 */
std::string checkFileInPath(std::string filename, std::vector<std::string> path_env)
{
  std::string filepath_str;
  for (int i = 0; i < path_env.size(); i++)
  {
    // Handle empty strings in PATH (e.g., "::" or trailing ":")
    if (path_env[i].empty())
    {
      continue;
    }

    // Use std::filesystem to correctly join the directory and filename
    std::filesystem::path full_path = std::filesystem::path(path_env[i]) / filename;
    filepath_str = full_path.string();

    // The logic required by the challenge:
    // 1. Check if the file exists.
    if (access(filepath_str.c_str(), F_OK) == 0)
    {
      // 2. If it exists, check if it has execute permissions.
      if (access(filepath_str.c_str(), X_OK) == 0)
      {
        // Found a file that exists AND is executable. Return it.
        return filepath_str;
      }
      // If file exists but is NOT executable, we skip it and continue the loop.
    }
    // If file does not exist in this directory, continue the loop.
  }
  // Searched all directories and found no executable match.
  return "";
}

void handle_type_command(std::vector<std::string> arguments, std::vector<std::string> path)
{
  // handle multiple types like a shell: ex 'type exit echo type ls cat' will return 5 lines similar to a single
  for (int i = 1; i < arguments.size(); i++)
  {
    if (arguments[i] == "echo" || arguments[i] == "exit" || arguments[i] == "type" || arguments[i] == "pwd" || arguments[i] == "cd")
    {
      std::cout << arguments[i] << " is a shell builtin\n";
    }
    else
    {
      // FIX 2: Use arguments[i] instead of arguments[1]
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
  if (directory.empty() || directory == "~")
  { // Handle "cd" or "cd ~"
    const char *home_dir = getenv("HOME");
    if (home_dir)
    {
      std::filesystem::current_path(home_dir);
    }
    else
    {
      // Fallback or error if HOME is not set, though for this challenge it should be.
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

  std::vector<std::string> path = split_string(getenv("PATH"), ':');

  std::string input;
  std::vector<std::string> arguments;

  while (true)
  {
    std::cout << "$ ";
    std::getline(std::cin, input);

    arguments = split_string(input, ' ');

    // Handle empty input
    if (arguments.empty() || arguments[0].empty())
    {
      continue;
    }

    if (arguments[0] == "exit")
    {
      // The challenge specifies "exit 0" but handles "exit" as well.
      // Let's just handle "exit" and any argument.
      return 0;
    }
    else if (arguments[0] == "echo")
    {
      // Handle "echo" with no arguments
      if (arguments.size() > 1)
      {
        std::cout << input.substr(input.find(" ") + 1) << "\n";
      }
      else
      {
        std::cout << "\n";
      }
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
        // "cd" with no arguments, go home
        handle_change_directory("~");
      }
      else
      {
        handle_change_directory(arguments[1]);
      }
    }
    else
    {
      // This is for executing external commands.
      // Now that checkFileInPath is fixed, this will also find the *correct* executable.
      std::string filepath = checkFileInPath(arguments[0], path);
      if (filepath.length() != 0)
      {
        // We found the executable. Time to fork and exec.

        // 1. Prepare arguments for execv
        // std::vector<std::string> arguments already has the command and its args.
        // We need a null-terminated array of C-style strings (char*).
        std::vector<char *> argv_c;
        for (const auto &arg : arguments)
        {
          // We cast away const, which is generally unsafe, but required by
          // the execv signature (which predates const). It's safe here
          // as execv doesn't modify the strings.
          argv_c.push_back(const_cast<char *>(arg.c_str()));
        }
        argv_c.push_back(NULL); // execv requires a NULL-terminated array

        // 2. Fork the process
        pid_t child_pid = fork();

        if (child_pid == -1)
        {
          // Fork failed
          perror("fork");
        }
        else if (child_pid == 0)
        {
          // This is the child process
          // execv replaces the current process image with the new program.
          if (execv(filepath.c_str(), argv_c.data()) == -1)
          {
            // execv only returns if an error occurred
            perror("execv");
            exit(1); // Exit child process with an error code
          }
        }
        else
        {
          // This is the parent process (the shell)
          int status;
          // Wait for the child process to terminate
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