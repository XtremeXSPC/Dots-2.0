#ifndef BREW_H
#define BREW_H

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sysctl.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

// --- Constants ---

/** @brief Absolute path to the Homebrew executable. Using an absolute path is crucial for robustness when running from environments like
 * Sketchybar, which may have a minimal PATH. */
static const char* BREW_EXECUTABLE_PATH = "/opt/homebrew/bin/brew";

/** @brief Maximum length for a single package name. */
static const int BREW_MAX_PACKAGE_NAME = 128;

/** @brief The initial size of the buffer that stores the concatenated list of outdated packages. */
static const int BREW_INITIAL_BUFFER_SIZE = 1024;

/** @brief The absolute maximum size for the package list buffer to prevent uncontrolled memory allocation. */
static const int BREW_MAX_BUFFER_SIZE = 16384;

// --- Error Codes ---

/**
 * @enum brew_error_t
 * @brief Defines possible error codes for brew operations.
 */
typedef enum {
  BREW_SUCCESS = 0,              /**< Operation completed successfully. */
  BREW_ERROR_NOT_INSTALLED,      /**< Homebrew executable not found. */
  BREW_ERROR_UPDATE_IN_PROGRESS, /**< An update operation is already running. */
  BREW_ERROR_MEMORY_ALLOCATION,  /**< Failed to allocate memory (malloc, realloc). */
  BREW_ERROR_COMMAND_EXECUTION,  /**< Failed to fork or execute a brew command. */
  BREW_ERROR_PIPE_CREATION,      /**< Failed to create a pipe for IPC. */
  BREW_ERROR_BUFFER_OVERFLOW,    /**< The list of outdated packages exceeds the maximum buffer size. */
  BREW_ERROR_INVALID_STATE,      /**< An operation was called on an uninitialized or invalid structure. */
} brew_error_t;

// --- Main Data Structure ---

/**
 * @struct brew_t
 * @brief Holds the state of Homebrew information.
 *
 * This structure tracks the number of outdated packages, a list of their names,
 * and metadata about when checks and updates were last performed.
 */
typedef struct {
  int          outdated_count;     /**< Number of outdated packages. */
  char*        package_list;       /**< Comma-separated string of outdated package names. */
  size_t       package_list_size;  /**< Current allocated size of package_list buffer. */
  time_t       last_update;        /**< Timestamp of the last successful `brew update`. */
  time_t       last_check;         /**< Timestamp of the last check for outdated packages. */
  brew_error_t last_error;         /**< The last error that occurred during an operation. */
  bool         update_in_progress; /**< Flag to prevent concurrent updates. */
} brew_t;

// --- Private Helper Function Prototypes ---

[[nodiscard]] static brew_error_t _brew_execute_command(const char* args[], char** output_buffer, size_t* buffer_size);
[[nodiscard]] static brew_error_t _brew_resize_buffer(brew_t* brew, size_t required_size);
static int                        _get_cpu_core_count();

// --- Public API ---

/**
 * @brief Initializes the brew state structure.
 * @param brew A pointer to the brew_t struct to initialize.
 * @return BREW_SUCCESS on success, or an error code on failure.
 */
[[nodiscard]] static inline brew_error_t brew_init(brew_t* brew) {
  if (!brew)
    return BREW_ERROR_INVALID_STATE;

  memset(brew, 0, sizeof(brew_t));
  brew->package_list = malloc(BREW_INITIAL_BUFFER_SIZE);
  if (!brew->package_list) {
    return BREW_ERROR_MEMORY_ALLOCATION;
  }
  brew->package_list[0]   = '\0';
  brew->package_list_size = BREW_INITIAL_BUFFER_SIZE;
  brew->last_error        = BREW_SUCCESS;

  // Check if brew is installed right away.
  if (access(BREW_EXECUTABLE_PATH, X_OK) != 0) {
    brew->last_error = BREW_ERROR_NOT_INSTALLED;
    return BREW_ERROR_NOT_INSTALLED;
  }

  return BREW_SUCCESS;
}

/**
 * @brief Frees all resources associated with the brew state.
 * @param brew A pointer to the brew_t struct to clean up.
 */
static inline void brew_cleanup(brew_t* brew) {
  if (brew) {
    free(brew->package_list);
    brew->package_list = NULL;
  }
}

/**
 * @brief Checks if a `brew update` operation is needed based on a time interval and system load.
 * @param brew A pointer to the brew_t struct.
 * @param update_interval_seconds The minimum time in seconds that must pass before a new update.
 * @return True if an update is needed, false otherwise.
 */
