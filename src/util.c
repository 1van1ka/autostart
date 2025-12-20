#include <ctype.h>
#include <string.h>
#include "util.h"

/**
 * Removes leading and trailing whitespace from a string
 * @param str String to trim (modified in place)
 * @return Pointer to the trimmed string
 */
char *trim(char *str) {
  char *end;

  // Trim leading whitespace
  while (isspace((unsigned char)*str))
    str++;

  // All spaces?
  if (*str == 0)
    return str;

  // Trim trailing whitespace
  end = str + strlen(str) - 1;
  while (end > str && isspace((unsigned char)*end))
    end--;

  // Write new null terminator
  *(end + 1) = 0;
  return str;
}

/**
 * Removes desktop entry specifiers (%u, %f, etc.) from command string
 * @param cmd Command string to clean (modified in place)
 */
void remove_desktop_specifiers(char *cmd) {
  char *src = cmd;
  char *dst = cmd;

  while (*src) {
    if (*src == '%') {
      // Skip the % and the next character
      src++;
      if (*src) {
        // Skip the specifier character (u, f, F, i, etc.)
        src++;
      }
      continue;
    }
    *dst++ = *src++;
  }
  *dst = '\0';
}
