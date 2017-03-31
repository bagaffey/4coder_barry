#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "4coder_default_include.cpp"
#include "4coder_jump_parsing.cpp"

#if !defined(Assert)
#define Assert assert
#endif

#if !defined(internal)
#define internal static
#endif

struct Parsed_Error
{
  int exists;
  
  String target_file_name;
  int target_line_number;
  int target_column_number;
  
  int source_buffer_id;
  int source_position;
};

static bool GlobalEditMode;
static char *GlobalCompilationBufferName = "*compilation*";

static char BuildDirectory[4096] = "./";

enum token_type
{
  Token_Unknown,
  Token_OpenParen,
  Token_CloseParen,
  Token_Asterisk,
  Token_Minus,
  Token_Plus,
  Token_ForwardSlash,
  Token_Percent,
  Token_Colon,
  Token_Number,
  Token_Comma,
  Token_EndOfStream,
};

struct token
{
  token_type Type;
  size_t TextLength;
  char *Text;
};

struct tokenizer
{
  char *At;
};

inline bool
IsEndOfLine(char C)
{
  bool Result = ((C == '\n') ||
                 (C == '\r'));
  return(Result);
}

inline bool
IsWhitespace(char C)
{
  bool Result = ((C == ' ') ||
                 (C == '\t') ||
                 (C == '\v') ||
                 (C == '\f') ||
                 IsEndOfLine(C));
  return(Result);
}