[[nodiscard]] static inline bool brew_needs_update(const brew_t* brew, int update_interval_seconds) {
  if (!brew)
    return false;

  // Time check
  time_t current_time = time(NULL);
  if ((current_time - brew->last_update) < update_interval_seconds) {
    return false;
  }

  // System load check to avoid running updates on a busy system.
  double load[1];
  if (getloadavg(load, 1) == 1) {
    static int core_count = 0;
    if (core_count == 0)
      core_count = _get_cpu_core_count();
    // The threshold is 75% of the number of cores.
    double high_load_threshold = (double)core_count * 0.75;
    if (load[0] > high_load_threshold) {
      return false; // Defer update if system is busy
    }
  }
  return true;
}

/**
 * @brief Runs `brew update` and then gets the list of outdated packages.
 * @param brew A pointer to the brew_t struct to update with new data.
 * @return BREW_SUCCESS on success, or an error code on failure.
 */
[[nodiscard]] static inline brew_error_t brew_fetch_outdated(brew_t* brew) {
  if (!brew)
    return BREW_ERROR_INVALID_STATE;
  if (brew->update_in_progress)
    return BREW_ERROR_UPDATE_IN_PROGRESS;

  brew->update_in_progress = true;
  brew->last_check         = time(NULL);

  // Step 1: Run `brew update`
  const char*  update_args[] = {BREW_EXECUTABLE_PATH, "update", NULL};
  brew_error_t err           = _brew_execute_command(update_args, NULL, NULL);

  if (err != BREW_SUCCESS) {
    brew->last_error         = err;
    brew->update_in_progress = false;
    return err;
  }
  brew->last_update = time(NULL);

  // Step 2: Run `brew outdated --quiet` to get the list.
  const char* outdated_args[] = {BREW_EXECUTABLE_PATH, "outdated", "--quiet", NULL};
  char*       package_output  = NULL;
  size_t      output_size     = 0;
  err                         = _brew_execute_command(outdated_args, &package_output, &output_size);

  if (err != BREW_SUCCESS) {
    free(package_output);
    brew->last_error         = err;
    brew->update_in_progress = false;
    return err;
  }

  // Step 3: Parse the output and populate the struct.
  brew->outdated_count     = 0;
  brew->package_list[0]    = '\0';
  size_t package_list_used = 0;

  char* line = strtok(package_output, "\n");
  while (line != NULL) {
    if (line[0] == '\0') {
      line = strtok(NULL, "\n");
      continue;
    }

    brew->outdated_count++;
    size_t line_len       = strlen(line);
    size_t required_space = package_list_used + line_len + 2; // +1 for comma, +1 for null terminator

    if (required_space > brew->package_list_size) {
      err = _brew_resize_buffer(brew, required_space);
      if (err != BREW_SUCCESS) {
        free(package_output);
        brew->last_error         = err;
        brew->update_in_progress = false;
        return err;
      }
    }

    if (package_list_used > 0) {
      strcat(brew->package_list, ",");
      package_list_used++;
    }
    strcat(brew->package_list, line);
    package_list_used += line_len;

    line = strtok(NULL, "\n");
  }

  free(package_output);
  brew->update_in_progress = false;
  brew->last_error         = BREW_SUCCESS;
  return BREW_SUCCESS;
}

/**
 * @brief Gets a human-readable string for a brew_error_t code.
 * @param error The error code.
 * @return A constant string describing the error.
 */
[[nodiscard]] static inline const char* brew_error_string(brew_error_t error) {
  switch (error) {
  case BREW_SUCCESS:
    return "Success";
  case BREW_ERROR_NOT_INSTALLED:
    return "Homebrew not found";
  case BREW_ERROR_UPDATE_IN_PROGRESS:
    return "Update already in progress";
  case BREW_ERROR_MEMORY_ALLOCATION:
    return "Memory allocation failed";
  case BREW_ERROR_COMMAND_EXECUTION:
    return "Command execution failed";
  case BREW_ERROR_PIPE_CREATION:
    return "IPC pipe creation failed";
  case BREW_ERROR_BUFFER_OVERFLOW:
    return "Output buffer overflow";
  case BREW_ERROR_INVALID_STATE:
    return "Invalid state";
  default:
    return "Unknown error";
  }
}

// --- Private Helper Function Implementations ---

/**
 * @brief [Private] Executes a command and captures its standard output.
 *
 * This function is the robust replacement for `popen` and `system`. It uses
 * `fork`, `execv`, and `pipe` for full control over process execution.
 *
 * @param args Null-terminated array of strings representing the command and its arguments.
 * @param output_buffer A pointer to a char pointer that will be allocated to store the output. The caller must free this buffer. If NULL,
 * output is discarded.
 * @param buffer_size A pointer to a size_t to store the size of the output buffer. Can be NULL if output is discarded.
 * @return BREW_SUCCESS on success, or an error code on failure.
 */
[[nodiscard]] static inline brew_error_t _brew_execute_command(const char* args[], char** output_buffer, size_t* buffer_size) {
  int pipefd[2];
  if (output_buffer && pipe(pipefd) == -1) {
    return BREW_ERROR_PIPE_CREATION;
  }

  pid_t pid = fork();
  if (pid == -1) {
    if (output_buffer) {
      close(pipefd[0]);
      close(pipefd[1]);
    }
    return BREW_ERROR_COMMAND_EXECUTION;
  }

  if (pid == 0) { // Child process
    if (output_buffer) {
      close(pipefd[0]);               // Close unused read end
      dup2(pipefd[1], STDOUT_FILENO); // Redirect stdout to pipe
      dup2(pipefd[1], STDERR_FILENO); // Redirect stderr to pipe as well
      close(pipefd[1]);
    } else {
      // Discard output if no buffer is provided
      freopen("/dev/null", "w", stdout);
      freopen("/dev/null", "w", stderr);
    }

    execv(args[0], (char* const*)args);
    // execv only returns on error
    exit(127);
  }

  // Parent process
  if (output_buffer) {
    close(pipefd[1]); // Close unused write end

    size_t capacity = 4096;
    size_t size     = 0;
    char*  buffer   = malloc(capacity);
    if (!buffer) {
      close(pipefd[0]);
      waitpid(pid, NULL, 0);
      return BREW_ERROR_MEMORY_ALLOCATION;
    }

    ssize_t bytes_read;
    while ((bytes_read = read(pipefd[0], buffer + size, capacity - size - 1)) > 0) {
      size += bytes_read;
      if (size >= capacity - 1) {
        capacity *= 2;
        char* new_buffer = realloc(buffer, capacity);
        if (!new_buffer) {
          free(buffer);
          close(pipefd[0]);
          waitpid(pid, NULL, 0);
          return BREW_ERROR_MEMORY_ALLOCATION;
        }
        buffer = new_buffer;
      }
    }
    buffer[size] = '\0';
    close(pipefd[0]);
    *output_buffer = buffer;
    if (buffer_size)
      *buffer_size = size;
  }

  int status;
  waitpid(pid, &status, 0);
  if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
    return BREW_SUCCESS;
  }

  // If the command failed but we captured output, we don't free it
  // so the caller might be able to inspect it. But in this design,
  // we'll just free it on error to keep things simple.
  if (output_buffer && *output_buffer) {
    free(*output_buffer);
    *output_buffer = NULL;
  }
  return BREW_ERROR_COMMAND_EXECUTION;
}

/**
 * @brief [Private] Resizes the package_list buffer if needed.
 */
[[nodiscard]] static inline brew_error_t _brew_resize_buffer(brew_t* brew, size_t needed_size) {
  if (!brew)
    return BREW_ERROR_INVALID_STATE;
  if (needed_size > BREW_MAX_BUFFER_SIZE)
    return BREW_ERROR_BUFFER_OVERFLOW;

  size_t new_size = brew->package_list_size;
  while (new_size < needed_size) {
    new_size *= 2;
  }
  if (new_size > BREW_MAX_BUFFER_SIZE)
    new_size = BREW_MAX_BUFFER_SIZE;

  if (new_size > brew->package_list_size) {
    char* new_buffer = realloc(brew->package_list, new_size);
    if (!new_buffer)
      return BREW_ERROR_MEMORY_ALLOCATION;
    brew->package_list      = new_buffer;
    brew->package_list_size = new_size;
  }
  return BREW_SUCCESS;
}

/**
 * @brief [Private] Gets the number of logical CPU cores.
 */
static inline int _get_cpu_core_count() {
  int    ncpu;
  size_t len = sizeof(ncpu);
  if (sysctlbyname("hw.ncpu", &ncpu, &len, NULL, 0) == 0 && ncpu > 0) {
    return ncpu;
  }
  return 2; // Return a safe default
}

#endif /* BREW_H */
